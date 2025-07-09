/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/kernel.h>

#include <golioth/gateway.h>

#include "block.h"

#include "downlink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(downlink);

struct downlink_context
{
    downlink_data_available_cb data_available_cb;
    void *cb_arg;
    struct k_fifo block_queue;
    struct block *current_block;
    size_t offset;
    bool complete;
    bool aborted;
};

static struct golioth_client *_client;

static void flush_block_queue(struct k_fifo *queue)
{
    struct block *block = k_fifo_get(queue, K_NO_WAIT);
    while (NULL != block)
    {
        block_free(block);

        block = k_fifo_get(queue, K_NO_WAIT);
    }
}

enum golioth_status downlink_block_cb(const uint8_t *data, size_t len, bool is_last, void *arg)
{
    struct downlink_context *downlink = arg;

    if (downlink->aborted)
    {
        flush_block_queue(&downlink->block_queue);
        downlink_finish(downlink);
        return GOLIOTH_ERR_NACK;
    }

    struct block *block = block_alloc(NULL, len);
    if (NULL == block)
    {
        LOG_ERR("Failed to allocate block");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    block_append(block, data, len);

    if (is_last)
    {
        block_mark_last(block);
    }
    k_fifo_put(&downlink->block_queue, block);

    if (NULL == downlink->current_block)
    {
        downlink->data_available_cb(downlink->cb_arg);
    }

    return GOLIOTH_OK;
}

void downlink_end_cb(enum golioth_status status,
                     const struct golioth_coap_rsp_code *coap_rsp_code,
                     void *arg)
{
    struct downlink_context *downlink = arg;

    if (GOLIOTH_OK != status)
    {
        /* TODO: Allow partial downlinks */

        downlink_abort(downlink);

        /* If transport is waiting for a block, kick it */

        if (NULL == downlink->current_block)
        {
            downlink->data_available_cb(downlink->cb_arg);
        }
    }
}

struct downlink_context *downlink_init(downlink_data_available_cb data_available_cb, void *cb_arg)
{
    LOG_INF("Starting downlink");

    struct downlink_context *downlink = malloc(sizeof(struct downlink_context));

    if (NULL != downlink)
    {
        downlink->data_available_cb = data_available_cb;
        downlink->cb_arg = cb_arg;
        downlink->current_block = NULL;
        downlink->offset = 0;
        downlink->complete = false;
        downlink->aborted = false;
        k_fifo_init(&downlink->block_queue);
    }

    return downlink;
}

int downlink_get_data(struct downlink_context *downlink, void *dst, size_t *dst_len, bool *is_last)
{
    *is_last = false;

    if (downlink->complete || downlink->aborted)
    {
        return -ENODATA;
    }

    size_t total_bytes_copied = 0;

    while (*dst_len)
    {
        if (NULL == downlink->current_block)
        {
            downlink->current_block = k_fifo_get(&downlink->block_queue, K_NO_WAIT);
            if (NULL == downlink->current_block)
            {
                *dst_len = total_bytes_copied;
                return -EAGAIN;
            }
        }

        size_t bytes_to_copy =
            MIN(*dst_len, block_length(downlink->current_block) - downlink->offset);
        block_get(downlink->current_block, downlink->offset, dst, bytes_to_copy);

        downlink->offset += bytes_to_copy;
        *dst_len -= bytes_to_copy;
        dst = (void *) ((intptr_t) dst + bytes_to_copy);
        total_bytes_copied += bytes_to_copy;

        if (block_length(downlink->current_block) == downlink->offset)
        {
            *is_last = block_is_last(downlink->current_block);

            block_free(downlink->current_block);
            downlink->offset = 0;
            downlink->current_block = NULL;

            if (*is_last)
            {
                downlink->complete = true;
                break;
            }
        }
    }

    *dst_len = total_bytes_copied;
    return 0;
}

bool downlink_is_complete(const struct downlink_context *downlink)
{
    return downlink->complete;
}

void downlink_finish(struct downlink_context *downlink)
{
    if (NULL != downlink->current_block)
    {
        block_free(downlink->current_block);
    }

    free(downlink);
}

void downlink_abort(struct downlink_context *downlink)
{
    /* Downlink will be aborted after the current in flight CoAP
       block request is completed. */

    downlink->aborted = true;

    /* If there are no more blocks, then just cleanup */

    if (downlink->complete)
    {
        downlink_finish(downlink);
    }
}

void downlink_module_init(struct golioth_client *client)
{
    _client = client;
}
