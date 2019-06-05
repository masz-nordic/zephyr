/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LOG_BACKEND_RAW_UART_H_
#define LOG_BACKEND_RAW_UART_H_

#include <logging/log_backend.h>

extern const struct log_backend_api log_backend_raw_uart_api;

/**
 * @brief raw UART backend definition
 *
 * @param _name Name of the instance.
 */
#define LOG_BACKEND_RAW_UART_DEFINE(_name) \
	LOG_BACKEND_DEFINE(_name, log_backend_raw_uart_api, true)

#ifdef __cplusplus
}
#endif

void log_backend_raw_uart_init(void);

#endif /* LOG_BACKEND_RAW_UART_H_ */
