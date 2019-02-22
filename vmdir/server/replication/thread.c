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



/*
 * Module Name: Replication thread
 *
 * Filename: thread.c
 *
 * Abstract:
 *
 */

#include "includes.h"

static
VMDIR_DC_CONNECTION_STATE
_VmDirReplicationConnect(
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr
    );

static
DWORD
_VmDirWaitForReplicationAgreement(
    PBOOLEAN pbExitReplicationThread
    );

static
VOID
_VmDirConsumePartner(
    PVMDIR_REPLICATION_CONTEXT      pContext,
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr,
    PVMDIR_DC_CONNECTION            pConnection,
    BOOLEAN                         *pbUpdateReplicationInterval
    );

static
DWORD
vdirReplicationThrFun(
    PVOID   pArg
    );

static
int
_VmDirContinueReplicationCycle(
    uint64_t *puiStartTimeInShutdown,
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr,
    struct berval   bervalSyncDoneCtrl
    );

static
int
_VmDirFetchReplicationPage(
    PVMDIR_DC_CONNECTION        pConnection,
    USN                         lastSupplierUsnProcessed,
    USN                         initUsn,
    PVMDIR_REPLICATION_PAGE*    ppPage
    );

static
VOID
_VmDirFreeReplicationPage(
    PVMDIR_REPLICATION_PAGE pPage
    );

static
VOID
_VmDirProcessReplicationPage(
    PVMDIR_REPLICATION_CONTEXT pContext,
    PVMDIR_REPLICATION_PAGE pPage
    );

static
int
VmDirParseEntryForDn(
    LDAPMessage *ldapEntryMsg,
    PSTR *ppszDn
    );

static
VOID
_VmDirReplicationUpdateTombStoneEntrySyncState(
    PVMDIR_REPLICATION_PAGE_ENTRY pPage
    );

static
DWORD
_VmDirUpdateReplicationInterval(
    BOOLEAN   bUpdateReplicationInterval
    );

static
BOOLEAN
_VmDirSkipReplicationCycle();

static
VOID
_VmDirUpdateInvocationIdInReplAgr(
    VOID
    );

DWORD
VmDirGetReplCycleCounter(
    VOID
    )
{
    BOOLEAN bInLock = FALSE;
    DWORD   dwCount = 0;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.replCycleDoneMutex);
    dwCount = gVmdirGlobals.dwReplCycleCounter;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.replCycleDoneMutex);

    return dwCount;
}

DWORD
InitializeReplicationThread(
    void)
{
    DWORD               dwError = 0;
    PVDIR_THREAD_INFO   pThrInfo = NULL;

    dwError = VmDirSrvThrInit(
            &pThrInfo,
            gVmdirGlobals.replAgrsMutex,
            gVmdirGlobals.replAgrsCondition,
            TRUE);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirCreateThread(
            &pThrInfo->tid,
            pThrInfo->bJoinThr,
            vdirReplicationThrFun,
            pThrInfo);
    BAIL_ON_VMDIR_ERROR(dwError);

    VmDirSrvThrAdd(pThrInfo);

cleanup:

    return dwError;

error:

    VmDirSrvThrFree(pThrInfo);

    goto cleanup;
}

VOID
VmDirPopulateInvocationIdInReplAgr(
    VOID
    )
{
    PSTR      pszCfgBaseDN = NULL;
    DWORD     dwError = 0;
    size_t    iCnt = 0;
    BOOLEAN   bInLock = FALSE;
    VDIR_ENTRY_ARRAY   entryArray = {0};
    PVDIR_ATTRIBUTE    pAttrCN = NULL;
    PVDIR_ATTRIBUTE    pAttrInvocID = NULL;
    PVMDIR_REPLICATION_AGREEMENT   pReplAgr = NULL;

    dwError = VmDirAllocateStringPrintf(
            &pszCfgBaseDN,
            "cn=%s,%s",
            VMDIR_CONFIGURATION_CONTAINER_NAME,
            gVmdirServerGlobals.systemDomainDN.lberbv_val);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSimpleEqualFilterInternalSearch(
            pszCfgBaseDN,
            LDAP_SCOPE_SUBTREE,
            ATTR_OBJECT_CLASS,
            OC_DIR_SERVER,
            &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.replAgrsMutex);

    //complexity should be a problem since size of 'n' number of partners is always small here.
    for (iCnt = 0; iCnt < entryArray.iSize; iCnt++)
    {
        pAttrCN = VmDirFindAttrByName(&entryArray.pEntry[iCnt], ATTR_CN);
        pAttrInvocID = VmDirFindAttrByName(&entryArray.pEntry[iCnt], ATTR_INVOCATION_ID);

        if (pAttrCN && pAttrInvocID)
        {
            for (pReplAgr = gVmdirReplAgrs; pReplAgr; pReplAgr = pReplAgr->next)
            {
                if (VmDirStringCompareA(pReplAgr->pszHostname, pAttrCN->vals[0].lberbv_val, FALSE) == 0 &&
                    pReplAgr->pszInvocationID == NULL)
                {
                    VmDirAllocateStringA(pAttrInvocID->vals[0].lberbv_val, &pReplAgr->pszInvocationID);
                }
            }
        }
    }

cleanup:
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.replAgrsMutex);
    VMDIR_SAFE_FREE_MEMORY(pszCfgBaseDN);
    VmDirFreeEntryArrayContent(&entryArray);
    return;

