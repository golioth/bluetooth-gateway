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

/**
 * Write data to the uplink.
 *
 * @param uplink The uplink context.
 * @param payload The payload to write.
 * @param len The length of the payload.
 * @param is_last true if this is the last chunk.
 * @return 0 on success, negative on error.
 */
int pouch_gateway_uplink_write(struct pouch_gateway_uplink *uplink,
                               const uint8_t *payload,
                               size_t len,
                               bool is_last);

/**
 * Open an uplink for the given downlink context.
 *
 * @param downlink The downlink context.
 * @return Pointer to the uplink context.
 */
struct pouch_gateway_uplink *pouch_gateway_uplink_open(
    struct pouch_gateway_downlink_context *downlink);

/**
 * Close the uplink.
 *
 * @param uplink The uplink context.
 */
void pouch_gateway_uplink_close(struct pouch_gateway_uplink *uplink);

/**
 * Initialize the uplink module with the Golioth client.
 *
 * @param c The Golioth client.
 */
void pouch_gateway_uplink_init(struct golioth_client *c);
