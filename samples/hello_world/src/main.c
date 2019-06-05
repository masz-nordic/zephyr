/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

void main(void)
{
	LOG_ERR("Hello World!");
}