error:
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "failed, error (%d)", dwError);
    goto cleanup;
}

// vdirReplicationThrFun is the main replication function that:
//  - Executes replication cycles endlessly
//  - Each replication cycle consist of processing all the RAs for this vmdird instance.
//  - Sleeps for gVmdirServerGlobals.replInterval between replication cycles.
//

/*
     1.  Wait for a replication agreement
     2.  While server running
     2a.   Load schema context
     2b.   Load credentials
     2c.   For each replication agreement
     2ci.      Connect to system
     2cii.     Fetch Page
     2ciii.    Process Page
*/
static
DWORD
vdirReplicationThrFun(
    PVOID   pArg
    )
{
    int                             retVal = 0;
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr = NULL;
    BOOLEAN                         bInReplAgrsLock = FALSE;
    BOOLEAN                         bInReplCycleDoneLock = FALSE;
    BOOLEAN                         bExitThread = FALSE;
    BOOLEAN                         bUpdateReplicationInterval = FALSE;
    DWORD                           dwReplicationIntervalInMilliSec = 0;
    int                             i = 0;
    VMDIR_REPLICATION_CONTEXT       sContext = {0};

    /*
     * This is to address the backend's writer mutex contention problem so that
     * the replication thread wouldn't be starved by the local operational threads'
     * write operations.
     */
    VmDirRaiseThreadPriority(DEFAULT_THREAD_PRIORITY_DELTA);

    retVal = _VmDirWaitForReplicationAgreement(&bExitThread);
    BAIL_ON_VMDIR_ERROR(retVal);

    // To handle - stand alone node - signaled by partner join
    _VmDirUpdateInvocationIdInReplAgr();

    if (bExitThread)
    {
        goto cleanup;
    }

    if (VmDirSchemaCtxAcquire(&sContext.pSchemaCtx) != 0)
    {
        VMDIR_LOG_ERROR(
                VMDIR_LOG_MASK_ALL,
                "vdirReplicationThrFun: VmDirSchemaCtxAcquire failed.");

        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_VMDIR_ERROR(retVal);
    }

    retVal = dequeCreate(&sContext.pFailedEntriesQueue);
    BAIL_ON_VMDIR_ERROR(retVal);

    while (1)
    {
        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            goto cleanup;
        }

        if (VmDirdState() == VMDIRD_STATE_STANDALONE)
        {
            VmDirSleep(1000);
            continue;
        }

        VMDIR_LOG_VERBOSE(
                VMDIR_LOG_MASK_ALL,
                "vdirReplicationThrFun: Executing replication cycle %u.",
                gVmdirGlobals.dwReplCycleCounter + 1);

        bUpdateReplicationInterval = FALSE;

        // purge RAs that have been marked as isDeleted = TRUE
        VmDirRemoveDeletedRAsFromCache();

        VMDIR_LOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
        pReplAgr = gVmdirReplAgrs;
        VMDIR_UNLOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);

        for (; pReplAgr != NULL; pReplAgr = pReplAgr->next )
        {
            if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
            {
                goto cleanup;
            }

            if (pReplAgr->isDeleted || _VmDirSkipReplicationCycle()) // skip deleted RAs
            {
                continue;
            }

            if ( VmDirIsLiveSchemaCtx(sContext.pSchemaCtx) == FALSE )
            { // schema changed via local node schema entry LDAP modify, need to pick up new schema.

                VmDirSchemaCtxRelease(sContext.pSchemaCtx);
                sContext.pSchemaCtx = NULL;
                if (VmDirSchemaCtxAcquire(&sContext.pSchemaCtx) != 0)
                {
                    VMDIR_LOG_ERROR(
                            VMDIR_LOG_MASK_ALL,
                            "vdirReplicationThrFun: VmDirSchemaCtxAcquire failed.");
                    continue;
                }
                VMDIR_LOG_INFO(
                        VMDIR_LOG_MASK_ALL,
                        "vdirReplicationThrFun: Acquires new schema context");
            }

            if (_VmDirReplicationConnect(pReplAgr) != DC_CONNECTION_STATE_CONNECTED)
            {
                continue;
            }

            _VmDirConsumePartner(&sContext, pReplAgr, &pReplAgr->dcConn, &bUpdateReplicationInterval);
            /*
             * To avoid race condition after resetting the vector
             * if this node plays consumer role before supplying the new vector value
             * could result in longer replication cycle.
             */
            VmDirSleep(100);
        }

        VMDIR_LOG_DEBUG(
                VMDIR_LOG_MASK_ALL,
                "vdirReplicationThrFun: Done executing the replication cycle.");

        VMDIR_LOCK_MUTEX(bInReplCycleDoneLock, gVmdirGlobals.replCycleDoneMutex);
        gVmdirGlobals.dwReplCycleCounter++;

        if ( gVmdirGlobals.dwReplCycleCounter == 1 )
        {   // during promotion scenario, start listening on ldap ports after first cycle.
            VmDirConditionSignal(gVmdirGlobals.replCycleDoneCondition);
        }
        VMDIR_UNLOCK_MUTEX(bInReplCycleDoneLock, gVmdirGlobals.replCycleDoneMutex);

        if (VmDirdState() == VMDIRD_STATE_RESTORE)
        {
            VMDIR_LOG_INFO(
                    VMDIR_LOG_MASK_ALL,
                    "vdirReplicationThrFun: Done restoring the server by catching up with it's replication partner(s).");
            VmDirForceExit();
            break;
        }

        VMDIR_LOG_DEBUG(
                VMDIR_LOG_MASK_ALL,
                "vdirReplicationThrFun: Sleeping for the replication interval: %d seconds.",
                gVmdirServerGlobals.replInterval);

        for (i=0; i<gVmdirServerGlobals.replInterval; i++)
        {
            if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
            {
                goto cleanup;
            }

            // An RPC call requested a replication cycle to start immediately
            if (VmDirdGetReplNow() == TRUE)
            {
                VmDirdSetReplNow(FALSE);
                break;
            }

            dwReplicationIntervalInMilliSec = _VmDirUpdateReplicationInterval(bUpdateReplicationInterval);

            VmDirSleep(dwReplicationIntervalInMilliSec);
        }
    } // Endless replication loop

