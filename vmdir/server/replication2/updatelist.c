/*
 * Copyright © 2018 VMware, Inc.  All Rights Reserved.
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



/*
 * Module Name: Replication update list
 *
 * Filename: updatelist.c
 *
 * Abstract:
 *
 */

#include "includes.h"

int
VmDirReplUpdateListFetch(
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr,
    PVMDIR_REPLICATION_UPDATE_LIST* ppReplUpdateList
    )
{
    int                            retVal       = LDAP_SUCCESS;
    PSTR                           pszUtdVector = NULL;
    LDAPControl*                   srvCtrls[2]  = {NULL, NULL};
    PVMDIR_REPLICATION_UPDATE_LIST pReplUpdateList = NULL;
    LDAP*                          pLd          = NULL;
    PSTR                           pszFilter    = NULL;
    LDAPMessage                    *pSearchRes   = NULL;
    LDAPControl                    **ppSearchResCtrls = NULL;
    int                            iEntriesReceived = 0;
    LDAPControl                    syncReqCtrl;
    USN                            lastSupplierUsnProcessed = 0;

    pLd = pReplAgr->dcConn.pLd;

    retVal = VmDirReplUpdateListAlloc(&pReplUpdateList);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirStringToINT64(
            pReplAgr->lastLocalUsnProcessed.lberbv.bv_val, NULL, &lastSupplierUsnProcessed);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    if (VmDirAllocateStringPrintf(
            &pszFilter,
            "%s>=%" PRId64,
            ATTR_USN_CHANGED,
            lastSupplierUsnProcessed + 1))
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    retVal = VmDirUTDVectorGlobalCacheToString(&pszUtdVector);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirCreateSyncRequestControl(
            gVmdirServerGlobals.invocationId.lberbv.bv_val,
            lastSupplierUsnProcessed,
            pszUtdVector,
            TRUE, // it's fetching first page if TRUE TODO: Not needed?
            &syncReqCtrl);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    srvCtrls[0] = &syncReqCtrl;
    srvCtrls[1] = NULL;

    retVal = ldap_search_ext_s(
            pLd,
            "",
            LDAP_SCOPE_SUBTREE,
            pszFilter,
            NULL,
            FALSE,
            srvCtrls,
            NULL,
            NULL, // V2 fetch all changes in one call, no timeout.
            LDAP_NO_LIMIT,
            &pSearchRes);

    if (retVal)
    {   // for all other errors, force disconnect
        pReplAgr->dcConn.connState = DC_CONNECTION_STATE_NOT_CONNECTED;
    }
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    iEntriesReceived = ldap_count_entries(pLd, pSearchRes);
    if (iEntriesReceived > 0)
    {
        LDAPMessage*    entry    = NULL;
        size_t          iEntries = 0;

        for (entry = ldap_first_entry(pLd, pSearchRes);
             entry != NULL;
             entry = ldap_next_entry(pLd, entry))
        {
            PVMDIR_REPLICATION_UPDATE pReplUpdate = NULL;

            retVal = VmDirReplUpdateCreate(pLd, entry, pReplAgr, &pReplUpdate);
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

            retVal = VmDirLinkedListInsertTail(pReplUpdateList->pLinkedList,
                                               (PVOID)pReplUpdate,
                                               NULL);
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

            ++iEntries;
        }

        if (iEntries != iEntriesReceived)
        {
            retVal = LDAP_OPERATIONS_ERROR;
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
        }
    }

    retVal = ldap_parse_result(pLd, pSearchRes, NULL, NULL, NULL, NULL, &ppSearchResCtrls, 0);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    if (ppSearchResCtrls[0] == NULL ||
        VmDirStringCompareA(ppSearchResCtrls[0]->ldctl_oid, LDAP_CONTROL_SYNC_DONE, TRUE) != 0)
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    retVal = VmDirReplUpdateListParseSyncDoneCtl(pReplUpdateList, ppSearchResCtrls);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    if (pReplUpdateList->newHighWaterMark > lastSupplierUsnProcessed)
    {
        VMDIR_LOG_INFO(
                    LDAP_DEBUG_REPL,
                    "%s: Updating hw from %" PRId64 "to %" PRId64,
                    __FUNCTION__,
                    lastSupplierUsnProcessed,
                    pReplUpdateList->newHighWaterMark);
    }

    *ppReplUpdateList = pReplUpdateList;

    if (iEntriesReceived > 0)
    {
        VMDIR_LOG_INFO(
                VMDIR_LOG_MASK_ALL,
                "%s: filter: '%s' to '%s' received: %d usn: %" PRId64 " utd: '%s'",
                __FUNCTION__,
                VDIR_SAFE_STRING(pszFilter),
                VDIR_SAFE_STRING(pReplAgr->dcConn.pszHostname),
                iEntriesReceived,
                lastSupplierUsnProcessed,
                VDIR_SAFE_STRING(pszUtdVector));
    }
    else
    {
        VMDIR_LOG_INFO(
                LDAP_DEBUG_REPL,
                "%s: received empty page: %s from: %s",
                __FUNCTION__,
                VDIR_SAFE_STRING(ppSearchResCtrls[0]->ldctl_value.bv_val),
                VDIR_SAFE_STRING(pReplAgr->dcConn.pszHostname));
    }

cleanup:
    if (ppSearchResCtrls)
    {
        ldap_controls_free(ppSearchResCtrls);
        ppSearchResCtrls = NULL;
    }

    if (pSearchRes)
    {
        ldap_msgfree(pSearchRes);
    }

    VmDirFreeCtrlContent(&syncReqCtrl);
    VMDIR_SAFE_FREE_MEMORY(pszFilter);
    VMDIR_SAFE_FREE_MEMORY(pszUtdVector);
    return retVal;

ldaperror:
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "%s: error: %d filter: '%s' received: %d usn: %llu utd: '%s'",
            __FUNCTION__,
            retVal,
            VDIR_SAFE_STRING(pszFilter),
            iEntriesReceived,
            lastSupplierUsnProcessed,
            VDIR_SAFE_STRING(pszUtdVector));

    VmDirFreeReplUpdateList(pReplUpdateList);
    pReplUpdateList = NULL;

    if (pReplAgr->dcConn.connState != DC_CONNECTION_STATE_CONNECTED)
    {   // unbind after _VmDirFreeReplicationPage call
        VDIR_SAFE_UNBIND_EXT_S(pReplAgr->dcConn.pLd);
    }
    goto cleanup;
}

