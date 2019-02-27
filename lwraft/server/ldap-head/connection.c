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

static
int
SetupLdapPort(
   int port,
   ber_socket_t *pIP4_sockfd,
   ber_socket_t *pIP6_sockfd
   );

static
int
BindListenOnPort(
    sa_family_t   addr_type,
    size_t        addr_size,
    void         *pServ_addr,
    ber_socket_t *pSockfd
    );

static
DWORD
ProcessAConnection(
    PVOID pArg
    );

static
DWORD
vmdirConnAcceptThrFunc(
    PVOID pArg
    );

static
DWORD
vmdirSSLConnAcceptThrFunc(
    PVOID pArg
    );

static
DWORD
vmdirConnAccept(
    Sockbuf_IO*         pSockbuf_IO,
    DWORD               dwPort,
    BOOLEAN             bIsLdaps
    );

static
int
NewConnection(
    PVDIR_CONNECTION_CTX pConnCtx,
    VDIR_CONNECTION **ppConnection
    );

static
BOOLEAN
_VmDirFlowCtrlThrEnter(
    VOID
    );

static
VOID
_VmDirCollectBindSuperLog(
    PVDIR_CONNECTION    pConn,
    PVDIR_OPERATION     pOp
    );

static
VOID
_VmDirScrubSuperLogContent(
    ber_tag_t opTag,
    PVDIR_SUPERLOG_RECORD   pSuperLogRec
    );

static
VOID
_VmDirFlowCtrlThrExit(
    VOID
    );

static
VOID
_VmDirPingAcceptThr(
    DWORD   dwPort
    );

