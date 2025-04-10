/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

struct golioth_client;
struct downlink_context;
typedef void (*downlink_data_available_cb)(void *);

void downlink_module_init(struct golioth_client *client);
struct downlink_context *downlink_start(const char *device_id,
                                        downlink_data_available_cb data_available_cb,
                                        void *arg);
void downlink_finish(struct downlink_context *downlink);
void downlink_abort(struct downlink_context *downlink);
int downlink_get_data(struct downlink_context *downlink, void *dst, size_t *dst_len, bool *is_last);
bool downlink_is_complete(const struct downlink_context *downlink);
