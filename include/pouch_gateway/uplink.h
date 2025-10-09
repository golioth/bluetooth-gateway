/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <golioth/client.h>
#include <zephyr/sys/sflist.h>
#include <zephyr/sys/slist.h>

#include <pouch_gateway/downlink.h>

struct pouch_block;

struct pouch_gateway_uplink;

int pouch_gateway_uplink_write(struct pouch_gateway_uplink *uplink,
                               const uint8_t *payload,
                               size_t len,
                               bool is_last);

struct pouch_gateway_uplink *pouch_gateway_uplink_open(
    struct pouch_gateway_downlink_context *downlink);
void pouch_gateway_uplink_close(struct pouch_gateway_uplink *uplink);

void pouch_gateway_uplink_init(struct golioth_client *c);
