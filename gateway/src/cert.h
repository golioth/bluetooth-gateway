/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

struct golioth_client;
struct device_cert_context;
struct server_cert_context;

#include <stdbool.h>
#include <stddef.h>

struct device_cert_context *device_cert_start(void);
int device_cert_push(struct device_cert_context *context, const void *data, size_t len);
void device_cert_abort(struct device_cert_context *context);
int device_cert_finish(struct device_cert_context *context);

struct server_cert_context *server_cert_start(void);
void server_cert_abort(struct server_cert_context *context);
bool server_cert_is_newest(const struct server_cert_context *context);
bool server_cert_is_complete(const struct server_cert_context *context);
int server_cert_get_data(struct server_cert_context *context,
                         void *dst,
                         size_t *dst_len,
                         bool *is_last);

void cert_module_on_connected(struct golioth_client *client);
