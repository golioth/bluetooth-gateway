/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <pouch/transport/ble_gatt/common/uuids.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(scan);

struct tf_svc_adv_data
{
    uint8_t sync_request;
} __packed;

struct tf_data
{
    bool is_tf;
    struct tf_svc_adv_data adv_data;
};

static struct bt_conn *default_conn;

static const struct bt_uuid_128 golioth_svc_uuid = BT_UUID_INIT_128(GOLIOTH_BLE_GATT_UUID_SVC_VAL);

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

void gateway_scan_start(void)
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