DWORD
VmDirAllocateConnection(
    PVDIR_CONNECTION* ppConn
    )
{
    DWORD   dwError = 0;
    PVDIR_CONNECTION    pConn = NULL;
    PVMDIR_THREAD_LOG_CONTEXT pLocalLogCtx = NULL;

    dwError = VmDirAllocateMemory(
            sizeof(VDIR_CONNECTION), (PVOID*)&pConn);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirGetThreadLogContextValue(&pLocalLogCtx);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (!pLocalLogCtx)
    {   // no pThrLogCtx set yet
        dwError = VmDirAllocateMemory(sizeof(VMDIR_THREAD_LOG_CONTEXT), (PVOID)&pConn->pThrLogCtx);
        BAIL_ON_VMDIR_ERROR(dwError);

        dwError = VmDirSetThreadLogContextValue(pConn->pThrLogCtx);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    *ppConn = pConn;

cleanup:
    return dwError;

error:
    VmDirDeleteConnection (&pConn);
    goto cleanup;
}

void
VmDirDeleteConnection(
    VDIR_CONNECTION **conn
    )
{
   if (conn && *conn)
   {
        if ((*conn)->sb)
        {
            VmDirSASLSessionClose((*conn)->pSaslInfo);
            VMDIR_SAFE_FREE_MEMORY((*conn)->pSaslInfo);

            VMDIR_LOG_VERBOSE( LDAP_DEBUG_ARGS, "Close socket (%s) bIsLdaps (%d)", (*conn)->szClientIP, (*conn)->bIsLdaps);
            // clean and free sockbuf (io descriptor and socket ...etc.)
            ber_sockbuf_free((*conn)->sb);
        }


        if ((*conn)->pThrLogCtx)
        {
            VmDirSetThreadLogContextValue(NULL);
            VmDirFreeThreadLogContext((*conn)->pThrLogCtx);
        }
        VmDirFreeAccessInfo(&((*conn)->AccessInfo));
        _VmDirScrubSuperLogContent(LDAP_REQ_UNBIND, &( (*conn)->SuperLogRec) );

        VMDIR_SAFE_FREE_MEMORY(*conn);
        *conn = NULL;
   }
}

DWORD
VmDirInitConnAcceptThread(
    void
    )
{
    DWORD               dwError = 0;
    PVDIR_THREAD_INFO   pLdapThrInfo = NULL;
    PVDIR_THREAD_INFO   pLdapsThrInfo = NULL;
    BOOLEAN             isIPV6AddressPresent = FALSE;
    BOOLEAN             isIPV4AddressPresent = FALSE;

    dwError = VmDirWhichAddressPresent(&isIPV4AddressPresent, &isIPV6AddressPresent);
    BAIL_ON_VMDIR_ERROR(dwError);
    if (isIPV4AddressPresent)
    {
       gVmdirServerGlobals.isIPV4AddressPresent = TRUE;
    }
    if (isIPV6AddressPresent)
    {
       gVmdirServerGlobals.isIPV6AddressPresent = TRUE;
    }

    dwError = VmDirSrvThrInit(
                &pLdapThrInfo,
                gVmdirGlobals.replCycleDoneMutex,
                gVmdirGlobals.replCycleDoneCondition,
                TRUE);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirCreateThread(
            &pLdapThrInfo->tid,
            pLdapThrInfo->bJoinThr,
            vmdirConnAcceptThrFunc,
            NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

    VmDirSrvThrAdd(pLdapThrInfo);
    pLdapThrInfo = NULL;

    dwError = VmDirSrvThrInit(
                &pLdapsThrInfo,
                gVmdirGlobals.replCycleDoneMutex,
                gVmdirGlobals.replCycleDoneCondition,
                TRUE);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirCreateThread(
            &pLdapsThrInfo->tid,
            pLdapsThrInfo->bJoinThr,
            vmdirSSLConnAcceptThrFunc,
            NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

    VmDirSrvThrAdd(pLdapsThrInfo);
    pLdapsThrInfo = NULL;

cleanup:

    return dwError;

error:

    VmDirSrvThrFree(pLdapThrInfo);
    VmDirSrvThrFree(pLdapsThrInfo);

    goto cleanup;
}

VOID
VmDirShutdownConnAcceptThread(
    VOID
    )
{
    DWORD   dwLdapPort = VmDirGetLdapPort();
    DWORD   dwLdapsPort = VmDirGetLdapsPort();

    _VmDirPingAcceptThr(dwLdapPort);
    _VmDirPingAcceptThr(dwLdapsPort);

    return;
}

void
VmDirFreeAccessInfo(
    PVDIR_ACCESS_INFO pAccessInfo
    )
{
    if (!pAccessInfo)
        return;

    if (pAccessInfo->pAccessToken)
    {
        VmDirReleaseAccessToken(&pAccessInfo->pAccessToken);
    }
    pAccessInfo->accessRoleBitmap = 0;

    VMDIR_SAFE_FREE_MEMORY(pAccessInfo->pszNormBindedDn);
    VMDIR_SAFE_FREE_MEMORY(pAccessInfo->pszBindedDn);
    VMDIR_SAFE_FREE_MEMORY(pAccessInfo->pszBindedObjectSid);
}

static
int
NewConnection(
    PVDIR_CONNECTION_CTX pConnCtx,
    VDIR_CONNECTION **ppConnection
    )
{
    int       retVal = LDAP_SUCCESS;
    ber_len_t max = SOCK_BUF_MAX_INCOMING;
    PVDIR_CONNECTION pConn = NULL;
    PSTR      pszLocalErrMsg = NULL;

    if (VmDirAllocateConnection(&pConn) != 0)
    {
       retVal = LDAP_OPERATIONS_ERROR;
       BAIL_ON_VMDIR_ERROR_WITH_MSG(retVal, pszLocalErrMsg, "NewConnection: VmDirAllocateMemory call failed");
    }

    pConn->bIsAnonymousBind = TRUE;  // default to anonymous bind
    pConn->bIsLdaps = pConnCtx->bIsLdaps;
    pConn->sd = pConnCtx->sockFd;    // pConn takes over sfd
    pConnCtx->sockFd = -1;

    retVal = VmDirGetNetworkInfoFromSocket(pConn->sd, pConn->szClientIP, sizeof(pConn->szClientIP), &pConn->dwClientPort, true);
    BAIL_ON_VMDIR_ERROR(retVal);

    retVal = VmDirGetNetworkInfoFromSocket(pConn->sd, pConn->szServerIP, sizeof(pConn->szServerIP), &pConn->dwServerPort, false);
    BAIL_ON_VMDIR_ERROR(retVal);

    VMDIR_LOG_DEBUG(VMDIR_LOG_MASK_ALL, "New connection (%s)", pConn->szClientIP);

    if ((pConn->sb = ber_sockbuf_alloc()) == NULL)
    {
       retVal = LDAP_OPERATIONS_ERROR;
       BAIL_ON_VMDIR_ERROR_WITH_MSG(retVal, pszLocalErrMsg, "New Connection (%s): ber_sockbuf_alloc() call failed with errno: %d", pConn->szClientIP, errno);
    }

    if (ber_sockbuf_ctrl(pConn->sb, LBER_SB_OPT_SET_MAX_INCOMING, &max) < 0)
    {
       retVal = LDAP_OPERATIONS_ERROR;
       BAIL_ON_VMDIR_ERROR_WITH_MSG(retVal, pszLocalErrMsg, "NewConnection (%s): ber_sockbuf_ctrl() failed while setting MAX_INCOMING", pConn->szClientIP);
    }

    if (ber_sockbuf_add_io(pConn->sb, pConnCtx->pSockbuf_IO, LBER_SBIOD_LEVEL_PROVIDER, (void *)&pConn->sd) != 0)
    {
       retVal = LDAP_OPERATIONS_ERROR;
       BAIL_ON_VMDIR_ERROR_WITH_MSG(retVal, pszLocalErrMsg, "NewConnection (%s): ber_sockbuf_addd_io() failed while setting LEVEL_PROVIDER", pConn->szClientIP);
    }

    //This is to disable NONBLOCK mode (when NULL passed in)
    if (ber_sockbuf_ctrl(pConn->sb, LBER_SB_OPT_SET_NONBLOCK, NULL) < 0)
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_VMDIR_ERROR_WITH_MSG(retVal, pszLocalErrMsg, "NewConnection (%s): ber_sockbuf_ctrl failed while setting NONBLOCK", pConn->szClientIP);
    }

    *ppConnection = pConn;
cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszLocalErrMsg);
    if (pConnCtx->sockFd != -1)
    {
        tcp_close(pConnCtx->sockFd);
        pConnCtx->sockFd = -1;
    }
    return retVal;

error:

    VmDirDeleteConnection(&pConn);
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "NewConnection failing with error %d", retVal);

    goto cleanup;
}

