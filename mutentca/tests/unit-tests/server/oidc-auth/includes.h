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
#ifndef _TEST_LWCA_SERVICE_OIDC_AUTH_INCLUDES_H_
#define _TEST_LWCA_SERVICE_OIDC_AUTH_INCLUDES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <config.h>
#include <mutentcasys.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#include <lwrpcrt/lwrpcrt.h>

#include <cmocka.h>

#include <mutentca.h>
#include <mutentcadb.h>
#include <mutentcaerror.h>
#include <mutentcacommon.h>
#include <mutentcasrvcommon.h>
#include <mutentcaoidc.h>

#include <vmhttpclient.h>

#include <ssotypes.h>
#include <ssocommon.h>
#include <ssoerrors.h>
#include <oidc_types.h>
#include <oidc.h>

#include "defines.h"
#include "prototypes.h"

#ifdef __cplusplus
}
#endif

#endif /* _TEST_LWCA_SERVICE_OIDC_AUTH_INCLUDES_H_ */
