/*
 * Copyright © 2012-2015 VMware, Inc.  All Rights Reserved.
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

#define VMDIR_FQDN_SEPARATOR    '.'

static
int
_VmDirSASLInteraction(
    LDAP *      pLD,
    unsigned    flags,
    void *      pDefaults,
    void *      pIn
    );

/*
 * Note: based on http://en.wikipedia.org/wiki/FQDN, a valid FQDN
 * always contains a trailing dot "." at the end of the string.
 * However, in our code, we will handle cases where the trailing dot is
 * omitted. I.e., we treat all of the following examples as valid FQDN:
 * "com.", "vmware.com.", "eng.vmware.com."
 * "com",  "vmware.com",  "eng.vmware.com"
 */
DWORD
VmDirFQDNToDNSize(
    PCSTR pszFQDN,
    UINT32 *sizeOfDN
)
{
    DWORD dwError  = 0;
    int    numElem = 1;
    int    numDots = 0;
    UINT32 sizeRet = 0;
    int len = (int)VmDirStringLenA(pszFQDN);
    int i;
    for ( i=0; i<=len; i++ )
    {
        if (pszFQDN[i] == VMDIR_FQDN_SEPARATOR )
        {
            numDots++;
            if ( i>0 && i<len-1 )
            {
                 numElem++;
            }
        }
    }
    sizeRet = len - numDots;
    // "dc=," for each elements except the last one does NOT has a ","
    sizeRet += 4 * numElem - 1;
    *sizeOfDN = sizeRet;
    return dwError;
}

/*
 * in: pszFQDN = "csp.com"
 * out: *ppszDN = "dc=csp,dc=com"
 */
DWORD
VmDirFQDNToDN(
    PCSTR pszFQDN,
    PSTR* ppszDN
)
{
    int len = (int)VmDirStringLenA(pszFQDN);
    int iStart = 0;
    int iStop = iStart + 1 ;
    int iDest = 0;
    int i;
    PSTR        pszDN = NULL;
    UINT32      dnSize = 0;

    // Calculate size needed to store DN
    DWORD dwError = VmDirFQDNToDNSize(pszFQDN, &dnSize);
    BAIL_ON_VMDIR_ERROR(dwError);

    // Allocate memory needed to store DN
    dwError = VmDirAllocateMemory(dnSize + 1, (PVOID*)&pszDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    for ( ; iStop < len; iStop++ )
    {
        if (pszFQDN[iStop] == VMDIR_FQDN_SEPARATOR)
        {
            (pszDN)[iDest++] = 'd';
            (pszDN)[iDest++] = 'c';
            (pszDN)[iDest++] = '=';
            for ( i= iStart; i<iStop; i++)
            {
                (pszDN)[iDest++] = pszFQDN[i];
            }
            (pszDN)[iDest++] = ',';
            iStart = iStop + 1;
            iStop = iStart;
        }
    }
    (pszDN)[iDest++] = 'd';
    (pszDN)[iDest++] = 'c';
    (pszDN)[iDest++] = '=';
    for ( i= iStart; i<iStop; i++)
    {
        (pszDN)[iDest++] = pszFQDN[i];
    }
    (pszDN)[iDest] = '\0';
    *ppszDN = pszDN;

cleanup:
    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszDN);
    goto cleanup;
}

/*
 * Qsort comparison function for char** data type
 */
int
VmDirQsortPPCHARCmp(
    const void*		ppStr1,
    const void*		ppStr2
    )
{

    if ((ppStr1 == NULL || *(char * const *)ppStr1 == NULL) &&
        (ppStr2 == NULL || *(char * const *)ppStr2 == NULL))
    {
        return 0;
    }

    if (ppStr1 == NULL || *(char * const *)ppStr1 == NULL)
    {
       return -1;
    }

    if (ppStr2 == NULL || *(char * const *)ppStr2 == NULL)
    {
       return 1;
    }

   return VmDirStringCompareA(* (char * const *) ppStr1, * (char * const *) ppStr2, TRUE);
}

