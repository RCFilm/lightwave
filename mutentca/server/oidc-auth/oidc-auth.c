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

static
DWORD
LwCAOIDCTokenAllocate(
    PLWCA_OIDC_TOKEN    *ppOIDCToken
    );

static
VOID
LwCAOIDCTokenFree(
    PLWCA_OIDC_TOKEN    pOIDCToken
    );

static
DWORD
LwCAOIDCHOTKValuesAllocate(
    PLWCA_OIDC_HOTK_VALUES  *ppHOTKValues
    );

static
VOID
LwCAOIDCHOTKValuesFree(
    PLWCA_OIDC_HOTK_VALUES  pHOTKValues
    );

static
DWORD
LwCAOIDCGetSigningCertPEM(
    PSTR*                   ppszOIDCSigningCertPEM
    );

static
DWORD
LwCAOIDCTokenParse(
    PLWCA_REQ_CONTEXT   pReqCtx,
    PLWCA_OIDC_TOKEN    pOIDCToken,
    PCSTR               pcszReqMethod,
    PCSTR               pcszReqContentType,
    PCSTR               pcszReqDate,
    PCSTR               pcszReqBody,
    PCSTR               pcszReqURI
    );

static
DWORD
LwCAOIDCTokenValidate(
    PLWCA_REQ_CONTEXT   pReqCtx,
    PLWCA_OIDC_TOKEN    pOIDCToken
    );

static
DWORD
LwCAOIDCTokenValidatePOP(
    PLWCA_REQ_CONTEXT   pReqCtx,
    PLWCA_OIDC_TOKEN    pOIDCToken
    );


DWORD
LwCAOIDCTokenAuthenticate(
    PLWCA_REQ_CONTEXT       pReqCtx,
    PCSTR                   pcszReqAuthHdr,
    PCSTR                   pcszReqMethod,
    PCSTR                   pcszReqContentType,
    PCSTR                   pcszReqDate,
    PCSTR                   pcszReqBody,
    PCSTR                   pcszReqURI,
    PBOOLEAN                pbAuthenticated
    )
{
    DWORD                   dwError = 0;
    PLWCA_OIDC_TOKEN        pOIDCToken = NULL;

    if (!pbAuthenticated || !pReqCtx)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    if (IsNullOrEmptyString(pcszReqAuthHdr))
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwError = LwCAOIDCTokenAllocate(&pOIDCToken);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(pcszReqAuthHdr, &pOIDCToken->pszReqAuthHdr);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAOIDCTokenParse(
                    pReqCtx,
                    pOIDCToken,
                    pcszReqMethod,
                    pcszReqContentType,
                    pcszReqDate,
                    pcszReqBody,
                    pcszReqURI);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAOIDCTokenValidate(pReqCtx, pOIDCToken);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(pOIDCToken->pszReqBindUPN, &pReqCtx->pszBindUPN);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAUPNToDN(pOIDCToken->pszReqBindUPN, &pReqCtx->pszBindUPNDN);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(pOIDCToken->pszReqBindUPNTenant, &pReqCtx->pszBindUPNTenant);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCACopyStringArray(pOIDCToken->pReqBindUPNGroups, &pReqCtx->pBindUPNGroups);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAOIDCTokenValidatePOP(pReqCtx, pOIDCToken);
    BAIL_ON_LWCA_ERROR(dwError);

    LWCA_LOG_INFO(
            "[%s:%d] Authenticated OIDC token. UPN (%s) ReqID (%s)",
            __FUNCTION__,
            __LINE__,
            LWCA_SAFE_STRING(pReqCtx->pszBindUPN),
            LWCA_SAFE_STRING(pReqCtx->pszRequestId));

    *pbAuthenticated = TRUE;


cleanup:

    LwCAOIDCTokenFree(pOIDCToken);

    return dwError;

error:

    if (pbAuthenticated)
    {
        *pbAuthenticated = FALSE;
    }
    LWCA_SAFE_FREE_STRINGA(pReqCtx->pszBindUPN);
    pReqCtx->pszBindUPN = NULL;
    LWCA_SAFE_FREE_STRINGA(pReqCtx->pszBindUPNDN);
    pReqCtx->pszBindUPNDN = NULL;
    LWCA_SAFE_FREE_STRINGA(pReqCtx->pszBindUPNTenant);
    pReqCtx->pszBindUPNTenant = NULL;
    LwCAFreeStringArray(pReqCtx->pBindUPNGroups);
    pReqCtx->pBindUPNGroups = NULL;

    LWCA_LOG_ERROR(
            "[%s:%d] Failed to authenticate OIDC token. Error (%d) UPN (%s) ReqId (%s)",
            __FUNCTION__,
            __LINE__,
            dwError,
            LWCA_SAFE_STRING(pReqCtx->pszBindUPN),
            LWCA_SAFE_STRING(pReqCtx->pszRequestId));

    goto cleanup;
}