static
int
SetupLdapPort(
   int port,
   ber_socket_t *pIP4_sockfd,
   ber_socket_t *pIP6_sockfd
   )
{
   struct sockaddr_in  serv_4addr = {0};
   struct sockaddr_in6 serv_6addr = {0};
   sa_family_t         addr_type = AF_INET;
   size_t              addr_size = 0;
   int                 retVal = LDAP_SUCCESS;

   *pIP4_sockfd = -1;
   *pIP6_sockfd = -1;

   if (gVmdirServerGlobals.isIPV4AddressPresent)
   {
       addr_type = AF_INET;

       memset((char *) &serv_4addr, 0, sizeof(serv_4addr));
       serv_4addr.sin_family = addr_type;
       serv_4addr.sin_addr.s_addr = INADDR_ANY;
       serv_4addr.sin_port = htons(port);

       addr_size = sizeof(struct sockaddr_in);
       retVal = BindListenOnPort(addr_type, addr_size, (void *)&serv_4addr, pIP4_sockfd);
       if (retVal != 0)
       {
           VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: error listening on port %d for ipv4", __func__, port);
           BAIL_WITH_VMDIR_ERROR(retVal, VMDIR_ERROR_IO);
       } else
       {
           VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "%s: start listening on port %d for ipv4", __func__, port);
       }
   }

   if (gVmdirServerGlobals.isIPV6AddressPresent)
   {
       addr_type = AF_INET6;

       memset((char *) &serv_6addr, 0, sizeof(serv_6addr));
       serv_6addr.sin6_family = addr_type;
       serv_6addr.sin6_port = htons(port);

       addr_size = sizeof(struct sockaddr_in6);
       retVal = BindListenOnPort(addr_type, addr_size, (void *)&serv_6addr, pIP6_sockfd);
       if (retVal != 0)
       {
           VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: error listening on port %d for ipv6", __func__, port);
           BAIL_WITH_VMDIR_ERROR(retVal, VMDIR_ERROR_IO);
       } else
       {
           VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "%s: start listening on port %d for ipv6", __func__, port);
       }
   }

cleanup:
   return retVal;

error:
    VmDirForceExit();  // could not listen on all LDAP ports, exiting service.
    goto cleanup;
}

static
int
BindListenOnPort(
   sa_family_t   addr_type,
   size_t        addr_size,
    void         *pServ_addr,
    ber_socket_t *pSockfd
)
{
#define LDAP_PORT_LISTEN_BACKLOG 128
   int  retVal = LDAP_SUCCESS;
   int  retValBind = 0;
   PSTR pszLocalErrMsg = NULL;
   int  on = 1;
   int  iRetries = 0;
   struct timeval sTimeout = {0};

   *pSockfd = -1;
   *pSockfd = socket(addr_type, SOCK_STREAM, 0);
   if (*pSockfd < 0)
   {
      retVal = LDAP_OPERATIONS_ERROR;
      BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                      "%s: socket() call failed with errno: %d", __func__, errno );
   }

   if (setsockopt(*pSockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)(&on), sizeof(on)) < 0)
   {
      retVal = LDAP_OPERATIONS_ERROR;
      BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                      "%s: setsockopt() SO_REUSEADDR call failed with errno: %d", __func__, errno );
   }

   if (setsockopt(*pSockfd,  IPPROTO_TCP, TCP_NODELAY, (const char *)(&on), sizeof(on) ) < 0)
   {
      retVal = LDAP_OPERATIONS_ERROR;
      BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                      "%s: setsockopt() TCP_NODELAY call failed with errno: %d", __func__, errno );
   }

   if (addr_type == AF_INET6)
   {
       if (setsockopt(*pSockfd, SOL_IPV6, IPV6_V6ONLY, (const char *)(&on), sizeof(on) ) < 0)
       {
           retVal = LDAP_OPERATIONS_ERROR;
           BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                      "%s: setsockopt() IPV6_V6ONLY call failed with errno: %d", __func__, errno );
       }
   }

   {
       sTimeout.tv_sec = IDLE_SOCK_TIMEOUT_CHECK_FREQ;
       sTimeout.tv_usec = 0;

       if (setsockopt(*pSockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &sTimeout, sizeof(sTimeout)) < 0)
       {
           retVal = LDAP_OPERATIONS_ERROR;
           BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                      "%s: setsockopt() SO_RCVTIMEO failed, errno: %d", __func__, errno );
       }
   }

   iRetries = 0;
   do {
       retValBind = bind(*pSockfd, (struct sockaddr *) pServ_addr, addr_size);
       if (retValBind == 0 ||
           (retValBind == -1 && errno != EADDRINUSE))
       {
           break;
       }

       VMDIR_LOG_WARNING(VMDIR_LOG_MASK_ALL, "%s bind call failed, error (%d)", __func__, errno);
       iRetries++;
       VmDirSleep(1000);
   } while (iRetries < MAX_NUM_OF_BIND_PORT_RETRIES);

   if (retValBind != 0)
   {
       retVal = LDAP_OPERATIONS_ERROR;
       //need to free socket ...
       BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                       "%s: bind() call failed with errno: %d", __func__, errno );
   }

   if (listen(*pSockfd, LDAP_PORT_LISTEN_BACKLOG) != 0)
   {
       retVal = LDAP_OPERATIONS_ERROR;
       BAIL_ON_VMDIR_ERROR_WITH_MSG( retVal, pszLocalErrMsg,
                       "%s: listen() call failed with errno: %d", __func__, errno );
   }

cleanup:

    VMDIR_SAFE_FREE_MEMORY(pszLocalErrMsg);
    return retVal;

error:

    if (*pSockfd >= 0)
    {
        tcp_close(*pSockfd);
        *pSockfd = -1;
    }
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, " error (%s)(%d), errno (%d)", VDIR_SAFE_STRING(pszLocalErrMsg), retVal, errno);
    goto cleanup;
}

