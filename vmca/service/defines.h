/*
 * Copyright © 2012-2018 VMware, Inc.  All Rights Reserved.
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
 * Module Name: VMCAService
 *
 * Filename: defines.h
 *
 * Abstract:
 *
 * Definitions/Macros
 *
 */


#define VMCA_OPTION_LOGGING_LEVEL 'l'
#define VMCA_OPTION_LOG_FILE_NAME 'L'
#define VMCA_OPTION_ENABLE_SYSLOG 's'
#define VMCA_OPTION_ENABLE_DAEMON 'd'
#define VMCA_OPTION_CONSOLE_LOGGING 'c'
#define VMCA_OPTIONS_VALID "l:L:p:scd"

//
// These values are hard-coded in the VMCA.reg file also,
// in case you change them, please change them in VMCA.reg
// file also.
//
#define VMCA_SERVER_VERSION_STRING "VMware Certificate Server  - Version  2.0.0 \nCopyright VMware Inc. All rights reserved."
#ifndef _WIN32
#define VMCA_PATH_SEPARATOR_CHAR '/'
#define VMCA_PATH_SEPARATOR_STR "/"
// #define VMCA_ROOT_CERT "RootCert"
// #define VMCA_ROOT_PRIVATE_KEY "RootPrivateKey"
// #define VMCA_ROOT_PRIVATE_KEY_PASS_PHRASE "RootPrivateKeyPassPhrase"
// #define VMCA_ROOT_CERT_DIR "/var/lib/vmware/vmca"

#else
#define VMCA_PATH_SEPARATOR_CHAR '\\'
#define VMCA_PATH_SEPARATOR_STR "\\"
// #define VMCA_ROOT_CERT "RootCert"
// #define VMCA_ROOT_PRIVATE_KEY "RootPrivateKey"
// #define VMCA_ROOT_PRIVATE_KEY_PASS_PHRASE "RootPrivateKeyPassPhrase"
// #define VMCA_ROOT_CERT_DIR "C:\\ProgramData\\VMware\\cis\\data\\vmcad"

#define _USE_32BIT_TIME_T // See MSDN http://msdn.microsoft.com/en-us/library/14h5k7ff(v=VS.80).aspx

#define BOOL BOOLEAN

#define VMCA_IF_HANDLE_T RPC_IF_HANDLE
#define VMCA_RPC_BINDING_VECTOR_P_T RPC_BINDING_VECTOR*
#define VMCA_RPC_AUTHZ_HANDLE RPC_AUTHZ_HANDLE
#define VMCA_RPC_BINDING_HANDLE RPC_BINDING_HANDLE
#define VMCA_RPC_C_AUTHN_LEVEL_PKT RPC_C_AUTHN_LEVEL_PKT

// should this service name match VMCA_NCALRPC_END_POINT ??
//#define VMCA_NT_SERVICE_NAME L"VMWareCertificateService"
#define VMCA_NT_SERVICE_NAME L"vmcasvc"


#define VMCA_CLOSE_HANDLE(handle) \
    {                              \
        if ((handle) != NULL)      \
        {                          \
            CloseHandle((handle)); \
            (handle) = NULL;       \
        }                          \
    }

#define VMCA_CLOSE_SERVICE_HANDLE(hServiceHandle) \
    {                                              \
         if ( (hServiceHandle) != NULL )           \
         {                                         \
             CloseServiceHandle((hServiceHandle)); \
             (hServiceHandle) = NULL;              \
         }                                         \
    }

#endif

#define    GetLastError() errno

#ifndef  __RPC_USER
#define  __RPC_USER
#endif

#define VMCA_ASSERT(x) assert( (x) )

#define _PTHREAD_FUNCTION_RTN_ASSERT(Function, ...)       \
    do {                                                  \
        int error = Function(__VA_ARGS__);                \
        VMCA_ASSERT(!error);                              \
    } while (0)

#define INITIALIZE_SRW_LOCK(pRWLock, pRWLockAttr) \
    _PTHREAD_FUNCTION_RTN_ASSERT(pthread_rwlock_init, pRWLock, pRWLockAttr)

#define ENTER_READERS_SRW_LOCK(bHasLock, pRWLock)                 \
    do {                                                          \
    assert (!bHasLock);                                           \
    _PTHREAD_FUNCTION_RTN_ASSERT(pthread_rwlock_rdlock, pRWLock); \
    bHasLock = true;                                              \
    } while(0)

#define LEAVE_READERS_SRW_LOCK(bHasLock, pRWLock)                 \
    do {                                                          \
    if (bHasLock) {                                               \
    _PTHREAD_FUNCTION_RTN_ASSERT(pthread_rwlock_unlock, pRWLock); \
    bHasLock = false;                                             \
    }                                                             \
    } while(0)

#define ENTER_WRITER_SRW_LOCK(bHasLock, pRWLock)                  \
    do {                                                          \
    assert (!bHasLock);                                           \
    _PTHREAD_FUNCTION_RTN_ASSERT(pthread_rwlock_wrlock, pRWLock); \
    bHasLock = true;                                              \
    } while(0)

#define LEAVE_WRITER_SRW_LOCK(bHasLock, pSRWLock)  LEAVE_READERS_SRW_LOCK(bHasLock, pSRWLock)


#define SQL_BUFFER_SIZE          1024


