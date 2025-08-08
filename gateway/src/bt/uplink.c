/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <pouch/transport/ble_gatt/common/packetizer.h>

#include "connect.h"
#include "downlink.h"
#include "types.h"

#include "../uplink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uplink_gatt);

/* Callback for handling BLE GATT Uplink Response */
static uint8_t tf_uplink_read_cb(struct bt_conn *conn,
                                 uint8_t err,
                                 struct bt_gatt_read_params *params,
                                 const void *data,
                                 uint16_t length)
{
    if (err)
    {
        LOG_ERR("Failed to read BLE GATT %s (err %d)", "Uplink", err);
        return BT_GATT_ITER_STOP;
    }

    bool is_first = false;
    bool is_last = false;
    const void *payload = NULL;
    ssize_t payload_len =
        golioth_ble_gatt_packetizer_decode(data, length, &payload, &is_first, &is_last);
    if (payload_len < 0)
    {
        LOG_ERR("Failed to decode BLE GATT %s (err %d)", "Uplink", (int) payload_len);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (data)
    {
        LOG_HEXDUMP_INF(data, length, "[READ] BLE GATT Uplink");
    }

    struct golioth_node_info *node = get_node_info(conn);

    int ret = pouch_uplink_write(node->uplink, payload, payload_len, is_last);
    if (ret)
    {
        LOG_ERR("Failed to write to pouch (err %d)", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (is_last)
    {
        pouch_uplink_close(node->uplink);
        node->uplink = NULL;

        return BT_GATT_ITER_STOP;
    }

    err = bt_gatt_read(conn, params);
    if (err)
    {
        LOG_ERR("BT (re)read request failed: %d", err);
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

void gateway_uplink_start(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    struct downlink_context *downlink = gateway_downlink_start(conn);

    node->uplink = pouch_uplink_open(downlink);
    if (node->uplink == NULL)
    {
        LOG_ERR("Failed to open pouch uplink");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    struct bt_gatt_read_params *read_params = &node->read_params;
    memset(read_params, 0, sizeof(*read_params));

    read_params->func = tf_uplink_read_cb;
    read_params->handle_count = 1;
    read_params->single.handle = node->attr_handles[GOLIOTH_GATT_ATTR_UPLINK];
    int err = bt_gatt_read(conn, read_params);
    if (err)
    {
        LOG_ERR("BT read request failed: %d", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

void gateway_uplink_cleanup(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (node->uplink)
    {
        pouch_uplink_close(node->uplink);
        node->uplink = NULL;
    }
}