cleanup:
    VmDirSchemaCtxRelease(sContext.pSchemaCtx);
    VMDIR_SAFE_FREE_STRINGA(sContext.pszKrb5ErrorMsg);
    VmDirReplicationClearFailedEntriesFromQueue(&sContext);
    dequeFree(sContext.pFailedEntriesQueue);
    VMDIR_UNLOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
    VMDIR_UNLOCK_MUTEX(bInReplCycleDoneLock, gVmdirGlobals.replCycleDoneMutex);
    return 0;

error:
    VmDirdStateSet(VMDIRD_STATE_FAILURE);
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "vdirReplicationThrFun: Replication has failed with unrecoverable error.");
    goto cleanup;
}

static
BOOLEAN
_VmDirSkipReplicationCycle()
{
    return (VmDirdState() == VMDIRD_STATE_READ_ONLY_DEMOTE ||
            VmDirdState() == VMDIRD_STATE_READ_ONLY);
}

static
DWORD
_VmDirUpdateReplicationInterval(
    BOOLEAN   bUpdateReplicationInterval
    )
{
    DWORD   dwReplicationTimeIntervalInMilliSec = 800;

    if (bUpdateReplicationInterval)
    {
        // Range 750 to 1250 milliseconds
        dwReplicationTimeIntervalInMilliSec = ((rand() % 750) + 500);

        VMDIR_LOG_INFO(
            VMDIR_LOG_MASK_ALL,
            "%s: Updated replication time interval (%d) milliseconds",
            __FUNCTION__,
            dwReplicationTimeIntervalInMilliSec);
    }

    return dwReplicationTimeIntervalInMilliSec;
}

static
VMDIR_DC_CONNECTION_STATE
_VmDirReplicationConnect(
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr
    )
{
    if (DC_CONNECTION_STATE_NOT_CONNECTED == pReplAgr->dcConn.connState ||
        DC_CONNECTION_STATE_FAILED        == pReplAgr->dcConn.connState)
    {
        VmDirInitDCConnThread(&pReplAgr->dcConn);
    }

    return pReplAgr->dcConn.connState;
}

