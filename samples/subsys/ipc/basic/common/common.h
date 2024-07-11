/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdatomic.h>

#define PACKET_SIZE_START (40)
#define DATA_SIZE         (128)
#define SENDING_TIME_MS   (1000)

typedef struct __attribute__((packed)) {
	uint8_t some;
	uint16_t data;
	uint32_t to;
	uint32_t send;
	uint16_t trough;
	uint8_t ipc;
} data_packet_t;

typedef struct {
	atomic_bool lock;
	unsigned long size;
	unsigned char buffer[DATA_SIZE];
} shared_t;

#endif /* __COMMON_H__ */