int
VmDirQsortPEIDCmp(
    const void * pEID1,
    const void * pEID2)
{
    if (pEID1 == NULL  && pEID2 == NULL)
    {
        return 0;
    }

    if (pEID1 == NULL && pEID2 != NULL)
    {
       return -1;
    }

    if (pEID1 != NULL && pEID2 == NULL)
    {
       return 1;
    }

    if (*((ENTRYID *)pEID1) < *((ENTRYID*)pEID2))
    {
        return -1;
    }
    else if (*((ENTRYID *)pEID1) == *((ENTRYID*)pEID2))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void const *
UtdVectorEntryGetKey(
    PLW_HASHTABLE_NODE  pNode,
    PVOID               pUnused
    )
{
    UptoDateVectorEntry * pUtdVectorEntry = LW_STRUCT_FROM_FIELD(pNode, UptoDateVectorEntry, Node);

    return pUtdVectorEntry->invocationId.lberbv.bv_val;
}

/*
 * VmDirGenOriginatingTimeStr(): Generates the current Universal Time (UTC) in YYYYMMDDHHMMSSmmm format
 * Return value:
 *      0: Indicates that the call succeeded.
 *     -1: Indicates that an error occurred, and errno is set to indicate the error.
 */

#define NSECS_PER_MSEC        1000000

int
VmDirGenOriginatingTimeStr(
    char * timeStr)
{
#ifndef _WIN32
    struct timespec tspec = {0};
    struct tm       tmTime = {0};
    int             retVal = 0;

    retVal = clock_gettime( CLOCK_REALTIME, &tspec );
    BAIL_ON_VMDIR_ERROR(retVal);

    if (gmtime_r(&tspec.tv_sec, &tmTime) == NULL)
    {
        retVal = errno;
        BAIL_ON_VMDIR_ERROR(retVal);
    }
    snprintf( timeStr, VMDIR_ORIG_TIME_STR_LEN, "%4d%02d%02d%02d%02d%02d.%03d",
              tmTime.tm_year+1900,
              tmTime.tm_mon+1,
              tmTime.tm_mday,
              tmTime.tm_hour,
              tmTime.tm_min,
              tmTime.tm_sec,
              (int)(tspec.tv_nsec/NSECS_PER_MSEC));

cleanup:
    return retVal;

error:
    goto cleanup;
#else
    int retVal = 0;
    SYSTEMTIME sysTime = {0};

    GetSystemTime( &sysTime );

    if( _snprintf_s(
        timeStr,
        VMDIR_ORIG_TIME_STR_LEN,
        VMDIR_ORIG_TIME_STR_LEN-1,
        "%4d%02d%02d%02d%02d%02d.%03d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour,
        sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds
        ) == -1 )
    {
        retVal = -1;
    }

    return retVal;
#endif
}

void
VmDirCurrentGeneralizedTimeWithOffset(
    PSTR    pszTimeBuf,
    int     iBufSize,
    DWORD dwOffset // in seconds
    )
{
    time_t      tNow = time(NULL);
    struct tm   tmpTm = {0};
    struct tm   *ptm = NULL;

    tNow -= dwOffset;

#ifdef _WIN32
    ptm = gmtime(&tNow);
#else
    gmtime_r(&tNow, &tmpTm);
    ptm = &tmpTm;
#endif

    VmDirStringPrintFA(pszTimeBuf, iBufSize, "%04d%02d%02d%02d%02d%02d.0Z",
            ptm->tm_year + 1900,
            ptm->tm_mon + 1,
            ptm->tm_mday,
            ptm->tm_hour,
            ptm->tm_min,
            ptm->tm_sec);
}

void
VmDirCurrentGeneralizedTime(
    PSTR    pszTimeBuf,
    int     iBufSize
    )
{
    VmDirCurrentGeneralizedTimeWithOffset(pszTimeBuf, iBufSize, 0);
}

VOID
VmDirForceExit(
    VOID
    )
{
    VmDirLog( LDAP_DEBUG_ANY, "Vmdird force exiting ...");

#ifndef _WIN32
    pid_t       processid = getpid();

    if ( kill( processid, SIGINT ) != 0 )
    {
        VmDirLog( LDAP_DEBUG_ANY, "VmDirForceExit failed (%u)", errno);
    }
#else

    if ( gVmdirGlobals.hStopServiceEvent != 0 )
    {
        if (SetEvent( gVmdirGlobals.hStopServiceEvent ) == 0)
        {
            VmDirLog( LDAP_DEBUG_ANY, "VmDirForceExit failed (%u)", GetLastError());
        }
    }
    else
    {
        VmDirLog( LDAP_DEBUG_ANY, "VmDirForceExit failed, invalid handle");
    }

#endif
}

DWORD
VmDirUuidFromString(
    PCSTR pStr,
    uuid_t* pGuid
)
{
    DWORD dwError = ERROR_SUCCESS;

    if ((pGuid == NULL) || (pStr == NULL) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

#if !defined(_WIN32) || defined(HAVE_DCERPC_WIN32)
    uuid_parse( (PSTR)pStr, *pGuid);
#else
    dwError = UuidFromStringA( (RPC_CSTR)pStr, pGuid );
    BAIL_ON_VMDIR_ERROR(dwError);
#endif

error:
    return dwError;
}

VOID
VmDirLogStackFrame(
    int     logLevel
    )
{
#ifndef _WIN32

#define BT_SIZE 30
    int     iNum = 0;
    VOID*   pBuffer[BT_SIZE] = {0};
    PSTR*   ppszStrings = NULL;

    iNum = backtrace(pBuffer, BT_SIZE);
    ppszStrings = backtrace_symbols(pBuffer, iNum);

    if (ppszStrings != NULL)
    {
        int iCnt = 0;

        for (iCnt = 0; iCnt < iNum; iCnt++)
        {
            VmDirLog( logLevel, "StackFrame (%d)(%s)", iCnt, VDIR_SAFE_STRING(ppszStrings[iCnt]) );
        }

        VMDIR_SAFE_FREE_MEMORY(ppszStrings);
    }

    return;

#else

    return;

#endif
}

DWORD
VmDirSrvCreateDN(
    PCSTR pszContainerName,
    PCSTR pszDomainDN,
    PSTR* ppszContainerDN
    )
{
    DWORD dwError = 0;
    PSTR  pszContainerDN = NULL;

    dwError = VmDirAllocateStringPrintf(&pszContainerDN, "cn=%s,%s", pszContainerName, pszDomainDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszContainerDN = pszContainerDN;

cleanup:
    return dwError;

error:
    *ppszContainerDN = NULL;
    goto cleanup;
}

DWORD
VmDirSrvCreateReplAgrsContainer(
    PVDIR_SCHEMA_CTX pSchemaCtx)
{
    DWORD   dwError = 0;

    PCSTR   pszReplAgrsContainerName =  VMDIR_REPL_AGRS_CONTAINER_NAME;
    PSTR    pszReplAgrsContainerDN = NULL;    // CN=Replication Agreements,<Server DN>

    dwError = VmDirSrvCreateDN(pszReplAgrsContainerName, gVmdirServerGlobals.serverObjDN.lberbv.bv_val,
                               &pszReplAgrsContainerDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSrvCreateContainer(pSchemaCtx, pszReplAgrsContainerDN, pszReplAgrsContainerName);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszReplAgrsContainerDN);
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirSrvCreateContainerWithEID(
    PVDIR_SCHEMA_CTX pSchemaCtx,
    PCSTR            pszContainerDN,
    PCSTR            pszContainerName,
    PVMDIR_SECURITY_DESCRIPTOR pSecDesc, // OPTIONAL
    ENTRYID          eID
    )
{
    DWORD dwError = 0;
    PSTR ppszAttributes[] =
    {
            ATTR_OBJECT_CLASS,  OC_TOP,
            ATTR_OBJECT_CLASS,  OC_CONTAINER,
            ATTR_CN,           (PSTR)pszContainerName,
            NULL
    };

    dwError = VmDirSimpleEntryCreate(pSchemaCtx, ppszAttributes, (PSTR)pszContainerDN, eID);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (pSecDesc != NULL)
    {
        dwError = VmDirSetSecurityDescriptorForDn(pszContainerDN, pSecDesc);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    VmDirLog(LDAP_DEBUG_ANY, "VmDirSrvCreateContainerWithEID failed. Error(%u)", dwError);
    goto cleanup;
}

DWORD
VmDirSrvCreateContainer(
    PVDIR_SCHEMA_CTX pSchemaCtx,
    PCSTR            pszContainerDN,
    PCSTR            pszContainerName
    )
{
    DWORD dwError = 0;

    dwError = VmDirSrvCreateContainerWithEID(pSchemaCtx, pszContainerDN, pszContainerName, NULL, 0);
    BAIL_ON_VMDIR_ERROR(dwError);

error:
    return dwError;
}

// Create THE domain object, and superior domain component objects.

#define MAX_DOMAIN_COMPONENT_VALUE_LEN  1024 // SJ-TBD: Should use a defined constant

DWORD
VmDirSrvCreateDomain(
    PVDIR_SCHEMA_CTX pSchemaCtx,
    BOOLEAN          bSetupHost,
    PCSTR            pszDomainDN
    )
{
    DWORD   dwError = 0;
    char    pszObjDC[MAX_DOMAIN_COMPONENT_VALUE_LEN];
    PSTR    ppszDomainAttrs[] =
    {
        ATTR_OBJECT_CLASS,              OC_TOP,
        ATTR_OBJECT_CLASS,              OC_DC_OBJECT,
        ATTR_OBJECT_CLASS,              OC_DOMAIN,
        ATTR_OBJECT_CLASS,              OC_DOMAIN_DNS,
        ATTR_DOMAIN_COMPONENT,          (PSTR)pszObjDC,
        ATTR_DOMAIN_FUNCTIONAL_LEVEL,   VDIR_DOMAIN_FUNCTIONAL_LEVEL,
        NULL
    };
    int     domainDNLen = (int) VmDirStringLenA(pszDomainDN);
    int     i = 0;
    int     startOfRdnInd = 0;
    int     startOfRdnValInd = 0;
    int     endOfRdnValInd = 0;
    int     rdnValLen = 0;
    PSTR    pszObjDN = 0;

    for (i = endOfRdnValInd = domainDNLen - 1; i >= 0; i--)
    {
        if (i == 0 || pszDomainDN[i] == RDN_SEPARATOR_CHAR)
        {
            startOfRdnInd = (i == 0) ? 0 : i + 1 /* for , */;
            startOfRdnValInd = startOfRdnInd + ATTR_DOMAIN_COMPONENT_LEN + 1 /* for = */;
            rdnValLen = endOfRdnValInd - startOfRdnValInd + 1;
            pszObjDN = (PSTR)pszDomainDN + startOfRdnInd;

            dwError = VmDirStringNCpyA(
                    pszObjDC,
                    MAX_DOMAIN_COMPONENT_VALUE_LEN,
                    pszObjDN + ATTR_DOMAIN_COMPONENT_LEN + 1,
                    rdnValLen);
            BAIL_ON_VMDIR_ERROR(dwError);

            pszObjDC[rdnValLen] = '\0';

            dwError = VmDirSimpleEntryCreate(
                    pSchemaCtx, ppszDomainAttrs, pszObjDN, 0);

            if (dwError == VMDIR_ERROR_BACKEND_ENTRY_EXISTS ||
                dwError == VMDIR_ERROR_ENTRY_ALREADY_EXIST)
            {
                // pass through if parent exists
                dwError = VMDIR_SUCCESS;
            }
            else if (dwError == 0)
            {
                // remove SD for new domain objects - its should be set
                // properly after the whole domain tree is created
                dwError = VmDirSimpleEntryDeleteAttribute(
                        pszObjDN, ATTR_OBJECT_SECURITY_DESCRIPTOR);
                BAIL_ON_VMDIR_ERROR(dwError);
            }
            BAIL_ON_VMDIR_ERROR(dwError);

            endOfRdnValInd = i - 1;
        }
    }

cleanup:
    return dwError;

error:
    VmDirLog(LDAP_DEBUG_ANY, "VmDirSrvCreateDomain failed. Error(%u)", dwError);
    goto cleanup;
}

DWORD
VmDirFindMemberOfAttribute(
    PVDIR_ENTRY pEntry,
    PVDIR_ATTRIBUTE* ppMemberOfAttr
    )
{
    DWORD               dwError = ERROR_SUCCESS;
    PVDIR_ATTRIBUTE     pMemberOfAttr = NULL;
    VDIR_OPERATION      searchOp = {0};
    BOOLEAN             bHasTxn = FALSE;

    if ( pEntry == NULL || ppMemberOfAttr == NULL )
    {
        dwError =  VMDIR_ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirInitStackOperation( &searchOp, VDIR_OPERATION_TYPE_INTERNAL,
LDAP_REQ_SEARCH, NULL );
    BAIL_ON_VMDIR_ERROR(dwError);

    searchOp.pBEIF = VmDirBackendSelect(NULL);

    // start txn
    dwError = searchOp.pBEIF->pfnBETxnBegin( searchOp.pBECtx, VDIR_BACKEND_TXN_READ );
    BAIL_ON_VMDIR_ERROR(dwError);

    bHasTxn = TRUE;

    dwError = VmDirBuildMemberOfAttribute( &searchOp, pEntry, &pMemberOfAttr );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppMemberOfAttr = pMemberOfAttr;

cleanup:
    if (bHasTxn)
    {
        searchOp.pBEIF->pfnBETxnCommit( searchOp.pBECtx);
    }
    VmDirFreeOperationContent(&searchOp);

    return dwError;

error:
    VmDirFreeAttribute(pMemberOfAttr);
    goto cleanup;
}

/* BuildMemberOfAttribute: For the given DN (dn), find out to which groups it belongs (appears in the member attribute),
 * including nested memberships. Return these group DNs as memberOf attribute (memberOfAttr).
 */
#define START_MAX_NUM_MEMBEROF_VALS     20
#define STEP_INC_NUM_MEMBEROF_VALS      10

DWORD
VmDirBuildMemberOfAttribute(
    PVDIR_OPERATION     pOperation,
    PVDIR_ENTRY         pEntry,
    PVDIR_ATTRIBUTE*    ppComputedAttr
    )
{
    DWORD               dwError = LDAP_SUCCESS;
    USHORT              currMaxNumVals = START_MAX_NUM_MEMBEROF_VALS;
    VDIR_FILTER *       f = NULL;
    VDIR_CANDIDATES *   cl = NULL;
    VDIR_ENTRY          groupEntry = {0};
    VDIR_ENTRY *        pGroupEntry = NULL;
    VDIR_BERVALUE *     currMemberDn = NULL;
    int                 currMemberDnInd = 0;
    int                 i = 0;
    unsigned int        j = 0;
    PSTR                pszLocalErrorMsg = NULL;
    PVDIR_ATTRIBUTE     pMemberofAttr = NULL;

    dwError = VmDirAllocateMemory( sizeof( VDIR_FILTER ), (PVOID *)&f );
    BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "no memory");

    f->choice = LDAP_FILTER_EQUALITY;
    f->filtComp.ava.type.lberbv.bv_val = ATTR_MEMBER;
    f->filtComp.ava.type.lberbv.bv_len = ATTR_MEMBER_LEN;
    if ((f->filtComp.ava.pATDesc = VmDirSchemaAttrNameToDesc( pOperation->pSchemaCtx, ATTR_MEMBER)) == NULL)
    {
        dwError = VmDirSchemaCtxGetErrorCode(pOperation->pSchemaCtx);
        BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "Attribute type member not defined.");
    }

    dwError = VmDirAttributeAllocate( ATTR_MEMBEROF, currMaxNumVals, pOperation->pSchemaCtx, &pMemberofAttr );
    BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "no memory");

    // Start the following 'for' loop with currMemberDn being the passed in memberDn, then in the loop process searched
    // group DNs as the member DNs => recursive group membership computation.
    for (currMemberDn = &(pEntry->dn), currMemberDnInd = -1, pMemberofAttr->numVals = 0;currMemberDn->lberbv.bv_val != NULL;)
    {
        dwError = VmDirNormalizeDN( currMemberDn, pOperation->pSchemaCtx );
        BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "DN normalization failed");

        f->filtComp.ava.value = *currMemberDn;

        dwError = pOperation->pBEIF->pfnBEGetCandidates(pOperation->pBECtx, f, 0);
        if (dwError == VMDIR_ERROR_BACKEND_ENTRY_NOTFOUND)
        {   // no candidates found
            dwError = 0;
        }
        if (dwError != 0)
        {
            BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg),
                                          "Backend GetCandidates failed. (%d)(%s)",
                                          pOperation->pBECtx->dwBEErrorCode,
                                          VDIR_SAFE_STRING(pOperation->pBECtx->pszBEErrorMsg));
        }

        cl = f->candidates;

        for (i = 0; i < cl->size; i++)
        {
            pGroupEntry = &groupEntry;
            // SJ-TBD: We should probably have an EID => DN index. That will avoid reading whole entry just to extract
            // the DN, as done below.
            dwError = pOperation->pBEIF->pfnBEIdToEntry(
                        pOperation->pBECtx,
                        pOperation->pSchemaCtx,
                        cl->eIds[i],
                        pGroupEntry,
                        VDIR_BACKEND_ENTRY_LOCK_READ);
            if (dwError == 0)
            {
                if (pMemberofAttr->numVals == (currMaxNumVals - 1))
                {
                    currMaxNumVals += STEP_INC_NUM_MEMBEROF_VALS;
                    dwError = VmDirReallocateMemoryWithInit( (PVOID)pMemberofAttr->vals, (PVOID *)(&(pMemberofAttr->vals)),
                                                       sizeof(VDIR_BERVALUE) * currMaxNumVals,
                                                       sizeof(VDIR_BERVALUE) * (pMemberofAttr->numVals + 1));
                    BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "no memory");
                }

                // Check if the groupDN is self
                if (pGroupEntry->dn.lberbv.bv_len != pEntry->dn.lberbv.bv_len ||
                    VmDirStringCompareA(pGroupEntry->dn.lberbv.bv_val, pEntry->dn.lberbv.bv_val, TRUE) != 0)
                { // No need to normalize DNs in above comparison because we are getting all these DNs from same
                  // source i.e. e->dn

                    // Check if the group DN already included
                    for (j = 0; j < pMemberofAttr->numVals; j++)
                    {
                        if (pGroupEntry->dn.lberbv.bv_len == pMemberofAttr->vals[j].lberbv.bv_len &&
                            VmDirStringCompareA(pGroupEntry->dn.lberbv.bv_val, pMemberofAttr->vals[j].lberbv.bv_val,
                                                TRUE) == 0)
                        { // No need to normalize DNs in above comparison because we are getting all these DNs from same
                          // source i.e. e->dn
                            break;
                        }

                    }
                    if (j == pMemberofAttr->numVals) // No duplicate/match found
                    {
                        dwError = VmDirBervalContentDup( &(pGroupEntry->dn), &(pMemberofAttr->vals[pMemberofAttr->numVals]));
                        BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, (pszLocalErrorMsg), "no memory");

                        pMemberofAttr->numVals++;
                    }
                }

                VmDirFreeEntryContent( pGroupEntry );
                pGroupEntry = NULL; // Reset to NULL so that DeleteEntry is no-op.
            }
            else
            {
                // Ignore BdbEIdToEntry errors. BdbEIdToEntry can fail because the entry may have been deleted
                // since we constructed the candidates list.
            }
        }

        DeleteCandidates( &(f->candidates) );
        currMemberDnInd++;
        currMemberDn = &(pMemberofAttr->vals[currMemberDnInd]);
    }

    if (pMemberofAttr && pMemberofAttr->numVals > 0 && ppComputedAttr)
    {
        *ppComputedAttr = pMemberofAttr;
    }
    else
    {
        VmDirFreeAttribute( pMemberofAttr );
    }

