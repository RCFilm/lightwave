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

#ifndef __LWCA_COMMON_H__
#define __LWCA_COMMON_H__

#include <pthread.h>
#include <dlfcn.h>
#include <regex.h>

#if !defined(NO_LIKEWISE)
#include <lw/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// We don't want the LikeWise headers since we conflict
// with Unix ODBC, and we are on Unix. Define all types ourselves
#if (defined NO_LIKEWISE &&  !defined _WIN32)

#ifndef LWCA_WCHAR16_T_DEFINED
#define LWCA_WCHAR16_T_DEFINED 1
typedef unsigned short int wchar16_t, *PWSTR;
#endif /* LWCA_WCHAR16_T_DEFINED */

#ifndef LWCA_PSTR_DEFINED
#define LWCA_PSTR_DEFINED 1
typedef char* PSTR;
#endif /* LWCA_PSTR_DEFINED */

#ifndef LWCA_PCSTR_DEFINED
#define LWCA_PCSTR_DEFINED 1
typedef const char* PCSTR;
#endif /* LWCA_PCSTR_DEFINED */

#ifndef LWCA_PCWSTR_DEFINED
#define LWCA_PCWSTR_DEFINED 1
typedef const wchar16_t* PCWSTR;
#endif /* LWCA_PCWSTR_DEFINED */

#ifndef LWCA_BYTE_DEFINED
#define LWCA_BYTE_DEFINED 1
typedef unsigned char BYTE;
#endif /* LWCA_BYTE_DEFINED */

#ifndef LWCA_VOID_DEFINED
#define LWCA_VOID_DEFINED 1

typedef void VOID, *PVOID;
#endif /* LWCA_VOID_DEFINED */

#ifndef LWCA_UINT8_DEFINED
#define LWCA_UINT8_DEFINED 1
typedef uint8_t  UINT8;
#endif /* LWCA_UINT8_DEFINED */

#ifndef LWCA_UINT32_DEFINED
#define LWCA_UINT32_DEFINED 1
typedef uint32_t UINT32;
#endif /* LWCA_UINT32_DEFINED */

#ifndef LWCA_DWORD_DEFINED
#define LWCA_DWORD_DEFINED 1
typedef uint32_t DWORD, *PDWORD;
#endif /* LWCA_DWORD_DEFINED */

#ifndef LWCA_BOOLEAN_DEFINED
#define LWCA_BOOLEAN_DEFINED 1
typedef UINT8 BOOLEAN, *PBOOLEAN;
#endif /* LWCA_BOOLEAN_DEFINED */

#endif /* defined NO_LIKEWISE &&  !defined _WIN32 */

typedef struct _LWCA_CFG_CONNECTION* PLWCA_CFG_CONNECTION;
typedef struct _LWCA_CFG_KEY*        PLWCA_CFG_KEY;


