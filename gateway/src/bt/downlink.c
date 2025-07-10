/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <pouch/transport/ble_gatt/common/packetizer.h>

#include "connect.h"
#include "types.h"

#include "../downlink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(downlink_gatt);

static void write_response_cb(struct bt_conn *conn,
                              uint8_t err,
                              struct bt_gatt_write_params *params);

static enum golioth_ble_gatt_packetizer_result downlink_packet_fill_cb(void *dst,
                                                                       size_t *dst_len,
                                                                       void *user_arg)
{
    bool last = false;

    int ret = downlink_get_data(user_arg, dst, dst_len, &last);
    if (-EAGAIN == ret)
    {
        LOG_DBG("Awaiting additional downlink data from cloud");
        return GOLIOTH_BLE_GATT_PACKETIZER_MORE_DATA;
    }
    if (0 > ret)
    {
        *dst_len = 0;
        return GOLIOTH_BLE_GATT_PACKETIZER_ERROR;
    }

    return last ? GOLIOTH_BLE_GATT_PACKETIZER_NO_MORE_DATA : GOLIOTH_BLE_GATT_PACKETIZER_MORE_DATA;
}

static int write_downlink_characteristic(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);
    struct bt_gatt_write_params *params = &node->write_params;
    uint16_t downlink_handle = node->attr_handles.downlink;

    size_t len = bt_gatt_get_mtu(conn) - BT_ATT_OVERHEAD;
    enum golioth_ble_gatt_packetizer_result ret =
        golioth_ble_gatt_packetizer_get(node->packetizer, node->downlink_scratch, &len);

    if (GOLIOTH_BLE_GATT_PACKETIZER_ERROR == ret)
    {
        ret = golioth_ble_gatt_packetizer_error(node->packetizer);
        LOG_ERR("Error getting downlink data %d", ret);
        return ret;
    }

    params->func = write_response_cb;
    params->handle = downlink_handle;
    params->offset = 0;
    params->data = node->downlink_scratch;
    params->length = len;

    LOG_INF("Writing %d bytes to handle %d", params->length, params->handle);

    int res = bt_gatt_write(conn, params);
    if (0 > res)
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    return 0;
}

static void write_response_cb(struct bt_conn *conn,
                              uint8_t err,
                              struct bt_gatt_write_params *params)
{
    LOG_INF("Received write response: %d", err);

    struct golioth_node_info *node = get_node_info(conn);

    if (downlink_is_complete(node->downlink_ctx))
    {
        downlink_finish(node->downlink_ctx);
        golioth_ble_gatt_packetizer_finish(node->packetizer);

        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    else
    {
        int ret = write_downlink_characteristic(conn);
        if (0 != ret)
        {
            downlink_abort(node->downlink_ctx);
            golioth_ble_gatt_packetizer_finish(node->packetizer);

            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
    }
}

static void downlink_data_available(void *arg)
{
    struct bt_conn *conn = arg;

    struct golioth_node_info *node = get_node_info(conn);

    int ret = write_downlink_characteristic(conn);
    if (0 != ret)
    {
        downlink_abort(node->downlink_ctx);
        golioth_ble_gatt_packetizer_finish(node->packetizer);

        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

struct downlink_context *gateway_downlink_start(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (0 == node->attr_handles.downlink)
    {
        LOG_ERR("Downlink characteristic undiscovered");
        return NULL;
    }

    if (!IS_ENABLED(CONFIG_GATEWAY_CLOUD))
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return NULL;
    }

    node->downlink_scratch = malloc(bt_gatt_get_mtu(conn) - BT_ATT_OVERHEAD);
    if (NULL == node->downlink_scratch)
    {
        LOG_ERR("Could not allocate space for downlink scratch buffer");
        return NULL;
    }

    node->downlink_ctx = downlink_init(downlink_data_available, conn);
    node->packetizer =
        golioth_ble_gatt_packetizer_start_callback(downlink_packet_fill_cb, node->downlink_ctx);

    return node->downlink_ctx;
}

void gateway_downlink_cleanup(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (node->downlink_scratch)
    {
        free(node->downlink_scratch);
        node->downlink_scratch = NULL;
    }
}
