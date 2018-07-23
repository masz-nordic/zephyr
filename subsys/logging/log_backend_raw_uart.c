/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <logging/log_backend_raw_uart.h>
#include <logging/log_core.h>
#include <logging/log_msg.h>
#include <logging/log_output.h>
#include <device.h>
#include <uart.h>

static void *ctx;

union log_msg_ids_convert {
	struct log_msg_ids ids;
	u16_t raw;
};

static void send_raw_std_uart(struct log_msg *msg, void *ctx)
{
	struct device *dev = (struct device *)ctx;
	u32_t str = (u32_t)log_msg_str_get(msg);
	u32_t arg;
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = msg->hdr.ids;

	uart_poll_out(dev, (u8_t)str);
	uart_poll_out(dev, (u8_t)(str >> 8));
	uart_poll_out(dev, (u8_t)(str >> 16));
	uart_poll_out(dev, (u8_t)(str >> 24));

	for(int i=0; i < log_msg_nargs_get(msg); i++) {
		arg = log_msg_arg_get(msg, i);
		uart_poll_out(dev, (u8_t)arg);
		uart_poll_out(dev, (u8_t)(arg >> 8));
		uart_poll_out(dev, (u8_t)(arg >> 16));
		uart_poll_out(dev, (u8_t)(arg >> 24));
	}

	uart_poll_out(dev, (u8_t)log_msg_ids_bytes.raw);
	uart_poll_out(dev, (u8_t)(log_msg_ids_bytes.raw >> 8));
}

static void send_raw_hexdump_uart(struct log_msg *msg, void *ctx)
{
	struct device *dev = (struct device *)ctx;
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = msg->hdr.ids;

	uart_poll_out(dev, 0xff);
	uart_poll_out(dev, 0xff);
	uart_poll_out(dev, 0xff);
	if(msg->hdr.params.hexdump.raw_string == 0) {
		uart_poll_out(dev, 0xff);
	} else {
		uart_poll_out(dev, 0xfe);
	}

	uart_poll_out(dev, (u8_t)msg->hdr.params.hexdump.length);
	uart_poll_out(dev, (u8_t)(msg->hdr.params.hexdump.length >> 8));
	uart_poll_out(dev, (u8_t)(msg->hdr.params.hexdump.length >> 16));
	uart_poll_out(dev, (u8_t)(msg->hdr.params.hexdump.length >> 24));

	for(int i=0; i < (msg->hdr.params.hexdump.length); i++) {;
		uart_poll_out(dev, msg->data.data.bytes[i]);
	}

	uart_poll_out(dev, (u8_t)log_msg_ids_bytes.raw);
	uart_poll_out(dev, (u8_t)(log_msg_ids_bytes.raw >> 8));
}

void log_send_raw_msg_uart(struct log_msg *msg, void *ctx)
{
	if (log_msg_is_std(msg)) {
		send_raw_std_uart(msg, ctx);
	} else {
		send_raw_hexdump_uart(msg, ctx);
	}
}

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);
	log_send_raw_msg_uart(msg, ctx);
	log_msg_put(msg);
}

void log_backend_raw_uart_init(void)
{
	ctx = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
}

static void panic(struct log_backend const *const backend)
{

}

const struct log_backend_api log_backend_raw_uart_api = {
	.put = put,
	.panic = panic,
};