static
DWORD
LwCAOIDCTokenAllocate(
    PLWCA_OIDC_TOKEN    *ppOIDCToken
    )
{
    DWORD               dwError = 0;
    PLWCA_OIDC_TOKEN    pOIDCToken = NULL;

    if (!ppOIDCToken)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    dwError = LwCAAllocateMemory(sizeof(LWCA_OIDC_TOKEN), (PVOID *)&pOIDCToken);
    BAIL_ON_LWCA_ERROR(dwError);

    *ppOIDCToken = pOIDCToken;


cleanup:

    return dwError;

error:

    LwCAOIDCTokenFree(pOIDCToken);
    if (ppOIDCToken)
    {
        *ppOIDCToken = NULL;
    }

    goto cleanup;
}

static
VOID
LwCAOIDCTokenFree(
    PLWCA_OIDC_TOKEN    pOIDCToken
    )
{
    if (pOIDCToken)
    {
        LWCA_SAFE_FREE_STRINGA(pOIDCToken->pszReqAuthHdr);
        LWCA_SAFE_FREE_STRINGA(pOIDCToken->pszReqOIDCToken);
        LwCAOIDCHOTKValuesFree(pOIDCToken->pReqHOTKValues);
        LWCA_SAFE_FREE_STRINGA(pOIDCToken->pszReqBindUPN);
        LWCA_SAFE_FREE_STRINGA(pOIDCToken->pszReqBindUPNTenant);
        LwCAFreeStringArray(pOIDCToken->pReqBindUPNGroups);
        LWCA_SAFE_FREE_MEMORY(pOIDCToken);
    }
}

static
DWORD
LwCAOIDCHOTKValuesAllocate(
    PLWCA_OIDC_HOTK_VALUES  *ppHOTKValues
    )
{
    DWORD                   dwError = 0;
    PLWCA_OIDC_HOTK_VALUES  pHOTKValues = NULL;

    if (!ppHOTKValues)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    dwError = LwCAAllocateMemory(sizeof(LWCA_OIDC_HOTK_VALUES), (PVOID *)&pHOTKValues);
    BAIL_ON_LWCA_ERROR(dwError);

    *ppHOTKValues = pHOTKValues;


cleanup:

    return dwError;

error:

    LwCAOIDCHOTKValuesFree(pHOTKValues);
    if (ppHOTKValues)
    {
        *ppHOTKValues = NULL;
    }

    goto cleanup;
}

static
VOID
LwCAOIDCHOTKValuesFree(
    PLWCA_OIDC_HOTK_VALUES  pHOTKValues
    )
{
    if (pHOTKValues)
    {
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqHOTKPEM);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqHexPOP);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqMethod);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqContentType);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqDate);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqBody);
        LWCA_SAFE_FREE_STRINGA(pHOTKValues->pszReqURI);
        LWCA_SAFE_FREE_MEMORY(pHOTKValues);
    }
}

static
DWORD
LwCAOIDCGetSigningCertPEM(
    PSTR                    *ppszOIDCSigningCertPEM
    )
{
    DWORD                   dwError = 0;
    DWORD                   dwOIDCError = 0;
    POIDC_SERVER_METADATA   pOidcMetadata = NULL;
    PSTR                    pszDomainName = NULL;
    PSTR                    pszDCName = NULL;
    PSTR                    pszOIDCSigningCertPEM = NULL;

    if (!ppszOIDCSigningCertPEM)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    dwError = LwCAGetDomainName(&pszDomainName);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAGetDCName(&pszDCName);
    BAIL_ON_LWCA_ERROR(dwError);

    dwOIDCError = OidcServerMetadataAcquire(
                        &pOidcMetadata,
                        pszDCName,
                        LWCA_OIDC_SERVER_PORT,
                        pszDomainName,
                        LIGHTWAVE_TLS_CA_PATH);
    dwError = LwCAOIDCToLwCAError(dwOIDCError);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(
                    OidcServerMetadataGetSigningCertificatePEM(pOidcMetadata),
                    &pszOIDCSigningCertPEM);
    BAIL_ON_LWCA_ERROR(dwError);

    *ppszOIDCSigningCertPEM = pszOIDCSigningCertPEM;


cleanup:

    OidcServerMetadataDelete(pOidcMetadata);
    LWCA_SAFE_FREE_STRINGA(pszDCName);
    LWCA_SAFE_FREE_STRINGA(pszDomainName);

    return dwError;

error:

    LWCA_LOG_ERROR(
            "[%s:%d] Failed to acquire OIDC metatdata. Error (%d) OIDC Error (%d)",
            __FUNCTION__,
            __LINE__,
            dwError,
            dwOIDCError);

    LWCA_SAFE_FREE_MEMORY(pszOIDCSigningCertPEM);
    if (ppszOIDCSigningCertPEM)
    {
        *ppszOIDCSigningCertPEM = NULL;
    }

    goto cleanup;
}

