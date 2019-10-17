/*
 * Copyright © 2017 VMware, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <lw/types.h>
#include <lw/hash.h>
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include <lw/ntstatus.h>
#include <lw/rtlstring.h>
#include <lw/atomic.h>
//#include <reg/lwreg.h>

#include <curl/curl.h>
#include <jansson.h>
//#include <yaml.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "defines.h"
#include "errorcode.h"
#include "structs.h"
#include "vmcommonincludes.h"
#include "vmsignature.h"
