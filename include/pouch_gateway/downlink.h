/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <golioth/client.h>

struct pouch_gateway_downlink_context;
typedef void (*pouch_gateway_downlink_data_available_cb)(void *);

void pouch_gateway_downlink_module_init(struct golioth_client *client);
struct pouch_gateway_downlink_context *pouch_gateway_downlink_init(
    pouch_gateway_downlink_data_available_cb data_available_cb,
    void *arg);
void pouch_gateway_downlink_finish(struct pouch_gateway_downlink_context *downlink);
void pouch_gateway_downlink_abort(struct pouch_gateway_downlink_context *downlink);
int pouch_gateway_downlink_get_data(struct pouch_gateway_downlink_context *downlink,
                                    void *dst,
                                    size_t *dst_len,
                                    bool *is_last);
bool pouch_gateway_downlink_is_complete(const struct pouch_gateway_downlink_context *downlink);

enum golioth_status pouch_gateway_downlink_block_cb(const uint8_t *data,
                                                    size_t len,
                                                    bool is_last,
                                                    void *arg);
void pouch_gateway_downlink_end_cb(enum golioth_status status,
                                   const struct golioth_coap_rsp_code *coap_rsp_code,
                                   void *arg);