cleanup:

    if (f)
    {
        memset(&(f->filtComp.ava.value), 0, sizeof(VDIR_BERVALUE)); // Since ava.value is NOT owned by filter.
        DeleteFilter( f );
    }
    VmDirFreeEntryContent( pGroupEntry );

    if (pOperation->ldapResult.pszErrMsg == NULL)
    {
        pOperation->ldapResult.pszErrMsg = pszLocalErrorMsg;
    }
    else
    {
        VMDIR_SAFE_FREE_MEMORY(pszLocalErrorMsg);
    }

    return dwError;

error:

    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "VmDirBuildMemberOfAttribute() failed, error string (%s), error code (%d)",
                     VDIR_SAFE_STRING(pszLocalErrorMsg), dwError );

    VmDirFreeAttribute( pMemberofAttr );

    goto cleanup;
}

DWORD
VmDirSASLGSSBind(
     LDAP*  pLD
     )
{
    DWORD   dwError = 0;

    dwError = ldap_sasl_interactive_bind_s( pLD,
                                            NULL,
                                            "GSSAPI",
                                            NULL,
                                            NULL,
                                            LDAP_SASL_QUIET,
                                            _VmDirSASLInteraction,
                                            NULL);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:

    return dwError;

error:

    VmDirLog(LDAP_DEBUG_ANY, "VmDirSASLGSSBind failed. (%d)(%s)", dwError, ldap_err2string(dwError));

    goto cleanup;
}

