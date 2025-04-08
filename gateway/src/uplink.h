#pragma once

#include <stdint.h>
#include <stddef.h>
#include <golioth/client.h>
#include <zephyr/sys/sflist.h>
#include <zephyr/sys/slist.h>

struct pouch_block;

struct pouch_uplink;

int pouch_uplink_write(struct pouch_uplink *uplink,
                       const uint8_t *payload,
                       size_t len,
                       bool is_last);

struct pouch_uplink *pouch_uplink_open(void);
void pouch_uplink_close(struct pouch_uplink *uplink);

void pouch_uplink_init(struct golioth_client *c);
