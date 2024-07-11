/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/mbox.h>
#include <zephyr/sys/atomic.h>

#include "common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(host, LOG_LEVEL_INF);

static const struct mbox_dt_spec rx_channel = MBOX_DT_SPEC_GET(DT_PATH(mbox_consumer), rx);
static const struct mbox_dt_spec tx_channel = MBOX_DT_SPEC_GET(DT_PATH(mbox_consumer), tx);
static shared_t *rx_data = (shared_t *)((uint8_t *)(DT_REG_ADDR(DT_NODELABEL(sram_rx))));
static shared_t *tx_data = (shared_t *)((uint8_t *)(DT_REG_ADDR(DT_NODELABEL(sram_tx))));

data_packet_t data_packet;

/**
 * @brief Send data to aother core
 * @param buffer
 * @param size
 * @return -1 if lock is not acquired, otherwise how many items were transfered
 */
int send_to_remote(const unsigned char *const restrict buffer, const unsigned int size)
{
	/* Try and get lock */
	if (atomic_flag_test_and_set(&tx_data->lock)) {
		/* Return -1 in case lock is not acquired (used by other core)*/
		return -1;
	}

	/* Limit size of buffer to be transfered to the size of destine buffer */
	tx_data->size = (size <= DATA_SIZE) ? size : DATA_SIZE;

	/* Copy from buffer[] to tx_data->buffer[] */
	for (unsigned int i = 0; i < tx_data->size; i++) {
		tx_data->buffer[i] = buffer[i];
	}

	/* Clear lock */
	atomic_flag_clear(&tx_data->lock);

	if (mbox_send_dt(&tx_channel, NULL)) {
		/* Return -2 in case when signal could not be sent by mbox driver */
		return -2;
	}

	/* Return how many items were transfered */
	return tx_data->size;
}

/**
 * @brief Get data from another core
 * @param buffer
 * @param size
 * @return -1 if lock is not acquired, otherwise how many items were read
 */
int read_from_remote(unsigned char *const restrict buffer, unsigned int size)
{
	/* Try and get lock */
	if (atomic_flag_test_and_set(&rx_data->lock)) {
		/* Return -1 in case lock is not acquired (used by other core)*/
		return -1;
	}

	/* Verify whether we are trying to read more items than are available */
	if (size >= rx_data->size) {
		size = rx_data->size;
	}

	/* Copy items from rx_data->buffer[] to buffer[] */
	for (unsigned int i = 0; i < size; i++) {
		buffer[i] = rx_data->buffer[i];
	}

	/* Clear shared_data.buffer_size (there is no more data available) */
	rx_data->size = 0;

	/* Clear lock */
	atomic_flag_clear(&rx_data->lock);

	/* Return how many items were read */
	return size;
}

/**
 * @brief Verify whether another core has data ready for us to read
 * @return -1 if lock is not acquired, otherwise how many items are available for reading
 */
int remote_has_data(void)
{
	int n_items;

	/* Try and get lock */
	if (atomic_flag_test_and_set(&rx_data->lock)) {
		/* Return -1 in case lock is not acquired (used by other core)*/
		return -1;
	}

	n_items = (int)rx_data->size;

	/* Clear lock */
	atomic_flag_clear(&rx_data->lock);

	return n_items;
}

static void mbox_callback(const struct device *instance, uint32_t channel, void *user_data,
			  struct mbox_msg *msg_data)
{
	(void)user_data;
	(void)msg_data;

	int bytes_all = remote_has_data();

	if (bytes_all > 0) {
		int bytes_read = read_from_remote((unsigned char *const restrict)&data_packet,
						  sizeof(data_packet));
		if (bytes_read < 0) {
			LOG_ERR("Failed to read from remote");
		} else {
			LOG_INF("Read %d bytes from remote, received %d", bytes_read, bytes_all);
		}
	} else {
		LOG_INF("No data from remote");
	}
}

static int mbox_init(void)
{
	int ret;

	ret = mbox_register_callback_dt(&rx_channel, mbox_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Could not register callback (%d)", ret);
		return ret;
	}

	ret = mbox_set_enabled_dt(&rx_channel, true);
	if (ret < 0) {
		LOG_ERR("Could not enable RX channel %d (%d)", rx_channel.channel_id, ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	int bytes_sent = 0;

	int ret = mbox_init();
	if (ret < 0) {
		LOG_ERR("Could not init mbox (%d)", ret);
		return 0;
	}

	/* clear the buffer locks and their size holders */
	atomic_flag_clear(&rx_data->lock);
	rx_data->size = 0;

	/* Send signal to other core */
	ret = mbox_send_dt(&tx_channel, NULL);
	if (ret < 0) {
		printk("Could not send (%d)\n", ret);
		return 0;
	}

	k_busy_wait(100000);

	/* Send message to other core */
	while (bytes_sent <= 0) {
		bytes_sent = send_to_remote((const unsigned char *const restrict)&data_packet,
					    sizeof(data_packet));
		if (bytes_sent < 0) {
			LOG_ERR("Failed to send to host: %d", bytes_sent);
		} else {
			LOG_INF("Sent %d bytes to host", bytes_sent);
		}
	}

	k_busy_wait(100000);

	LOG_INF("Basic core-communication HOST demo ended");

	return 0;
}