static
DWORD
_VmDirReadLdapHead(
    VDIR_CONNECTION* pConn,
    BerElement*     ber
    )
{
    DWORD   dwError = 0;
    DWORD   dwreTries = 0;
    DWORD   dwSockIdleTime = 0;
    ber_len_t   len = 0;
    ber_tag_t   tag = LBER_ERROR;

    // Read complete LDAP request message (tag, length, and real message).
    while (dwreTries < MAX_NUM_OF_SOCK_READ_RETRIES)
    {
        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            BAIL_WITH_VMDIR_ERROR(dwError, LDAP_NOTICE_OF_DISCONNECT);
        }

        if ((tag = ber_get_next(pConn->sb, &len, ber)) == LDAP_TAG_MESSAGE)
        {
            goto cleanup; // done reading socket
        }

        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            if (ber->ber_len == 0)
            {
                dwSockIdleTime += IDLE_SOCK_TIMEOUT_CHECK_FREQ;
                if (dwSockIdleTime < gVmdirGlobals.dwLdapRecvTimeoutSec)
                {
                    continue; // go back to ber_get_next
                }

                BAIL_WITH_VMDIR_ERROR(dwError, LDAP_NOTICE_OF_DISCONNECT);
            }

            //This may occur when not all data have recieved - set to EAGAIN/EWOULDBLOCK by ber_get_next,
            // and in such case ber->ber_len > 0;
            if (dwreTries > 0 && dwreTries % 5 == 0)
            {
                VMDIR_LOG_WARNING(VMDIR_LOG_MASK_ALL,
                        "%s: ber_get_next() failed with errno = %d, peer (%s), re-trying (%d)",
                        __func__,
                        errno,
                        pConn->szClientIP,
                        dwreTries);
            }
            VmDirSleep(200);
            dwreTries++;
            continue;   // go back to ber_get_next
        }
        else
        {
            BAIL_WITH_VMDIR_ERROR(dwError, LDAP_NOTICE_OF_DISCONNECT);
        }
    }
    BAIL_WITH_VMDIR_ERROR(dwError, LDAP_NOTICE_OF_DISCONNECT);

cleanup:
    return dwError;

error:
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL,
            "%s: disconnect peer (%s), errno (%d), idle time (%d), dwError (%d) buf read (%d)",
            __FUNCTION__, pConn->szClientIP, errno, dwSockIdleTime, dwError, ber->ber_len);

    goto cleanup;
}

/*
 *  We own pConnection and delete it when done.
 */
