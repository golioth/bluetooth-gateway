/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#include <pouch/transport/ble_gatt/common/uuids.h>
#include <pouch/transport/ble_gatt/common/packetizer.h>

#include "bt.h"
#include "downlink.h"
#include "uplink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt);

#define BT_ATT_OVERHEAD 3 /* opcode (1) + handle (2) */

static struct bt_conn *default_conn;

static const struct bt_uuid_128 golioth_svc_uuid = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_SVC_VAL);
static const struct bt_uuid_128 golioth_info_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_INFO_CHRC_VAL);
static const struct bt_uuid_128 golioth_downlink_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_DOWNLINK_CHRC_VAL);
static const struct bt_uuid_128 golioth_uplink_chrc_uuid =
    BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_UPLINK_CHRC_VAL);

struct tf_svc_adv_data
{
    uint8_t sync_request;
} __packed;

struct tf_data
{
    bool is_tf;
    struct tf_svc_adv_data adv_data;
};

struct golioth_node_info
{
    struct
    {
        uint16_t info;
        uint16_t downlink;
        uint16_t uplink;
    } attr_handles;
    union
    {
        struct bt_gatt_discover_params discover_params;
        struct bt_gatt_read_params read_params;
        struct bt_gatt_write_params write_params;
    };
    struct pouch_uplink *uplink;
    struct downlink_context *downlink_ctx;
    void *downlink_scratch;
    struct golioth_ble_gatt_packetizer *packetizer;
};

static struct golioth_node_info connected_nodes[CONFIG_BT_MAX_CONN];

static bool data_cb(struct bt_data *data, void *user_data)
{
    struct tf_data *tf = user_data;

    switch (data->type)
    {
        case BT_DATA_SVC_DATA128:
        {
            struct tf_svc_adv_data *adv_data;

            if (data->data_len >= sizeof(golioth_svc_uuid.val) + sizeof(*adv_data)
                && memcmp(golioth_svc_uuid.val, data->data, sizeof(golioth_svc_uuid.val)) == 0)
            {
                adv_data = (void *) &data->data[sizeof(golioth_svc_uuid.val)];

                tf->is_tf = true;
                tf->adv_data = *adv_data;
            }
            else
            {
                return false;
            }
            return true;
        }
        default:
            return true;
    }
}

static void device_found(const bt_addr_le_t *addr,
                         int8_t rssi,
                         uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    struct tf_data tf = {
        .is_tf = false,
    };
    int err;

    /* We're only interested in connectable events */
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND)
    {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    LOG_DBG("Device found: %s (RSSI %d)", addr_str, rssi);

    bt_data_parse(ad, data_cb, &tf);
    LOG_DBG("is_tf=%d sync_request=%d", (int) tf.is_tf, (int) tf.adv_data.sync_request);

    if (tf.is_tf && tf.adv_data.sync_request)
    {
        err = bt_le_scan_stop();
        if (err)
        {
            LOG_ERR("Failed to stop scanning");
            return;
        }

        err = bt_conn_le_create(addr,
                                BT_CONN_LE_CREATE_CONN,
                                BT_LE_CONN_PARAM_DEFAULT,
                                &default_conn);
        if (err)
        {
            LOG_ERR("Create auto conn to failed (%d)", err);
            return;
        }
    }
}

static void start_scan(void)
{
    int err;

    /* This demo doesn't require active scan */
    err = bt_le_scan_start(BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_PASSIVE,
                                            BT_LE_SCAN_OPT_NONE,
                                            BT_GAP_SCAN_FAST_INTERVAL_MIN,
                                            BT_GAP_SCAN_FAST_WINDOW),
                           device_found);
    if (err)
    {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return;
    }

    LOG_INF("Scanning successfully started");
}

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
    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];
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

    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];

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

    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];

    int ret = write_downlink_characteristic(conn);
    if (0 != ret)
    {
        downlink_abort(node->downlink_ctx);
        golioth_ble_gatt_packetizer_finish(node->packetizer);

        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void start_downlink(struct bt_conn *conn)
{
    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];

    if (0 == node->attr_handles.downlink)
    {
        LOG_ERR("Downlink characteristic undiscovered");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    node->downlink_scratch = malloc(bt_gatt_get_mtu(conn) - BT_ATT_OVERHEAD);
    if (NULL == node->downlink_scratch)
    {
        LOG_ERR("Could not allocate space for downlink scratch buffer");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    node->downlink_ctx = downlink_start(NULL, downlink_data_available, conn);
    node->packetizer =
        golioth_ble_gatt_packetizer_start_callback(downlink_packet_fill_cb, node->downlink_ctx);
}

/* Callback for handling BLE GATT Uplink Response */
static uint8_t tf_uplink_read_cb(struct bt_conn *conn,
                                 uint8_t read_err,
                                 struct bt_gatt_read_params *params,
                                 const void *data,
                                 uint16_t length)
{
    if (read_err)
    {
        LOG_ERR("Failed to read BLE GATT Uplink (err %d)", read_err);
        return BT_GATT_ITER_STOP;
    }

    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];

    bool is_first = false;
    bool is_last = false;
    const void *payload = NULL;
    ssize_t payload_len =
        golioth_ble_gatt_packetizer_decode(data, length, &payload, &is_first, &is_last);
    if (payload_len < 0)
    {
        LOG_ERR("Failed to decode BLE GATT Uplink (err %d)", (int) payload_len);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (data)
    {
        LOG_HEXDUMP_INF(data, length, "[READ] BLE GATT Uplink");
    }

    int err = pouch_uplink_write(node->uplink, payload, payload_len, is_last);
    if (err)
    {
        LOG_ERR("Failed to write to pouch (err %d)", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (is_last)
    {
        start_downlink(conn);

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
                struct bt_gatt_read_params *read_params = &connected_nodes[conn_idx].read_params;
                memset(read_params, 0, sizeof(*read_params));

                read_params->func = tf_uplink_read_cb;
                read_params->handle_count = 1;
                read_params->single.handle = uplink_handle;
                int err = bt_gatt_read(conn, read_params);
                if (err)
                {
                    LOG_ERR("BT read request failed: %d", err);
                }
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

        start_scan();
        return;
    }

    LOG_INF("Connected: %s", addr);

    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];

    node->uplink = pouch_uplink_open();
    if (node->uplink == NULL)
    {
        LOG_ERR("Failed to open pouch uplink");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    memset(&node->attr_handles, 0, sizeof(node->attr_handles));

    struct bt_gatt_discover_params *discover_params = &node->discover_params;

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

    struct golioth_node_info *node = &connected_nodes[bt_conn_index(conn)];
    if (node->uplink)
    {
        pouch_uplink_close(node->uplink);
        node->uplink = NULL;
    }
    if (node->downlink_scratch)
    {
        free(node->downlink_scratch);
        node->downlink_scratch = NULL;
    }

    LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = bt_connected,
    .disconnected = bt_disconnected,
};

int bt_app_start(void)
{
    int err;

    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return 0;
    }

    LOG_INF("Bluetooth initialized");

    start_scan();

    return 0;
}