static
DWORD
LwCAOIDCTokenParse(
    PLWCA_REQ_CONTEXT       pReqCtx,
    PLWCA_OIDC_TOKEN        pOIDCToken,
    PCSTR                   pcszReqMethod,
    PCSTR                   pcszReqContentType,
    PCSTR                   pcszReqDate,
    PCSTR                   pcszReqBody,
    PCSTR                   pcszReqURI
    )
{
    DWORD                   dwError = 0;
    PSTR                    pszReqAuthHdrCpy = NULL;
    PSTR                    pszOIDCTokenType = NULL;
    PSTR                    pszOIDCToken = NULL;
    PSTR                    pszReqHexPOP = NULL;
    PSTR                    pszStrTokCtx = NULL;
    PLWCA_OIDC_HOTK_VALUES  pReqHOTKValues = NULL;

    if (!pOIDCToken || !pReqCtx)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    dwError = LwCAAllocateStringA(pOIDCToken->pszReqAuthHdr, &pszReqAuthHdrCpy);
    BAIL_ON_LWCA_ERROR(dwError);

    pszOIDCTokenType = LwCAStringTokA(pszReqAuthHdrCpy, " ", &pszStrTokCtx);
    if (IsNullOrEmptyString(pszOIDCTokenType) ||
        IsNullOrEmptyString(pszStrTokCtx))
    {
        LWCA_LOG_ERROR(
                "[%s:%d] No OIDC token type in request header. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    pszOIDCToken = LwCAStringTokA(pszStrTokCtx, ":", &pszReqHexPOP);
    if (IsNullOrEmptyString(pszOIDCToken))
    {
        LWCA_LOG_ERROR(
                "[%s:%d] No OIDC token in request header. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwError = LwCAAllocateStringA(pszOIDCToken, &pOIDCToken->pszReqOIDCToken);
    BAIL_ON_LWCA_ERROR(dwError);

    if (LwCAStringCompareA(pszOIDCTokenType, LWCA_OIDC_BEARER_KEY, FALSE) == 0)
    {
        if (!IsNullOrEmptyString(pszReqHexPOP))
        {
            LWCA_LOG_ERROR(
                    "[%s:%d] Bearer token should not have POP. ReqID (%s)",
                    __FUNCTION__,
                    __LINE__,
                    LWCA_SAFE_STRING(pReqCtx->pszRequestId));
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }

        pOIDCToken->oidcTokenType = LWCA_OIDC_TOKEN_TYPE_BEARER;
    }
    else if (LwCAStringCompareA(pszOIDCTokenType, LWCA_OIDC_HOTK_KEY, FALSE) == 0)
    {
        if (IsNullOrEmptyString(pszReqHexPOP))
        {
            LWCA_LOG_ERROR(
                    "[%s:%d] HOTK token missing POP. ReqID (%s)",
                    __FUNCTION__,
                    __LINE__,
                    LWCA_SAFE_STRING(pReqCtx->pszRequestId));
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }

        if (IsNullOrEmptyString(pcszReqMethod))
        {
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }
        if (IsNullOrEmptyString(pcszReqContentType))
        {
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }
        if (IsNullOrEmptyString(pcszReqDate))
        {
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }
        if (IsNullOrEmptyString(pcszReqURI))
        {
            BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
        }

        pOIDCToken->oidcTokenType = LWCA_OIDC_TOKEN_TYPE_HOTK;

        dwError = LwCAOIDCHOTKValuesAllocate(&pReqHOTKValues);
        BAIL_ON_LWCA_ERROR(dwError);

        dwError = LwCAAllocateStringA(pszReqHexPOP, &pReqHOTKValues->pszReqHexPOP);
        BAIL_ON_LWCA_ERROR(dwError);

        dwError = LwCAAllocateStringA(pcszReqMethod, &pReqHOTKValues->pszReqMethod);
        BAIL_ON_LWCA_ERROR(dwError);

        dwError = LwCAAllocateStringA(pcszReqContentType, &pReqHOTKValues->pszReqContentType);
        BAIL_ON_LWCA_ERROR(dwError);

        dwError = LwCAAllocateStringA(pcszReqDate, &pReqHOTKValues->pszReqDate);
        BAIL_ON_LWCA_ERROR(dwError);

        if (!IsNullOrEmptyString(pcszReqBody))
        {
            dwError = LwCAAllocateStringA(pcszReqBody, &pReqHOTKValues->pszReqBody);
            BAIL_ON_LWCA_ERROR(dwError);
        }

        dwError = LwCAAllocateStringA(pcszReqURI, &pReqHOTKValues->pszReqURI);
        BAIL_ON_LWCA_ERROR(dwError);

        pOIDCToken->pReqHOTKValues = pReqHOTKValues;
    }
    else
    {
        LWCA_LOG_ERROR(
                "[%s:%d] Unsupported token type presented. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_UNKNOWN_TOKEN);
    }


cleanup:

    LWCA_SAFE_FREE_STRINGA(pszReqAuthHdrCpy);

    return dwError;

error:

    LwCAOIDCHOTKValuesFree(pReqHOTKValues);
    pOIDCToken->pReqHOTKValues = NULL;
    LWCA_SAFE_FREE_STRINGA(pOIDCToken->pszReqOIDCToken);
    pOIDCToken->oidcTokenType = LWCA_OIDC_TOKEN_TYPE_UNKNOWN;

    goto cleanup;
}

static
DWORD
LwCAOIDCTokenValidate(
    PLWCA_REQ_CONTEXT       pReqCtx,
    PLWCA_OIDC_TOKEN        pOIDCToken
    )
{
    DWORD                   dwError = 0;
    DWORD                   dwOIDCError = 0;
    size_t                  szReqNumBindUPNGroups = 0;
    PSTR                    pszOIDCSigningCert = NULL;
    PSTR                    pszReqHOTKPEM = NULL;
    PSTR                    pszReqBindUPN = NULL;
    PSTR                    pszReqBindUPNTenant = NULL;
    const PSTRING           *ppGroups = NULL;
    PLWCA_STRING_ARRAY      pReqBindUPNGroups = NULL;
    POIDC_ACCESS_TOKEN      pOIDCAccessToken = NULL;

    if (!pOIDCToken || !pReqCtx)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    if (IsNullOrEmptyString(pOIDCToken->pszReqOIDCToken))
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwOIDCError = OidcAccessTokenParse(&pOIDCAccessToken, pOIDCToken->pszReqOIDCToken);
    dwError = LwCAOIDCToLwCAError(dwOIDCError);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(OidcAccessTokenGetSubject(pOIDCAccessToken), &pszReqBindUPN);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(OidcAccessTokenGetTenant(pOIDCAccessToken), &pszReqBindUPNTenant);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAAllocateStringA(OidcAccessTokenGetHolderOfKeyPEM(pOIDCAccessToken), &pszReqHOTKPEM);
    BAIL_ON_LWCA_ERROR(dwError);

    dwError = LwCAOIDCGetSigningCertPEM(&pszOIDCSigningCert);
    BAIL_ON_LWCA_ERROR(dwError);

    if (IsNullOrEmptyString(pszReqBindUPN) ||
        IsNullOrEmptyString(pszReqBindUPNTenant))
    {
        LWCA_LOG_ERROR(
                "[%s,%d] OIDC token does not have subject claim or tenant claim. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwOIDCError = OidcAccessTokenValidate(
                            pOIDCAccessToken,
                            pszOIDCSigningCert,
                            NULL,
                            LWCA_OIDC_RS_SCOPE,
                            LWCA_OIDC_CLOCK_TOLERANCE);
    dwError = LwCAOIDCToLwCAError(dwOIDCError);
    BAIL_ON_LWCA_ERROR(dwError);

    OidcAccessTokenGetGroups(pOIDCAccessToken, &ppGroups, &szReqNumBindUPNGroups);
    if (!ppGroups || szReqNumBindUPNGroups == 0)
    {
        LWCA_LOG_ERROR(
                "[%s,%d] OIDC token does not have user groups. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwError = LwCACreateStringArray(
                        (PSTR *)ppGroups,
                        (DWORD)szReqNumBindUPNGroups,
                        &pReqBindUPNGroups);
    BAIL_ON_LWCA_ERROR(dwError);

    pOIDCToken->pszReqBindUPN = pszReqBindUPN;
    pOIDCToken->pszReqBindUPNTenant = pszReqBindUPNTenant;
    pOIDCToken->pReqBindUPNGroups = pReqBindUPNGroups;
    if (pOIDCToken->oidcTokenType == LWCA_OIDC_TOKEN_TYPE_HOTK)
    {
        pOIDCToken->pReqHOTKValues->pszReqHOTKPEM = pszReqHOTKPEM;
    }


cleanup:

    LWCA_SAFE_FREE_STRINGA(pszOIDCSigningCert);
    OidcAccessTokenDelete(pOIDCAccessToken);

    return dwError;

error:

    LWCA_SAFE_FREE_STRINGA(pszReqBindUPN);
    pOIDCToken->pszReqBindUPN = NULL;
    LWCA_SAFE_FREE_STRINGA(pszReqBindUPNTenant);
    pOIDCToken->pszReqBindUPNTenant = NULL;
    LwCAFreeStringArray(pReqBindUPNGroups);
    pOIDCToken->pReqBindUPNGroups = NULL;
    LWCA_SAFE_FREE_STRINGA(pszReqHOTKPEM);
    if (pOIDCToken->oidcTokenType == LWCA_OIDC_TOKEN_TYPE_HOTK)
    {
        pOIDCToken->pReqHOTKValues->pszReqHOTKPEM = NULL;
    }

    LWCA_LOG_ERROR(
            "[%s:%d] Failed to validate OIDC token. Error (%d) OIDC Error (%d) ReqID (%s)",
            __FUNCTION__,
            __LINE__,
            dwError,
            dwOIDCError,
            LWCA_SAFE_STRING(pReqCtx->pszRequestId));

    goto cleanup;
}

static
DWORD
LwCAOIDCTokenValidatePOP(
    PLWCA_REQ_CONTEXT           pReqCtx,
    PLWCA_OIDC_TOKEN            pOIDCToken
    )
{
    DWORD                       dwError = 0;
    PLWCA_OIDC_HOTK_VALUES      pReqHOTKValues = NULL;
    BOOLEAN                     bVerified = FALSE;

    if (!pOIDCToken || !pReqCtx)
    {
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_INVALID_PARAMETER);
    }

    if (pOIDCToken->oidcTokenType != LWCA_OIDC_TOKEN_TYPE_HOTK)
    {
        // POP validation is only for HOTK tokens - other supported token types
        // do not present a POP
        goto cleanup;
    }

    pReqHOTKValues = pOIDCToken->pReqHOTKValues;

    if (!pReqHOTKValues)
    {
        LWCA_LOG_ERROR(
                "[%s:%d] Missing POP signature or HOTK PEM. ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }

    dwError = VmHttpClientVerifySignedRequest(
                        pReqHOTKValues->pszReqMethod,
                        pReqHOTKValues->pszReqBody,
                        pReqHOTKValues->pszReqContentType,
                        pReqHOTKValues->pszReqDate,
                        pReqHOTKValues->pszReqURI,
                        pReqHOTKValues->pszReqHexPOP,
                        pReqHOTKValues->pszReqHOTKPEM,
                        &bVerified);
    BAIL_ON_LWCA_ERROR(dwError);

    if (!bVerified)
    {
        LWCA_LOG_ALERT(
                "[%s:%d] Request POP validation failed! ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
        BAIL_WITH_LWCA_ERROR(dwError, LWCA_ERROR_OIDC_BAD_AUTH_DATA);
    }
    else
    {
        LWCA_LOG_INFO(
                "[%s:%d] Validated request POP. UPN (%s) ReqID (%s)",
                __FUNCTION__,
                __LINE__,
                LWCA_SAFE_STRING(pReqCtx->pszBindUPN),
                LWCA_SAFE_STRING(pReqCtx->pszRequestId));
    }


cleanup:

    return dwError;

error:

    goto cleanup;
}
