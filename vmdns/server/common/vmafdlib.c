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

#ifdef _WIN32

#define VMAFD_VECS_CLIENT_LIBRARY   "\\libvmafdclient.dll"
#define VMAFD_KEY_ROOT              VMAFD_CONFIG_SOFTWARE_KEY_PATH
#define VMAFD_LIB_KEY               VMDNS_REG_KEY_INSTALL_PATH

#elif LIGHTWAVE_BUILD

#define VMAFD_VECS_CLIENT_LIBRARY   "/libvmafdclient.so"
#define VMAFD_KEY_ROOT              VMAFD_CONFIG_KEY_ROOT
#define VMAFD_LIB_KEY               VMAFD_REG_KEY_PATH

#else

#define VMAFD_VECS_CLIENT_LIBRARY   "/lib64/libvmafdclient.so"
#define VMAFD_KEY_ROOT              VMAFD_CONFIG_KEY_ROOT
#define VMAFD_LIB_KEY               VMAFD_REG_KEY_PATH

#endif

DWORD
VmDnsOpenVmAfdClientLib(
    VMDNS_LIB_HANDLE*   pplibHandle
    )
{
    DWORD               dwError = 0;
    CHAR                pszRegLibPath[VMDNS_PATH_MAX] = {0};
    PSTR                pszVmafdLibPath = NULL;
    VMDNS_LIB_HANDLE    plibHandle = NULL;

#ifdef _WIN32

    dwError = VmDnsStringCpyA(
            pszRegLibPath,
            VMDNS_PATH_MAX,
            WIN_SYSTEM32_PATH);
    BAIL_ON_VMDNS_ERROR(dwError);

#elif LIGHTWAVE_BUILD

    dwError = VmDnsStringCpyA(
            pszRegLibPath,
            VMDNS_PATH_MAX,
            VMDNS_LIB_DIR);
    BAIL_ON_VMDNS_ERROR(dwError);

#else

    PSTR pszVmafdName = NULL;

    dwError = VmDnsGetRegKeyValue(
            VMAFD_KEY_ROOT,
            VMAFD_LIB_KEY,
            pszRegLibPath,
            sizeof(pszRegLibPath) - 1);
    BAIL_ON_VMDNS_ERROR(dwError);

    // find the first vmafd in path key "/usr/lib/vmware-vmafd/...."
    pszVmafdName = strstr(pszRegLibPath, VMAFD_NAME);

    dwError = pszVmafdName ? 0 : ERROR_NO_SUCH_FILE_OR_DIRECTORY;
    BAIL_ON_VMDNS_ERROR(dwError);

    pszVmafdName[strlen(VMAFD_NAME)] = '\0';

#endif

    // construct full path to libvmafdclient
    dwError = VmDnsAllocateStringPrintfA(
            &pszVmafdLibPath,
            "%s%s",
            pszRegLibPath,
            VMAFD_VECS_CLIENT_LIBRARY);
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmDnsLoadLibrary(pszVmafdLibPath, &plibHandle);
    BAIL_ON_VMDNS_ERROR(dwError);

    *pplibHandle = plibHandle;

cleanup:
    VMDNS_SAFE_FREE_MEMORY(pszVmafdLibPath);
    return dwError;

error:
    goto cleanup;
}
