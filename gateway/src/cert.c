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

static uint8_t server_crt_buf[CONFIG_GATEWAY_SERVER_CERT_MAX_LEN];
static atomic_t server_crt_len;
static atomic_t server_crt_id;

struct server_cert_context
{
    size_t id;
    size_t offset;
};

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

    if (IS_ENABLED(CONFIG_GATEWAY_CLOUD))
    {
        size_t len = sizeof(server_crt_buf);
        status = golioth_gateway_server_cert_get(client, server_crt_buf, &len, 5);
        if (status != GOLIOTH_OK)
        {
            LOG_ERR("Failed to download server certificate: %d", status);
        }

        server_crt_update(len);
    }

    LOG_HEXDUMP_DBG(server_crt_buf, atomic_get(&server_crt_len), "Server certificate");
}
