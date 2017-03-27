/*
 *  (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License. You may obtain
 *  a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */

#include <unistd.h>
#include "openvswitch/vlog.h"
#include "netdev-provider.h"
#include "ofproto/ofproto-provider.h"
#include "netdev-sim.h"
#include "ofproto-sim-provider.h"
#include "eventlog.h"
#include "sim-copp-plugin.h"
#include "ops-classifier-sim.h"
#include "sim-stp.h"

#define init libovs_sim_plugin_LTX_init
#define run libovs_sim_plugin_LTX_run
#define wait libovs_sim_plugin_LTX_wait
#define destroy libovs_sim_plugin_LTX_destroy
#define netdev_register libovs_sim_plugin_LTX_netdev_register
#define ofproto_register libovs_sim_plugin_LTX_ofproto_register
#define SLEEP_INTERVAL 5

VLOG_DEFINE_THIS_MODULE(sim_plugin);

static void
robustServiceCmd(const char* serviceName, bool isStart)
{
    int retry = 10;
    char serviceCmd[MAX_CMD_LEN];
    char checkCmd[MAX_CMD_LEN];

    snprintf(serviceCmd, MAX_CMD_LEN, "systemctl %s %s",
             (isStart) ? "start" : "stop", serviceName);
    snprintf(checkCmd, MAX_CMD_LEN, "systemctl is-active %s", serviceName);

    while (retry > 0)
    {
       if (system(serviceCmd) != 0) {
            VLOG_ERR("[%u] Command \"%s\" returned non-zero code. Retry in %u sec",
                     (10 - retry), serviceCmd, SLEEP_INTERVAL);
            retry--;
            sleep(SLEEP_INTERVAL);
            /* As the start/stop cmd itself returned non-zero code no need to
               execute code below to check service status as obviously it was not changed */
            continue;
        }

        if (system(checkCmd) != 0)
        {
            if (!isStart) break;
        }
        else if (isStart) break;

        VLOG_ERR("[%u] Command \"%s\" has no effect. Retry in %u sec",
                 (10 - retry), serviceCmd, SLEEP_INTERVAL);
        retry--;
        sleep(SLEEP_INTERVAL);
    }
}

void
init(void)
{
    int retval;
    char cmd_str[MAX_CMD_LEN];
    bool start = true;
    bool stop = false;

    /* Event log initialization for sFlow */
    retval = event_log_init("SFLOW");
    if(retval < 0) {
        VLOG_ERR("Event log initialization failed for SFLOW");
    }

    memset(cmd_str, 0, sizeof(cmd_str));
    /* Cleaning up the Internal "ASIC" OVS everytime ops-switchd daemon is
    * started or restarted or killed to keep the "ASIC" OVS database in sync
    * with the OpenSwitch OVS database.
    * Here, we initially stop the "ASIC" ovs-vswitchd-sim and ovsdb-server
    * daemons, then delete the "ASIC" database file and start the
    * ovsdb-server again which recreates the "ASIC" database file and finally
    * start the ovs-vswitchd-sim daemon before ops-switchd daemon gets
    * restarted.*/
    robustServiceCmd("openvswitch-sim", stop);
    robustServiceCmd("ovsdb-server-sim", stop);

    if (access(ASIC_OVSDB_PATH, F_OK) != -1) {
        snprintf(cmd_str, MAX_CMD_LEN, "sudo rm -rf %s", ASIC_OVSDB_PATH);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to delete Internal 'ASIC' OVS ovsdb.db file");
        }
    } else {
        VLOG_DBG("Internal 'ASIC' OVS ovsdb.db file does not exist");
    }

    robustServiceCmd("openvswitch-sim", start);
    robustServiceCmd("ovsdb-server-sim", start);

    register_qos_extension();
    sim_copp_init();
    /* Register ASIC plugins */
    register_asic_plugins();

    /* Initialize classifier debug */
    classifier_sim_init();

    /* register STP plugin */
    register_stp_plugins();
}

void
run(void)
{
}

void
wait(void)
{
}

void
destroy(void)
{
}

void
netdev_register(void)
{
    netdev_sim_register();
}

void
ofproto_register(void)
{
    ofproto_class_register(&ofproto_sim_provider_class);
}