#define VMCA_RPC_SAFE_FREE_MEMORY(mem) \
    if ((mem) != NULL) \
    { \
        VMCARpcFreeMemory(mem); \
    }


#define VMCA_SAFE_FREE_HZN_PSTR(PTR)    \
    do {                          \
        if ((PTR)) {              \
            HZNFree(PTR);         \
            (PTR) = NULL;         \
        }                         \
    } while(0)

#define BAIL_ON_VMCA_INVALID_POINTER(p, errCode)  \
        if (p == NULL) {                          \
            errCode = ERROR_INVALID_PARAMETER;    \
            BAIL_ON_VMCA_ERROR(errCode);          \
        }

#define BAIL_ON_VMREST_ERROR(dwError)           \
    if (dwError)                                \
    {                                           \
        goto error;                             \
    }

#define PARSER_CHECK_NULL(input, dwError)       \
    if (input == NULL)                          \
    {                                           \
        dwError = 415;                          \
    }

#define CHECK_BAD_MALLOC(input, dwError)        \
    if (input == NULL)                          \
    {                                           \
        dwError = ERROR_OUTOFMEMORY;            \
    }

#define HANDLE_NULL_PARAM(input, dwError)       \
    if (input == NULL)                          \
    {                                           \
        dwError = ERROR_INVALID_PARAMETER;      \
    }

// C REST ENGINE CONFIG VALUES
#define VMCA_REST_SSL_CERT "/root/mycert.pem"
#define VMCA_REST_SSL_KEY "/root/mycert.pem"
#define VMCA_HTTP_PORT_STR "7777"
#define VMCA_HTTPS_PORT_STR "7778"
#define VMCA_HTTP_PORT_NUM  7777
#define VMCA_HTTPS_PORT_NUM 7778
#define VMCA_REST_CLIENT_CNT 64
#define VMCA_REST_WORKER_TH_CNT 64

#define VMCA_REST_CONN_TIMEOUT_SEC 30
#define VMCA_MAX_DATA_PER_CONN_MB  25
#define VMCA_DAEMON_NAME     "vmcad";
#define VMCA_REST_STOP_TIMEOUT_SEC 10

#define VMCARESTMAXPAYLOADLENGTH 4096

//Rest port config
#define VMCA_HTTP_PORT_REG_KEY "RestListenHTTPPort"
#define VMCA_HTTPS_PORT_REG_KEY "RestListenHTTPSPort"


//VMCA HTTP ENDPOINT URI VALUES
#define VMCA_CRL_URI "vmca/crl"
#define VMCA_ROOT_URI "vmca/root"
#define VMCA_CERTS_URI "vmca/certificates"
#define VMCA_URI "vmca"

// VMCA REST PARAMETER KEYS
#define VMCA_ADD_ROOT_PARAM_KEY_CERT "cert"
#define VMCA_ADD_ROOT_PARAM_KEY_PRIVKEY "privateKey"
#define VMCA_ADD_ROOT_PARAM_KEY_OVERWRITE "overwrite"
#define VMCA_ENUM_CERTS_PARAM_KEY_FLAG "flag"
#define VMCA_ENUM_CERTS_PARAM_KEY_NUMBER "number"
#define VMCA_GET_SIGNED_CERT_PARAM_KEY_CSR "csr"
#define VMCA_GET_SIGNED_CERT_PARAM_KEY_NOT_BF "notBefore"
#define VMCA_GET_SIGNED_CERT_PARAM_KEY_DURATION "duration"
#define VMCA_REVOKE_CERT_PARAM_KEY_CERT "cert"

// VMCA REST RETURN KEYS
#define VMCA_ENUM_CERTS_RETURN_KEY "cert"

//REST AUTH
#define VMCA_DEFAULT_CLOCK_TOLERANCE 60.0
#define VMCA_DEFAULT_SCOPE_STRING "rs_vmca"
#define VMCA_GROUP_PERMISSION_STRING "CAAdmins"
#define VMCA_BASIC_AUTH_STRING "Basic "
#define VMCA_SUCCESS_MESSAGE "{Success: \"success\"}"

#ifdef _WIN32
#define VMCASleep(X) Sleep((X) * 1000)
#else
#define VMCASleep(X) sleep((X))
#endif


#define FILE_CHUNK (64 * 1024)
#define VMCA_TIME_LAG_OFFSET_CRL (10*60)
#define VMCA_TIME_LAG_OFFSET_CERTIFICATE (5*60)
#define VMCA_MIN_CA_CERT_PRIV_KEY_LENGTH (2048)

#define VMCA_TIME_SECS_PER_MINUTE           ( 60)
#define VMCA_TIME_SECS_PER_HOUR             ( 60 * VMCA_TIME_SECS_PER_MINUTE)
#define VMCA_TIME_SECS_PER_DAY              ( 24 * VMCA_TIME_SECS_PER_HOUR)
#define VMCA_TIME_SECS_PER_WEEK             (  7 * VMCA_TIME_SECS_PER_DAY)
#define VMCA_TIME_SECS_PER_YEAR             (366 * VMCA_TIME_SECS_PER_DAY)

#define VMCA_VALIDITY_SYNC_BACK_DATE        (VMCA_TIME_SECS_PER_WEEK * 2)
#define VMCA_MAX_CERT_DURATION              (VMCA_TIME_SECS_PER_YEAR * 10)
