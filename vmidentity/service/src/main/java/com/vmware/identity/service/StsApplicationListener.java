/*
 *  Copyright (c) 2012-2015 VMware, Inc.  All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License.  You may obtain a copy
 *  of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, without
 *  warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */
package com.vmware.identity.service;

import javax.servlet.ServletContextEvent;
import javax.servlet.ServletContextListener;

import com.vmware.certificate.VMCAException;
import com.vmware.identity.diagnostics.DiagnosticsLoggerFactory;
import com.vmware.identity.diagnostics.IDiagnosticsLogger;
import com.vmware.identity.heartbeat.VmAfdHeartbeat;
import com.vmware.certificate.VMCAAdapter2;
import com.vmware.identity.idm.server.IdentityManager;

public class StsApplicationListener implements ServletContextListener {
    private static final int port = 443;
    private static final String serviceName = "Security Token Service";
    private static final IDiagnosticsLogger log = DiagnosticsLoggerFactory.getLogger(StsApplicationListener.class);

    private VmAfdHeartbeat heartbeat = new VmAfdHeartbeat(serviceName, port);
    private STSHealthChecker healthChecker = STSHealthChecker.getInstance();

    @Override
    public void contextInitialized(ServletContextEvent arg0) {
        try {
            heartbeat.startBeating();
            log.info("Heartbeat started for {} port {}", serviceName, port);

            VMCAAdapter2.VMCAInitOpenSSL();
            log.info("Starting health check thread");

            (new Thread(healthChecker)).start();
        } catch (VMCAException e) {
            log.error("Failed to init VMCA SSL library", e);
        } catch (Exception e) {
            log.error("Error when initializing context", e);
            throw new IllegalStateException(e);
        }
    }

    @Override
    public void contextDestroyed(ServletContextEvent arg0) {
        try {
            heartbeat.stopBeating();
            log.info("Heartbeat stopped for {} port {}", serviceName, port);

            IdentityManager.getIdmInstance().notifyAppStopping();
            log.info("Idm notified of app stop");

            VMCAAdapter2.VMCACleanupOpenSSL();
            log.info("VMCA cleaned up");

            healthChecker.stop();
            log.info("Stopped health check thread");
        } catch (VMCAException e) {
            log.error("Failed to cleanup VMCA SSL library", e);
        } catch (Exception e) {
            log.error("Error when destroying context", e);
        }
    }
}
