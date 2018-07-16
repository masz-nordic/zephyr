/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <logging/log_frontend.h>
#include <hal/nrf_gpio.h>

#if (CONFIG_LOG_FRONTEND_PORT_NUMBER == 0)
	#define LOG_FRONTEND_GPIO_REG_PTR NRF_P0
#elif (CONFIG_LOG_FRONTEND_PORT_NUMBER == 1)
	#define LOG_FRONTEND_GPIO_REG_PTR NRF_P1
#endif

_Static_assert((CONFIG_LOG_FRONTEND_PORT_NUMBER == 1) &&
	       (CONFIG_LOG_FRONTEND_LSB_PIN_NUMBER < 8),
	       "Pin number for least significant bit on port 1"
	       "must be less than 8");

#define MASK_OF_LOG_PINS (0x1ff << CONFIG_LOG_FRONTEND_LSB_PIN_NUMBER)
#define HEXDUMP_STR_BYTES 0xffffffff

union log_msg_ids_convert {
	struct log_msg_ids ids;
	u16_t raw;
};

void log_frontend_init()
{
	nrf_gpio_port_dir_output_set(LOG_FRONTEND_GPIO_REG_PTR,
				     MASK_OF_LOG_PINS);
}

static u32_t create_mask_gpio(u8_t message)
{
	u32_t mask = 0;

	mask = BIT(CONFIG_LOG_FRONTEND_LSB_PIN_NUMBER+8) |
	       (message << CONFIG_LOG_FRONTEND_LSB_PIN_NUMBER);

	return mask;
}

static void send_message_gpio_8b(u8_t message)
{
	u32_t mask_to_set = create_mask_gpio(message);

	nrf_gpio_port_out_clear(LOG_FRONTEND_GPIO_REG_PTR, MASK_OF_LOG_PINS);
	nrf_gpio_port_out_set(LOG_FRONTEND_GPIO_REG_PTR, mask_to_set);
}

static void send_message_gpio_16b(u16_t message)
{
	send_message_gpio_8b((u8_t)message);
	send_message_gpio_8b((u8_t)(message >> 8));
}

static void send_message_gpio_32b(u32_t message)
{
	send_message_gpio_16b((u16_t)message);
	send_message_gpio_16b((u16_t)(message >> 16));
}

void log_frontend_nrf_0(const char *str, struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	unsigned int key = irq_lock();

	send_message_gpio_32b((u32_t)str);
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}

void log_frontend_nrf_1(const char *str,
			u32_t arg0,
			struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	unsigned int key = irq_lock();

	send_message_gpio_32b((u32_t)str);
	send_message_gpio_32b(arg0);
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}

void log_frontend_nrf_2(const char *str,
			u32_t arg0,
			u32_t arg1,
			struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	unsigned int key = irq_lock();

	send_message_gpio_32b((u32_t)str);
	send_message_gpio_32b(arg0);
	send_message_gpio_32b(arg1);
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}

void log_frontend_nrf_3(const char *str,
			u32_t arg0,
			u32_t arg1,
			u32_t arg2,
			struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	unsigned int key = irq_lock();

	send_message_gpio_32b((u32_t)str);
	send_message_gpio_32b(arg0);
	send_message_gpio_32b(arg1);
	send_message_gpio_32b(arg2);
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}

void log_frontend_nrf_n(const char *str,
			u32_t *args,
			u32_t narg,
			struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	unsigned int key = irq_lock();

	send_message_gpio_32b((u32_t)str);
	for(int i=0; i<narg; i++) {
		send_message_gpio_32b(args[i]);
	}
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}

void log_frontend_nrf_hexdump(const u8_t *data,
		 	      u32_t length,
			      struct log_msg_ids src_level)
{
	union log_msg_ids_convert log_msg_ids_bytes;
	log_msg_ids_bytes.ids = src_level;

	if(length > CONFIG_LOG_FRONTEND_HEXDUMP_MAX_LENGTH) {
		length = CONFIG_LOG_FRONTEND_HEXDUMP_MAX_LENGTH;
	}

	unsigned int key = irq_lock();

	send_message_gpio_32b(HEXDUMP_STR_BYTES);
	send_message_gpio_32b(length);
	for(int i=0; i<length; i++) {
		send_message_gpio_8b(data[i]);
	}
	send_message_gpio_16b(log_msg_ids_bytes.raw);

	irq_unlock(key);
}