DWORD
_VmDirWaitForReplicationAgreement(
    PBOOLEAN pbExitReplicationThread
    )
{
    DWORD dwError = 0;
    BOOLEAN bInReplAgrsLock = FALSE;
    int retVal = 0;
    PSTR pszPartnerHostName = NULL;

    assert(pbExitReplicationThread != NULL);

    VMDIR_LOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
    while (gVmdirReplAgrs == NULL)
    {
        VMDIR_UNLOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
        if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
        {
            goto cleanup;
        }

        if (VmDirdState() == VMDIRD_STATE_RESTORE)
        {
            VMDIR_LOG_INFO(VMDIR_LOG_MASK_ALL, "_VmDirWaitForReplicationAgreement: Done restoring the server, no partner to catch up with.");
            VmDirForceExit();
            *pbExitReplicationThread = TRUE;
            goto cleanup;
        }

        VMDIR_LOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
        // wait till a replication agreement is created e.g. by vdcpromo
        if (VmDirConditionWait( gVmdirGlobals.replAgrsCondition, gVmdirGlobals.replAgrsMutex ) != 0)
        {
            VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "_VmDirWaitForReplicationAgreement: VmDirConditionWait failed.");
            dwError = LDAP_OPERATIONS_ERROR;
            BAIL_ON_VMDIR_ERROR( dwError );

        }
        VMDIR_UNLOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);

        // When 1st RA is created for non-1st replica => try to perform special 1st replication cycle
        if (gVmdirReplAgrs)
        {
            if ( gFirstReplCycleMode == FIRST_REPL_CYCLE_MODE_COPY_DB )
            {
                if ((retVal=VmDirReplURIToHostname(gVmdirReplAgrs->ldapURI, &pszPartnerHostName)) !=0 ||
                    (retVal=VmDirFirstReplicationCycle( pszPartnerHostName, gVmdirReplAgrs)) != 0)
                {
                    VMDIR_LOG_WARNING( VMDIR_LOG_MASK_ALL,
                          "vdirReplicationThrFun: VmDirReplURIToHostname or VmDirFirstReplicationCycle failed, error (%d).",
                          gVmdirReplAgrs->ldapURI, retVal);
                    VmDirForceExit();
                    *pbExitReplicationThread = TRUE;
                    dwError = LDAP_OPERATIONS_ERROR;
                    BAIL_ON_VMDIR_ERROR( dwError );
                }
                else
                {
                    VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "vdirReplicationThrFun: VmDirFirstReplicationCycle() SUCCEEDED." );
                }
            }
            else if (gFirstReplCycleMode == FIRST_REPL_CYCLE_MODE_NONE)
            {
                // already promoted node and first partner added
            }
            else
            {
                assert(0);  // Lightwave server always use DB copy to bootstrap partner node.
            }
        }
        VMDIR_LOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
    }

cleanup:
    VMDIR_UNLOCK_MUTEX(bInReplAgrsLock, gVmdirGlobals.replAgrsMutex);
    return dwError;

error:
    goto cleanup;
}

/*
 * Don't stop cycle even if there is nothing in the re-apply queue.
 * because supplier may have other changes needed that could cause ordering issues
 * if not consumed in same replication cycle.
 */
static
int
_VmDirContinueReplicationCycle(
    uint64_t *puiStartTimeInShutdown,
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr,
    struct berval   bervalSyncDoneCtrl
    )
{
    int retVal = LDAP_SUCCESS;

    if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
    {
        if (*puiStartTimeInShutdown == 0)
        {
            *puiStartTimeInShutdown = VmDirGetTimeInMilliSec();
        }
        else if ((VmDirGetTimeInMilliSec() - *puiStartTimeInShutdown) >=
                 gVmdirGlobals.dwReplConsumerThreadTimeoutInMilliSec)
        {
            VMDIR_LOG_WARNING(
                VMDIR_LOG_MASK_ALL,
                "%s: Force quit cycle without updating cookies, supplier %s initUsn: %s syncdonectrl: %s",
                __FUNCTION__,
                pReplAgr->ldapURI,
                VDIR_SAFE_STRING(pReplAgr->lastLocalUsnProcessed.lberbv.bv_val),
                VDIR_SAFE_STRING(bervalSyncDoneCtrl.bv_val));
            retVal = LDAP_CANCELLED;
        }
    }
    else if (_VmDirSkipReplicationCycle())
    {
        retVal = LDAP_CANCELLED;
    }

    return retVal;
}