static
int
_VmDirSASLInteraction(
    LDAP *      pLD,
    unsigned    flags,
    void *      pDefaults,
    void *      pIn
    )
{
    // dummy function to staisfy ldap_sasl_interactive_bind call
    return LDAP_SUCCESS;
}

/*
 * Given an UPN, return entry DN
 */
DWORD
VmDirUPNToDN(
    PCSTR       pszUPN,
    PSTR*       ppszEntryDN
    )
{
    DWORD               dwError = 0;
    VDIR_ENTRY_ARRAY    entryResultArray = {0};
    PSTR                pszLocalDN = NULL;

    if ( IsNullOrEmptyString(pszUPN) || !ppszEntryDN )
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirSimpleEqualFilterInternalSearch( "",
                                                    LDAP_SCOPE_SUBTREE,
                                                    ATTR_KRB_UPN,
                                                    pszUPN,
                                                    &entryResultArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if ( entryResultArray.iSize == 1)
    {
        dwError = VmDirAllocateStringA( entryResultArray.pEntry[0].dn.lberbv.bv_val,
                                        &pszLocalDN);
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else if ( entryResultArray.iSize == 0)
    {
        dwError = VMDIR_ERROR_ENTRY_NOT_FOUND;
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else
    {
        dwError = VMDIR_ERROR_DATA_CONSTRAINT_VIOLATION;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    *ppszEntryDN = pszLocalDN;
    pszLocalDN = NULL;

cleanup:

    VMDIR_SAFE_FREE_MEMORY(pszLocalDN);
    VmDirFreeEntryArrayContent(&entryResultArray);

    return dwError;

error:

    VmDirLog( LDAP_DEBUG_ANY, "UPN (%s) lookup failed (%u)", VDIR_SAFE_STRING(pszUPN), dwError );

    goto cleanup;
}

/*
 * Given a tenant and UPN, return entry DN
 */
DWORD
VmDirTenantizeUPNToDN(
    PCSTR       pszTenant,
    PCSTR       pszUPN,
    PSTR*       ppszEntryDN
    )
{
    DWORD               dwError = 0;
    VDIR_ENTRY_ARRAY    entryResultArray = {0};
    PSTR                pszLocalDN = NULL;
    PSTR                pszLocalTenantDN = NULL;
    PSTR                pszLocalTenantizedUPN = NULL;

    if (IsNullOrEmptyString(pszTenant) || IsNullOrEmptyString(pszUPN) || !ppszEntryDN)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, ERROR_INVALID_PARAMETER);
    }

    dwError = VmDirDomainNameToDN(pszTenant, &pszLocalTenantDN);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringPrintf(
        &pszLocalTenantizedUPN,
        "%s/%s",
        pszUPN,
        pszTenant);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSimpleEqualFilterInternalSearch(
        pszLocalTenantDN,
        LDAP_SCOPE_SUBTREE,
        ATTR_TENANTIZED_UPN,
        pszLocalTenantizedUPN,
        &entryResultArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryResultArray.iSize == 1)
    {
        dwError = VmDirAllocateStringA(
            entryResultArray.pEntry[0].dn.lberbv.bv_val,
            &pszLocalDN);
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else if (entryResultArray.iSize == 0)
    {
        dwError = VMDIR_ERROR_ENTRY_NOT_FOUND;
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else
    {
        dwError = VMDIR_ERROR_DATA_CONSTRAINT_VIOLATION;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    *ppszEntryDN = pszLocalDN;
    pszLocalDN = NULL;

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszLocalDN);
    VMDIR_SAFE_FREE_MEMORY(pszLocalTenantDN);
    VMDIR_SAFE_FREE_MEMORY(pszLocalTenantizedUPN);
    VmDirFreeEntryArrayContent(&entryResultArray);

    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirUPNToDNBerWrap(
    PCSTR           pszUPN,
    PVDIR_BERVALUE  pBervDN
    )
{
    DWORD   dwError = 0;
    PSTR    pszLocalDN = NULL;

    if ( IsNullOrEmptyString(pszUPN) || !pBervDN )
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirUPNToDN( pszUPN, &pszLocalDN );
    BAIL_ON_VMDIR_ERROR(dwError);

    pBervDN->lberbv.bv_val = pszLocalDN;
    pBervDN->lberbv.bv_len = VmDirStringLenA( pszLocalDN );
    pBervDN->bOwnBvVal = TRUE;
    pszLocalDN = NULL;

cleanup:

    VMDIR_SAFE_FREE_MEMORY(pszLocalDN);

    return dwError;

error:
    goto cleanup;
}

/*
 * check if targetDN live under (or equal) to ancestorDN
 */
DWORD
VmDirIsAncestorDN(
    PVDIR_BERVALUE  pBervAncestorDN,
    PVDIR_BERVALUE  pBervTargetDN,
    PBOOLEAN        pbResult
    )
{
    DWORD       dwError = 0;
    BOOLEAN     bRtn = FALSE;

    assert( pBervAncestorDN && pBervTargetDN && pbResult);

    if (pBervAncestorDN->bvnorm_val == NULL || pBervTargetDN->bvnorm_val == NULL)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    if (pBervTargetDN->bvnorm_len == pBervAncestorDN->bvnorm_len &&
        VmDirStringCompareA(pBervTargetDN->bvnorm_val, pBervAncestorDN->bvnorm_val, FALSE) == 0
       )
    {
        bRtn = TRUE;
    }

    if (pBervTargetDN->bvnorm_len > pBervAncestorDN->bvnorm_len                                                         &&
        pBervTargetDN->bvnorm_val[pBervTargetDN->bvnorm_len - pBervAncestorDN->bvnorm_len - 1] == RDN_SEPARATOR_CHAR    &&
        VmDirStringCompareA(pBervTargetDN->bvnorm_val + (pBervTargetDN->bvnorm_len - pBervAncestorDN->bvnorm_len),
                            pBervAncestorDN->bvnorm_val,
                            FALSE) == 0
       )
    {
        bRtn = TRUE;
    }

    *pbResult = bRtn;

cleanup:

    return dwError;

error:

    goto cleanup;
}

DWORD
VmDirHasSingleAttrValue(
    PVDIR_ATTRIBUTE pAttr
    )
{
    DWORD   dwError = 0;

    if ( pAttr->numVals != 1 || pAttr->vals == NULL || pAttr->vals[0].lberbv_val == NULL )
    {
        dwError = VMDIR_ERROR_BAD_ATTRIBUTE_DATA;
    }

    return dwError;
}

DWORD
VmDirValidatePrincipalName(
    PVDIR_ATTRIBUTE pAttr,
    PSTR*           ppErrMsg
    )
{
    DWORD   dwError = 0;
    PSTR    pszLocalErrMsg = NULL;

    dwError = VmDirHasSingleAttrValue( pAttr );
    BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, pszLocalErrMsg, "Invalid %s value", pAttr->type.lberbv_val);

    if ( VmDirStringChrA( pAttr->vals[0].lberbv_val,'@') == NULL )
    {
        dwError = VMDIR_ERROR_BAD_ATTRIBUTE_DATA;
        BAIL_ON_VMDIR_ERROR_WITH_MSG( dwError, pszLocalErrMsg,
                                      "Invalid %s value (%s), char '@' not found.",
                                      pAttr->type.lberbv_val,
                                      pAttr->vals[0].lberbv_val);
    }

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszLocalErrMsg);

    return dwError;

error:
    if (ppErrMsg)
    {
        *ppErrMsg = pszLocalErrMsg;
        pszLocalErrMsg = NULL;
    }

    goto cleanup;
}

/*
 * Determine domain functional level
 */
DWORD
VmDirSrvGetDomainFunctionalLevel(
    PDWORD pdwLevel
    )
{
    DWORD               dwError = 0;
    VDIR_ENTRY_ARRAY    entryArray = {0};
    PVDIR_ATTRIBUTE     pAttrDomainLevel = NULL;
    DWORD               dwLevel = 0;

    if (gVmdirServerGlobals.systemDomainDN.bvnorm_val == NULL)
    {
        dwError = ERROR_NOT_JOINED;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirFilterInternalSearch(gVmdirServerGlobals.systemDomainDN.bvnorm_val, LDAP_SCOPE_BASE, "(objectclass=*)", 0, NULL, &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if ( entryArray.iSize == 1)
    {
        pAttrDomainLevel = VmDirEntryFindAttribute(
                                ATTR_DOMAIN_FUNCTIONAL_LEVEL,
                                &entryArray.pEntry[0]);
        if (!pAttrDomainLevel)
        {
            dwError = VMDIR_ERROR_NO_SUCH_ATTRIBUTE;
            BAIL_ON_VMDIR_ERROR(dwError);
        }

        dwLevel = atoi(pAttrDomainLevel->vals[0].lberbv_val);
    }
    else if ( entryArray.iSize == 0)
    {
        dwError = VMDIR_ERROR_ENTRY_NOT_FOUND;
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else
    {
        dwError = VMDIR_ERROR_DATA_CONSTRAINT_VIOLATION;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    *pdwLevel = dwLevel;

cleanup:

    VmDirFreeEntryArrayContent(&entryArray);
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirInitSrvDFLGlobal(
    VOID
    )
{
    DWORD   dwError = 0;
    DWORD   dwCurrentDfl = VDIR_DFL_DEFAULT;

     dwError = VmDirSrvGetDomainFunctionalLevel(&dwCurrentDfl);
     BAIL_ON_VMDIR_ERROR(dwError);

     if (dwCurrentDfl > VMDIR_MAX_DFL)
     {
         VMDIR_LOG_ERROR(
                 VMDIR_LOG_MASK_ALL,
                 "Server cannot support domain functional level (%d) > max level (%d)",
                 dwCurrentDfl, VMDIR_MAX_DFL);
         BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_FUNC_LVL);
     }

     gVmdirServerGlobals.dwDomainFunctionalLevel = dwCurrentDfl;

     VMDIR_LOG_INFO(
             VMDIR_LOG_MASK_ALL,
             "Domain Functional Level (%d)",
             gVmdirServerGlobals.dwDomainFunctionalLevel);

error:
    return dwError;
}

PCSTR
VmDirLdapModOpTypeToName(
    VDIR_LDAP_MOD_OP modOp
    )
{
    struct
    {
        VDIR_LDAP_MOD_OP  modOp;
        PCSTR             pszOpName;
    }
    static modTypeToNameTable[] =
    {
        { MOD_OP_ADD,    "add" },
        { MOD_OP_DELETE, "del" },
        { MOD_OP_REPLACE,"rep" },
    };

    // make sure VDIR_MOD_OPERATION_TYPE enum starts with 0
    return modOp >= VMDIR_ARRAY_SIZE(modTypeToNameTable) ?
             VMDIR_PCSTR_UNKNOWN :  modTypeToNameTable[modOp].pszOpName;
}

PCSTR
VmDirLdapReqCodeToName(
    ber_tag_t reqCode
    )
{
    struct
    {
        ber_tag_t   reqCode;
        PCSTR       pszOpName;
    }
    static regCodeToNameTable[] =
    {
        {LDAP_REQ_BIND,       "Bind"},
        {LDAP_REQ_UNBIND,     "Unbind"},
        {LDAP_REQ_SEARCH,     "Search"},
        {LDAP_REQ_MODIFY,     "Modify"},
        {LDAP_REQ_ADD,        "Add"},
        {LDAP_REQ_DELETE,     "Delete"},
        {LDAP_REQ_MODDN,      "Moddn"},
        {LDAP_REQ_MODRDN,     "Modrdn"},
        {LDAP_REQ_RENAME,     "Rename"},
        {LDAP_REQ_COMPARE,    "Compare"},
        {LDAP_REQ_ABANDON,    "Abandon"},
        {LDAP_REQ_EXTENDED,   "Extended"},
    };

    PCSTR pszName = VMDIR_PCSTR_UNKNOWN;
    int i=0;

    for (; i< VMDIR_ARRAY_SIZE(regCodeToNameTable); i++)
    {
        if (reqCode == regCodeToNameTable[i].reqCode)
        {
            pszName = regCodeToNameTable[i].pszOpName;
            break;
        }
    }

    return pszName;
}

PCSTR
VmDirOperationTypeToName(
    VDIR_OPERATION_TYPE opType
    )
{
    struct
    {
        VDIR_OPERATION_TYPE opType;
        PCSTR               pszOpName;
    }
    static operationTypeToNameTable[] =
    {
        {VDIR_OPERATION_TYPE_EXTERNAL,  "Ext"},
        {VDIR_OPERATION_TYPE_INTERNAL,  "Int"},
        {VDIR_OPERATION_TYPE_REPL,      "Rep"},
    };

    PCSTR pszName = VMDIR_PCSTR_UNKNOWN;
    int i=0;

    for (; i< VMDIR_ARRAY_SIZE(operationTypeToNameTable); i++)
    {
        if (opType == operationTypeToNameTable[i].opType)
        {
            pszName = operationTypeToNameTable[i].pszOpName;
            break;
        }
    }

    return pszName;
}

PCSTR
VmDirMdbStateToName(
    MDB_state_op opType
    )
{
    struct
    {
        MDB_state_op    opType;
        PCSTR           pszOpName;
    }
    static operationTypeToNameTable[] =
    {
        {MDB_STATE_CLEAR,      "CLEAR"},
        {MDB_STATE_READONLY,   "READONLY"},
        {MDB_STATE_KEEPXLOGS,  "KEEPXLOGS"},
        {MDB_STATE_GETXLOGNUM, "GETXLOGNUM"},
    };

    PCSTR pszName = VMDIR_PCSTR_UNKNOWN;
    int i=0;

    for (; i< VMDIR_ARRAY_SIZE(operationTypeToNameTable); i++)
    {
        if (opType == operationTypeToNameTable[i].opType)
        {
            pszName = operationTypeToNameTable[i].pszOpName;
            break;
        }
    }

    return pszName;
}

/*
 * Compare Attribute lberbv value only, no attribute normalization is done.
 * Used in replication conflict resolution to suppress benign warning log.
 *
 * Should NOT be used as attribute semantics comparison.
 */
BOOLEAN
VmDirIsSameConsumerSupplierEntryAttr(
    PVDIR_ATTRIBUTE pAttr,
    PVDIR_ENTRY     pSrcEntry,
    PVDIR_ENTRY     pDstEntry
    )
{
    BOOLEAN         bIsSameAttr = TRUE;
    DWORD           dwError = 0;
    PVDIR_ATTRIBUTE pSrcAttr = NULL;
    PVDIR_ATTRIBUTE pDstAttr = NULL;
    unsigned        i = 0;
    unsigned        j = 0;

    if (!pAttr || !pSrcEntry || !pDstEntry)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_PARAMETER);
    }

    pSrcAttr = VmDirFindAttrByName(pSrcEntry, pAttr->type.lberbv_val);
    pDstAttr = VmDirFindAttrByName(pDstEntry, pAttr->type.lberbv_val);

    if (pSrcAttr && pDstAttr && pSrcAttr->numVals == pDstAttr->numVals)
    {
        for (i=0; bIsSameAttr && i < pSrcAttr->numVals; i++)
        {
             for (j = 0; j < pDstAttr->numVals; j++)
             {
                 if (pSrcAttr->vals[i].lberbv_len == pDstAttr->vals[j].lberbv_len &&
                     memcmp( pSrcAttr->vals[i].lberbv_val, pDstAttr->vals[j].lberbv_val, pSrcAttr->vals[i].lberbv_len) == 0
                    )
                 {
                     break;
                 }
             }

             if (j == pDstAttr->numVals)
             {
                 bIsSameAttr = FALSE;
             }
        }
    }
    else
    {
        bIsSameAttr = FALSE;
    }

error:
    return bIsSameAttr && dwError==0;
}

/*
 * Sort function -
 * Array of PVDIR_BERVALUE
 */
int
VmDirPVdirBValCmp(
    const void *p1,
    const void *p2
    )
{

    PVDIR_BERVALUE* ppBV1 = (PVDIR_BERVALUE*) p1;
    PVDIR_BERVALUE* ppBV2 = (PVDIR_BERVALUE*) p2;

    if ((ppBV1 == NULL || *ppBV1 == NULL) &&
        (ppBV2 == NULL || *ppBV2 == NULL))
    {
        return 0;
    }

    if (ppBV1 == NULL || *ppBV1 == NULL)
    {
        return -1;
    }

    if (ppBV2 == NULL || *ppBV2 == NULL)
    {
        return 1;
    }

    if ( (*ppBV1)->lberbv_len > (*ppBV2)->lberbv_len )
    {
        return -1;
    }
    else if ( (*ppBV1)->lberbv_len < (*ppBV2)->lberbv_len )
    {
        return 1;
    }
    else
    {
        return memcmp((*ppBV1)->lberbv_val, (*ppBV2)->lberbv_val, (*ppBV1)->lberbv_len);
    }
}

// Delete all entries from a base DN and optionally the base self
DWORD
VmDirInternalDeleteTree(
    PCSTR   pBaseDN,
    BOOLEAN bInclusive
    )
{
    DWORD dwError = 0;
    int i = 0, j=0, iStop=0;
    PSTR* ppDNArray = NULL;
    PCSTR pDn = NULL;
    VDIR_ENTRY_ARRAY entryArray = {0};

    iStop = bInclusive ? 0 : 1;

    dwError = VmDirFilterInternalSearch(pBaseDN, LDAP_SCOPE_SUBTREE,
                "objectClass=*", 0, NULL, &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryArray.iSize < 1)
    {
        goto cleanup;
    }

    dwError = VmDirAllocateMemory(sizeof(PSTR)*entryArray.iSize, (PVOID*)&ppDNArray );
    BAIL_ON_VMDIR_ERROR(dwError);

    for (i = 0; i < entryArray.iSize; i++)
    {
        dwError = VmDirAllocateStringA(entryArray.pEntry[i].dn.lberbv_val, &(ppDNArray[i]));
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    qsort(ppDNArray, entryArray.iSize, sizeof(PSTR), VmDirCompareStrByLen);

    for (i=entryArray.iSize-1; i>=iStop; i--)
    {
       pDn = ppDNArray[i];
       for (j=0; j<entryArray.iSize; j++)
       {
           if (VmDirStringCompareA(pDn, entryArray.pEntry[j].dn.lberbv_val, FALSE)==0)
           {
               dwError = VmDirDeleteEntry(&entryArray.pEntry[j]);
               BAIL_ON_VMDIR_ERROR(dwError);
               break;
           }
       }
    }

cleanup:
    if (ppDNArray)
    {
        for (i = 0; i < entryArray.iSize; i++)
        {
            VMDIR_SAFE_FREE_MEMORY(ppDNArray[i]);
        }
        VMDIR_SAFE_FREE_MEMORY(ppDNArray);
    }
    VmDirFreeEntryArrayContent(&entryArray);
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirInternalGetDSERootServerCN(
    PSTR*   ppServerCN
    )
{
    DWORD   dwError = 0;
    PSTR    pszLocalServerCN = NULL;
    PVDIR_ENTRY     pEntry = NULL;
    PVDIR_ATTRIBUTE pAttrServerDN = NULL;

    if (!ppServerCN)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_PARAMETER);
    }

    dwError = VmDirSimpleDNToEntry(PERSISTED_DSE_ROOT_DN, &pEntry);
    BAIL_ON_VMDIR_ERROR(dwError);

    pAttrServerDN = VmDirEntryFindAttribute(ATTR_SERVER_NAME, pEntry);
    assert(pAttrServerDN);

    dwError = VmDirDnLastRDNToCn(pAttrServerDN->vals[0].lberbv_val, &pszLocalServerCN);
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppServerCN = pszLocalServerCN;

cleanup:
    VmDirFreeEntry(pEntry);

    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszLocalServerCN);

    goto cleanup;
}

DWORD
VmDirInternalSearchSeverObj(
    PCSTR               pszServerObjName,
    PVDIR_OPERATION     pSearchOp
    )
{
    DWORD           dwError = 0;
    PVDIR_FILTER    pSearchFilter = NULL;

    if (!pszServerObjName || !pSearchOp)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_PARAMETER);
    }

    pSearchOp->pBEIF = VmDirBackendSelect(NULL);
    assert(pSearchOp->pBEIF);

    pSearchOp->reqDn.lberbv.bv_val = "";
    pSearchOp->reqDn.lberbv.bv_len = 0;
    pSearchOp->request.searchReq.scope = LDAP_SCOPE_SUBTREE;

    dwError = VmDirConcatTwoFilters(
        pSearchOp->pSchemaCtx,
        ATTR_CN,
        (PSTR) pszServerObjName,
        ATTR_OBJECT_CLASS,
        OC_DIR_SERVER,
        &pSearchFilter);
    BAIL_ON_VMDIR_ERROR(dwError);

    pSearchOp->request.searchReq.filter = pSearchFilter;
    pSearchFilter = NULL;

    dwError = VmDirInternalSearch(pSearchOp);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (pSearchOp->internalSearchEntryArray.iSize != 1)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_STATE);
    }

cleanup:
    return dwError;

error:
    DeleteFilter(pSearchFilter);
    goto cleanup;
}

