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

int main(VOID)
{
    int ret = 0;

    const struct CMUnitTest LwCASrvJSONUtils_Tests[] =
    {
        cmocka_unit_test_setup_teardown(LwCAJsonLoadObjectFromFile_ValidInput, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonLoadObjectFromFile_InvalidInput, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetObjectFromKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetObjectFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetStringFromKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetStringFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetStringArrayFromKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetStringArrayFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetTimeFromKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetTimeFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetIntegerFromKey_PositiveValid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetIntegerFromKey_NegativeValid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetIntegerFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetUnsignedIntegerFromKey_PositiveValid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetUnsignedIntegerFromKey_NegativeValid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetUnsignedIntegerFromKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetBooleanFromKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAJsonGetBooleanFromKey_Invalid, NULL, NULL),
    };

    const struct CMUnitTest LwCASrvCAAPIs_Tests[] =
    {
        cmocka_unit_test_setup_teardown(Test_LwCACreateCertificate_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACreateCertificate_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACreateCertArray_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACreateCertArray_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACreateKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACreateKey_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACopyCertArray_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACopyCertArray_Invalid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACopyKey_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(Test_LwCACopyKey_Invalid, NULL, NULL),
    };

    const struct CMUnitTest LwCAAuthToken_Tests[] =
    {
        cmocka_unit_test_setup_teardown(LwCAAuthTokenGetHOTK_Valid, NULL, NULL),
        cmocka_unit_test_setup_teardown(LwCAAuthTokenGetHOTK_InvalidInput, NULL, NULL),
    };

    const struct CMUnitTest LwCASrvInit_Valid_Tests[] =
    {
        cmocka_unit_test(Test_LwCAGetRootCAId_Valid),
        cmocka_unit_test(Test_LwCAGetCAEndpoint_Valid),
    };

    const struct CMUnitTest LwCASrvInit_Invalid_Tests[] =
    {
        cmocka_unit_test(Test_LwCAGetRootCAId_Invalid),
        cmocka_unit_test(Test_LwCAGetCAEndpoint_Invalid),
    };

    ret = cmocka_run_group_tests_name("MutentCA server common JSON Util Tests", LwCASrvJSONUtils_Tests, NULL, NULL);
    if (ret)
    {
        fail_msg("%s", "MutentCA server common JSON util tests failed");
    }

    ret = cmocka_run_group_tests_name("MutentCA server common CA API Tests", LwCASrvCAAPIs_Tests, NULL, NULL);
    if (ret)
    {
        fail_msg("%s", "MutentCA server common CA API tests failed");
    }

    ret = cmocka_run_group_tests_name("MutentCA server common Auth Token Tests", LwCAAuthToken_Tests, NULL, NULL);
    if (ret)
    {
        fail_msg("%s", "MutentCA server common Auth Token tests failed");
    }

    ret = cmocka_run_group_tests_name("MutentCA server common init tests", LwCASrvInit_Valid_Tests, Test_LwCASrvInitTests_Valid_Setup, Test_LwCASrvInitTests_Valid_Teardown);
    if (ret)
    {
        fail_msg("%s", "MutentCA server common init tests failed");
    }

    ret = cmocka_run_group_tests_name("MutentCA server common init tests", LwCASrvInit_Invalid_Tests, Test_LwCASrvInitTests_Invalid_Setup, Test_LwCASrvInitTests_Invalid_Teardown);
    if (ret)
    {
        fail_msg("%s", "MutentCA server common init tests failed");
    }

    return 0;
}