static
VOID
_VmDirConsumePartner(
    PVMDIR_REPLICATION_CONTEXT      pContext,
    PVMDIR_REPLICATION_AGREEMENT    pReplAgr,
    PVMDIR_DC_CONNECTION            pConnection,
    BOOLEAN                         *pbUpdateReplicationInterval
    )
{
    int       retVal = LDAP_SUCCESS;
    USN       initUsn = 0;
    USN       lastSupplierUsnProcessed = 0;
    PSTR      pszUtdVector = NULL;
    BOOLEAN   bContinue = FALSE;
    BOOLEAN   bCompleteReplCycle = FALSE;
    uint64_t  uiStartTime = 0;
    uint64_t  uiEndTime = 0;
    uint64_t  uiStartTimeInShutdown = 0;
    PLW_HASHMAP    pUtdVectorMap = NULL;
    struct berval  bervalSyncDoneCtrl = {0};
    PVMDIR_REPLICATION_PAGE     pPage = NULL;
    PVMDIR_REPLICATION_METRICS  pReplMetrics = NULL;

    uiStartTime = VmDirGetTimeInMilliSec();
    /*
     * Replication thread acquires lock while main thread is blocked
     * to complete shutdown, don't perform replication cycle
     */
    if (VmDirdState() == VMDIRD_STATE_SHUTDOWN)
    {
        retVal = LDAP_CANCELLED;
        goto cleanup;
    }

    retVal = VmDirDDVectorUpdate(pReplAgr->pszInvocationID, 0);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirUTDVectorCacheToString(&pszUtdVector);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirStringToUTDVector(pszUtdVector, &pUtdVectorMap);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirStringToINT64(pReplAgr->lastLocalUsnProcessed.lberbv.bv_val, NULL, &initUsn);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    lastSupplierUsnProcessed = initUsn;

    do // do-while (bMoreUpdatesExpected == TRUE); paged results loop
    {
        bCompleteReplCycle = FALSE;

        _VmDirFreeReplicationPage(pPage);
        pPage = NULL;

        retVal = _VmDirContinueReplicationCycle(&uiStartTimeInShutdown, pReplAgr, bervalSyncDoneCtrl);
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

        retVal = _VmDirFetchReplicationPage(
                pConnection,
                lastSupplierUsnProcessed, // used in search filter
                initUsn,                  // used in syncRequestCtrl to send(supplier) high watermark.
                &pPage);
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

        _VmDirProcessReplicationPage(pContext, pPage);

        if (pPage->lastSupplierUsnProcessed > lastSupplierUsnProcessed)
        {
            VMDIR_LOG_INFO(
                    LDAP_DEBUG_REPL,
                    "%s: Updating hw from %" PRId64 "to %" PRId64,
                    __FUNCTION__,
                    lastSupplierUsnProcessed,
                    pPage->lastSupplierUsnProcessed);
            lastSupplierUsnProcessed = pPage->lastSupplierUsnProcessed;
        }

        retVal = VmDirUpdateUtdVectorLocalCache(
                pUtdVectorMap,
                &(pPage->searchResCtrls[0]->ldctl_value));
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

        // Check if sync done control contains explicit continue indicator
        bContinue = VmDirStringStrA(
                   pPage->searchResCtrls[0]->ldctl_value.bv_val,
                   VMDIR_REPL_CONT_INDICATOR) ?
                   TRUE : FALSE;

        retVal = VmDirDDVectorParseString(
                VmDirStringStrA(pPage->searchResCtrls[0]->ldctl_value.bv_val, "vector:"),
                &bCompleteReplCycle);
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }
    while (bContinue && bCompleteReplCycle == FALSE);

    if (retVal == LDAP_SUCCESS)
    {
        retVal = VmDirSyncDoneCtrlFromLocalCache(
                lastSupplierUsnProcessed,
                pUtdVectorMap,
                &bervalSyncDoneCtrl);
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

        retVal = VmDirReplCookieUpdate(
                pContext->pSchemaCtx,
                &bervalSyncDoneCtrl,
                pReplAgr);
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

        VMDIR_LOG_INFO(
                VMDIR_LOG_MASK_ALL,
                "Replication supplier %s USN range (%llu,%s) processed",
                pReplAgr->ldapURI,
                initUsn,
                pReplAgr->lastLocalUsnProcessed.lberbv_val);
    }

collectmetrics:
    uiEndTime = VmDirGetTimeInMilliSec();
    if (VmDirReplMetricsCacheFind(pReplAgr->pszHostname, &pReplMetrics) == 0)
    {
        if (retVal == LDAP_SUCCESS)
        {
            VmMetricsHistogramUpdate(
                    pReplMetrics->pTimeCycleSucceeded,
                    VMDIR_RESPONSE_TIME(uiStartTime, uiEndTime));
        }
        // avoid collecting benign error counts
        else if (retVal != LDAP_BUSY &&         // role exclusion
                 retVal != LDAP_UNAVAILABLE &&  // server in mid-shutdown
                 retVal != LDAP_SERVER_DOWN &&  // connection lost
                 retVal != LDAP_TIMEOUT)        // connection lost
        {
            VmMetricsHistogramUpdate(
                    pReplMetrics->pTimeCycleFailed,
                    VMDIR_RESPONSE_TIME(uiStartTime, uiEndTime));
        }
    }

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszUtdVector);
    VmDirDDVectorClear();
    LwRtlHashMapClear(pUtdVectorMap, VmDirSimpleHashMapPairFreeKeyOnly, NULL);
    LwRtlFreeHashMap(&pUtdVectorMap);
    VmDirReplicationClearFailedEntriesFromQueue(pContext);
    VMDIR_SAFE_FREE_MEMORY(bervalSyncDoneCtrl.bv_val);
    _VmDirFreeReplicationPage(pPage);
    return;

ldaperror:
    *pbUpdateReplicationInterval = (*pbUpdateReplicationInterval || (retVal == LDAP_BUSY));
    if (retVal != LDAP_BUSY)
    {
        VMDIR_LOG_ERROR(
                VMDIR_LOG_MASK_ALL,
                "%s failed, error code (%d)",
                __FUNCTION__,
                retVal);
    }

    goto collectmetrics;
}

