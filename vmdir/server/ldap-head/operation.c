/*
 * Copyright © 2012-2015 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */



#include "includes.h"

/*
 * Initialize a VDIR_OPERTION structure
 * 1. opType :      OPERATION TYPE
 * 2. pSchemaCtx:   SCHEMA CONTEXT
 */
DWORD
VmDirInitStackOperation(
    PVDIR_OPERATION         pOp,
    VDIR_OPERATION_TYPE     opType,
    ber_tag_t               requestCode,
    PVDIR_SCHEMA_CTX        pSchemaCtx
    )
{
    DWORD               dwError = 0;
    PVDIR_SCHEMA_CTX    pLocalSchemaCtx = NULL;

    BAIL_ON_VMDIR_INVALID_POINTER( pOp, dwError );

    memset( pOp, 0, sizeof( *pOp ) );

    pOp->opType  = opType;
    pOp->reqCode = requestCode;

    if ( pOp->reqCode == LDAP_REQ_ADD )
    {
        dwError = VmDirAllocateMemory( sizeof(*(pOp->request.addReq.pEntry)),
                                       (PVOID)&(pOp->request.addReq.pEntry) );
        BAIL_ON_VMDIR_ERROR( dwError );
    }

    if ( pSchemaCtx )
    {
        pLocalSchemaCtx = VmDirSchemaCtxClone( pSchemaCtx );
        if ( !pLocalSchemaCtx )
        {
            dwError = ERROR_NO_SCHEMA;
            BAIL_ON_VMDIR_ERROR(dwError);
        }
    }
    else
    {
        dwError = VmDirSchemaCtxAcquire(&pLocalSchemaCtx);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirAllocateMemory( sizeof( *pOp->pBECtx ), (PVOID) &(pOp->pBECtx) );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateConnection(&pOp->conn);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirWriteQueueElementAllocate(&pOp->pWriteQueueEle);
    BAIL_ON_VMDIR_ERROR(dwError);

    pOp->bOwnConn = TRUE;
    pOp->pSchemaCtx = pLocalSchemaCtx;
    pLocalSchemaCtx = NULL;

cleanup:

   return dwError;

error:

    if ( pLocalSchemaCtx )
    {
        VmDirSchemaCtxRelease( pLocalSchemaCtx );
    }

   goto cleanup;
}

int
VmDirExternalOperationCreate(
    BerElement*       ber,
    ber_int_t         msgId,
    ber_tag_t         reqCode,
    PVDIR_CONNECTION  pConn,
    PVDIR_OPERATION*  ppOperation
    )
{
   int retVal = 0;
   PVDIR_OPERATION pOperation = NULL;

   VmDirLog( LDAP_DEBUG_TRACE, "NewOperation: Begin" );

   retVal = VmDirAllocateMemory( sizeof(*pOperation), (PVOID *)&pOperation );
   BAIL_ON_VMDIR_ERROR( retVal );

   pOperation->protocolVer = 0;
   pOperation->protocol = 0;
   pOperation->reqCode = reqCode;
   pOperation->ber = ber;
   pOperation->msgId = msgId;
   pOperation->conn = pConn;
   pOperation->bOwnConn = FALSE;
   pOperation->opType = VDIR_OPERATION_TYPE_EXTERNAL;

   retVal = VmDirAllocateMemory( sizeof(*pOperation->pBECtx), (PVOID)&(pOperation->pBECtx));
   BAIL_ON_VMDIR_ERROR( retVal );

   retVal = VmDirSchemaCtxAcquire(&pOperation->pSchemaCtx);
   BAIL_ON_VMDIR_ERROR( retVal );

   if ( pOperation->reqCode == LDAP_REQ_ADD )
   {
       retVal = VmDirAllocateMemory( sizeof(*(pOperation->request.addReq.pEntry)),
                                     (PVOID)&(pOperation->request.addReq.pEntry) );
       BAIL_ON_VMDIR_ERROR( retVal );
   }

   retVal = VmDirWriteQueueElementAllocate(&pOperation->pWriteQueueEle);
   BAIL_ON_VMDIR_ERROR(retVal);

   *ppOperation = pOperation;

cleanup:
   VmDirLog( LDAP_DEBUG_TRACE, "%s: End", __FUNCTION__ );

   return retVal;

error:
   VmDirLog( LDAP_DEBUG_TRACE, "%s: acquire schema context failed",
           __FUNCTION__ );

   VmDirFreeOperation(pOperation);
   *ppOperation = NULL;

   goto cleanup;
}

void
VmDirFreeOperation(
    PVDIR_OPERATION pOperation
    )
{
    if (pOperation)
    {
        VmDirFreeOperationContent(pOperation);
        VMDIR_SAFE_FREE_MEMORY(pOperation);
    }
}

void
VmDirFreeOperationContent(
    PVDIR_OPERATION op
    )
{
    DWORD dwError = ERROR_SUCCESS;

    if (op)
    {
        if(op->raftPingCtrl)
        {
            VDIR_RAFT_PING_CONTROL_VALUE* csv = &op->raftPingCtrl->value.raftPingCtrlVal;
            VMDIR_SAFE_FREE_MEMORY(csv->pszFQDN);
        }

        if (op->raftVoteCtrl)
        {
            VDIR_RAFT_VOTE_CONTROL_VALUE* cvv = &op->raftVoteCtrl->value.raftVoteCtrlVal;
            VMDIR_SAFE_FREE_MEMORY(cvv->pszCandidateId);
        }

        if (op->statePingCtrl)
        {
            VDIR_STATE_PING_CONTROL_VALUE* csv = &op->statePingCtrl->value.statePingCtrlVal;
            VMDIR_SAFE_FREE_MEMORY(csv->pszFQDN);
            VMDIR_SAFE_FREE_MEMORY(csv->pszInvocationId);
        }

        if (op->syncDoneCtrl)
        {
            PLW_HASHTABLE_NODE      pNode = NULL;
            LW_HASHTABLE_ITER       iter = LW_HASHTABLE_ITER_INIT;
            UptoDateVectorEntry *   pUtdVectorEntry = NULL;
            SyncDoneControlValue *  syncDoneCtrlVal = &op->syncDoneCtrl->value.syncDoneCtrlVal;

            while ((pNode = LwRtlHashTableIterate(syncDoneCtrlVal->htUtdVector, &iter)))
            {
                dwError = LwRtlHashTableRemove(syncDoneCtrlVal->htUtdVector, pNode);
                assert(dwError == 0);
                pUtdVectorEntry = LW_STRUCT_FROM_FIELD(pNode, UptoDateVectorEntry, Node);
                VmDirFreeBervalContent(&pUtdVectorEntry->invocationId);
                VMDIR_SAFE_FREE_MEMORY(pUtdVectorEntry);
            }
            LwRtlFreeHashTable(&syncDoneCtrlVal->htUtdVector);
            assert(syncDoneCtrlVal->htUtdVector == NULL);

            VMDIR_SAFE_FREE_MEMORY(op->syncDoneCtrl);
        }

        if (op->dbCopyCtrl)
        {
            VMDIR_SAFE_FREE_MEMORY(op->dbCopyCtrl->value.dbCopyCtrlVal.pszPath);
            VMDIR_SAFE_FREE_MEMORY(op->dbCopyCtrl->value.dbCopyCtrlVal.pszData);
        }

        switch (op->reqCode)
        {
            case LDAP_REQ_BIND:
                 VmDirFreeBindRequest(&op->request.bindReq, FALSE);
                 if (op->ldapResult.replyInfo.type == REP_SASL)
                 {
                     VmDirFreeBervalContent( &(op->ldapResult.replyInfo.replyData.bvSaslReply) );
                 }
                 break;

            case LDAP_REQ_ADD:
                 VmDirFreeAddRequest(&op->request.addReq, FALSE);
                 break;

            case LDAP_REQ_SEARCH:
                 VmDirFreeSearchRequest(&op->request.searchReq, FALSE);
                 break;

            case LDAP_REQ_MODIFY:
            case LDAP_REQ_MODDN:
                 VmDirFreeModifyRequest(&op->request.modifyReq, FALSE);
                 break;

            case LDAP_REQ_DELETE:
                 VmDirFreeDeleteRequest(&op->request.deleteReq, FALSE);
                 break;

            default:
                 break;
        }

        if (op->reqControls)
        {
            DeleteControls(&(op->reqControls));
        }

        VmDirFreeEntryArrayContent(&op->internalSearchEntryArray);
        VmDirFreeBervalContent(&op->reqDn);
        VMDIR_SAFE_FREE_MEMORY(op->ldapResult.pszErrMsg);
        VmDirBackendCtxFree(op->pBECtx);
        VMDIR_SAFE_FREE_MEMORY(op->pszFilters);

        if (op->bOwnConn)
        {
            VmDirDeleteConnection(&op->conn);
        }

        VmDirWriteQueueElementFree(op->pWriteQueueEle);
        op->pWriteQueueEle = NULL;

        VmDirSchemaCtxRelease(op->pSchemaCtx);
   }
}
