/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FIFO_H__
#define __FIFO_H__

#include <zephyr/kernel.h>

extern struct k_fifo pouches_fifo;

struct pouch_fifo_item
{
    sys_sfnode_t node;
    size_t len;
    uint8_t data[];
};

#endif /* __FIFO_H__ */