static
DWORD
ProcessAConnection(
    PVOID pArg
    )
{
    VDIR_CONNECTION* pConn = NULL;
    int              retVal = LDAP_SUCCESS;
    ber_tag_t        tag = LBER_ERROR;
    ber_len_t        len = 0;
    BerElement *     ber = NULL;
    ber_int_t        msgid = -1;
    PVDIR_OPERATION  pOperation = NULL;
    BOOLEAN          bDownOpThrCount = FALSE;
    PVDIR_CONNECTION_CTX pConnCtx = NULL;
    METRICS_LDAP_OPS operationTag = METRICS_LDAP_OP_IGNORE;
    uint64_t         iStartTime = 0;
    uint64_t         iEndTime = 0;

    // increment operation thread counter
    retVal = VmDirSyncCounterIncrement(gVmdirGlobals.pOperationThrSyncCounter);
    BAIL_ON_VMDIR_ERROR(retVal);
    bDownOpThrCount = TRUE;

    pConnCtx = (PVDIR_CONNECTION_CTX)pArg;
    assert(pConnCtx);

    retVal = NewConnection(pConnCtx, &pConn);
    if (retVal != LDAP_SUCCESS)
    {
        VMDIR_LOG_ERROR(
                VMDIR_LOG_MASK_ALL,
                "%s: NewConnection [%d] failed with error: %d",
                __func__,
                pConnCtx->sockFd,
                retVal);
        goto error;
    }

    while (TRUE)
    {
        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            goto cleanup;
        }

        ber = ber_alloc();
        if (ber == NULL)
        {
            VMDIR_LOG_ERROR(
                    VMDIR_LOG_MASK_ALL,
                    "ProcessAConnection: ber_alloc() failed.");

            retVal = LDAP_NOTICE_OF_DISCONNECT;
            BAIL_ON_VMDIR_ERROR(retVal);
        }

        /* An LDAP request message looks like:
         * LDAPMessage ::= SEQUENCE {
         *                    messageID       MessageID,
         *                    protocolOp      CHOICE {
         *                       bindRequest     BindRequest,
         *                       unbindRequest   UnbindRequest,
         *                       searchRequest   SearchRequest,
         *                       ... },
         *                       controls       [0] Controls OPTIONAL }
         */


        iStartTime = VmDirGetTimeInMilliSec();

        retVal = _VmDirReadLdapHead(pConn, ber);
        BAIL_ON_VMDIR_ERROR(retVal);

        // Read LDAP request messageID (tag, length (not returned since it is implicit/integer), and messageID value)
        if ((tag = ber_get_int(ber, &msgid)) != LDAP_TAG_MSGID)
        {
            VMDIR_LOG_ERROR(
                    VMDIR_LOG_MASK_ALL,
                    "ProcessAConnection: ber_get_int() call failed.");

            retVal = LDAP_NOTICE_OF_DISCONNECT;
            BAIL_ON_VMDIR_ERROR( retVal );
        }

        // Read protocolOp (tag) and length of the LDAP operation message, and leave the pointer at the beginning
        // of the LDAP operation message (to be parsed by PerformXYZ methods).
        if ((tag = ber_peek_tag(ber, &len)) == LBER_ERROR)
        {
            VMDIR_LOG_ERROR(
                    VMDIR_LOG_MASK_ALL,
                    "ProcessAConnection: ber_peek_tag() call failed.");

            retVal = LDAP_NOTICE_OF_DISCONNECT;
            BAIL_ON_VMDIR_ERROR( retVal );
        }

        retVal = VmDirExternalOperationCreate(ber, msgid, tag, pConn, &pOperation);
        if (retVal)
        {
            VMDIR_LOG_ERROR(
                    VMDIR_LOG_MASK_ALL,
                    "ProcessAConnection: NewOperation() call failed.");

            retVal = LDAP_OPERATIONS_ERROR;
        }
        BAIL_ON_VMDIR_ERROR( retVal );

        operationTag = METRICS_LDAP_OP_IGNORE;

        switch (tag)
        {
        case LDAP_REQ_BIND:
            retVal = VmDirPerformBind(pOperation);
            if (retVal != LDAP_SASL_BIND_IN_PROGRESS)
            {
                _VmDirCollectBindSuperLog(pConn, pOperation); // ignore error
            }
            break;

        case LDAP_REQ_ADD:
            retVal = VmDirPerformAdd(pOperation);
            operationTag = METRICS_LDAP_OP_ADD;
            break;

        case LDAP_REQ_SEARCH:
            retVal = VmDirPerformSearch(pOperation);
            operationTag = METRICS_LDAP_OP_SEARCH;
            break;

        case LDAP_REQ_UNBIND:
            retVal = VmDirPerformUnbind(pOperation);
            break;

        case LDAP_REQ_MODIFY:
            retVal = VmDirPerformModify(pOperation);
            operationTag = METRICS_LDAP_OP_MODIFY;
            break;

        case LDAP_REQ_DELETE:
            retVal = VmDirPerformDelete(pOperation);
            operationTag = METRICS_LDAP_OP_DELETE;
            break;

        case LDAP_REQ_MODDN:
            retVal = VmDirPerformRename(pOperation);
            break;

        case LDAP_REQ_EXTENDED:
            retVal = VmDirPerformExtended(pOperation);
            break;

        case LDAP_REQ_COMPARE:
        case LDAP_REQ_ABANDON:
            VMDIR_LOG_INFO(
                    VMDIR_LOG_MASK_ALL,
                    "ProcessAConnection: %d Operation is not yet implemented..",
                    tag);

            pOperation->ldapResult.errCode = retVal = LDAP_UNWILLING_TO_PERFORM;
            // ignore following VmDirAllocateStringA error.
            VmDirAllocateStringA("Operation is not yet implemented.", &pOperation->ldapResult.pszErrMsg);
            VmDirSendLdapResult(pOperation);
            break;

        default:
            pOperation->ldapResult.errCode = LDAP_PROTOCOL_ERROR;
            retVal = LDAP_NOTICE_OF_DISCONNECT;
            break;
        }

        iEndTime = VmDirGetTimeInMilliSec();

        if (operationTag != METRICS_LDAP_OP_IGNORE)
        {
            VmDirLdapMetricsUpdate(
                    operationTag,
                    METRICS_LDAP_OP_TYPE_EXTERNAL,
                    VmDirMetricsMapLdapErrorToEnum(pOperation->ldapResult.errCode),
                    METRICS_LAYER_PROTOCOL,
                    iStartTime,
                    iEndTime);
        }

        // If this is a multi-stage operation don't overwrite the start time if it's already set.
        pConn->SuperLogRec.iStartTime = pConn->SuperLogRec.iStartTime ? pConn->SuperLogRec.iStartTime : iStartTime;
        pConn->SuperLogRec.iEndTime = iEndTime;

        VmDirOPStatisticUpdate(tag, pConn->SuperLogRec.iEndTime - pConn->SuperLogRec.iStartTime);

        if (tag != LDAP_REQ_BIND)
        {
            VmDirLogOperation(gVmdirGlobals.pLogger, tag, pConn, pOperation->ldapResult.errCode);
            _VmDirScrubSuperLogContent(tag, &pConn->SuperLogRec);
        }

        VmDirFreeOperation(pOperation);
        pOperation = NULL;

        ber_free(ber, 1);
        ber = NULL;

        if (retVal == LDAP_NOTICE_OF_DISCONNECT) // returned as a result of protocol parsing error.
        {
            // RFC 4511, section 4.1.1: If the server receives an LDAPMessage from the client in which the LDAPMessage
            // SEQUENCE tag cannot be recognized, the messageID cannot be parsed, the tag of the protocolOp is not
            // recognized as a request, or the encoding structures or lengths of data fields are found to be incorrect,
            // then the server **SHOULD** return the Notice of Disconnection, with the resultCode
            // set to protocolError, and **MUST** immediately terminate the LDAP session as described in Section 5.3.

            goto cleanup;
        }
    }

cleanup:
    if (retVal == LDAP_NOTICE_OF_DISCONNECT)
    {
        // Optionally send Notice of Disconnection with rs->err.
    }
    if (ber != NULL)
    {
        ber_free( ber, 1 );
    }
    VmDirDeleteConnection(&pConn);
    VMDIR_SAFE_FREE_MEMORY(pConnCtx);
    VmDirFreeOperation(pOperation);

    VmDirMDBTxnAbortAll();

    if (bDownOpThrCount)
    {
        VmDirSyncCounterDecrement(gVmdirGlobals.pOperationThrSyncCounter);
    }

    _VmDirFlowCtrlThrExit();

    // TODO: should we return dwError ?
    return 0;

error:
    goto cleanup;
}


/*
 * Connection accept thread for clear socket
 */
