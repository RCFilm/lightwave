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

#include "includes.h"

/*
 * REST_MODULE (from copenapitypes.h)
 * callback indices must correspond to:
 *      GET, PUT, POST, DELETE, PATCH
 */
static REST_MODULE _rest_module[] =
{
    {
        "/v1/mutentca/crl",
        {LwCARestGetRootCACRL, NULL, NULL, NULL, NULL}
    },
    {
        "/v1/mutentca/intermediate/*/crl",
        {LwCARestGetIntermediateCACRL, NULL, NULL, NULL, NULL}
    },
    {0}
};

DWORD
LwCARestCRLModule(
    PREST_MODULE*   ppRestModule
    )
{
    *ppRestModule = _rest_module;
    return 0;
}

/*
 * Returns Root CA CRL
 */
DWORD
LwCARestGetRootCACRL(
    PVOID   pIn,
    PVOID*  ppOut
    )
{
    DWORD                   dwError         = 0;
    PLWCA_REST_OPERATION    pRestOp         = NULL;
    PLWCA_CRL               pCrl            = NULL;
    PSTR                    pszRootCAId     = NULL;
    PLWCA_STRING_ARRAY      pCrlList        = NULL;

    if (!pIn)
    {
        dwError = LWCA_ERROR_INVALID_PARAMETER;
        BAIL_ON_LWCA_ERROR(dwError);
    }

    pRestOp = (PLWCA_REST_OPERATION)pIn;

    dwError = LwCAGetRootCAId(&pszRootCAId);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAGetCACrl(pRestOp->pReqCtx, pszRootCAId, &pCrl);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCACreateStringArray(&pCrl, 1, &pCrlList);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCARestResultSetStrArrayData(
                  pRestOp->pResult,
                  LWCA_JSON_KEY_CRL,
                  pCrlList);
    BAIL_ON_LWCA_ERROR(dwError);

cleanup:
    LwCASetRestResult(pRestOp, dwError);
    LwCAFreeCrl(pCrl);
    LWCA_SAFE_FREE_STRINGA(pszRootCAId);
    LwCAFreeStringArray(pCrlList);

    return dwError;

error:
    LWCA_LOG_ERROR(
            "%s failed, error (%d)",
            __FUNCTION__,
            dwError);

    goto cleanup;
}

/*
 * Returns Intermediate CA CRL
 */
DWORD
LwCARestGetIntermediateCACRL(
    PVOID   pIn,
    PVOID*  ppOut
    )
{
    DWORD                   dwError         = 0;
    PLWCA_REST_OPERATION    pRestOp         = NULL;
    PSTR                    pszCAId         = NULL;
    PLWCA_CRL               pCrl            = NULL;
    PLWCA_STRING_ARRAY      pCrlList        = NULL;

    if (!pIn)
    {
        dwError = LWCA_ERROR_INVALID_PARAMETER;
        BAIL_ON_LWCA_ERROR(dwError);
    }

    pRestOp = (PLWCA_REST_OPERATION)pIn;

    dwError = LwCARestGetStrParam(pRestOp, LWCA_REST_PARAM_CA_ID, &pszCAId, TRUE);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAGetCACrl(pRestOp->pReqCtx, pszCAId, &pCrl);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCACreateStringArray(&pCrl, 1, &pCrlList);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCARestResultSetStrArrayData(
                  pRestOp->pResult,
                  LWCA_JSON_KEY_CRL,
                  pCrlList);
    BAIL_ON_LWCA_ERROR(dwError);

cleanup:
    LwCASetRestResult(pRestOp, dwError);
    LWCA_SAFE_FREE_STRINGA(pszCAId);
    LwCAFreeCrl(pCrl);
    LwCAFreeStringArray(pCrlList);

    return dwError;

error:
    LWCA_LOG_ERROR(
            "%s failed, error (%d)",
            __FUNCTION__,
            dwError);

    goto cleanup;
}
