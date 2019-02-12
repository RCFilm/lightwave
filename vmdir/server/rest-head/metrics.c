/*
 * Copyright © 2018 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the ?~@~\License?~@~]); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ?~@~\AS IS?~@~] BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "includes.h"

static
VOID
_VmDirRESTLogExpensiveOperation(
    PVDIR_REST_OPERATION    pRestOp,
    METRICS_LDAP_OPS        operation,
    uint64_t                iRespTime
    );

DWORD
VmDirRestMetricsInit(
    VOID
    )
{
    DWORD   dwError = 0;
    DWORD   i = 0, j = 0;

    // use identical bucket for all histograms
    uint64_t buckets[8] = {50, 100, 250, 500, 1000, 2500, 3000, 4000};

    // use this template to construct labels
    VM_METRICS_LABEL    labels[2] =
    {
            {"operation",   NULL},
            {"code",        NULL}
    };

    for (i = 0; i < METRICS_LDAP_OP_COUNT; i++)
    {
        for (j = 0; j < METRICS_LDAP_ERROR_COUNT; j++)
        {
            labels[0].pszValue = VmDirMetricsLdapOperationString(i);
            labels[1].pszValue = VmDirMetricsLdapErrorString(j);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap",
                    labels, 2,
                    "Histogram for REST LDAP request durations in total",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_PROTOCOL]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the middle layer",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_MIDDLELAYER]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_preplugins",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the pre-plugins",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_PRE_PLUGINS]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_writequeue",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the writequeue",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_WRITE_QUEUE]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_backend",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the backend",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_BACKEND]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_backend_txnbegin",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the backend txnbegin",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_BACKEND_TXN_BEGIN]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_backend_txncommit",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the backend txncommit",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_BACKEND_TXN_COMMIT]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmMetricsHistogramNew(
                    pmContext,
                    "vmdir_rest_ldap_middlelayer_postplugins",
                    labels, 2,
                    "Histogram for REST LDAP request durations in the post-plugins",
                    buckets, 8,
                    &gpRestLdapMetrics[i][j][METRICS_LAYER_BACKEND_TXN_COMMIT]);
            BAIL_ON_VMDIR_ERROR(dwError);
        }
    }

cleanup:
    return dwError;

error:
    VMDIR_LOG_ERROR(
            VMDIR_LOG_MASK_ALL,
            "%s failed (%d)",
            __FUNCTION__,
            dwError);

    goto cleanup;
}

VOID
VmDirRestMetricsUpdate(
    METRICS_LDAP_OPS    operation,
    METRICS_LDAP_ERRORS error,
    METRICS_LAYERS      layer,
    uint64_t            iStartTime,
    uint64_t            iEndTime
    )
{
    PVM_METRICS_HISTOGRAM   pHistogram = NULL;

    pHistogram = gpRestLdapMetrics[operation][error][layer];
    if (pHistogram)
    {
        VmMetricsHistogramUpdate(
                pHistogram, VMDIR_RESPONSE_TIME(iStartTime, iEndTime));
    }
}

VOID
VmDirRestMetricsUpdateFromHandler(
    PVDIR_REST_OPERATION    pRestOp,
    uint64_t                iStartTime,
    uint64_t                iEndTime
    )
{
    METRICS_LDAP_OPS    operation = METRICS_LDAP_OP_IGNORE;
    METRICS_LDAP_ERRORS error = METRICS_LDAP_OTHER;
    METRICS_LAYERS      layer = METRICS_LAYER_PROTOCOL;

    if (pRestOp)
    {
        if (pRestOp->pMethod)
        {
            if (pRestOp->pMethod->pFnImpl == VmDirRESTLdapAdd)
            {
                operation = METRICS_LDAP_OP_ADD;
            }
            else if (pRestOp->pMethod->pFnImpl == VmDirRESTLdapModify)
            {
                operation = METRICS_LDAP_OP_MODIFY;
            }
            else if (pRestOp->pMethod->pFnImpl == VmDirRESTLdapDelete)
            {
                operation = METRICS_LDAP_OP_DELETE;
            }
            else if (pRestOp->pMethod->pFnImpl == VmDirRESTLdapSearch)
            {
                operation = METRICS_LDAP_OP_SEARCH;
            }
        }

        if (pRestOp->pResult)
        {
            error = VmDirMetricsMapLdapErrorToEnum(pRestOp->pResult->errCode);
        }

        if (operation != METRICS_LDAP_OP_IGNORE)
        {
            VmDirRestMetricsUpdate(operation, error, layer, iStartTime, iEndTime);
        }

        if (pRestOp->pConn->AccessInfo.pAccessToken)   // process request locally
        {
            _VmDirRESTLogExpensiveOperation(pRestOp, operation, iEndTime-iStartTime);
        }
    }
}

VOID
VmDirRestMetricsShutdown(
    VOID
    )
{
    DWORD   i = 0, j = 0, k = 0;

    for (i = 0; i < METRICS_LDAP_OP_COUNT; i++)
    {
        for (j = 0; j < METRICS_LDAP_ERROR_COUNT; j++)
        {
            for (k = 0; k < METRICS_LAYER_COUNT; k++)
            {
                gpRestLdapMetrics[i][j][k] = NULL;
            }
        }
    }
}

static
VOID
_VmDirRESTLogExpensiveOperation(
    PVDIR_REST_OPERATION    pRestOp,
    METRICS_LDAP_OPS        operation,
    uint64_t                iRespTime
    )
{
    PVDIR_SUPERLOG_RECORD pSupLog = &pRestOp->pConn->SuperLogRec;

    if (operation == METRICS_LDAP_OP_SEARCH &&
        iRespTime > gVmdirServerGlobals.dwEfficientReadOpTimeMS)
    {
        VMDIR_LOG_WARNING(
            VMDIR_LOG_MASK_ALL,
            "Inefficient search %d MS - base=(%s), scope=%s, filter=%s, scan cnt=%d, return cnt=%d BindDN=(%s)",
            iRespTime,
            VDIR_SAFE_STRING(pSupLog->opInfo.searchInfo.pszBaseDN),
            VDIR_SAFE_STRING(pSupLog->opInfo.searchInfo.pszScope),
            VDIR_SAFE_STRING(pSupLog->pszOperationParameters),
            pSupLog->opInfo.searchInfo.dwScanned,
            pSupLog->opInfo.searchInfo.dwReturned,
            VDIR_SAFE_STRING(pRestOp->pConn->AccessInfo.pszBindedDn));
    }
}
