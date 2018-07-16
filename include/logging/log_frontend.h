/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LOG_FRONTEND_NRF_H_
#define LOG_FRONTEND_NRF_H_

#include <logging/log_core.h>

void log_frontend_init();

/** @brief Standard log with no arguments.
 *
 * @param str           String.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_0(const char *str, struct log_msg_ids src_level);

/** @brief Standard log with one argument.
 *
 * @param str           String.
 * @param arg1	        First argument.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_1(const char *str,
			u32_t arg0,
			struct log_msg_ids src_level);

/** @brief Standard log with two arguments.
 *
 * @param str           String.
 * @param arg1	        First argument.
 * @param arg2	        Second argument.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_2(const char *str,
			u32_t arg0,
			u32_t arg1,
			struct log_msg_ids src_level);

/** @brief Standard log with three arguments.
 *
 * @param str           String.
 * @param arg1	        First argument.
 * @param arg2	        Second argument.
 * @param arg3	        Third argument.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_3(const char *str,
			u32_t arg0,
			u32_t arg1,
			u32_t arg2,
			struct log_msg_ids src_level);

/** @brief Standard log with arguments list.
 *
 * @param str		String.
 * @param args		Array with arguments.
 * @param narg		Number of arguments in the array.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_n(const char *str,
			u32_t *args,
			u32_t narg,
			struct log_msg_ids src_level);

/** @brief Hexdump log.
 *
 * @param data		Data.
 * @param length	Data length.
 * @param src_level	Log identification.
 */
void log_frontend_nrf_hexdump(const u8_t *data,
		 	      u32_t length,
			      struct log_msg_ids src_level);


#endif /* LOG_FRONTEND_NRF_H_ */
