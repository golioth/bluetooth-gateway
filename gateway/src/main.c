/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>

#include <golioth/client.h>
#include <golioth/gateway.h>
#include <samples/common/sample_credentials.h>

#include <pouch/transport/gatt/common/types.h>

#include <pouch_gateway/bt/connect.h>
#include <pouch_gateway/bt/scan.h>
#include <pouch_gateway/cert.h>
#include <pouch_gateway/downlink.h>
#include <pouch_gateway/uplink.h>

#include <git_describe.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

struct golioth_client *client;

#ifdef CONFIG_POUCH_GATEWAY_CLOUD

static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void connect_golioth_client(void)
{
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();
    if (client_config == NULL || client_config->credentials.psk.psk_id_len == 0
        || client_config->credentials.psk.psk_len == 0)
    {
        LOG_ERR("No credentials found.");
        LOG_ERR(
            "Please store your credentials with the following commands, then reboot the device.");
        LOG_ERR("\tsettings set golioth/psk-id <your-psk-id>");
        LOG_ERR("\tsettings set golioth/psk <your-psk>");
        return;
    }

    client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);
}

#ifdef CONFIG_NRF_MODEM
#include <modem/lte_lc.h>
static void lte_handler(const struct lte_lc_evt *const evt)
{
    if (evt->type == LTE_LC_EVT_NW_REG_STATUS)
    {

        if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME)
            || (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING))
        {

            if (!client)
            {
                connect_golioth_client();
            }
        }
    }
}
#endif /* CONFIG_NRF_MODEM */

#ifdef CONFIG_MODEM_HL7800
#define NET_MGMT_MASK (NET_EVENT_DNS_SERVER_ADD | NET_EVENT_L4_DISCONNECTED)
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/net_mgmt.h>

static K_SEM_DEFINE(network_connected, 0, 1);

static struct net_mgmt_event_callback cb;

static void net_mgmt_cb(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
    switch (event)
    {
        case NET_EVENT_DNS_SERVER_ADD:
            LOG_INF("Network connectivity established and IP address assigned");
            k_sem_give(&network_connected);
            break;
        case NET_EVENT_L4_DISCONNECTED:
            break;
        default:
            break;
    }
}

void wait_for_network(void)
{
    net_mgmt_init_event_callback(&cb, net_mgmt_cb, NET_MGMT_MASK);
    net_mgmt_add_event_callback(&cb);
    conn_mgr_mon_resend_status();

    LOG_INF("Waiting for network connection...");
    k_sem_take(&network_connected, K_FOREVER);
}
#endif /* CONFIG_MODEM_HL7800 */

static void connect_to_cloud(void)
{
#if defined(CONFIG_NRF_MODEM)
    LOG_INF("Connecting to LTE, this may take some time...");
    lte_lc_connect_async(lte_handler);
#else
#if defined(CONFIG_NET_L2_ETHERNET) && defined(CONFIG_NET_DHCPV4)
    net_dhcpv4_start(net_if_get_default());
#elif defined(CONFIG_MODEM_HL7800)
    wait_for_network();
#endif
    connect_golioth_client();
#endif
    k_sem_take(&connected, K_FOREVER);
}

#else /* CONFIG_POUCH_GATEWAY_CLOUD */

static inline void connect_to_cloud(void) {}

#endif /* CONFIG_POUCH_GATEWAY_CLOUD */

static void bt_connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err)
    {
        LOG_ERR("Failed to connect to %s %u %s", addr, err, bt_hci_err_to_str(err));

        bt_conn_unref(conn);

        pouch_gateway_scan_start();
        return;
    }

    LOG_INF("Connected: %s", addr);

    pouch_gateway_bt_start(conn);
}

static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

    pouch_gateway_bt_stop(conn);

    bt_conn_unref(conn);

    pouch_gateway_scan_start();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected,
};

void pouch_gateway_bt_finished(struct bt_conn *conn)
{
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

int main(void)
{
    LOG_INF("Gateway Version: " STRINGIFY(GIT_DESCRIBE));
    LOG_INF("Pouch BLE Transport Protocol Version: %d", POUCH_GATT_VERSION);

    connect_to_cloud();

    pouch_gateway_cert_module_on_connected(client);
    pouch_gateway_uplink_init(client);
    pouch_gateway_downlink_module_init(client);

    int err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth initialized");

    pouch_gateway_scan_start();

#ifdef CONFIG_POUCH_GATEWAY_CLOUD
    while (true)
    {
        k_sem_take(&connected, K_FOREVER);
        pouch_gateway_cert_module_on_connected(client);
    }
#endif

    return 0;
}
