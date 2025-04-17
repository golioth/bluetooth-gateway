/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <pouch/transport/ble_gatt/common/uuids.h>

#include <gateway/bt/scan.h>

#include "downlink.h"
#include "types.h"
#include "uplink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(connect);

static const struct bt_uuid_128 golioth_svc_uuid = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_SVC_VAL);
static const struct bt_uuid_128 golioth_info_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_INFO_CHRC_VAL);
static const struct bt_uuid_128 golioth_downlink_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_DOWNLINK_CHRC_VAL);
static const struct bt_uuid_128 golioth_uplink_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_UPLINK_CHRC_VAL);


static struct golioth_node_info connected_nodes[CONFIG_BT_MAX_CONN];

static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    uint8_t conn_idx = bt_conn_index(conn);

    if (NULL == attr)
    {
        if (BT_GATT_DISCOVER_CHARACTERISTIC == params->type)
        {
            uint16_t uplink_handle = connected_nodes[conn_idx].attr_handles.uplink;
            if (0 != uplink_handle)
            {
                gateway_uplink_start(conn);
            }
            else
            {
                LOG_ERR("Could not discover Uplink characteristic");
            }
        }

        return BT_GATT_ITER_STOP;
    }

    if (BT_GATT_DISCOVER_PRIMARY == params->type)
    {
        struct bt_gatt_service_val *svc = attr->user_data;

        if (0 == bt_uuid_cmp(&golioth_svc_uuid.uuid, svc->uuid))
        {
            params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
            params->start_handle = attr->handle + 1;
            params->end_handle = svc->end_handle;
            params->uuid = NULL;

            bt_gatt_discover(conn, params);

            return BT_GATT_ITER_STOP;
        }

        return BT_GATT_ITER_CONTINUE;
    }

    if (BT_GATT_DISCOVER_CHARACTERISTIC == params->type)
    {
        struct bt_gatt_chrc *chrc = attr->user_data;

        if (0 == bt_uuid_cmp(&golioth_info_chrc_uuid.uuid, chrc->uuid))
        {
            connected_nodes[conn_idx].attr_handles.info = chrc->value_handle;
        }
        else if (0 == bt_uuid_cmp(&golioth_downlink_chrc_uuid.uuid, chrc->uuid))
        {
            connected_nodes[conn_idx].attr_handles.downlink = chrc->value_handle;
        }
        else if (0 == bt_uuid_cmp(&golioth_uplink_chrc_uuid.uuid, chrc->uuid))
        {
            connected_nodes[conn_idx].attr_handles.uplink = chrc->value_handle;
        }
        else
        {
            LOG_WRN("Discovered Unknown characteristic: %d", chrc->value_handle);
        }

        return BT_GATT_ITER_CONTINUE;
    }

    return BT_GATT_ITER_STOP;
}

static void bt_connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err)
    {
        LOG_ERR("Failed to connect to %s %u %s", addr, err, bt_hci_err_to_str(err));

        gateway_scan_start();
        return;
    }

    LOG_INF("Connected: %s", addr);

    uint8_t conn_idx = bt_conn_index(conn);
    memset(&connected_nodes[conn_idx], 0, sizeof(connected_nodes[conn_idx]));

    struct bt_gatt_discover_params *discover_params = &connected_nodes[conn_idx].discover_params;

    discover_params->func = discover_func;
    discover_params->type = BT_GATT_DISCOVER_PRIMARY;
    discover_params->start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params->uuid = &golioth_svc_uuid.uuid;

    err = bt_gatt_discover(conn, discover_params);
    if (err)
    {
        LOG_ERR("Failed to start discovery: %d", err);
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