BOOLEAN
VmDirIsDeletedContainer(
    PCSTR   pszDN
    )
{
    BOOLEAN bRtn = FALSE;

    if (pszDN &&
        VmDirStringCompareA(pszDN, gVmdirServerGlobals.delObjsContainerDN.lberbv_val, FALSE) == 0)
    {
        bRtn = TRUE;
    }

    return bRtn;
}

BOOLEAN
VmDirIsTombStoneObject(
    PCSTR   pszDN
    )
{
    BOOLEAN bRtn = FALSE;

    if (pszDN &&
        VmDirStringEndsWith(pszDN, gVmdirServerGlobals.delObjsContainerDN.lberbv_val, FALSE))
    {
        bRtn = TRUE;
    }

    return bRtn;
}

// following gVmdirServerGlobals global variables must be initialized in a promoted server.
VOID
VmDirAssertServerGlobals(
    VOID
    )
{
    assert(gVmdirServerGlobals.systemDomainDN.lberbv_val);
    assert(gVmdirServerGlobals.bvDCGroupDN.lberbv_val);
    assert(gVmdirServerGlobals.bvDCClientGroupDN.lberbv_val);
    assert(gVmdirServerGlobals.bvSchemaManagersGroupDN.lberbv_val);
    assert(gVmdirServerGlobals.bvServicesRootDN.lberbv_val);
    assert(gVmdirServerGlobals.delObjsContainerDN.lberbv_val);
    assert(gVmdirServerGlobals.serverObjDN.lberbv_val);
    assert(gVmdirServerGlobals.bvServerObjName.lberbv_val);
    assert(gVmdirServerGlobals.dcAccountDN.lberbv_val);
    assert(gVmdirServerGlobals.dcAccountUPN.lberbv_val);
    assert(gVmdirServerGlobals.invocationId .lberbv_val);
    assert(gVmdirServerGlobals.bvDefaultAdminDN.lberbv_val);
    assert(gVmdirServerGlobals.pszSiteName);
    assert(gVmdirServerGlobals.dwDomainFunctionalLevel > 0);
}

