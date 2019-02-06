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

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief    Default Lightwave AuthZ Access Check Function
 *
 * @details  LwCAAuthZLWCheckAccess is the entry point to authorize a request
 *           against the default Lightwave policy.
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    apiPermissions indicates what API permissions to authorize the request
 *           against.
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure
 */
DWORD
LwCAAuthZLWCheckAccess(
    PLWCA_REQ_CONTEXT               pReqCtx,                // IN
    PCSTR                           pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA            *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    LWCA_AUTHZ_API_PERMISSION       apiPermissions,         // IN
    PBOOLEAN                        pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for Get CA Trusted Root Cert
 *
 * @details  LwCAAuthZLWCheckGetCACert will determine if a requestor is allowed
 *           to get a CA's trusted root certificate.  The default Lightwave
 *           rules are:
 *               - Anyone can obtain a CA's trusted root certificate
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure
 */
DWORD
LwCAAuthZLWCheckGetCACert(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for Create CA
 *
 * @details  LwCAAuthZLWCheckGetCACRL will determine if a requestor is allowed to
 *           get a CA's CRL.  The default Lightwave rules are:
 *               - Anyone can obtain a CA's CRL
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure
 */
DWORD
LwCAAuthZLWCheckGetCACRL(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for Create CA
 *
 * @details  LwCAAuthZLWCheckCACreate will determine if a requestor is allowed to
 *           create a CA.  The default Lightwave rules are:
 *               - Must be a member of either CAAdmins or CAOperators
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure
 */
DWORD
LwCAAuthZLWCheckCACreate(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for Revoke CA
 *
 * @details  LwCAAuthZLWCheckCARevoke will determine if a requestor is allowed to
 *           revoke a CA.  The default Lightwave rules are:
 *               - Must be a member of CAAdmins group
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure
 */
DWORD
LwCAAuthZLWCheckCARevoke(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for CSR
 *
 * @details  LwCAAuthZLWCheckCertSign will determine if a requestor is allowed to
 *           submit a CSR.  The default Lightwave rules are:
 *               - Any Lightwave user in the same tenant as requested in the request
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure*
 */
DWORD
LwCAAuthZLWCheckCertSign(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );

/**
 * @brief    Default Lightwave Access Check for Revoke Certificate
 *
 * @details  LwCAAuthZLWCheckCertRevoke will determine if a requestor is allowed to
 *           revoke a certificate.  The default Lightwave rules are:
 *               - Must be a member of CAAdmins
 *
 * @param    pReqCtx holds information about the requestor
 * @param    pcszCAId is CA that the request is for.
 * @param    pX509Data is a wrapper which holds a pointer to the X509 data to use
 *           to authorize the API
 * @param    pbAuthorized returns a boolean to indicate if the user is authorized
 *
 * @return   DWORD indicating function success/failure*
 */
DWORD
LwCAAuthZLWCheckCertRevoke(
    PLWCA_REQ_CONTEXT           pReqCtx,                // IN
    PCSTR                       pcszCAId,               // IN
    LWCA_AUTHZ_X509_DATA        *pX509Data,             // IN OPTIONAL: Only needed if AuthZ check requires X509 data
    PBOOLEAN                    pbAuthorized            // OUT
    );


#ifdef __cplusplus
}
#endif
