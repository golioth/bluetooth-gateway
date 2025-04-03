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

#include <pouch/transport/ble_gatt/peripheral.h>

#include "bt.h"
#include "fifo.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt);

static struct bt_conn *default_conn;

static const uint8_t tf_svc_uuid[] = {GOLIOTH_BLE_GATT_UUID_SVC_VAL};

#define TF_PACKET_START 0x01
#define TF_PACKET_MORE 0x02
#define TF_PACKET_END 0x03

struct tf_packet
{
    uint8_t ctrl;
    uint8_t data[];
} __packed;

struct tf_svc_adv_data
{
    uint8_t sync_request;
} __packed;

struct tf_data
{
    bool is_tf;
    struct tf_svc_adv_data adv_data;
};

static bool data_cb(struct bt_data *data, void *user_data)
{
    struct tf_data *tf = user_data;

    switch (data->type)
    {
        case BT_DATA_SVC_DATA128:
        {
            struct tf_svc_adv_data *adv_data;

            if (data->data_len >= sizeof(tf_svc_uuid) + sizeof(*adv_data)
                && memcmp(tf_svc_uuid, data->data, sizeof(tf_svc_uuid)) == 0)
            {
                adv_data = (void *) &data->data[sizeof(tf_svc_uuid)];

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

static struct net_buf_simple *pouch = NET_BUF_SIMPLE(256);

static uint8_t tf_uplink_read_cb(struct bt_conn *conn,
                                 uint8_t err,
                                 struct bt_gatt_read_params *params,
                                 const void *data,
                                 uint16_t length);

#define TF_UUID_GOLIOTH_UPLINK_CHRC_VAL \
    BT_UUID_128_ENCODE(0x89a316ae, 0x89b7, 0x4ef6, 0xb1d3, 0x5c9a6e27d273)

static struct bt_gatt_read_params uplink_read_params = {
    .func = tf_uplink_read_cb,
    .by_uuid =
        {
            .uuid = BT_UUID_DECLARE_128(TF_UUID_GOLIOTH_UPLINK_CHRC_VAL),
            .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
            .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
        },
};

/* Callback for handling BLE GATT Uplink Response */
static uint8_t tf_uplink_read_cb(struct bt_conn *conn,
                                 uint8_t err,
                                 struct bt_gatt_read_params *params,
                                 const void *data,
                                 uint16_t length)
{
    const struct tf_packet *pkt = data;

    if (err)
    {
        LOG_ERR("Failed to read BLE GATT Uplink (err %d)", err);
        return BT_GATT_ITER_STOP;
    }

    if (data)
    {
        LOG_HEXDUMP_INF(data, length, "[READ] BLE GATT Uplink");
    }

    if (length < sizeof(*pkt))
    {
        return BT_GATT_ITER_STOP;
    }

    net_buf_simple_add_mem(pouch, pkt->data, length - sizeof(*pkt));

    if (pkt->ctrl == TF_PACKET_END)
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

        LOG_HEXDUMP_DBG(pouch->data, pouch->len, "pouch");

        struct pouch_fifo_item *item;

        item = malloc(sizeof(*item) + pouch->len);
        if (!item)
        {
            LOG_ERR("Failed to alloc new item");
            return BT_GATT_ITER_STOP;
        }

        memcpy(item->data, pouch->data, pouch->len);
        item->len = pouch->len;

        k_fifo_put(&pouches_fifo, item);

        return BT_GATT_ITER_STOP;
    }

    err = bt_gatt_read(conn, &uplink_read_params);
    if (err)
    {
        LOG_ERR("BT (re)read request failed: %d", err);
        return BT_GATT_ITER_STOP;
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

    net_buf_simple_init(pouch, 0);

    err = bt_gatt_read(conn, &uplink_read_params);
    if (err)
    {
        LOG_ERR("BT read request failed: %d", err);
        return;
    }
}

static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

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
