/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "cert.h"

#include <golioth/gateway.h>
#include <golioth/golioth_status.h>

#include <zephyr/sys/atomic_types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cert);

static struct golioth_client *_client;

static uint8_t server_crt_buf[CONFIG_GATEWAY_SERVER_CERT_MAX_LEN];
static atomic_t server_crt_len;
static atomic_t server_crt_id;

struct device_cert_context
{
    size_t len;
    uint8_t buf[CONFIG_GATEWAY_DEVICE_CERT_MAX_LEN];
};

struct server_cert_context
{
    size_t id;
    size_t offset;
};

struct device_cert_context *device_cert_start(void)
{
    struct device_cert_context *context = malloc(sizeof(struct device_cert_context));

    context->len = 0;

    return context;
}

int device_cert_push(struct device_cert_context *context, const void *data, size_t len)
{
    if (context->len + len > CONFIG_GATEWAY_DEVICE_CERT_MAX_LEN)
    {
        return -ENOSPC;
    }

    memcpy(&context->buf[context->len], data, len);
    context->len += len;

    return 0;
}

void device_cert_abort(struct device_cert_context *context)
{
    free(context);
}

int device_cert_finish(struct device_cert_context *context)
{
    enum golioth_status status;

    status = golioth_gateway_device_cert_set(_client, context->buf, context->len, 5);
    if (status != GOLIOTH_OK)
    {
        LOG_ERR("Failed to finish device cert: %d", status);
        return -EIO;
    }

    device_cert_abort(context);

    return 0;
}

struct server_cert_context *server_cert_start(void)
{
    struct server_cert_context *context = malloc(sizeof(struct server_cert_context));

    context->id = atomic_get(&server_crt_id);
    context->offset = 0;

    return context;
}

bool server_cert_is_newest(const struct server_cert_context *context)
{
    return context->id == atomic_get(&server_crt_id);
}

static void server_crt_update(size_t len)
{
    atomic_set(&server_crt_len, len);
    atomic_inc(&server_crt_id);
}

bool server_cert_is_complete(const struct server_cert_context *context)
{
    return context->offset >= atomic_get(&server_crt_len);
}

int server_cert_get_data(struct server_cert_context *context,
                         void *dst,
                         size_t *dst_len,
                         bool *is_last)
{
    size_t len = atomic_get(&server_crt_len);

    *is_last = false;

    if (context->offset >= len)
    {
        return -ENODATA;
    }

    if (*dst_len > len - context->offset)
    {
        *dst_len = len - context->offset;
    }

    memcpy(dst, &server_crt_buf[context->offset], *dst_len);
    context->offset += *dst_len;

    if (context->offset >= len)
    {
        *is_last = true;
    }

    return 0;
}

void server_cert_abort(struct server_cert_context *context)
{
    free(context);
}

void cert_module_on_connected(struct golioth_client *client)
{
    enum golioth_status status;

    _client = client;

    if (IS_ENABLED(CONFIG_GATEWAY_CLOUD))
    {
        size_t len = sizeof(server_crt_buf);
        status = golioth_gateway_server_cert_get(client, server_crt_buf, &len);
        if (status != GOLIOTH_OK)
        {
            LOG_ERR("Failed to download server certificate: %d", status);
            return;
        }

        server_crt_update(len);
    }
    else if (IS_ENABLED(CONFIG_GATEWAY_SERVER_CERT_BUILTIN))
    {
        static const uint8_t server_crt_offline[] = {
#include "server.der.inc"
        };

        memcpy(server_crt_buf, server_crt_offline, sizeof(server_crt_offline));
        server_crt_update(sizeof(server_crt_offline));

        LOG_INF("Loaded builtin server cert");
    }

    LOG_HEXDUMP_DBG(server_crt_buf, atomic_get(&server_crt_len), "Server certificate");
}