static
DWORD
vmdirConnAcceptThrFunc(
    PVOID pArg
    )
{
    DWORD   port = VmDirGetLdapPort();

    return vmdirConnAccept(&ber_sockbuf_io_tcp, port, FALSE);
}

/*
 * Connection accept thread for SSL socket
 */
static
DWORD
vmdirSSLConnAcceptThrFunc(
    PVOID pArg
    )
{
    DWORD   port = VmDirGetLdapsPort();

    return vmdirConnAccept(gpVdirBerSockbufIOOpenssl, port, TRUE);
}

static
DWORD
vmdirConnAccept(
    Sockbuf_IO*         pSockbuf_IO,
    DWORD               dwPort,
    BOOLEAN             bIsLdaps
    )
{
    ber_socket_t         newsockfd = -1;
    int                  retVal = LDAP_SUCCESS;
    ber_socket_t         ip4_fd = -1;
    ber_socket_t         ip6_fd = -1;
    ber_socket_t         max_fd = -1;
    VMDIR_THREAD         threadId;
    BOOLEAN              bInLock = FALSE;
    int                  iLocalLogMask = 0;
    PVDIR_CONNECTION_CTX pConnCtx = NULL;
    fd_set               event_fd_set;
    fd_set               poll_fd_set;
    struct timeval       timeout = {0};

    // Wait for instance to be promoted or RaftLoadGlobals to complete.
    VMDIR_LOG_WARNING( VMDIR_LOG_MASK_ALL, "Connection accept thread: Have NOT yet started listening on LDAP port (%u),"
              " waiting for the 1st replication cycle to be over.", dwPort);

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.replCycleDoneMutex);
    // wait till 1st replication cycle is over
    if (VmDirConditionWait( gVmdirGlobals.replCycleDoneCondition, gVmdirGlobals.replCycleDoneMutex ) != 0)
    {
        VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "Connection accept thread: VmDirConditionWait failed." );
        retVal = LDAP_OPERATIONS_ERROR;
        goto cleanup;
    }
    // also wake up the other (normal LDAP port/SSL LDAP port listner) LDAP connection accept thread,
    // waiting on 1st replication cycle to be over
    // BUGBUG Does not handle spurious wake up
    VmDirConditionSignal(gVmdirGlobals.replCycleDoneCondition);
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.replCycleDoneMutex);

    if (VmDirdState() == VMDIRD_STATE_SHUTDOWN) // Asked to shutdown before we started accepting
    {
        goto cleanup;
    }

    gVmdirGlobals.bIsLDAPPortOpen = TRUE;
    VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "Connection accept thread: listening on LDAP port (%u).", dwPort);

    iLocalLogMask = VmDirLogGetMask();
    ber_set_option(NULL, LBER_OPT_DEBUG_LEVEL, &iLocalLogMask);

    retVal = SetupLdapPort(dwPort, &ip4_fd, &ip6_fd);
    BAIL_ON_VMDIR_ERROR(retVal);

    FD_ZERO(&event_fd_set);
    if (ip4_fd >= 0)
    {
        FD_SET (ip4_fd, &event_fd_set);
        if (ip4_fd > max_fd)
        {
            max_fd = ip4_fd;
        }
    }

    if (ip6_fd >= 0)
    {
        FD_SET (ip6_fd, &event_fd_set);
        if (ip6_fd > max_fd)
        {
            max_fd = ip6_fd;
        }
    }

    retVal = VmDirSyncCounterIncrement(gVmdirGlobals.pPortListenSyncCounter);
    if (retVal != 0 )
    {
        VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "%s: VmDirSyncCounterIncrement(gVmdirGlobals.pPortListenSyncCounter) returned error", __func__);
        BAIL_ON_VMDIR_ERROR(retVal);
    }

    while (TRUE)
    {
        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            goto cleanup;
        }

        poll_fd_set = event_fd_set;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        retVal = select ((int)max_fd+1, &poll_fd_set, NULL, NULL, &timeout);
        if (retVal < 0 )
        {
#ifdef _WIN32
            errno = WSAGetLastError();
#endif
            VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: select() (port %d) call failed: %d.", __func__, dwPort, errno);
            VmDirSleep( 1000 );
            continue;
        } else if (retVal == 0)
        {
            continue;
        }

        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            goto cleanup;
        }

        if (ip4_fd >= 0 && FD_ISSET(ip4_fd, &poll_fd_set))
        {
            newsockfd = accept(ip4_fd, (struct sockaddr *) NULL, NULL);
        } else if (ip6_fd >= 0 && FD_ISSET(ip6_fd, &poll_fd_set))
        {
            newsockfd = accept(ip6_fd, (struct sockaddr *) NULL, NULL);
        } else
        {
            VMDIR_LOG_INFO( LDAP_DEBUG_CONNS, "%s: select() returned with no data (port %d), return: %d",
                            __func__, dwPort, retVal);
            continue;
        }

        if (newsockfd < 0)
        {
#ifdef _WIN32
            errno = WSAGetLastError();
#endif
            if (errno != EAGAIN && errno != EWOULDBLOCK )
            {
                VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: accept() (port %d) failed with errno: %d.",
                                 __func__, dwPort, errno );
            }
            continue;
        }

        if ( _VmDirFlowCtrlThrEnter() == TRUE )
        {
            tcp_close(newsockfd);
            newsockfd = -1;
            VMDIR_LOG_WARNING( VMDIR_LOG_MASK_ALL, "Maxmimum number of concurrent LDAP threads reached. Blocking new connection" );

            continue;
        }

        retVal = VmDirAllocateMemory(
                sizeof(VDIR_CONNECTION_CTX),
                (PVOID*)&pConnCtx);
        BAIL_ON_VMDIR_ERROR(retVal);

        pConnCtx->sockFd  = newsockfd;
        newsockfd = -1;
        pConnCtx->pSockbuf_IO = pSockbuf_IO;
        pConnCtx->bIsLdaps = bIsLdaps;

        retVal = VmDirCreateThread(&threadId, FALSE, ProcessAConnection, (PVOID)pConnCtx);
        if (retVal != 0)
        {
            VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: VmDirCreateThread() (port) failed with errno: %d",
                             __func__, dwPort, errno );

            tcp_close(pConnCtx->sockFd);
            _VmDirFlowCtrlThrExit();
            VMDIR_SAFE_FREE_MEMORY(pConnCtx);
            continue;
        }
        else
        {
            pConnCtx = NULL; //thread take ownership on pConnCtx
            VmDirFreeVmDirThread(&threadId);
        }
    }

