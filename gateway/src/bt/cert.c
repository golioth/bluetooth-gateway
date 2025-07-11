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

#include "cert.h"
#include "connect.h"
#include "uplink.h"

#include "../cert.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cert_gatt);

static void gateway_server_cert_write_start(struct bt_conn *conn);

static void write_response_cb(struct bt_conn *conn,
                              uint8_t err,
                              struct bt_gatt_write_params *params);

static void server_cert_cleanup(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (node->server_cert_scratch)
    {
        free(node->server_cert_scratch);
        node->server_cert_scratch = NULL;
    }

    if (node->server_cert_ctx)
    {
        server_cert_abort(node->server_cert_ctx);
        node->server_cert_ctx = NULL;
    }

    if (node->packetizer)
    {
        golioth_ble_gatt_packetizer_finish(node->packetizer);
        node->packetizer = NULL;
    }
}

static uint8_t server_cert_read_cb(struct bt_conn *conn,
                                   uint8_t err,
                                   struct bt_gatt_read_params *params,
                                   const void *data,
                                   uint16_t length)
{
    if (err)
    {
        LOG_ERR("Failed to read BLE GATT %s (err %d)", "server cert", err);
        return BT_GATT_ITER_STOP;
    }

    if (length == 0)
    {
        gateway_server_cert_write_start(conn);
        return BT_GATT_ITER_STOP;
    }

    bool is_first = false;
    bool is_last = false;
    const void *payload = NULL;
    ssize_t payload_len =
        golioth_ble_gatt_packetizer_decode(data, length, &payload, &is_first, &is_last);
    if (payload_len < 0)
    {
        LOG_ERR("Failed to decode BLE GATT %s (err %d)", "server cert", (int) payload_len);
        server_cert_cleanup(conn);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (data)
    {
        LOG_HEXDUMP_DBG(data, length, "[READ] BLE GATT server cert");
    }

    if (is_last)
    {
        /* TODO: if serial matches, then server_cert write can be ignored */
        gateway_server_cert_write_start(conn);
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

static int write_server_cert_characteristic(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);
    struct bt_gatt_write_params *params = &node->write_params;
    uint16_t server_cert_handle = node->attr_handles.server_cert;
    size_t len = bt_gatt_get_mtu(conn) - BT_ATT_OVERHEAD;
    enum golioth_ble_gatt_packetizer_result ret =
        golioth_ble_gatt_packetizer_get(node->packetizer, node->server_cert_scratch, &len);

    if (GOLIOTH_BLE_GATT_PACKETIZER_ERROR == ret)
    {
        ret = golioth_ble_gatt_packetizer_error(node->packetizer);
        LOG_ERR("Error getting %s data %d", "server cert", ret);
        return ret;
    }

    params->func = write_response_cb;
    params->handle = server_cert_handle;
    params->offset = 0;
    params->data = node->server_cert_scratch;
    params->length = len;

    LOG_HEXDUMP_DBG(node->server_cert_scratch, params->length, "server_cert write");
    LOG_DBG("Writing %d bytes to handle %d", params->length, params->handle);

    int res = bt_gatt_write(conn, params);
    if (0 > res)
    {
        server_cert_cleanup(conn);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    return 0;
}

static void write_response_cb(struct bt_conn *conn,
                              uint8_t err,
                              struct bt_gatt_write_params *params)
{
    LOG_DBG("Received write response: %d", err);

    struct golioth_node_info *node = get_node_info(conn);

    if (server_cert_is_complete(node->server_cert_ctx))
    {
        bool is_newest = server_cert_is_newest(node->server_cert_ctx);

        server_cert_cleanup(conn);

        if (is_newest)
        {
            gateway_uplink_start(conn);
        }
        else
        {
            // There was certificate update in the meantime, so send it once again.
            LOG_INF("Noticed certificate update, sending once again");
            gateway_server_cert_write_start(conn);
        }
    }
    else
    {
        int ret = write_server_cert_characteristic(conn);
        if (0 != ret)
        {
            server_cert_cleanup(conn);
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
    }
}

static enum golioth_ble_gatt_packetizer_result server_cert_fill_cb(void *dst,
                                                                   size_t *dst_len,
                                                                   void *user_arg)
{
    bool last = false;

    int ret = server_cert_get_data(user_arg, dst, dst_len, &last);
    if (-EAGAIN == ret)
    {
        LOG_DBG("Awaiting additional %s data from cloud", "server cert");
        return GOLIOTH_BLE_GATT_PACKETIZER_MORE_DATA;
    }
    if (ret < 0)
    {
        *dst_len = 0;
        return GOLIOTH_BLE_GATT_PACKETIZER_ERROR;
    }

    return last ? GOLIOTH_BLE_GATT_PACKETIZER_NO_MORE_DATA : GOLIOTH_BLE_GATT_PACKETIZER_MORE_DATA;
}

static void gateway_server_cert_write_start(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    if (0 == node->attr_handles.server_cert)
    {
        LOG_ERR("%s characteristic undiscovered", "server cert");
        server_cert_cleanup(conn);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    node->server_cert_scratch = malloc(bt_gatt_get_mtu(conn) - BT_ATT_OVERHEAD);
    if (NULL == node->server_cert_scratch)
    {
        LOG_ERR("Could not allocate space for %s scratch buffer", "server cert");
        server_cert_cleanup(conn);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    node->server_cert_ctx = server_cert_start();
    node->packetizer =
        golioth_ble_gatt_packetizer_start_callback(server_cert_fill_cb, node->server_cert_ctx);

    write_server_cert_characteristic(conn);
}

static void gateway_server_cert_serial_read_start(struct bt_conn *conn)
{
    struct golioth_node_info *node = get_node_info(conn);

    struct bt_gatt_read_params *read_params = &node->read_params;
    memset(read_params, 0, sizeof(*read_params));

    read_params->func = server_cert_read_cb;
    read_params->handle_count = 1;
    read_params->single.handle = node->attr_handles.server_cert;
    int err = bt_gatt_read(conn, read_params);
    if (err)
    {
        LOG_ERR("BT read request failed: %d", err);
        server_cert_cleanup(conn);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

void gateway_cert_exchange_start(struct bt_conn *conn)
{
    gateway_server_cert_serial_read_start(conn);
}
