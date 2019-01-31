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



#include "includes.h"

static
DWORD
ParseArgs(
    int   argc,
    char* argv[],
    PVMW_IC_SETUP_PARAMS* ppSetupParams
    );

static
DWORD
VmwDeployBuildParams(
    PCSTR pszDomainController,
    PCSTR pszDomain,
    PCSTR pszMachineAccount,
    PCSTR pszOrgUnit,
    PCSTR pszUsername,
    PCSTR pszPassword,
    PCSTR pszSubjectAltName,
    PCSTR pszSiteName,
    PCSTR pszLwCAServer,
    PCSTR pszLwCAId,
    BOOLEAN bDisableAfdListener,
    BOOLEAN bDisableDNS,
    BOOLEAN bUseMachineAccount,
    BOOLEAN bMachinePreJoined,
    BOOLEAN bGenMachineSSL,
    BOOLEAN bAtomicJoin,
    BOOLEAN bInsecure,
    BOOLEAN bMultiTenantedCAEnabled,
    PVMW_IC_SETUP_PARAMS* ppSetupParams
    );

static
VOID
ShowUsage(
    VOID
    );

int
LightwaveDomainJoin(
    int argc,
    char* argv[])
{
    DWORD dwError = 0;
    PVMW_IC_SETUP_PARAMS pSetupParams = NULL;
    PVMW_DEPLOY_LOG_CONTEXT pContext = NULL;
    int retCode = 0;
    PSTR pszErrorMsg = NULL;
    PSTR pszErrorDesc = NULL;
    DWORD dwError2 = 0;

    if (argc == 0 || argv[0] == NULL || !strcmp(argv[0], "--help"))
    {
        ShowUsage();
        goto cleanup;
    }

    setlocale(LC_ALL, "");

    dwError = VmwDeployInitialize();
    BAIL_ON_DEPLOY_ERROR(dwError);

    dwError = ParseArgs(argc, argv, &pSetupParams);
    if (dwError)
    {
        ShowUsage();
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    dwError = VmwDeployCreateLogContext(
                    VMW_DEPLOY_LOG_TARGET_FILE,
                    VMW_DEPLOY_LOG_LEVEL_INFO,
                    ".",
                    &pContext);
    BAIL_ON_DEPLOY_ERROR(dwError);

    dwError = VmwDeploySetLogContext(pContext);
    BAIL_ON_DEPLOY_ERROR(dwError);

    dwError = VmwDeploySetupInstance(pSetupParams);
    BAIL_ON_DEPLOY_ERROR(dwError);

    fprintf(stdout, "Domain Join was successful\n");

cleanup:

    if (pSetupParams)
    {
        VmwDeployFreeSetupParams(pSetupParams);
    }
    if (pContext)
    {
        VmwDeployReleaseLogContext(pContext);
    }
    VmwDeployShutdown();

    return dwError;

error:

    dwError2 = VmwDeployGetError(
                     dwError,
                     &pszErrorMsg,
                     &retCode);
    if (dwError2 || retCode == 1)
    {
        if (!VmAfdGetErrorMsgByCode(dwError, &pszErrorDesc))
        {
            fprintf(stderr, "Domain join failed. Error %u: %s \n", dwError, pszErrorDesc);
        }
        else
        {
            fprintf(stderr, "Domain join failed with error: %u\n", dwError);
        }
    }
    else
    {
        fprintf(
            stderr,
            "Domain join failed, error= %s %u\n",
            pszErrorMsg,
            dwError);
    }

    VMW_DEPLOY_LOG_ERROR("Domain join failed. Error code: %u", dwError);

    if (pszErrorMsg)
    {
        VmwDeployFreeMemory(pszErrorMsg);
        pszErrorMsg = NULL;
    }

    goto cleanup;
}

static
DWORD
ParseArgs(
    int   argc,
    char* argv[],
    PVMW_IC_SETUP_PARAMS* ppSetupParams
    )
{
    DWORD dwError     = 0;
    PSTR  pszDomainController   = NULL;
    PSTR  pszDomain = NULL;
    PSTR  pszMachineAccount = NULL;
    PSTR  pszOrgUnit = NULL;
    PSTR  pszSubjectAltName = NULL;
    PSTR  pszUsername = NULL;
    PSTR  pszPassword = NULL;
    PSTR  pszSiteName = NULL;
    PSTR  pszLwCAServer = NULL;
    PSTR  pszLwCAId = NULL;
    BOOLEAN bDisableAfdListener = FALSE;
    BOOLEAN bUseMachineAccount = FALSE;
    BOOLEAN bMachinePreJoined = FALSE;
    BOOLEAN bDisableDNS = FALSE;
    BOOLEAN bGenMachineSSL = TRUE;
    BOOLEAN bAtomicJoin = FALSE;
    BOOLEAN bInsecure = FALSE;
    BOOLEAN bMultiTenantedCAEnabled = FALSE;

    enum PARSE_MODE
    {
        PARSE_MODE_OPEN = 0,
        PARSE_MODE_DOMAIN_CONTROLLER,
        PARSE_MODE_DOMAIN,
        PARSE_MODE_USERNAME,
        PARSE_MODE_PASSWORD,
        PARSE_MODE_MACHINE_ACCOUNT,
        PARSE_MODE_ORG_UNIT,
        PARSE_MODE_SSL_SUBJECT_ALT_NAME,
        PARSE_MODE_SITE,
        PARSE_MODE_CA_SERVER,
        PARSE_MODE_CA_ID
    } parseMode = PARSE_MODE_OPEN;
    int iArg = 0;
    PVMW_IC_SETUP_PARAMS pSetupParams = NULL;

    for (; iArg < argc; iArg++)
    {
        char* pszArg = argv[iArg];

        switch (parseMode)
        {
            case PARSE_MODE_OPEN:
                if (!strcmp(pszArg, "--username"))
                {
                    parseMode = PARSE_MODE_USERNAME;
                }
                else if (!strcmp(pszArg, "--password"))
                {
                    parseMode = PARSE_MODE_PASSWORD;
                }
                else if (!strcmp(pszArg, "--domain-controller"))
                {
                    parseMode = PARSE_MODE_DOMAIN_CONTROLLER;
                }
                else if (!strcmp(pszArg, "--domain"))
                {
                    parseMode = PARSE_MODE_DOMAIN;
                }
                else if (!strcmp(pszArg, "--machine-account-name"))
                {
                    parseMode = PARSE_MODE_MACHINE_ACCOUNT;
                }
                else if (!strcmp(pszArg, "--org-unit"))
                {
                    parseMode = PARSE_MODE_ORG_UNIT;
                }
                else if (!strcmp(pszArg, "--disable-afd-listener"))
                {
                    bDisableAfdListener = TRUE;
                }
                else if (!strcmp(pszArg, "--disable-dns"))
                {
                    bDisableDNS = TRUE;
                }
                else if (!strcmp(pszArg, "--use-machine-account"))
                {
                    bUseMachineAccount = TRUE;
                }
                else if (!strcmp(pszArg, "--prejoined"))
                {
                    bMachinePreJoined = TRUE;
                }
                else if (!strcmp(pszArg, "--skip-gen-machine-ssl"))
                {
                    bGenMachineSSL = FALSE;
                }
                else if (!strcmp(pszArg, "--atomic"))
                {
                    bAtomicJoin = TRUE;
                }
                else if (!strcmp(pszArg, "--insecure"))
                {
                    bInsecure = TRUE;
                }
                else if (!strcmp(pszArg, "--help"))
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }
                else if (!strcmp(pszArg, "--ssl-subject-alt-name"))
                {
                    parseMode = PARSE_MODE_SSL_SUBJECT_ALT_NAME;
                }
                else if (!strcmp(pszArg, "--site"))
                {
                    parseMode = PARSE_MODE_SITE;
                }
                else if (!strcmp(pszArg, "--enable-multi-tenanted-ca"))
                {
                    bMultiTenantedCAEnabled = TRUE;
                }
                else if (!strcmp(pszArg, "--ca-server"))
                {
                    parseMode = PARSE_MODE_CA_SERVER;
                }
                else if (!strcmp(pszArg, "--ca-id"))
                {
                    parseMode = PARSE_MODE_CA_ID;
                }
                else
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                break;

            case PARSE_MODE_USERNAME:

                if (pszUsername)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszUsername = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_PASSWORD:

                if (pszPassword)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszPassword = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_DOMAIN:

                if (pszDomain)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszDomain = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_DOMAIN_CONTROLLER:

                if (pszDomainController)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszDomainController = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_MACHINE_ACCOUNT:

                if (pszMachineAccount)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszMachineAccount = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_ORG_UNIT:

                if (pszOrgUnit)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszOrgUnit = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_SSL_SUBJECT_ALT_NAME:

                if (pszSubjectAltName)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszSubjectAltName = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_SITE:

                if (pszSiteName)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszSiteName = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_CA_SERVER:

                if (pszLwCAServer)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszLwCAServer = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            case PARSE_MODE_CA_ID:

                if (pszLwCAId)
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    BAIL_ON_DEPLOY_ERROR(dwError);
                }

                pszLwCAId = pszArg;

                parseMode = PARSE_MODE_OPEN;

                break;

            default:

                dwError = ERROR_INVALID_PARAMETER;
                BAIL_ON_DEPLOY_ERROR(dwError);

                break;
        }
    }

    if ( bUseMachineAccount && IsNullOrEmptyString(pszMachineAccount))
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (bMultiTenantedCAEnabled && (IsNullOrEmptyString(pszLwCAServer) || IsNullOrEmptyString(pszLwCAId)))
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    dwError = VmwDeployBuildParams(
                    pszDomainController,
                    pszDomain,
                    pszMachineAccount,
                    pszOrgUnit,
                    pszUsername,
                    pszPassword,
                    pszSubjectAltName,
                    pszSiteName,
                    pszLwCAServer,
                    pszLwCAId,
                    bDisableAfdListener,
                    bDisableDNS,
                    bUseMachineAccount,
                    bMachinePreJoined,
                    bGenMachineSSL,
                    bAtomicJoin,
                    bInsecure,
                    bMultiTenantedCAEnabled,
                    &pSetupParams);
    BAIL_ON_DEPLOY_ERROR(dwError);

    *ppSetupParams = pSetupParams;

