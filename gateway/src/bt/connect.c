/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "connect.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/util.h>

#include <pouch/transport/ble_gatt/common/uuids.h>

#include <gateway/bt/scan.h>

#include "cert.h"
#include "downlink.h"
#include "types.h"
#include "uplink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(connect);

static const struct bt_uuid_128 golioth_svc_uuid_128 =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_SVC_VAL_128);
static const struct bt_uuid_16 golioth_svc_uuid_16 =
    BT_UUID_INIT_16(GOLIOTH_BLE_GATT_UUID_SVC_VAL_16);
static const struct bt_uuid_128 char_uuids[GOLIOTH_GATT_ATTRS] = {
    [GOLIOTH_GATT_ATTR_INFO] = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_INFO_CHRC_VAL),
    [GOLIOTH_GATT_ATTR_DOWNLINK] = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_DOWNLINK_CHRC_VAL),
    [GOLIOTH_GATT_ATTR_UPLINK] = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_UPLINK_CHRC_VAL),
    [GOLIOTH_GATT_ATTR_SERVER_CERT] = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_SERVER_CERT_CHRC_VAL),
    [GOLIOTH_GATT_ATTR_DEVICE_CERT] = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_DEVICE_CERT_CHRC_VAL),
};
static const struct bt_uuid_16 gatt_ccc_uuid = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

BUILD_ASSERT(ARRAY_SIZE(char_uuids) == GOLIOTH_GATT_ATTRS,
             "Missing characteristic UUID definitions");

static struct golioth_node_info connected_nodes[CONFIG_BT_MAX_CONN];

static uint8_t discover_descriptors(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    struct bt_gatt_discover_params *params)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (attr)
    {
        int attr_idx = GOLIOTH_GATT_ATTRS;
        /* Find the value handle closest to but lower than this handle */
        for (int i = 0; i < GOLIOTH_GATT_ATTRS; i++)
        {
            if (node->attr_handles[i].value < attr->handle)
            {
                if (attr_idx == GOLIOTH_GATT_ATTRS)
                {
                    attr_idx = i;
                }
                else if (node->attr_handles[i].value > node->attr_handles[attr_idx].value)
                {
                    attr_idx = i;
                }
            }
        }

        if (attr_idx != GOLIOTH_GATT_ATTRS)
        {
            node->attr_handles[attr_idx].ccc = attr->handle;
            LOG_DBG("Found CCC descriptor handle %d for value handle %d",
                    node->attr_handles[attr_idx].ccc,
                    node->attr_handles[attr_idx].value);
        }

        return BT_GATT_ITER_CONTINUE;
    }

    if (node->attr_handles[GOLIOTH_GATT_ATTR_SERVER_CERT].value
        && node->attr_handles[GOLIOTH_GATT_ATTR_DEVICE_CERT].value)
    {
        gateway_cert_exchange_start(conn);
    }
    else
    {
        LOG_WRN("Could not discover %s characteristics", "certificate");
        LOG_INF("Starting uplink without cert exchange");
        gateway_uplink_start(conn);
    }

    return BT_GATT_ITER_STOP;
}

static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *params)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (attr)
    {
        struct bt_gatt_chrc *chrc = attr->user_data;
        for (int i = 0; i < GOLIOTH_GATT_ATTRS; i++)
        {
            if (0 == bt_uuid_cmp(chrc->uuid, &char_uuids[i].uuid))
            {
                node->attr_handles[i].value = chrc->value_handle;
                return BT_GATT_ITER_CONTINUE;
            }
        }

        LOG_WRN("Discovered Unknown characteristic: %d", chrc->value_handle);
        return BT_GATT_ITER_CONTINUE;
    }

    if (!node->attr_handles[GOLIOTH_GATT_ATTR_UPLINK].value
        || !node->attr_handles[GOLIOTH_GATT_ATTR_DOWNLINK].value)
    {
        LOG_ERR("Could not discover %s characteristics", "pouch");
        (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    params->start_handle = params->end_handle;
    for (int i = 0; i < GOLIOTH_GATT_ATTRS; i++)
    {
        if (node->attr_handles[i].value < params->start_handle)
        {
            params->start_handle = node->attr_handles[i].value;
        }
    }

    /* Descriptors start after the value handle (i.e. 2 after the characteristic handle) */
    params->start_handle += 2;
    params->func = discover_descriptors;
    params->type = BT_GATT_DISCOVER_DESCRIPTOR;
    params->uuid = &gatt_ccc_uuid.uuid;

    int err = bt_gatt_discover(conn, params);
    if (err)
    {
        LOG_ERR("Error discovering descriptors: %d", err);
        (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    return BT_GATT_ITER_STOP;
}

static uint8_t discover_services(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 struct bt_gatt_discover_params *params)
{
    if (!attr)
    {
        if (params->uuid == &golioth_svc_uuid_16.uuid)
        {
            LOG_DBG("Could not find 16-bit UUID, beginning search for 128-bit");
            params->uuid = &golioth_svc_uuid_128.uuid;

            int err = bt_gatt_discover(conn, params);
            if (err)
            {
                LOG_ERR("Failed to start discovery: %d", err);
                (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            }
        }
        else
        {
            LOG_ERR("Missing pouch service");
            (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        return BT_GATT_ITER_STOP;
    }

    struct bt_gatt_service_val *svc = attr->user_data;

    if (0 == bt_uuid_cmp(&golioth_svc_uuid_16.uuid, svc->uuid)
        || 0 == bt_uuid_cmp(&golioth_svc_uuid_128.uuid, svc->uuid))
    {
        params->func = discover_characteristics;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        params->start_handle = attr->handle + 1;
        params->end_handle = svc->end_handle;
        params->uuid = NULL;

        int err = bt_gatt_discover(conn, params);
        if (err)
        {
            LOG_ERR("Error discovering characteristics: %d", err);
            (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

static void bt_connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err)
    {
        LOG_ERR("Failed to connect to %s %u %s", addr, err, bt_hci_err_to_str(err));

        bt_conn_unref(conn);

        gateway_scan_start();
        return;
    }

    LOG_INF("Connected: %s", addr);

    uint8_t conn_idx = bt_conn_index(conn);
    memset(&connected_nodes[conn_idx], 0, sizeof(connected_nodes[conn_idx]));

    struct bt_gatt_discover_params *discover_params = &connected_nodes[conn_idx].discover_params;

    discover_params->func = discover_services;
    discover_params->type = BT_GATT_DISCOVER_PRIMARY;
    discover_params->start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params->uuid = &golioth_svc_uuid_16.uuid;

    err = bt_gatt_discover(conn, discover_params);
    if (err)
    {
        LOG_ERR("Failed to start discovery: %d", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

    gateway_uplink_cleanup(conn);
    gateway_downlink_cleanup(conn);

    bt_conn_unref(conn);

    gateway_scan_start();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected,
};

struct golioth_node_info *get_node_info(const struct bt_conn *conn)
{
    return &connected_nodes[bt_conn_index(conn)];
}
