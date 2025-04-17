/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define BT_ATT_OVERHEAD 3 /* opcode (1) + handle (2) */

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
    struct downlink_context *downlink_ctx;
    void *downlink_scratch;
    struct golioth_ble_gatt_packetizer *packetizer;
    struct pouch_uplink *uplink;
};
