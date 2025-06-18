/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <hal/nrf_gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

int main(void)
{
	nrf_gpio_pin_dir_set(NRF_GPIO_PIN_MAP(9, 0), NRF_GPIO_PIN_DIR_OUTPUT);
	while (1) {
		nrf_gpio_pin_toggle(NRF_GPIO_PIN_MAP(9, 0));
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