cleanup:

    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.replCycleDoneMutex);

    if (ip4_fd >= 0)
    {
        tcp_close(ip4_fd);
    }
    if (ip6_fd >= 0)
    {
        tcp_close(ip6_fd);
    }
    if (newsockfd >= 0)
    {
        tcp_close(newsockfd);
    }
#ifndef _WIN32
    raise(SIGTERM);
#endif

    VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "%s: Connection accept thread: stop (port %d)", __func__, dwPort);

    return retVal;

error:
    goto cleanup;
}

/*
 VmDirWhichAddressPresent: Check if ipv4 or ipv6 addresses exist
 */

DWORD
VmDirWhichAddressPresent(
    BOOLEAN *pIPV4AddressPresent,
    BOOLEAN *pIPV6AddressPresent
    )
{
    int                 retVal = 0;
#ifndef _WIN32
    struct ifaddrs *    myaddrs = NULL;
    struct ifaddrs *    ifa = NULL;
#else
    PADDRINFOA          myaddrs = NULL;
    PADDRINFOA          ifa = NULL;
    unsigned long       loopback_addr = 0;
    struct sockaddr_in  *pIp4Addr = NULL;
    struct addrinfo     hints = {0};
#endif

    *pIPV4AddressPresent = FALSE;
    *pIPV6AddressPresent = FALSE;

#ifndef _WIN32
    retVal = getifaddrs(&myaddrs);
#else
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    loopback_addr = inet_addr("127.0.0.1");

    if( getaddrinfo("", NULL, &hints, &myaddrs ) != 0 )
    {
        retVal = WSAGetLastError();
    }
#endif
    BAIL_ON_VMDIR_ERROR( retVal );

    for (ifa = myaddrs; ifa != NULL; ifa = VMDIR_ADDR_INFO_NEXT(ifa))
    {
        if ((VMDIR_ADDR_INFO_ADDR(ifa) == NULL)
#ifndef _WIN32 // because getaddrinfo() does NOT set ai_flags in the returned address info structures.
            || ((VMDIR_ADDR_INFO_FLAGS(ifa) & IFF_UP) == 0)
            || ((VMDIR_ADDR_INFO_FLAGS(ifa) & IFF_LOOPBACK) != 0)
#endif
           )
        {
            continue;
        }
        if (VMDIR_ADDR_INFO_ADDR(ifa)->sa_family == AF_INET6)
        {
            *pIPV6AddressPresent = TRUE;
#ifdef _WIN32
            VMDIR_LOG_INFO(LDAP_DEBUG_CONNS, "%s: ipv6 address exists", __func__);
#else
            VMDIR_LOG_INFO(LDAP_DEBUG_CONNS, "%s: ipv6 address exists on interface %s", __func__, ifa->ifa_name);
#endif
        }
        else if (VMDIR_ADDR_INFO_ADDR(ifa)->sa_family == AF_INET)
        {
#ifdef _WIN32
            pIp4Addr = (struct sockaddr_in *) VMDIR_ADDR_INFO_ADDR(ifa);
            if (memcmp(&pIp4Addr->sin_addr.s_addr,
                &loopback_addr,
                sizeof(loopback_addr)) == 0)
            {
                continue;
            }
#endif

            *pIPV4AddressPresent = TRUE;
#ifdef _WIN32
            VMDIR_LOG_INFO(LDAP_DEBUG_CONNS, "%s: ipv4 address exists", __func__);
#else
            VMDIR_LOG_INFO(LDAP_DEBUG_CONNS, "%s: ipv4 address exists on interface %s", __func__, ifa->ifa_name);
#endif
        }
    }

    if ( !(*pIPV4AddressPresent) && !(*pIPV6AddressPresent))
    {
        VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "%s: neither ipv4 nor ipv6 address exist on interfaces", __func__);
        retVal = -1;
    }

cleanup:
    if (myaddrs)
    {
#ifndef _WIN32
        freeifaddrs(myaddrs);
#else
        freeaddrinfo(myaddrs);
#endif
    }
    return retVal;

error:
    goto cleanup;
}


/*
 * return TRUE if we are over the limit of MaxLdapOpThrs
 */