static
int
_VmDirFetchReplicationPage(
    PVMDIR_DC_CONNECTION        pConnection,
    USN                         lastSupplierUsnProcessed,
    USN                         initUsn,
    PVMDIR_REPLICATION_PAGE*    ppPage
    )
{
    int retVal = LDAP_SUCCESS;
    BOOLEAN bLogErr = TRUE;
    PSTR    pszUtdVector = NULL;
    LDAPControl*    srvCtrls[2] = {NULL, NULL};
    LDAPControl**   ctrls = NULL;
    PVMDIR_REPLICATION_PAGE pPage = NULL;
    LDAP*   pLd = NULL;
    struct timeval  tv = {0};
    struct timeval* pTv = NULL;

    if (gVmdirGlobals.dwLdapSearchTimeoutSec > 0)
    {
        tv.tv_sec =  gVmdirGlobals.dwLdapSearchTimeoutSec;
        pTv = &tv;
    }

    pLd = pConnection->pLd;

    if (VmDirAllocateMemory(sizeof(*pPage), (PVOID)&pPage))
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    pPage->iEntriesRequested = gVmdirServerGlobals.replPageSize;

    if (VmDirAllocateStringPrintf(
            &pPage->pszFilter,
            "%s>=%" PRId64,
            ATTR_USN_CHANGED,
            lastSupplierUsnProcessed + 1))
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    retVal = VmDirUTDVectorCacheToString(&pszUtdVector);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    retVal = VmDirCreateSyncRequestControl(
            gVmdirServerGlobals.invocationId.lberbv.bv_val,
            initUsn,
            pszUtdVector,
            initUsn == lastSupplierUsnProcessed || 0 == lastSupplierUsnProcessed, // it's fetching first page if TRUE
            &(pPage->syncReqCtrl));
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    srvCtrls[0] = &(pPage->syncReqCtrl);
    srvCtrls[1] = NULL;

    retVal = ldap_search_ext_s(
            pLd,
            "",
            LDAP_SCOPE_SUBTREE,
            pPage->pszFilter,
            NULL,
            FALSE,
            srvCtrls,
            NULL,
            pTv, // default 60 sec - time out on client/server side.
            pPage->iEntriesRequested,
            &(pPage->searchRes));

    if (retVal == LDAP_BUSY)
    {
        VMDIR_LOG_INFO(
                LDAP_DEBUG_REPL,
                "%s: partner (%s) is busy",
                __FUNCTION__,
                pConnection->pszHostname);

        bLogErr = FALSE;
    }
    else if (retVal)
    {   // for all other errors, force disconnect
        pConnection->connState = DC_CONNECTION_STATE_NOT_CONNECTED;
    }
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    pPage->iEntriesReceived = ldap_count_entries(pLd, pPage->searchRes);
    if (pPage->iEntriesReceived > 0)
    {
        LDAPMessage *entry = NULL;
        size_t iEntries = 0;

        if (VmDirAllocateMemory(
                pPage->iEntriesReceived * sizeof(VMDIR_REPLICATION_PAGE_ENTRY),
                (PVOID)&pPage->pEntries))
        {
            retVal = LDAP_OPERATIONS_ERROR;
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
        }

        for (entry = ldap_first_entry(pLd, pPage->searchRes);
             entry != NULL && iEntries < pPage->iEntriesRequested;
             entry = ldap_next_entry(pLd, entry))
        {
            int entryState = -1;
            USN ulPartnerUSN = 0;

            retVal = ldap_get_entry_controls(pLd, entry, &ctrls);
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

            retVal = ParseAndFreeSyncStateControl(&ctrls, &entryState, &ulPartnerUSN);
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

            retVal = VmDirAllocateStringA(
                    pConnection->pszHostname, &pPage->pEntries[iEntries].pszPartner);
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

            pPage->pEntries[iEntries].entry = entry;
            pPage->pEntries[iEntries].entryState = entryState;
            pPage->pEntries[iEntries].ulPartnerUSN = ulPartnerUSN;
            pPage->pEntries[iEntries].dwDnLength = 0;
            if (VmDirParseEntryForDn(entry, &(pPage->pEntries[iEntries].pszDn)) == 0)
            {
                pPage->pEntries[iEntries].dwDnLength = (DWORD)VmDirStringLenA(pPage->pEntries[iEntries].pszDn);
            }
            pPage->pEntries[iEntries].pBervEncodedEntry = NULL;

            iEntries++;
        }

        if (iEntries != pPage->iEntriesReceived)
        {
            retVal = LDAP_OPERATIONS_ERROR;
            BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
        }
    }

    retVal = ldap_parse_result(pLd, pPage->searchRes, NULL, NULL, NULL, NULL, &pPage->searchResCtrls, 0);
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    if (pPage->searchResCtrls[0] == NULL ||
        VmDirStringCompareA(pPage->searchResCtrls[0]->ldctl_oid, LDAP_CONTROL_SYNC_DONE, TRUE) != 0)
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    // Get last local USN processed from the cookie
    retVal = VmDirStringToINT64(
            pPage->searchResCtrls[0]->ldctl_value.bv_val,
            NULL,
            &(pPage->lastSupplierUsnProcessed));
    BAIL_ON_SIMPLE_LDAP_ERROR(retVal);

    *ppPage = pPage;

    if (pPage->iEntriesReceived > 0)
    {
        VMDIR_LOG_INFO(
                VMDIR_LOG_MASK_ALL,
                "%s: filter: '%s' to '%s' requested: %d received: %d usn: %" PRId64 " utd: '%s'",
                __FUNCTION__,
                VDIR_SAFE_STRING(pPage->pszFilter),
                VDIR_SAFE_STRING(pConnection->pszHostname),
                pPage->iEntriesRequested,
                pPage->iEntriesReceived,
                initUsn,
                VDIR_SAFE_STRING(pszUtdVector));
    }
    else
    {
        VMDIR_LOG_INFO(
                LDAP_DEBUG_REPL,
                "%s: received empty page: %s from: %s",
                __FUNCTION__,
                VDIR_SAFE_STRING(pPage->searchResCtrls[0]->ldctl_value.bv_val),
                VDIR_SAFE_STRING(pConnection->pszHostname));
    }

cleanup:
    if (ctrls)
    {
        ldap_controls_free(ctrls);
        ctrls = NULL;
    }
    VMDIR_SAFE_FREE_MEMORY(pszUtdVector);
    return retVal;

ldaperror:
    if (bLogErr && pPage)
    {
        VMDIR_LOG_ERROR(
                VMDIR_LOG_MASK_ALL,
                "%s: error: %d filter: '%s' requested: %d received: %d usn: %llu utd: '%s'",
                __FUNCTION__,
                retVal,
                VDIR_SAFE_STRING(pPage->pszFilter),
                pPage->iEntriesRequested,
                pPage->iEntriesReceived,
                initUsn,
                VDIR_SAFE_STRING(pszUtdVector));
    }
    _VmDirFreeReplicationPage(pPage);
    pPage = NULL;

    if (pConnection->connState != DC_CONNECTION_STATE_CONNECTED)
    {   // unbind after _VmDirFreeReplicationPage call
        VDIR_SAFE_UNBIND_EXT_S(pConnection->pLd);
    }
    goto cleanup;
}