/*
 * Perform Add/Modify/Delete on entries in page
 */
VOID
VmDirReplUpdateListApply(
    PVMDIR_REPLICATION_UPDATE_LIST pReplUpdateList
    )
{
    PVDIR_LINKED_LIST_NODE    pNode       = NULL;
    PVMDIR_REPLICATION_UPDATE pReplUpdate = NULL;

    pNode = pReplUpdateList->pLinkedList->pHead;
    while (pNode)
    {
        pReplUpdate = (PVMDIR_REPLICATION_UPDATE) pNode->pElement;

        VmDirReplUpdateApply(pReplUpdate);

        pNode = pNode->pNext;
    }

    return;
}

DWORD
VmDirReplUpdateListExpand(
    PVMDIR_REPLICATION_UPDATE_LIST    pReplUpdateList
    )
{
    DWORD                             dwError = 0;
    PVDIR_SORTED_LINKED_LIST          pNewReplUpdateList = NULL;
    PVDIR_LINKED_LIST_NODE            pCurrNode = NULL;
    PVDIR_LINKED_LIST_NODE            pCachedPrevNode = NULL;

    if (!pReplUpdateList || !pReplUpdateList->pLinkedList)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_PARAMETER);
    }

    dwError = VmDirSortedLinkedListCreate(
            VmDirSortedLinkedListInsertCompareReplUpdate, &pNewReplUpdateList);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirLinkedListGetTail(pReplUpdateList->pLinkedList, &pCurrNode);
    BAIL_ON_VMDIR_ERROR(dwError);

    while (pCurrNode)
    {
        dwError = VmDirReplUpdateSplit(
                (PVMDIR_REPLICATION_UPDATE) pCurrNode->pElement, pNewReplUpdateList);
        BAIL_ON_VMDIR_ERROR(dwError);

        /*
         * At this point pCurrNode->pElement (pCombinedUpdate) would have only changes
         * wrt one USN (least USN in USN list)
         */
        dwError = VmDirSortedLinkedListInsert(pNewReplUpdateList, pCurrNode->pElement);
        BAIL_ON_VMDIR_ERROR(dwError);

        pCachedPrevNode = pCurrNode->pPrev;

        dwError = VmDirLinkedListRemove(pReplUpdateList->pLinkedList, pCurrNode);
        BAIL_ON_VMDIR_ERROR(dwError);

        pCurrNode = pCachedPrevNode;
    }

    VmDirFreeReplUpdateListLinkedList(pReplUpdateList->pLinkedList);

    pReplUpdateList->pLinkedList = pNewReplUpdateList->pList;
    pNewReplUpdateList->pList = NULL;