/*
 * Copy specified attribute as a string
*/
DWORD
VmDirCopySingleAttributeString(
    PVDIR_ENTRY  pEntry,
    PCSTR        pszAttribute,
    BOOL         bOptional,
    PSTR*        ppszOut
    )
{
    DWORD   dwError = 0;
    PSTR   pszOut = NULL;
    PVDIR_ATTRIBUTE pAttr = NULL;

    if (!pEntry || IsNullOrEmptyString(pszAttribute) || !ppszOut)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_INVALID_PARAMETER);
    }

    for (pAttr = pEntry->attrs; pAttr; pAttr = pAttr->next)
    {
        if (VmDirStringCompareA(pAttr->pATDesc->pszName, pszAttribute, FALSE) == 0)
        {
            if (pAttr->vals[0].lberbv_len > 0)
            {
                dwError = VmDirAllocateStringA(
                        pAttr->vals[0].lberbv_val,
                        &pszOut);
                BAIL_ON_VMDIR_ERROR(dwError);
            }
            else if (!bOptional)
            {
                BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_NO_SUCH_ATTRIBUTE);
            }

            break;
        }
    }

    *ppszOut = pszOut;

cleanup:

    return dwError;

error:

    VMDIR_SAFE_FREE_MEMORY(pszOut);
    if (ppszOut)
    {
        *ppszOut = NULL;
    }
    goto cleanup;
}