static
VOID
_VmDirFreeReplicationPage(
    PVMDIR_REPLICATION_PAGE pPage
    )
{
    if (pPage)
    {
        VMDIR_SAFE_FREE_MEMORY(pPage->pszFilter);

        if (pPage->pEntries)
        {
            int i = 0;
            for (i = 0; i < pPage->iEntriesReceived; i++)
            {
                VmDirReplicationFreePageEntryContent(&pPage->pEntries[i]);
            }
            VMDIR_SAFE_FREE_MEMORY(pPage->pEntries);
        }

        if (pPage->searchResCtrls)
        {
            ldap_controls_free(pPage->searchResCtrls);
        }

        if (pPage->searchRes)
        {
            ldap_msgfree(pPage->searchRes);
        }

        VmDirFreeCtrlContent(&pPage->syncReqCtrl);

        VMDIR_SAFE_FREE_MEMORY(pPage);
    }
}

static
VOID
_VmDirReplicationUpdateTombStoneEntrySyncState(
    PVMDIR_REPLICATION_PAGE_ENTRY pPage
    )
{
    DWORD  dwError = 0;
    PSTR   pszObjectGuid = NULL;
    PSTR   pszTempString = NULL;
    PSTR   pszContext = NULL;
    PSTR   pszDupDn = NULL;
    VDIR_ENTRY_ARRAY  entryArray = {0};
    VDIR_BERVALUE     bvParentDn = VDIR_BERVALUE_INIT;
    VDIR_BERVALUE     bvDn = VDIR_BERVALUE_INIT;

    dwError = VmDirStringToBervalContent(pPage->pszDn, &bvDn);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirGetParentDN(&bvDn, &bvParentDn);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (VmDirIsDeletedContainer(bvParentDn.lberbv_val) &&
        pPage->entryState == LDAP_SYNC_ADD)
    {
        dwError = VmDirAllocateStringA(pPage->pszDn, &pszDupDn);
        BAIL_ON_VMDIR_ERROR(dwError);

        // Tombstone DN format: cn=<cn value>#objectGUID:<objectguid value>,<DeletedObjectsContainer>
        pszTempString = VmDirStringStrA(pszDupDn, "#objectGUID:");
        pszObjectGuid = VmDirStringTokA(pszTempString, ",", &pszContext);
        pszObjectGuid = pszObjectGuid + VmDirStringLenA("#objectGUID:");

        dwError = VmDirSimpleEqualFilterInternalSearch("", LDAP_SCOPE_SUBTREE, ATTR_OBJECT_GUID, pszObjectGuid, &entryArray);
        BAIL_ON_VMDIR_ERROR(dwError);

        if (entryArray.iSize == 1)
        {
            pPage->entryState = LDAP_SYNC_DELETE;
            VMDIR_LOG_INFO(
                VMDIR_LOG_MASK_ALL,
                "%s: (tombstone handling) change sync state to delete: (%s)",
                __FUNCTION__,
                pPage->pszDn);
        }
    }

cleanup:
    VmDirFreeBervalContent(&bvParentDn);
    VmDirFreeBervalContent(&bvDn);
    VMDIR_SAFE_FREE_MEMORY(pszDupDn);
    VmDirFreeEntryArrayContent(&entryArray);
    return;

error:
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "%s: error = (%d)",
            __FUNCTION__,
            dwError);
    goto cleanup;
}

/*
 * Perform Add/Modify/Delete on entries in page
 */