cleanup:
    VmDirFreeSortedLinkedList(pNewReplUpdateList);
    return dwError;

error:
    if (pNewReplUpdateList)
    {
        VmDirFreeReplUpdateListLinkedList(pNewReplUpdateList->pList);
        pNewReplUpdateList->pList = NULL;
    }
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "failed, error (%d)", dwError);
    goto cleanup;
}

DWORD
VmDirReplUpdateListAlloc(
    PVMDIR_REPLICATION_UPDATE_LIST* ppReplUpdateList
    )
{
    DWORD                          dwError = VMDIR_SUCCESS;
    PVMDIR_REPLICATION_UPDATE_LIST pUpdateList = NULL;

    dwError = VmDirAllocateMemory(sizeof(VMDIR_REPLICATION_UPDATE_LIST), (PVOID*) &pUpdateList);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirUTDVectorCacheInit(&(pUpdateList->pNewUtdVector));
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirLinkedListCreate(&(pUpdateList->pLinkedList));
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppReplUpdateList = pUpdateList;
cleanup:
    return dwError;

error:
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "%s: error = (%d)",
            __FUNCTION__,
            dwError);

    VmDirFreeReplUpdateList(pUpdateList);
    *ppReplUpdateList = NULL;

    goto cleanup;
}

VOID
VmDirFreeReplUpdateList(
    PVMDIR_REPLICATION_UPDATE_LIST pUpdateList
    )
{
    if (pUpdateList)
    {
        VmDirFreeReplUpdateListLinkedList(pUpdateList->pLinkedList);

        if (pUpdateList->pNewUtdVector)
        {
            VmDirFreeUTDVectorCache(pUpdateList->pNewUtdVector);
        }

        VMDIR_SAFE_FREE_MEMORY(pUpdateList);
    }
}

int
VmDirReplUpdateListParseSyncDoneCtl(
    PVMDIR_REPLICATION_UPDATE_LIST  pReplUpdateList,
    LDAPControl**                   ppSearchResCtrls
    )
{
    int     retVal = LDAP_SUCCESS;
    PSTR    pszUTDVector = NULL;
    PSTR    pszStart = NULL;
    PSTR    pszEnd = NULL;
    DWORD   dwVectorLen = 0;

    retVal = VmDirStringToINT64(
                        ppSearchResCtrls[0]->ldctl_value.bv_val,
                        NULL,
                        &(pReplUpdateList->newHighWaterMark));
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    pszStart = VmDirStringStrA(ppSearchResCtrls[0]->ldctl_value.bv_val, ",");
    if (!pszStart)
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }
    /*Just after first comma*/
    pszStart++;

    pszEnd = ppSearchResCtrls[0]->ldctl_value.bv_val + ppSearchResCtrls[0]->ldctl_value.bv_len - 1;
    dwVectorLen = pszEnd - pszStart + 1;

    /*Allocate +1 for NULL char*/
    retVal = VmDirAllocateAndCopyMemory((PVOID)pszStart, dwVectorLen, (PVOID*)&pszUTDVector);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirUTDVectorCacheReplace(pReplUpdateList->pNewUtdVector, pszUTDVector);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszUTDVector);
    return retVal;

ldaperror:
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "%s: error = (%d)",
            __FUNCTION__,
            retVal);
    goto cleanup;
}

VOID
VmDirFreeReplUpdateListLinkedList(
    PVDIR_LINKED_LIST    pUpdateLinkedList
    )
{
    PVDIR_LINKED_LIST_NODE       pNode = NULL;
    PVMDIR_REPLICATION_UPDATE    pUpdate = NULL;

    if (pUpdateLinkedList)
    {
        VmDirLinkedListGetHead(pUpdateLinkedList, &pNode);
        while (pNode)
        {
            pUpdate = (PVMDIR_REPLICATION_UPDATE) pNode->pElement;
            VmDirFreeReplUpdate(pUpdate);
            pNode = pNode->pNext;
        }
        VmDirFreeLinkedList(pUpdateLinkedList);
    }
}
