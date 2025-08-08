/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <zephyr/bluetooth/gatt.h>

#define BT_ATT_OVERHEAD 3 /* opcode (1) + handle (2) */

enum golioth_gatt_attr
{
    GOLIOTH_GATT_ATTR_INFO,
    GOLIOTH_GATT_ATTR_DOWNLINK,
    GOLIOTH_GATT_ATTR_UPLINK,
    GOLIOTH_GATT_ATTR_SERVER_CERT,
    GOLIOTH_GATT_ATTR_DEVICE_CERT,

    GOLIOTH_GATT_ATTRS,
};

struct golioth_node_info
{
    uint16_t attr_handles[GOLIOTH_GATT_ATTRS];
    union
    {
        struct bt_gatt_discover_params discover_params;
        struct bt_gatt_read_params read_params;
        struct bt_gatt_write_params write_params;
    };
    struct downlink_context *downlink_ctx;
    void *downlink_scratch;
    void *server_cert_scratch;
    struct golioth_ble_gatt_packetizer *packetizer;
    struct pouch_uplink *uplink;
    struct device_cert_context *device_cert_ctx;
    struct server_cert_context *server_cert_ctx;
};