/*
 * Fetch attribute value under dn.
 * Assumes single result and only operates on first result.
*/
DWORD
VmDirDNCopySingleAttributeString(
    PCSTR   pszDN,
    PCSTR   pszAttr,
    PSTR    *ppszAttrVal
    )
{
    DWORD dwError = 0;
    PSTR pszAttrVal = NULL;
    VDIR_ENTRY_ARRAY entryArray = {0};

    if (IsNullOrEmptyString(pszDN) ||
        IsNullOrEmptyString(pszAttr) ||
        !ppszAttrVal)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, ERROR_INVALID_PARAMETER);
    }

    dwError = VmDirFilterInternalSearch(
                  pszDN,
                  LDAP_SCOPE_BASE,
                  "objectClass=*",
                  0,
                  NULL,
                  &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryArray.iSize == 0)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, VMDIR_ERROR_ENTRY_NOT_FOUND);
    }

    dwError = VmDirCopySingleAttributeString(
                  &entryArray.pEntry[0],
                  pszAttr, FALSE,
                  &pszAttrVal);
    BAIL_ON_VMDIR_ERROR( dwError );

    *ppszAttrVal = pszAttrVal;

cleanup:
    VmDirFreeEntryArrayContent(&entryArray);
    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszAttrVal);
    goto cleanup;
}