static
BOOLEAN
_VmDirFlowCtrlThrEnter(
    VOID
    )
{
    BOOLEAN bInLock = FALSE;
    DWORD   dwFlowCtrl = 0;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.pFlowCtrlMutex);

    if ( gVmdirGlobals.dwMaxFlowCtrlThr > 0 )
    {
        dwFlowCtrl = gVmdirGlobals.dwMaxFlowCtrlThr;
        gVmdirGlobals.dwMaxFlowCtrlThr--;
    }

    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.pFlowCtrlMutex);

    VMDIR_LOG_INFO( LDAP_DEBUG_CONNS, "FlowCtrlThr-- %d", dwFlowCtrl);

    return ( dwFlowCtrl == 0 );
}

/*
 * Ldap operation thr exit
 */
static
VOID
_VmDirFlowCtrlThrExit(
    VOID
    )
{
    BOOLEAN bInLock = FALSE;
    DWORD   dwFlowCtrl = 0;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.pFlowCtrlMutex);

    gVmdirGlobals.dwMaxFlowCtrlThr++;
    dwFlowCtrl = gVmdirGlobals.dwMaxFlowCtrlThr;

    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.pFlowCtrlMutex);

    VMDIR_LOG_INFO( LDAP_DEBUG_CONNS, "FlowCtrlThr++ %d", dwFlowCtrl);

    return;
}


//
// Some information in the logging record is per-connection and some is per-operation.
// This routine sanitizes the structure appropriately depending upon where in the
// process we are.
//
static
void
_VmDirScrubSuperLogContent(
    ber_tag_t opTag,
    PVDIR_SUPERLOG_RECORD pSuperLogRec)
{
    if (pSuperLogRec)
    {
        // common to all operations
        pSuperLogRec->iStartTime = pSuperLogRec->iEndTime = 0;
        VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->pszOperationParameters);

        if (opTag == LDAP_REQ_SEARCH || opTag == LDAP_REQ_UNBIND)
        {
            VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->opInfo.searchInfo.pszAttributes);
            VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->opInfo.searchInfo.pszBaseDN);
            VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->opInfo.searchInfo.pszScope);
            VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->opInfo.searchInfo.pszIndexResults);
        }

        if (opTag == LDAP_REQ_UNBIND)
        {
            VMDIR_SAFE_FREE_MEMORY(pSuperLogRec->pszBindID);
            memset(pSuperLogRec, 0, sizeof(VDIR_SUPERLOG_RECORD));
        }
    }
}

static
VOID
_VmDirCollectBindSuperLog(
    PVDIR_CONNECTION    pConn,
    PVDIR_OPERATION     pOp
    )
{
    DWORD   dwError = 0;

    pConn->SuperLogRec.iEndTime = VmDirGetTimeInMilliSec();

    if (pOp->reqDn.lberbv.bv_val) // TODO, for failed SASL bind scenario, we need DN/UPN a well.
    {
        dwError = VmDirAllocateStringA(pOp->reqDn.lberbv.bv_val, &(pConn->SuperLogRec.pszBindID));
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    VmDirLogOperation(gVmdirGlobals.pLogger, LDAP_REQ_BIND, pConn, pOp->ldapResult.errCode);

    //
    // Flush times once we log.
    //
    pConn->SuperLogRec.iStartTime = pConn->SuperLogRec.iEndTime = 0;

cleanup:
    return;

error:
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "%s failed, error code %d", __FUNCTION__, dwError);

    goto cleanup;
}

/*
 * During server shutdown, connect to listening thread to break select blocking call.
 * So listening thread can shutdown gracefully.
 */
DWORD
VmDirPingIPV6AcceptThr(
    DWORD   dwPort
    )
{
    DWORD   dwError = 0;
    ber_socket_t        sockfd = -1;
    struct sockaddr_in6 servaddr6 = {0};
    struct in6_addr loopbackaddr = IN6ADDR_LOOPBACK_INIT;

    if ((sockfd = socket (AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        VMDIR_LOG_VERBOSE(VMDIR_LOG_MASK_ALL, "[%s,%d] failed", __FUNCTION__, __LINE__);
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_IO);
    }

    memset(&servaddr6, 0, sizeof(servaddr6));
    servaddr6.sin6_family = AF_INET6;
    servaddr6.sin6_addr = loopbackaddr;
    servaddr6.sin6_port =  htons( (USHORT)dwPort );

    // ping accept thread
    if (connect(sockfd, (struct sockaddr *) &servaddr6, sizeof(servaddr6)) < 0)
    {
        VMDIR_LOG_VERBOSE(VMDIR_LOG_MASK_ALL, "[%s,%d] failed %d", __FUNCTION__, __LINE__, errno);
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_IO);
    }

error:
    if (sockfd >= 0)
    {
        tcp_close(sockfd);
    }
    return dwError;
}

DWORD
VmDirPingIPV4AcceptThr(
    DWORD   dwPort
    )
{
    DWORD   dwError = 0;
    ber_socket_t        sockfd = -1;
    struct  sockaddr_in servaddr = {0};

    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        VMDIR_LOG_VERBOSE(VMDIR_LOG_MASK_ALL, "[%s,%d] failed", __FUNCTION__, __LINE__);
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_IO);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr("127.0.0.1");
    servaddr.sin_port =  htons( (USHORT)dwPort );

    // ping accept thread
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
    {
        VMDIR_LOG_VERBOSE(VMDIR_LOG_MASK_ALL, "[%s,%d] failed %d", __FUNCTION__, __LINE__, errno);
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_IO);
    }

error:
    if (sockfd >= 0)
    {
        tcp_close(sockfd);
    }
    return dwError;
}

static
VOID
_VmDirPingAcceptThr(
    DWORD   dwPort
    )
{
    VmDirPingIPV4AcceptThr(dwPort);
    VmDirPingIPV6AcceptThr(dwPort);

    return;
}
