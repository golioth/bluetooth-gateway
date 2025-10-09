/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

struct golioth_client;
struct pouch_gateway_device_cert_context;
struct pouch_gateway_server_cert_context;

#include <stdbool.h>
#include <stddef.h>

struct pouch_gateway_device_cert_context *pouch_gateway_device_cert_start(void);
int pouch_gateway_device_cert_push(struct pouch_gateway_device_cert_context *context,
                                   const void *data,
                                   size_t len);
void pouch_gateway_device_cert_abort(struct pouch_gateway_device_cert_context *context);
int pouch_gateway_device_cert_finish(struct pouch_gateway_device_cert_context *context);

struct pouch_gateway_server_cert_context *pouch_gateway_server_cert_start(void);
void pouch_gateway_server_cert_abort(struct pouch_gateway_server_cert_context *context);
bool pouch_gateway_server_cert_is_newest(const struct pouch_gateway_server_cert_context *context);
bool pouch_gateway_server_cert_is_complete(const struct pouch_gateway_server_cert_context *context);
int pouch_gateway_server_cert_get_data(struct pouch_gateway_server_cert_context *context,
                                       void *dst,
                                       size_t *dst_len,
                                       bool *is_last);

void pouch_gateway_cert_module_on_connected(struct golioth_client *client);
