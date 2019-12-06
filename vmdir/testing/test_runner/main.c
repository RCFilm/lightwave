/*
 * Copyright © 2012-2016 VMware, Inc.  All Rights Reserved.
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

#define DEFAULT_INTERNAL_USER_NAME "integration_tests_lua"
#define DEFAULT_TEST_COMPUTER_NAME  "integration_tests_computer"
#define DEFAULT_TEST_CONTAINER_NAME "testcontainer"

VOID
ShowUsage(
    PVOID pvState
    )
{
    printf("Usage: vmdir_integration_test <args>\n");
    printf("Required arguments:\n");
    printf("\t-H/--host <host> -- The host to connect to.\n");
    printf("\t-u/--username <user name> -- user to connect with.\n");
    printf("\t-w/--password <password> -- The password to authenticate with\n");
    printf("\t-d/--domain domain -- The domain to use (e.g., vsphere.local)\n");
    printf("\t-b/--break -- Break into debugger if a test fails.\n");
    printf("\t-k/--keep-going -- Don't stop on failed test result.\n");
    printf("\t-r/--remote-only -- skip IPC test cases.\n");
    printf("\t-t/--test -- The directory containing tests or the test DLL itself\n");
    printf("\t-s/--skip-cleanup -- skip test data cleanup.\n");
}

DWORD
PostValidationRoutine(
    PVOID pvContext
    )
{
    PVMDIR_TEST_STATE pContext = (PVMDIR_TEST_STATE)pvContext;

    //
    // These parameters are all required.
    //
    if (pContext->pszServerName == NULL ||
        pContext->pszUserName == NULL ||
        pContext->pszDomain == NULL ||
        pContext->pszTest == NULL)
    {
        return VMDIR_ERROR_INVALID_PARAMETER;
    }

    return 0;
}

DWORD
TestInfrastructureCleanup(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD dwError = 0;

    if (pState->pLd == NULL)
    {
        return 0;
    }

    dwError = VmDirTestDeleteUser(pState, NULL, VmDirTestGetComputerCn(pState));
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirTestDeleteUser(pState, NULL, VmDirTestGetInternalUserCn(pState));
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirTestDeleteContainer(pState, NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    VmDirFreeStringA((PSTR)pState->pszBaseDN);
    VmDirTestLdapUnbind(pState->pLd);
    VmDirTestLdapUnbind(pState->pSecondLd);
    VmDirTestLdapUnbind(pState->pLdLimited);
    VmDirTestLdapUnbind(pState->pLdAnonymous);
    VmDirTestLdapUnbind(pState->pLdCustom);
    return 0;

error:
    printf("Test cleanup failed with error %d\n", dwError);
    goto cleanup;
}

DWORD
_VmDirSetOptionalSecondNode(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD   dwError = 0;
    DWORD   dwCnt = 0;
    PSTR    pszName = NULL;
    PVMDIR_SERVER_INFO  pServerInfo = NULL;
    DWORD               dwServerInfoCount = 0;

    dwError = VmDirGetServers(
                pState->pszServerName,
                pState->pszUserName,
                pState->pszPassword,
                &pServerInfo,
                &dwServerInfoCount
                );
    BAIL_ON_VMDIR_ERROR(dwError);

    for (dwCnt=0; dwCnt<dwServerInfoCount; dwCnt++)
    {
        VMDIR_SAFE_FREE_MEMORY(pszName);

        dwError = VmDirDnLastRDNToCn(
            pServerInfo[dwCnt].pszServerDN,
            &pszName);
        BAIL_ON_VMDIR_ERROR(dwError);

        if (!VmDirStringStartsWith(pszName, pState->pszServerName, FALSE))
        {
            pState->pszSecondServerName = pszName;
            pszName = NULL;
            break;
        }
    }

cleanup:
    for (dwCnt=0; dwCnt<dwServerInfoCount; dwCnt++)
    {
        VMDIR_SAFE_FREE_MEMORY(pServerInfo[dwCnt].pszServerDN);
    }

    VMDIR_SAFE_FREE_MEMORY(pServerInfo);
    VMDIR_SAFE_FREE_MEMORY(pszName);

    return dwError;

error:
    printf("%s error %d\n", __FUNCTION__, dwError);
    goto cleanup;
}

DWORD
_VmDirTestCreateComputerAndConnection(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD dwError = 0;
    PSTR pszUserUPN = NULL;
    LDAP *pLd;

    dwError = VmDirTestCreateComputer(
                pState,
                NULL,
                VmDirTestGetComputerCn(pState));
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringPrintf(
                &pszUserUPN,
                "%s@%s",
                VmDirTestGetComputerCn(pState),
                pState->pszDomain);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSafeLDAPBind(
                &pLd,
                pState->pszServerName,
                pszUserUPN,
                pState->pszPassword);
    BAIL_ON_VMDIR_ERROR(dwError);

    pState->pLdComputer = pLd;

cleanup:
    VMDIR_SAFE_FREE_STRINGA(pszUserUPN);
    return dwError;
error:
    printf("%s failed with error %d\n", __FUNCTION__, dwError);
    goto cleanup;
}

DWORD
_VmDirTestCreateLimitedUserAndConnection(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD dwError = 0;
    PSTR pszUserUPN = NULL;
    LDAP *pLd;

    dwError = VmDirTestCreateUser(
                pState,
                NULL,
                VmDirTestGetInternalUserCn(pState),
                NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringPrintf(
                &pszUserUPN,
                "%s@%s",
                VmDirTestGetInternalUserCn(pState),
                pState->pszDomain);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSafeLDAPBind(
                &pLd,
                pState->pszServerName,
                pszUserUPN,
                pState->pszPassword);
    BAIL_ON_VMDIR_ERROR(dwError);

    pState->pLdLimited = pLd;

cleanup:
    VMDIR_SAFE_FREE_STRINGA(pszUserUPN);
    return dwError;
error:
    printf("%s failed with error %d\n", __FUNCTION__, dwError);
    goto cleanup;
}

DWORD
_VmDirTestCreateTestContainer(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD dwError = 0;
    PCSTR valsCn[] = {DEFAULT_TEST_CONTAINER_NAME, NULL};
    PCSTR valsClass[] = {OC_TOP, OC_CONTAINER, NULL};
    PCSTR valsAcl[] = {NULL, NULL};
    PSTR pszDN = NULL;
    PSTR pszDomainSid = NULL;
    PSTR pszAclString = NULL;
    PSTR pszLimitedUserSid = NULL;
    LDAPMod mod[] = {
        {LDAP_MOD_ADD, ATTR_CN, {(PSTR*)valsCn}},
        {LDAP_MOD_ADD, ATTR_OBJECT_CLASS, {(PSTR*)valsClass}},
        {LDAP_MOD_ADD, ATTR_ACL_STRING, {(PSTR*)valsAcl}},
    };
    LDAPMod *attrs[] = {&mod[0], &mod[1], &mod[2], NULL};

    dwError = VmDirAllocateStringPrintf(
                &pszDN,
                "cn=%s,%s",
                DEFAULT_TEST_CONTAINER_NAME,
                pState->pszBaseDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirTestGetDomainSid(pState, pState->pszBaseDN, &pszDomainSid);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirTestGetUserSid(pState, VmDirTestGetInternalUserCn(pState), NULL, &pszLimitedUserSid);
    BAIL_ON_VMDIR_ERROR(dwError);

    // O:%s-500G:BAD:(A;;RPWP;;;S-1-7-32-666)(A;;GXNRNWGXCCDCRPWP;;;BA)(A;;GXNRNWGXCCDCRPWP;;;%s-500)",
    dwError = VmDirAllocateStringPrintf(
                &pszAclString,
                "O:%s-500G:BAD:(A;;CCDCRPWP;;;BA)(A;;CCDCRPWP;;;%s-500)(A;;CCRPWP;;;%s)",
                pszDomainSid,
                pszDomainSid,
                pszLimitedUserSid);
    BAIL_ON_VMDIR_ERROR(dwError);

    valsAcl[0] = pszAclString;

    dwError = ldap_add_ext_s(
                pState->pLd,
                pszDN,
                attrs,
                NULL,
                NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    VMDIR_SAFE_FREE_STRINGA(pszAclString);
    VMDIR_SAFE_FREE_STRINGA(pszLimitedUserSid);
    VMDIR_SAFE_FREE_STRINGA(pszDomainSid);
    VMDIR_SAFE_FREE_STRINGA(pszDN);
    return dwError;
error:
    printf("%s failed with error %d\n", __FUNCTION__, dwError);
    goto cleanup;
}

DWORD
_TestAcquireAdminToken(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD   dwError = 0;
    PSTR    pszToken = NULL;
    VMDIR_OIDC_ACQUIRE_TOKEN_INFO TokenInfo = {0};

    TokenInfo.method = METHOD_PASSWORD;
    TokenInfo.pszDomain = pState->pszDomain;
    TokenInfo.pszUPN = pState->pszUserUPN;
    TokenInfo.pszPassword = pState->pszPassword;
    TokenInfo.pszScope = OIDC_TOKEN_SCOPE_VMDIR;

    dwError = VmDirTestOidcTokenAcquire(
        pState->pszSTSServerName,
        OIDC_DEFAULT_PORT,
        &TokenInfo,
        &pszToken);
    BAIL_ON_VMDIR_ERROR(dwError);

    pState->pszAdminAccessToken = pszToken;

cleanup:
    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszToken);
    goto cleanup;
}

DWORD
TestInfrastructureInitialize(
    PVMDIR_TEST_STATE pState
    )
{
    DWORD dwError = 0;
    PSTR pszLdapUri = NULL;

    pState->pfnCleanupCallback = TestInfrastructureCleanup;
    pState->pszTestContainerName = DEFAULT_TEST_CONTAINER_NAME;
    pState->pszInternalUserName = DEFAULT_INTERNAL_USER_NAME;
    pState->pszComputerName = DEFAULT_TEST_COMPUTER_NAME;

    dwError = _VmDirSetOptionalSecondNode(pState);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirDomainNameToDN(pState->pszDomain, (PSTR*)&pState->pszBaseDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    // assume default user container
    dwError = VmDirAllocateStringPrintf(
                (PSTR*)&pState->pszUserDN,
                "cn=%s,cn=users,%s",
                pState->pszUserName,
                pState->pszBaseDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSafeLDAPBind(
                &pState->pLd,
                pState->pszServerName,
                pState->pszUserUPN,
                pState->pszPassword);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (pState->pszSecondServerName)
    {
        DWORD dwCnt = 0;

        for (dwCnt=0; dwCnt < 10; dwCnt++)
        {
            dwError = VmDirSafeLDAPBind(
                        &pState->pSecondLd,
                        pState->pszSecondServerName,
                        pState->pszUserUPN,
                        pState->pszPassword);
            if (dwError)
            {
                printf("Connect to %s failed %d \n", pState->pszSecondServerName, dwError);
                VmDirSleep(1000);
                continue;
            }
        }

        if (dwError)
        {
            printf("Failed to connect to second node %s \n", pState->pszSecondServerName);
            dwError = 0;
        }
        else
        {
            printf("Connected to second node %s \n", pState->pszSecondServerName);
        }
    }

    dwError = _TestAcquireAdminToken(pState);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirTestRestPing(
        pState->pszServerName,
        DEFAULT_HTTPS_PORT_NUM,
        pState->pszAdminAccessToken,
        NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

    //
    // Cleanup any leftover state from a previous run.
    //
    (VOID)VmDirTestDeleteContainer(pState, NULL);

    dwError = VmDirTestCreateAnonymousConnection(
                pState->pszServerName,
                &pState->pLdAnonymous);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = _VmDirTestCreateLimitedUserAndConnection(pState);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = _VmDirTestCreateComputerAndConnection(pState);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = _VmDirTestCreateTestContainer(pState);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;
error:
    VMDIR_SAFE_FREE_STRINGA(pszLdapUri);
    goto cleanup;
}

DWORD
_VmDirExecuteTestModule(
    PVMDIR_TEST_STATE pState,
    PCSTR pszModule
    )
{
    PVOID pDllHandle = NULL;
    PTEST_SETUP_CALLBACK pfnTestSetup = NULL;
    PTEST_RUNNER_CALLBACK pfnTestRunner = NULL;
    PTEST_CLEANUP_CALLBACK pfnTestCleanup = NULL;
    DWORD dwError = 0;

    printf("Executing test module: %s ...\n", pszModule);

    // Need to make sure that there's a slash in the name
    pDllHandle = dlopen(pszModule, RTLD_NOW | RTLD_LOCAL);
    if (pDllHandle == NULL)
    {
        printf("error ==> %s\n", dlerror());
        BAIL_WITH_VMDIR_ERROR(dwError, errno);
    }

    pfnTestSetup = (PTEST_SETUP_CALLBACK)dlsym(pDllHandle, "TestSetup");
    pfnTestRunner = (PTEST_RUNNER_CALLBACK)dlsym(pDllHandle, "TestRunner");
    pfnTestCleanup = (PTEST_CLEANUP_CALLBACK)dlsym(pDllHandle, "TestCleanup");
    if (pfnTestSetup == NULL || pfnTestRunner == NULL || pfnTestCleanup == NULL)
    {
        printf("error ==> %s\n", dlerror());
        BAIL_WITH_VMDIR_ERROR(dwError, errno);
    }

    dwError = (*pfnTestSetup)(pState);
    if (dwError != 0)
    {
        printf("Test setup failed with error %d\n", dwError);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = (*pfnTestRunner)(pState);
    if (dwError != 0)
    {
        //
        // If there's an error let the user know, but don't bail yet so we
        // can call the cleanup callback.
        //
        printf("Test module %s failed with error %d\n", pszModule, dwError);
    }

cleanup:
    (*pfnTestCleanup)(pState);

    if (pDllHandle != NULL)
    {
        (VOID)dlclose(pDllHandle);
    }

    return dwError;
error:
    goto cleanup;
}

int
VmDirMain(
    int argc,
    char* argv[]
    )
{
    DWORD dwError = 0;
    VMDIR_TEST_STATE State = { 0 };
    PVMDIR_STRING_LIST pStringList = NULL;
    DWORD dwIndex = 0;
    VMDIR_COMMAND_LINE_OPTION CommandLineOptions[] =
    {
        {'H', "host", CL_STRING_PARAMETER, &State.pszServerName},
        {'S', "stshost", CL_STRING_PARAMETER, &State.pszSTSServerName},
        {'u', "username", CL_STRING_PARAMETER, &State.pszUserName},
        {'w', "password", CL_STRING_PARAMETER, &State.pszPassword},
        {'d', "domain", CL_STRING_PARAMETER, &State.pszDomain},
        {'b', "break", CL_NO_PARAMETER, &State.bBreakIntoDebugger},
        {'k', "keep-going", CL_NO_PARAMETER, &State.bKeepGoing},
        {'r', "remote-only", CL_NO_PARAMETER, &State.bRemoteOnly},
        {'t', "test", CL_STRING_PARAMETER, &State.pszTest},
        {'s', "skip-cleanup", CL_NO_PARAMETER, &State.bSkipCleanup},
        {0, 0, 0, 0}
    };

    VMDIR_PARSE_ARG_CALLBACKS Callbacks =
    {
        PostValidationRoutine,
        ShowUsage,
        &State
    };

    dwError = VmDirParseArguments(
                CommandLineOptions,
                &Callbacks,
                argc,
                argv);
    BAIL_ON_VMDIR_ERROR(dwError);


    dwError = VmDirAllocateStringPrintf(
                (PSTR*)&State.pszUserUPN,
                "%s@%s",
                State.pszUserName,
                State.pszDomain);
    BAIL_ON_VMDIR_ERROR(dwError);

    SSL_library_init();

    dwError = OidcClientGlobalInit();
    BAIL_ON_VMDIR_ERROR(dwError);

    printf("VmDir integration tests starting ...\n");

    dwError = TestInfrastructureInitialize(&State);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirStringListInitialize(&pStringList, 8);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = _VmDirEnumerateTests(State.pszTest, pStringList);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (pStringList->dwCount == 0)
    {
        printf("No tests found!\n");
        goto cleanup;
    }

    for (dwIndex = 0; dwIndex < pStringList->dwCount; dwIndex++)
    {
        _VmDirExecuteTestModule(&State, pStringList->pStringList[dwIndex]);
    }

cleanup:
    TestInfrastructureCleanup(&State);

    return dwError;

error:
    printf("Integration test failed with error 0n%d\n", dwError);
    goto cleanup;
}

#ifdef _WIN32

int wmain(int argc, wchar_t* argv[])
{
    DWORD dwError = 0;
    PSTR* ppszArgs = NULL;
    int   iArg = 0;

    dwError = VmDirAllocateMemory(sizeof(PSTR) * argc, (PVOID*)&ppszArgs);
    BAIL_ON_VMDIR_ERROR(dwError);

    for (; iArg < argc; iArg++)
    {
        dwError = VmDirAllocateStringAFromW(argv[iArg], &ppszArgs[iArg]);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirMain(argc, ppszArgs);
    BAIL_ON_VMDIR_ERROR(dwError);

error:
    if (ppszArgs)
    {
        for (iArg = 0; iArg < argc; iArg++)
        {
            VMDIR_SAFE_FREE_MEMORY(ppszArgs[iArg]);
        }

        VmDirFreeMemory(ppszArgs);
    }

    return dwError;
}

#else

int main(int argc, char* argv[])
{
    return VmDirMain(argc, argv);
}

#endif