#define LWCA_ASCII_aTof(c)     ( (c) >= 'a' && (c) <= 'f' )
#define LWCA_ASCII_AToF(c)     ( (c) >= 'A' && (c) <= 'F' )
#define LWCA_ASCII_DIGIT(c)    ( (c) >= '0' && (c) <= '9' )
#define LWCA_ASCII_SPACE(c) \
    ( (c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' )

#define LWCA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LWCA_MAX(a, b) ((a) > (b) ? (a) : (b))

#define LWCA_SAFE_FREE_STRINGA(PTR)     \
    do {                                \
        if ((PTR)) {                    \
            LwCAFreeStringA(PTR);       \
            (PTR) = NULL;               \
        }                               \
    } while(0)

#define LWCA_SAFE_FREE_STRINGW(PTR)     \
    do {                                \
        if ((PTR)) {                    \
            LwCAFreeStringW(PTR);       \
            (PTR) = NULL;               \
        }                               \
    } while(0)

#define LWCA_SAFE_FREE_MEMORY(PTR)  \
    do {                            \
        if ((PTR)) {                \
            LwCAFreeMemory(PTR);    \
            (PTR) = NULL;           \
        }                           \
    } while(0)

#define LWCA_SECURE_SAFE_FREE_MEMORY(PTR, n)     \
    do {                                         \
        if ((PTR)) {                             \
            if ((n) > 0) {                       \
                memset(PTR, 0, n);               \
            }                                    \
            LwCAFreeMemory(PTR);                 \
            (PTR) = NULL;                        \
        }                                        \
    } while(0)

#define LWCA_LOCK_MUTEX(bInLock, mutex)     \
    do {                                    \
        if (!(bInLock))                     \
        {                                   \
            pthread_mutex_lock(mutex);      \
            (bInLock) = TRUE;               \
        }                                   \
    } while (0)

#define LWCA_TRYLOCK_MUTEX(bInLock, mutex, dwError)                 \
    do {                                                            \
        if (!(bInLock))                                             \
        {                                                           \
            int iResult = pthread_mutex_trylock(mutex);             \
            if (iResult == 0)                                       \
            {                                                       \
                (bInLock) = TRUE;                                   \
            }                                                       \
            else                                                    \
            {                                                       \
                (dwError) = LWCA_ERRNO_TO_LWCAERROR(iResult);       \
            }                                                       \
        }                                                           \
    } while (0)

#define LWCA_UNLOCK_MUTEX(bInLock, mutex)   \
    do {                                    \
        if ((bInLock))                      \
        {                                   \
            pthread_mutex_unlock(mutex);    \
            (bInLock) = FALSE;              \
        }                                   \
    } while (0)

#define LWCA_SAFE_JSON_DECREF(PTR)      \
    do {                                \
        if ((PTR)) {                    \
            LwCAJsonCleanupObject(PTR); \
            (PTR) = NULL;               \
        }                               \
    } while(0)

#define LWCA_SAFE_FREE_MUTEX(mutex)      \
    do {                                  \
        if ((mutex)) {                    \
            LwCAFreeMutex(mutex);        \
            (mutex) = NULL;               \
        }                                 \
    } while(0)

#define LWCA_SAFE_FREE_CONDITION(cond)   \
    do {                                  \
        if ((cond)) {                     \
            LwCAFreeCondition(cond);     \
            (cond) = NULL;                \
        }                                 \
    } while(0)

typedef enum
{
    LWCA_LOG_TYPE_CONSOLE = 0,
    LWCA_LOG_TYPE_FILE,
    LWCA_LOG_TYPE_SYSLOG
} LWCA_LOG_TYPE;

#ifndef _LWCA_LOG_LEVEL_DEFINED_
#define _LWCA_LOG_LEVEL_DEFINED_
typedef enum
{
   LWCA_LOG_LEVEL_EMERGENCY = 0,
   LWCA_LOG_LEVEL_ALERT,
   LWCA_LOG_LEVEL_CRITICAL,
   LWCA_LOG_LEVEL_ERROR,
   LWCA_LOG_LEVEL_WARNING,
   LWCA_LOG_LEVEL_NOTICE,
   LWCA_LOG_LEVEL_INFO,
   LWCA_LOG_LEVEL_DEBUG
} LWCA_LOG_LEVEL;
#endif /* _LWCA_LOG_LEVEL_DEFINED_ */

DWORD
LwCAInitLog(
    VOID
    );

VOID
LwCATerminateLogging(
    VOID
    );

VOID
LwCALog(
   LWCA_LOG_LEVEL level,
   const char*      fmt,
   ...
   );

typedef struct _LWCA_LOG_HANDLE* PLWCA_LOG_HANDLE;

extern PLWCA_LOG_HANDLE gpLwCALogHandle;
extern LWCA_LOG_LEVEL   gLwCALogLevel;
extern HANDLE           gpEventLog;
extern LWCA_LOG_TYPE    gLwCALogType;
extern LWCA_LOG_LEVEL LwCALogGetLevel();

#define _LWCA_LOG( Level, Format, ... )             \
    do                                              \
    {                                               \
        LwCALog(                                    \
               (Level),                             \
               (Format),                            \
               ##__VA_ARGS__);                      \
    } while (0)

#define LWCA_LOG_GENERAL( Level, Format, ... ) \
    _LWCA_LOG( (Level), (Format), ##__VA_ARGS__ )

#define LWCA_LOG_ERROR( Format, ... ) \
    LWCA_LOG_GENERAL( LWCA_LOG_LEVEL_ERROR, (Format), ##__VA_ARGS__ )

#define LWCA_LOG_ALERT( Format, ... ) \
    LWCA_LOG_GENERAL( LWCA_LOG_LEVEL_ALERT, (Format), ##__VA_ARGS__ )

#define LWCA_LOG_WARNING( Format, ... ) \
    LWCA_LOG_GENERAL( LWCA_LOG_LEVEL_WARNING, (Format), ##__VA_ARGS__ )

#define LWCA_LOG_INFO( Format, ... ) \
    LWCA_LOG_GENERAL( LWCA_LOG_LEVEL_INFO, (Format), ##__VA_ARGS__ )

#define LWCA_LOG_VERBOSE( Format, ... ) \
    LWCA_LOG_GENERAL( LWCA_LOG_LEVEL_DEBUG, (Format), ##__VA_ARGS__ )

#define LWCA_LOG_DEBUG( Format, ... )       \
    LWCA_LOG_GENERAL(                       \
        LWCA_LOG_LEVEL_DEBUG,               \
        "[file: %s][line: %d] (Format)",    \
        __FILE__, __LINE__, ##__VA_ARGS__)

#define BAIL_ON_LWCA_ERROR(dwError)                                         \
    if (dwError)                                                            \
    {                                                                       \
        LWCA_LOG_WARNING("error code: %#010x", dwError);                    \
        goto error;                                                         \
    }

#define BAIL_WITH_LWCA_ERROR(dwError, ERROR_CODE)                           \
    dwError = ERROR_CODE;                                                   \
    BAIL_ON_LWCA_ERROR(dwError);


#define BAIL_ON_LWCA_ERROR_WITH_MSG(dwError, pszErrMsg)                     \
    if (dwError)                                                            \
    {                                                                       \
        LWCA_LOG_ERROR("[%s:%d] %s. error: %d",                             \
                       __FUNCTION__,                                        \
                       __LINE__,                                            \
                       pszErrMsg,                                           \
                       dwError);                                            \
        BAIL_ON_LWCA_ERROR(dwError);                                        \
    }

#define BAIL_ON_LWCA_POLICY_CFG_ERROR_WITH_MSG(dwError, pszErrMsg)          \
    if (dwError)                                                            \
    {                                                                       \
        dwError = LWCA_POLICY_CONFIG_PARSE_ERROR;                           \
        BAIL_ON_LWCA_ERROR_WITH_MSG(dwError, pszErrMsg);                    \
    }

#define BAIL_ON_LWCA_INVALID_POINTER(p, errCode)                            \
    if (p == NULL) {                                                        \
        errCode = LWCA_ERROR_INVALID_PARAMETER;                             \
        BAIL_ON_LWCA_ERROR(errCode);                                        \
    }

#define BAIL_ON_LWCA_INVALID_STR_PARAMETER(input, dwError)                  \
    if (IsNullOrEmptyString((input)))                                       \
    {                                                                       \
        dwError = LWCA_ERROR_INVALID_PARAMETER;                             \
        BAIL_ON_LWCA_ERROR(dwError);                                        \
    }

#define BAIL_ON_SSL_ERROR(dwError, ERROR_CODE)                              \
    if (dwError == 0)                                                       \
    {                                                                       \
        dwError = ERROR_CODE;                                               \
        LWCA_LOG_WARNING("error code: %#010x", dwError);                    \
        if (ERROR_CODE == LWCA_CERT_IO_FAILURE)                             \
        {                                                                   \
            LWCA_LOG_ERROR(" Failed at %s %d \n",                           \
                            __FUNCTION__,                                   \
                            __LINE__);                                      \
        }                                                                   \
        goto error;                                                         \
    } else {                                                                \
        dwError = 0;                                                        \
    }

#define BAIL_ON_COAPI_ERROR_WITH_MSG(dwError, errMsg)                       \
    if (dwError)                                                            \
    {                                                                       \
        LWCA_LOG_ERROR("[%s:%d] %s. copenapi error (%d)",                   \
                            __FUNCTION__,                                   \
                            __LINE__,                                       \
                            errMsg,                                         \
                            dwError);                                       \
        dwError = LWCA_COAPI_ERROR;                                         \
        goto error;                                                         \
    }

#define BAIL_ON_CREST_ERROR_WITH_MSG(dwError, errMsg)                       \
    if (dwError)                                                            \
    {                                                                       \
        LWCA_LOG_ERROR("[%s:%d] %s. c-rest-engine error (%d)",              \
                            __FUNCTION__,                                   \
                            __LINE__,                                       \
                            errMsg,                                         \
                            dwError);                                       \
        dwError = LWCA_CREST_ENGINE_ERROR;                                  \
        goto error;                                                         \
    }

#define BAIL_ON_JSON_ERROR_WITH_MSG(dwError, errMsg)                        \
    if (dwError)                                                            \
    {                                                                       \
        LWCA_LOG_ERROR("[%s:%d] %s. jansson api error (%d)",                \
                            __FUNCTION__,                                   \
                            __LINE__,                                       \
                            errMsg,                                         \
                            dwError);                                       \
        dwError = LWCA_JSON_ERROR;                                          \
        goto error;                                                         \
    }

#define BAIL_ON_JSON_PARSE_ERROR(dwError)       \
    if ((dwError))                              \
    {                                           \
        (dwError) = LWCA_JSON_PARSE_ERROR;      \
        goto error;                             \
    }

#define BAIL_ON_LWCA_ERROR_NO_LOG(dwError) \
    if ((dwError)) { goto error; }

#define BAIL_ON_LWCA_POLICY_VIOLATION(bIsValid) \
    if ((bIsValid) == FALSE) { goto error; }

#ifndef IsNullOrEmptyString
#define IsNullOrEmptyString(str) (!(str) || !*(str))
#endif /* IsNullOrEmptyString */

#ifndef LWCA_SAFE_STRING
#define LWCA_SAFE_STRING(str) ((str) ? (str) : "")
#endif /* LWCA_SAFE_STRING */

#ifndef LWCA_RESPONSE_TIME
#define LWCA_RESPONSE_TIME(val) ((val) ? (val) : 1)
#endif /* LWCA_RESPONSE_TIME */

// Event logs related constants.

#define LWCA_EVENT_SOURCE   "Lightwave MutentCA Service"

#define LWCA_TIME_SECS_PER_MINUTE           ( 60)
#define LWCA_TIME_SECS_PER_HOUR             ( 60 * LWCA_TIME_SECS_PER_MINUTE)
#define LWCA_TIME_SECS_PER_DAY              ( 24 * LWCA_TIME_SECS_PER_HOUR)
#define LWCA_TIME_SECS_PER_WEEK             (  7 * LWCA_TIME_SECS_PER_DAY)
#define LWCA_TIME_SECS_PER_YEAR             (366 * LWCA_TIME_SECS_PER_DAY)

// MutentCA OIDC constants

#define LWCA_OIDC_VMDIR_SCOPE               "openid id_groups at_groups rs_vmdir"

// MutentCA VMDir LDAP constants

#define LWCA_LDAP_SCOPE                     "scope"
#define LWCA_LDAP_SCOPE_SUB                 "sub"
#define LWCA_LDAP_ATTRS                     "attrs"
#define LWCA_LDAP_ATTR_UPN                  "userPrincipalName"
#define LWCA_LDAP_ATTR_DN                   "dn"
#define LWCA_LDAP_FILTER                    "filter"
#define LWCA_LDAP_FILTER_UPN_FMT            "("LWCA_LDAP_ATTR_UPN"=%s)"
#define LWCA_VMDIR_RESP_RESULT              "result"
#define LWCA_VMDIR_REST_LDAP_PORT           7478
#define LWCA_VMDIR_REST_LDAP_URI_PREFIX     "/v1/vmdir/ldap"

// HTTP Status Codes

#define LWCA_HTTP_RESP_OK                    200
#define LWCA_HTTP_RESP_NOT_FOUND             404

// misc
#define MSECS_PER_SEC   1000
#define NSECS_PER_MSEC  1000000

typedef struct _LWCA_CERT_VALIDITY
{
    time_t tmNotBefore;
    time_t tmNotAfter;
} LWCA_CERT_VALIDITY, *PLWCA_CERT_VALIDITY;

typedef PSTR PLWCA_CERT_REQUEST;

SIZE_T
LwCAStringLenA(
    PCSTR pszStr
    );

DWORD
LwCAAllocateMemory(
    DWORD dwSize,
    PVOID *ppMemory
    );

DWORD
LwCAReallocateMemory(
    PVOID        pMemory,
    PVOID*       ppNewMemory,
    DWORD        dwSize
    );

DWORD
LwCAReallocateMemoryWithInit(
    PVOID         pMemory,
    PVOID*        ppNewMemory,
    size_t        dwNewSize,
    size_t        dwOldSize
    );

DWORD
LwCACopyMemory(
    PVOID       pDst,
    size_t      dstSize,
    const void* pSrc,
    size_t      cpySize
    );

DWORD
LwCAAllocateAndCopyMemory(
    PVOID   pBlob,
    size_t  iBlobSize,
    PVOID*  ppOutBlob
    );

VOID
LwCAFreeMemory(
    PVOID pMemory
    );

DWORD
LwCAAllocateStringA(
    PCSTR pszString,
    PSTR * ppszString
    );

DWORD
LwCAAllocateStringWithLengthA(
    PCSTR pszString,
    DWORD dwSize,
    PSTR * ppszString
    );

DWORD
LwCAAllocateStringPrintfA(
    PSTR* ppszString,
    PCSTR pszFormat,
    ...
    );

VOID
LwCAFreeStringA(
    PSTR pszString
    );

VOID
LwCAFreeStringW(
    PWSTR pszString
    );

DWORD
LwCACreateStringArray(
    PSTR                *ppszSrc,
    DWORD               dwSrcLen,
    PLWCA_STRING_ARRAY* ppStrOutputArray
    );

DWORD
LwCACopyStringArrayA(
    PSTR            **pppszDst,
    DWORD           dwDstLen,
    PSTR            *ppszSrc,
    DWORD           dwSrcLen
    );

DWORD
LwCACopyStringArray(
    PLWCA_STRING_ARRAY  pStrInputArray,
    PLWCA_STRING_ARRAY* ppStrOutputArray
    );

VOID
LwCAFreeStringA(
    PSTR pszString
    );

VOID
LwCAFreeStringArrayA(
    PSTR* ppszStrings,
    DWORD dwCount
    );

VOID
LwCAFreeStringArray(
    PLWCA_STRING_ARRAY pStrArray
    );

DWORD
LwCARequestContextCreate(
    PLWCA_REQ_CONTEXT       *ppReqCtx
    );

VOID
LwCARequestContextFree(
    PLWCA_REQ_CONTEXT       pReqCtx
    );

DWORD
LwCAGetStringLengthW(
    PCWSTR  pwszStr,
    PSIZE_T pLength
    );

ULONG
LwCAAllocateStringW(
    PCWSTR pwszSrc,
    PWSTR* ppwszDst
    );

ULONG
LwCAAllocateStringWFromA(
    PCSTR pszSrc,
    PWSTR* ppwszDst
    );

ULONG
LwCAAllocateStringAFromW(
    PCWSTR pwszSrc,
    PSTR*  ppszDst
    );

BOOL
LwCAStringIsEqualW (
    PCWSTR pwszStr1,
    PCWSTR pwszStr2,
    BOOLEAN bIsCaseSensitive
    );

BOOLEAN
LwCAIsValidSecret(
    PWSTR pszTheirs,
    PWSTR pszOurs
    );

int
LwCAStringCompareW(
    PCWSTR pwszStr1,
    PCWSTR pwszStr2,
    BOOLEAN bIsCaseSensitive
    );

int
LwCAStringNCompareA(
    PCSTR pszStr1,
    PCSTR pszStr2,
    size_t n,
    BOOLEAN bIsCaseSensitive
    );

DWORD
LwCAStringNCpyA(
    PSTR strDestination,
    size_t numberOfElements,
    PCSTR strSource,
    size_t count
    );

PSTR
LwCAStringChrA(
    PCSTR str,
    int c
    );

PSTR
LwCAStringTokA(
    PSTR strToken,
    PCSTR strDelimit,
    PSTR* context
    );

DWORD
LwCAStringCountSubstring(
    PCSTR pszHaystack,
    PCSTR pszNeedle,
    int *pnCount
    );

DWORD
LwCAStringCatA(
    PSTR strDestination,
    size_t numberOfElements,
    PCSTR strSource
    );

VOID
LwCAStringTrimSpace(
    PSTR    pszStr
    );

DWORD
LwCAGetUTCTimeString(
    PSTR *pszTimeString
    );

int
LwCAStringCompareA(
    PCSTR pszStr1,
    PCSTR pszStr2,
    BOOLEAN bIsCaseSensitive
    );

int
LwCAStringCompareW(
    PCWSTR pszStr1,
    PCWSTR pszStr2,
    BOOLEAN bIsCaseSensitive
    );

DWORD
LwCAStringToLower(
    PSTR pszString,
    PSTR *ppszNewString
    );

int
LwCAStringToInt(
    PCSTR pszStr
    );

VOID
LwCASetBit(
    unsigned long *flag,
    int bit
    );

LWCA_LOG_LEVEL
LwCALogGetLevel(
    );

VOID
LwCALogSetLevel(
    LWCA_LOG_LEVEL level
    );

DWORD
LwCAOpenFilePath(
    PCSTR szFileName,
    PCSTR szOpenMode,
    FILE** fp
    );

DWORD
LwCACopyFile(
    PCSTR pszSrc,
    PCSTR pszDest
    );

DWORD
LwCAReadFileToString(
    PCSTR   pcszFilePath,
    PSTR    *ppszData
    );

BOOLEAN
LwCAStringStartsWith(
    PCSTR   pszStr,
    PCSTR   pszPrefix,
    BOOLEAN bIsCaseSensitive
    );

// misc.c

typedef VOID*       LWCA_LIB_HANDLE;

DWORD
LwCALoadLibrary(
    PCSTR           pszLibPath,
    LWCA_LIB_HANDLE* ppLibHandle
    );

VOID
LwCACloseLibrary(
    LWCA_LIB_HANDLE  pLibHandle
    );

VOID*
LwCAGetLibSym(
    LWCA_LIB_HANDLE  pLibHandle,
    PCSTR           pszFunctionName
    );

DWORD
LwCABytesToHexString(
    PUCHAR  pData,
    DWORD   length,
    PSTR*   pszHexString,
    BOOLEAN bLowerCase
    );

DWORD
LwCAHexStringToBytes(
    PSTR    pszHexStr,
    PUCHAR* ppData,
    size_t* pLength
    );

DWORD
LwCAGetInstallDirectory(
    PSTR *ppszInstallDir
    );

DWORD
LwCAGetDataDirectory(
    PSTR *ppszDataDir
    );

DWORD
LwCAGetLogDirectory(
    PSTR *ppszLogDir
    );

DWORD
LwCACreateKey(
    PBYTE       pData,
    DWORD       dwLength,
    PLWCA_KEY   *ppKey
    );

DWORD
LwCACopyKey(
    PLWCA_KEY pKey,
    PLWCA_KEY *ppKey
    );

VOID
LwCAFreeKey(
    PLWCA_KEY pKey
    );

DWORD
LwCADbCreateCAData(
    PCSTR                       pcszSubjectName,
    PCSTR                       pcszParentCAId,
    PCSTR                       pcszActiveCertSKI,
    PCSTR                       pcszAuthBlob,
    LWCA_CA_STATUS              status,
    PLWCA_DB_CA_DATA            *ppCAData
    );

DWORD
LwCADbCopyCAData(
    PLWCA_DB_CA_DATA        pCADataSrc,
    PLWCA_DB_CA_DATA        *ppCADataDst
    );

VOID
LwCADbFreeCAData(
    PLWCA_DB_CA_DATA        pCAData
    );

DWORD
LwCADbCreateRootCertData(
    PCSTR                       pcszCAId,
    PLWCA_DB_CERT_DATA          pCertData,
    PLWCA_CERTIFICATE           pRootCertPEM,
    PLWCA_KEY                   pEncryptedPrivateKey,
    PCSTR                       pcszChainOfTrust,
    PCSTR                       pcszCRLNumber,
    PCSTR                       pcszLastCRLUpdate,
    PCSTR                       pcszNextCRLUpdate,
    PLWCA_DB_ROOT_CERT_DATA     *ppRootCertData
    );

DWORD
LwCADbCopyRootCertData(
    PLWCA_DB_ROOT_CERT_DATA     pRootCertDataSrc,
    PLWCA_DB_ROOT_CERT_DATA     *ppRootCertDataDst
    );

DWORD
LwCADbCopyRootCertDataArray(
    PLWCA_DB_ROOT_CERT_DATA_ARRAY   pRootCertDataArraySrc,
    PLWCA_DB_ROOT_CERT_DATA_ARRAY   *ppRootCertDataArrayDst
    );

VOID
LwCADbFreeRootCertData(
    PLWCA_DB_ROOT_CERT_DATA     pRootCertData
    );

VOID
LwCADbFreeRootCertDataArray(
    PLWCA_DB_ROOT_CERT_DATA_ARRAY   pRootCertDataArray
    );

DWORD
LwCARootCertArrayToCertArray(
    PLWCA_DB_ROOT_CERT_DATA_ARRAY   pRootCertDataArray,
    PLWCA_CERTIFICATE_ARRAY         *ppCertArray
    );

DWORD
LwCADbCreateCertData(
    PCSTR                   pcszIssuer,
    PCSTR                   pcszSerialNumber,
    PCSTR                   pcszIssuerSerialNumber,
    PCSTR                   pcszSKI,
    PCSTR                   pcszAKI,
    PCSTR                   pcszRevokedDate,
    PCSTR                   pcszTimeValidFrom,
    PCSTR                   pcszTimeValidTo,
    DWORD                   dwRevokedReason,
    LWCA_CERT_STATUS        status,
    PLWCA_DB_CERT_DATA      *ppCertData
    );

DWORD
LwCADbCopyCertData(
    PLWCA_DB_CERT_DATA pCertData,
    PLWCA_DB_CERT_DATA *ppCertData
    );

DWORD
LwCADbCopyCertDataArray(
    PLWCA_DB_CERT_DATA_ARRAY pCertDataArray,
    PLWCA_DB_CERT_DATA_ARRAY *ppCertDataArray
    );

VOID
LwCADbFreeCertData(
    PLWCA_DB_CERT_DATA pCertData
    );

VOID
LwCADbFreeCertDataArray(
    PLWCA_DB_CERT_DATA_ARRAY pCertDataArray
    );

DWORD
LwCACreateCertArray(
    PSTR                     *ppszCertificates,
    DWORD                    dwCount,
    PLWCA_CERTIFICATE_ARRAY  *ppCertArray
    );

DWORD
LwCACreateStringArrayFromCertArray(
    PLWCA_CERTIFICATE_ARRAY     pCertArray,
    PLWCA_STRING_ARRAY          *ppStrArray
    );

DWORD
LwCACopyCertArray(
    PLWCA_CERTIFICATE_ARRAY     pCertArray,
    PLWCA_CERTIFICATE_ARRAY     *ppCertArray
    );

VOID
LwCAFreeCertificates(
    PLWCA_CERTIFICATE_ARRAY pCertArray
    );

DWORD
LwCACreateCertificate(
    PCSTR               pcszCertificate,
    PLWCA_CERTIFICATE   *ppCertificate
    );

VOID
LwCAFreeCertificate(
    PLWCA_CERTIFICATE pCertificate
    );

DWORD
LwCAUuidGenerate(
    PSTR    *ppszUuid
    );

uint64_t
LwCAGetTimeInMilliSec(
    VOID
    );

DWORD
LwCAGetCanonicalHostName(
    PCSTR pszHostname,
    PSTR* ppszCanonicalHostname
    );

// regexutil.c

typedef regex_t REGEX, *PREGEX;

DWORD
LwCARegexInit(
    PCSTR       pcszPattern,
    PREGEX      *ppRegex
    );

DWORD
LwCARegexValidate(
    PCSTR       pcszValue,
    PREGEX      pRegex,
    PBOOLEAN    pbIsValid
    );

VOID
LwCARegexFree(
    PREGEX      pRegex
    );

/////////////////////////////Actual LwCA Common Functions///////////////////

DWORD
LwCACommonInit(
    VOID
    );

DWORD
LwCACommonShutdown(
    VOID
    );

DWORD
LwCAOpenSSLInitialize(
    VOID
    );

DWORD
LwCAOpenSSLCleanup(
    VOID
    );

int
LwCAisBitSet(
    unsigned long flag,
    int bit
    );

void
LwCAClearBit(
    unsigned long flag,
    int bit
    );

void
LwCAToggleBit(
    unsigned long flag,
    int bit
    );

DWORD
LwCAGetLogDirectory(
    PSTR *ppszLogDir
    );

// thread.c

typedef pthread_t LWCA_THREAD;

typedef LWCA_THREAD* PLWCA_THREAD;

typedef DWORD (LwCAStartRoutine)(PVOID);
typedef LwCAStartRoutine* PLWCA_START_ROUTINE;

typedef struct _LWCA_MUTEX
{
    BOOLEAN                 bInitialized;
    pthread_mutex_t         critSect;
} LWCA_MUTEX, *PLWCA_MUTEX;

typedef struct _LWCA_COND
{
    BOOLEAN                 bInitialized;
    pthread_cond_t          cond;
} LWCA_COND, *PLWCA_COND;

typedef struct _LWCA_RWLOCK
{
    pthread_rwlock_t    rwLock;
} LWCA_RWLOCK, *PLWCA_RWLOCK;

typedef struct _LWCA_THREAD_START_INFO
{
    LwCAStartRoutine*      pStartRoutine;
    PVOID                   pArgs;
} LWCA_THREAD_START_INFO, *PLWCA_THREAD_START_INFO;

DWORD
LwCAAllocateMutex(
    PLWCA_MUTEX* ppMutex
    );

VOID
LwCAFreeMutex(
    PLWCA_MUTEX pMutex
    );

DWORD
LwCALockMutex(
    PLWCA_MUTEX pMutex
    );

DWORD
LwCAUnlockMutex(
    PLWCA_MUTEX pMutex
    );

BOOLEAN
LwCAIsMutexInitialized(
    PLWCA_MUTEX pMutex
    );

DWORD
LwCAAllocateCondition(
    PLWCA_COND* ppCondition
    );

VOID
LwCAFreeCondition(
    PLWCA_COND pCondition
    );

DWORD
LwCAConditionWait(
    PLWCA_COND pCondition,
    PLWCA_MUTEX pMutex
    );

DWORD
LwCAConditionTimedWait(
    PLWCA_COND pCondition,
    PLWCA_MUTEX pMutex,
    DWORD dwMilliseconds
    );

DWORD
LwCAConditionSignal(
    PLWCA_COND pCondition
    );

DWORD
LwCACreateThread(
    PLWCA_THREAD pThread,
    BOOLEAN bDetached,
    PLWCA_START_ROUTINE pStartRoutine,
    PVOID pArgs
    );

DWORD
LwCAInitializeMutexContent(
    PLWCA_MUTEX            pMutex
    );

VOID
LwCAFreeMutexContent(
    PLWCA_MUTEX            pMutex
    );

DWORD
LwCAInitializeConditionContent(
    PLWCA_COND             pCondition
    );

VOID
LwCAFreeConditionContent(
    PLWCA_COND             pCondition
    );

DWORD
LwCAThreadJoin(
    PLWCA_THREAD pThread,
    PDWORD pRetVal
    );

VOID
LwCAFreeThread(
    PLWCA_THREAD pThread
    );

// vmafd.c

DWORD
LwCAGetVecsMachineCert(
    PSTR*   ppszCert,
    PSTR*   ppszKey
    );

DWORD
LwCAGetVecsMutentCACert(
    PSTR*   ppszCert,
    PSTR*   ppszKey
    );

DWORD
LwCAOpenVmAfdClientLib(
    LWCA_LIB_HANDLE*   pplibHandle
    );

DWORD
LwCAGetDCName(
    PSTR        *ppszDCName
    );

DWORD
LwCAGetDomainName(
    PSTR        *ppszDomainName
    );

// ldap.c

DWORD
LwCAIsValidDN(
    PCSTR       pcszDN,
    PBOOLEAN    pbIsValid
    );

DWORD
LwCADNToRDNArray(
    PCSTR               pcszDN,
    BOOLEAN             bNotypes,
    PLWCA_STRING_ARRAY* ppRDNStrArray
    );

DWORD
LwCAUPNToDN(
    PCSTR                   pcszUPN,
    PSTR                    *ppszDN
    );

DWORD
LwCADNSNameToDCDN(
    PCSTR       pcszDNSName,
    PSTR        *ppszDN
    );

// jsonutils.c

typedef struct json_t   _LWCA_JSON_OBJECT;
typedef _LWCA_JSON_OBJECT   *PLWCA_JSON_OBJECT;

DWORD
LwCAJsonLoadObjectFromFile(
    PCSTR                   pcszFilePath,
    PLWCA_JSON_OBJECT       *ppJsonConfig
    );

DWORD
LwCAJsonLoadObjectFromString(
    PCSTR               pcszJsonStr,
    PLWCA_JSON_OBJECT   *ppJsonConfig
    );

DWORD
LwCAJsonGetObjectFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    PLWCA_JSON_OBJECT       *ppJsonValue
    );

DWORD
LwCAJsonGetConstStringFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    PCSTR                   *ppcszValue
    );

DWORD
LwCAJsonGetStringFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    PSTR                    *ppszValue
    );

DWORD
LwCAJsonGetStringArrayFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    PLWCA_STRING_ARRAY      *ppStrArrValue
    );

DWORD
LwCAJsonGetTimeFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    time_t                  *ptValue
    );

DWORD
LwCAJsonGetIntegerFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    int                     *piValue
    );

DWORD
LwCAJsonGetUnsignedIntegerFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    DWORD                   *pdwValue
    );

DWORD
LwCAJsonGetBooleanFromKey(
    PLWCA_JSON_OBJECT       pJson,
    BOOLEAN                 bOptional,
    PCSTR                   pcszKey,
    BOOLEAN                 *pbValue
    );

BOOLEAN
LwCAJsonIsArray(
    PLWCA_JSON_OBJECT       pJson
    );

SIZE_T
LwCAJsonArraySize(
    PLWCA_JSON_OBJECT       pJson
    );

DWORD
LwCAJsonArrayGetBorrowedRef(
    PLWCA_JSON_OBJECT       pJsonIn,
    SIZE_T                  idx,
    PLWCA_JSON_OBJECT       *ppJsonOut // Borrowed reference, do not free
    );

DWORD
LwCAJsonObjectCreate(
    PLWCA_JSON_OBJECT  *ppJson
    );

DWORD
LwCAJsonArrayCreate(
    PLWCA_JSON_OBJECT  *ppJson
    );

DWORD
LwCAJsonArrayStringCopy(
    PLWCA_JSON_OBJECT   pSrc,
    PLWCA_JSON_OBJECT   *ppDest
    );

DWORD
LwCAJsonSetStringToObject(
    PLWCA_JSON_OBJECT   pObj,
    PCSTR               pcszKey,
    PCSTR               pcszValue
    );

DWORD
LwCAJsonSetJsonToObject(
    PLWCA_JSON_OBJECT       pObj,
    PCSTR                   pcszKey,
    PLWCA_JSON_OBJECT       pJson
    );

DWORD
LwCAJsonAppendStringToArray(
    PLWCA_JSON_OBJECT   pArray,
    PCSTR               pcszValue
    );

DWORD
LwCAJsonAppendJsonToArray(
    PLWCA_JSON_OBJECT       pArray,
    PLWCA_JSON_OBJECT       pJson
    );

VOID
LwCAJsonCleanupObject(
    PLWCA_JSON_OBJECT       pJson
    );

DWORD
LwCAJsonDumps(
    PLWCA_JSON_OBJECT   pJson,
    size_t              flags,
    PSTR                *ppszDest
    );

DWORD
LwCAJsonArrayGetStringAtIndex(
    PLWCA_JSON_OBJECT   pArray,
    int                 index,
    PSTR                *ppszValue
    );

// token_util.c

DWORD
LwCAGetAccessToken(
    PCSTR   pcszServer,
    PCSTR   pcszDomain,
    PCSTR   pcszOidcScope,
    PSTR    *ppszToken
    );

#ifdef __cplusplus
}
#endif

#endif /* __LWCA_COMMON_H__ */
