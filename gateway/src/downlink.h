/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

struct golioth_client;
struct downlink_context;
typedef void (*downlink_data_available_cb)(void *);

void downlink_module_init(struct golioth_client *client);
struct downlink_context *downlink_init(downlink_data_available_cb data_available_cb, void *arg);
void downlink_finish(struct downlink_context *downlink);
void downlink_abort(struct downlink_context *downlink);
int downlink_get_data(struct downlink_context *downlink, void *dst, size_t *dst_len, bool *is_last);
bool downlink_is_complete(const struct downlink_context *downlink);

enum golioth_status downlink_block_cb(const uint8_t *data, size_t len, bool is_last, void *arg);
void downlink_end_cb(enum golioth_status status,
                     const struct golioth_coap_rsp_code *coap_rsp_code,
                     void *arg);