cleanup:

    return dwError;

error:

    *ppSetupParams = NULL;

    if (pSetupParams)
    {
        VmwDeployFreeSetupParams(pSetupParams);
    }

    goto cleanup;
}

static
DWORD
VmwDeployBuildParams(
    PCSTR pszDomainController,
    PCSTR pszDomain,
    PCSTR pszMachineAccount,
    PCSTR pszOrgUnit,
    PCSTR pszUsername,
    PCSTR pszPassword,
    PCSTR pszSubjectAltName,
    PCSTR pszSiteName,
    PCSTR pszLwCAServer,
    PCSTR pszLwCAId,
    BOOLEAN bDisableAfdListener,
    BOOLEAN bDisableDNS,
    BOOLEAN bUseMachineAccount,
    BOOLEAN bMachinePreJoined,
    BOOLEAN bGenMachineSSL,
    BOOLEAN bAtomicJoin,
    BOOLEAN bInsecure,
    BOOLEAN bMultiTenantedCAEnabled,
    PVMW_IC_SETUP_PARAMS* ppSetupParams
    )
{
    DWORD dwError = 0;
    PVMW_IC_SETUP_PARAMS pSetupParams = NULL;
    PSTR pszPassword1 = NULL;
    PCSTR pszUserNamePrompt = NULL;

    dwError = VmwDeployAllocateMemory(
                    sizeof(*pSetupParams),
                    (VOID*)&pSetupParams);
    BAIL_ON_DEPLOY_ERROR(dwError);

    if (IsNullOrEmptyString(pszDomain))
    {
        pszDomain = VMW_DEFAULT_DOMAIN_NAME;
    }

    pSetupParams->dir_svc_mode = VMW_DIR_SVC_MODE_CLIENT;

    if (!IsNullOrEmptyString(pszDomainController))
    {
        dwError = VmwDeployAllocateStringA(
                        pszDomainController,
                        &pSetupParams->pszServer);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    dwError = VmwDeployGetHostname(&pSetupParams->pszHostname);
    BAIL_ON_DEPLOY_ERROR(dwError);

    dwError = VmwDeployAllocateStringA(
                        pszDomain,
                        &pSetupParams->pszDomainName);
    BAIL_ON_DEPLOY_ERROR(dwError);

    if (!IsNullOrEmptyString(pszMachineAccount))
    {
        dwError = VmwDeployAllocateStringA(
                        pszMachineAccount,
                        &pSetupParams->pszMachineAccount);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (!IsNullOrEmptyString(pszOrgUnit))
    {
        dwError = VmwDeployAllocateStringA(
                        pszOrgUnit,
                        &pSetupParams->pszOrgUnit);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (!pszUsername)
    {
        pszUsername = (bUseMachineAccount && !IsNullOrEmptyString(pszMachineAccount))
                            ? pszMachineAccount : VMW_ADMIN_NAME;
    }

    if (!pszPassword)
    {
        pszUserNamePrompt = pszUsername;

        dwError = VmwDeployReadPassword(
                        pszUserNamePrompt,
                        pszDomain,
                        &pszPassword1);
        BAIL_ON_DEPLOY_ERROR(dwError);

        pszPassword = pszPassword1;
    }

    if (!IsNullOrEmptyString(pszSubjectAltName))
    {
        dwError = VmwDeployAllocateStringA(
                        pszSubjectAltName,
                        &pSetupParams->pszSubjectAltName);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (!IsNullOrEmptyString(pszSiteName))
    {
        dwError = VmwDeployAllocateStringA(
                            pszSiteName,
                            &pSetupParams->pszSite);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (!IsNullOrEmptyString(pszLwCAServer))
    {
        dwError = VmwDeployAllocateStringA(
                            pszLwCAServer,
                            &pSetupParams->pszLwCAServer);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    if (!IsNullOrEmptyString(pszLwCAId))
    {
        dwError = VmwDeployAllocateStringA(
                            pszLwCAId,
                            &pSetupParams->pszLwCAId);
        BAIL_ON_DEPLOY_ERROR(dwError);
    }

    dwError = VmwDeployAllocateStringA(pszUsername, &pSetupParams->pszUsername);
    BAIL_ON_DEPLOY_ERROR(dwError);

    dwError = VmwDeployAllocateStringA(pszPassword, &pSetupParams->pszPassword);
    BAIL_ON_DEPLOY_ERROR(dwError);

    pSetupParams->bDisableAfdListener = bDisableAfdListener;
    pSetupParams->bDisableDNS = bDisableDNS;
    pSetupParams->bUseMachineAccount = bUseMachineAccount;
    pSetupParams->bMachinePreJoined = bMachinePreJoined;
    pSetupParams->bGenMachineSSL = bGenMachineSSL;
    pSetupParams->bAtomicJoin = bAtomicJoin;
    pSetupParams->bInsecure = bInsecure;
    pSetupParams->bMultiTenantedCAEnabled = bMultiTenantedCAEnabled;

    *ppSetupParams = pSetupParams;

cleanup:

    if (pszPassword1)
    {
        VmwDeployFreeMemory(pszPassword1);
    }

    return dwError;

error:

    *ppSetupParams = NULL;

    if (pSetupParams)
    {
        VmwDeployFreeSetupParams(pSetupParams);
    }

    goto cleanup;
}

static
VOID
ShowUsage(
    VOID
    )
{
    PSTR pszUsageText =
           "Usage: lightwave domain join { arguments }\n"
           "Arguments:\n"
           "    [--domain-controller <domain controller's hostname or IP Address>]\n"
           "    [--domain    <fully qualified domain name. default: vsphere.local>]\n"
           "    [--machine-account-name <preferred computer account name. default: <hostname>]\n"
           "    [--org-unit <organizational unit. default: Computers]\n"
           "    [--disable-afd-listener]\n"
           "    [--disable-dns]\n"
           "    [--use-machine-account] Use machine account credentials to join\n"
           "    [--prejoined] Machine account is already created in directory\n"
           "    [--insecure] Trust lightwave server certificates. Only applicable with --prejoined where the client uses REST interfaces.\n"
           "    [--skip-gen-machine-ssl] Skips generation of machine SSL certificate\n"
           "    [--atomic] Perform atomic join\n"
           "    [--enable-multi-tenanted-ca] join to a multi tenanted ca enabled server\n"
           "    [--ca-server] ca server when enable-multi-tenanted-ca is set\n"
           "    [--ca-id] ca id when enable-multi-tenanted-ca is set\n"
           "    [--username <account name>]\n"
           "    [--password <password>]\n"
           "    [--ssl-subject-alt-name <subject alternate name on generated SSL certificate. Default: hostname>]\n"
           "    [--site <sitename>] Specific region to join to\n";

    printf("%s", pszUsageText);
}