static
VOID
_VmDirProcessReplicationPage(
    PVMDIR_REPLICATION_CONTEXT pContext,
    PVMDIR_REPLICATION_PAGE pPage
    )
{
    PVDIR_SCHEMA_CTX pSchemaCtx = NULL;
    size_t i = 0;

    pSchemaCtx = pContext->pSchemaCtx;
    pPage->iEntriesProcessed = 0;
    pPage->iEntriesOutOfSequence = 0;
    for (i = 0; i < pPage->iEntriesReceived; i++)
    {
        int errVal = 0;
        int entryState = 0;

        _VmDirReplicationUpdateTombStoneEntrySyncState(pPage->pEntries+i);

        entryState = pPage->pEntries[i].entryState;

        if (entryState == LDAP_SYNC_ADD)
        {
            errVal = ReplAddEntry(pSchemaCtx, pPage->pEntries+i, &pSchemaCtx);
            pContext->pSchemaCtx = pSchemaCtx ;
        }
        else if (entryState == LDAP_SYNC_MODIFY)
        {
            errVal = ReplModifyEntry(pSchemaCtx, pPage->pEntries+i, &pSchemaCtx);
            pContext->pSchemaCtx = pSchemaCtx ;
        }
        else if (entryState == LDAP_SYNC_DELETE)
        {
            errVal = ReplDeleteEntry(pSchemaCtx, pPage->pEntries+i);
        }
        else
        {
            errVal = LDAP_OPERATIONS_ERROR;
        }
        pPage->pEntries[i].errVal = errVal;
        if (errVal == LDAP_SUCCESS)
        {
            pPage->iEntriesProcessed++;
        }
        else
        {
            VmDirReplicationPushFailedEntriesToQueue(pContext, pPage->pEntries+i);
        }

        if (errVal)
        {
            // TODO Consolidate error log and move this to internal operations
            VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL,
                    "%s: sync_state = (%d) dn = (%s) error = (%d)",
                    __FUNCTION__,
                    entryState,
                    pPage->pEntries[i].pszDn,
                    pPage->pEntries[i].errVal);
        }
        else
        {
            VMDIR_LOG_DEBUG(LDAP_DEBUG_REPL,
                    "%s: sync_state = (%d) dn = (%s) error = (%d)",
                    __FUNCTION__,
                    entryState,
                    pPage->pEntries[i].pszDn,
                    pPage->pEntries[i].errVal);
        }
    }

    pPage->iEntriesOutOfSequence = dequeGetSize(pContext->pFailedEntriesQueue);

    VMDIR_LOG_INFO(
        LDAP_DEBUG_REPL,
        "%s: "
        "filter: '%s' requested: %d received: %d last usn: %llu "
        "processed: %d out-of-sequence: %d ",
        __FUNCTION__,
        VDIR_SAFE_STRING(pPage->pszFilter),
        pPage->iEntriesRequested,
        pPage->iEntriesReceived,
        pPage->lastSupplierUsnProcessed,
        pPage->iEntriesProcessed,
        pPage->iEntriesOutOfSequence);

    VmDirReapplyFailedEntriesFromQueue(pContext);

    return;
}

static
int
VmDirParseEntryForDn(
    LDAPMessage *ldapEntryMsg,
    PSTR *ppszDn
    )
{
    int      retVal = LDAP_SUCCESS;
    BerElement *ber = NULL;
    BerValue lberbv = {0};
    PSTR pszDn = NULL;

    ber = ber_dup(ldapEntryMsg->lm_ber);
    if (ber == NULL)
    {
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }

    // Get entry DN. 'm' => pOperation->reqDn.lberbv.bv_val points to DN within (in-place) ber
    if ( ber_scanf(ber, "{m", &(lberbv)) == LBER_ERROR )
    {
        VMDIR_LOG_WARNING(VMDIR_LOG_MASK_ALL, "VmDirParseEntryForDnLen: ber_scanf failed" );
        retVal = LDAP_OPERATIONS_ERROR;
        BAIL_ON_SIMPLE_LDAP_ERROR(retVal);
    }
    if (lberbv.bv_val)
    {
        VmDirAllocateStringA(lberbv.bv_val, &pszDn);
    }

    *ppszDn = pszDn;

cleanup:

    if (ber)
    {
        ber_free(ber, 0);
    }
    return retVal;

ldaperror:

    VMDIR_SAFE_FREE_STRINGA(pszDn);
    goto cleanup;
}

static
VOID
_VmDirUpdateInvocationIdInReplAgr(
    VOID
    )
{
    BOOLEAN   bInLock = FALSE;
    BOOLEAN   bUpdate = FALSE;
    PVMDIR_REPLICATION_AGREEMENT  pReplAgr = NULL;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.replAgrsMutex);

    for (pReplAgr = gVmdirReplAgrs; pReplAgr; pReplAgr = pReplAgr->next)
    {
        if (pReplAgr->pszInvocationID == NULL)
        {
            bUpdate = TRUE;
            break;
        }
    }

    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.replAgrsMutex);

    if (bUpdate)
    {
        VmDirPopulateInvocationIdInReplAgr();
    }
}
