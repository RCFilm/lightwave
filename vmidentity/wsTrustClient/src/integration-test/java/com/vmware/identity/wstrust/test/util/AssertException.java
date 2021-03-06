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
package com.vmware.identity.wstrust.test.util;


/**
 * AssertException is used by {@link Assert} class to indicate an assertion
 * failure. It should not be explicitly caught.
 */
public class AssertException extends RuntimeException
{

   public AssertException(String message)
   {
      super(message);
   }

}
