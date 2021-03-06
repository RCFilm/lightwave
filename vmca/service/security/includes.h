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

#ifndef _VMCA_SERVICE_SECURITY_INCLUDES_H_
#define _VMCA_SERVICE_SECURITY_INCLUDES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <config.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#include <openssl/x509v3.h>
#endif

#include <lwca_security.h>

#include <vmmemory.h>

#include <lw/types.h>
#include <lw/base.h>
#include <lw/hash.h>
#include <lwerror.h>

#include <dce/rpc.h>
#include <dce/dcethread.h>

#include <vmcatypes.h>
#include <vmca_error.h>
#include <vmcacommon.h>
#include <vmcaplugin.h>
#include <vmcasrvcommon.h>

#include "structs.h"
#include "externs.h"
#include "defines.h"
#include "prototypes.h"

#ifdef __cplusplus
}
#endif

#endif /* _VMCA_SERVICE_SECURITY_INCLUDES_H_ */
