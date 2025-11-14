/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pouch_gateway_info_context;

#define POUCH_GATEWAY_INFO_FLAG_DEVICE_PROVISIONED BIT(0)

struct pouch_gateway_info_context *pouch_gateway_info_start(void);
int pouch_gateway_info_push(struct pouch_gateway_info_context *context,
                            const void *data,
                            size_t len);
void pouch_gateway_info_abort(struct pouch_gateway_info_context *context);
int pouch_gateway_info_finish(struct pouch_gateway_info_context *context,
                              bool *server_cert_provisioned,
                              bool *device_cert_provisioned);