DWORD
VmDirAllocateBerValueAVsnprintf(
    PVDIR_BERVALUE pbvValue,
    PCSTR pszFormat,
    ...
    )
{
    DWORD dwError = 0;
    PSTR pszValue = NULL;
    va_list args;

    va_start(args, pszFormat);
    dwError = VmDirVsnprintf(&pszValue, pszFormat, args);
    va_end(args);

    BAIL_ON_VMDIR_ERROR(dwError);

    pbvValue->lberbv_val = pszValue;
    pbvValue->lberbv_len = VmDirStringLenA(pszValue);
    pbvValue->bOwnBvVal = TRUE;

cleanup:
    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszValue);
    goto cleanup;
}

DWORD
VmDirFillMDBIteratorDataContent(
    PVOID    pKey,
    DWORD    dwKeySize,
    PVOID    pValue,
    DWORD    dwValueSize,
    PVMDIR_COMPACT_KV_PAIR    pMDBIteratorData
    )
{
    DWORD    dwError = 0;
    DWORD    dwSize = 0;
    PVOID    pKeyAndValue = NULL;

    if (pKey == NULL || dwKeySize == 0 || pMDBIteratorData == NULL)
    {
        BAIL_WITH_VMDIR_ERROR(dwError, ERROR_INVALID_PARAMETER);
    }

    assert(pMDBIteratorData->pKeyAndValue == NULL);

    dwSize = dwKeySize + dwValueSize;

    dwError = VmDirAllocateMemory(dwSize, &pKeyAndValue);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirCopyMemory(pKeyAndValue, dwSize, pKey, dwKeySize);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (dwValueSize)
    {
        dwError = VmDirCopyMemory(
                pKeyAndValue + dwKeySize, dwSize - dwKeySize, pValue, dwValueSize);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    pMDBIteratorData->pKeyAndValue = pKeyAndValue;
    pMDBIteratorData->dwKeySize = dwKeySize;
    pMDBIteratorData->dwValueSize = dwValueSize;
    pKeyAndValue = NULL;

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pKeyAndValue);
    return dwError;

error:
    VMDIR_LOG_ERROR(VMDIR_LOG_MASK_ALL, "failed error: %d", dwError);
    goto cleanup;
}

VOID
VmDirFreeMDBIteratorDataContents(
    PVMDIR_COMPACT_KV_PAIR    pMDBIteratorData
    )
{
    if (pMDBIteratorData)
    {
        pMDBIteratorData->dwKeySize = 0;
        pMDBIteratorData->dwValueSize = 0;
        VMDIR_SAFE_FREE_MEMORY(pMDBIteratorData->pKeyAndValue);
    }
}

VOID
VmDirResetPPolicyState(
    PVDIR_PPOLICY_STATE pPPolicyState
    )
{
    memset(pPPolicyState, 0, sizeof(*pPPolicyState));
    pPPolicyState->PPolicyError = PP_noError;
}
