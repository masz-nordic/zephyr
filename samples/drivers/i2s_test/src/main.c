/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <nrfx.h>
#include <device.h>
#include <i2s.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define I2S_DEV "I2S_0"

#define NB_OF_SAMPLES			256
#define NB_OF_CHANNELS			2
#define SINGLE_SAMPLE_SIZE_BYTES	2
#define FRAME_CLOCK_FREQUENCY_HZ	48000
#define WRITES_NUMBER			8


#define TRANSFER_BLOCK_TIME_MS		((BLOCK_SIZE_BYTES * WRITES_NUMBER * 1000) / (FRAME_CLOCK_FREQUENCY_HZ * NB_OF_CHANNELS * SINGLE_SAMPLE_SIZE_BYTES)) 
#if (SINGLE_SAMPLE_SIZE_BYTES > 3)
#error SINGLE_SAMPLE_SIZE_BYTES CAN NOT BE HIGHER THAN 3!
#else
#define BLOCK_SIZE_BYTES		(NB_OF_SAMPLES * NB_OF_CHANNELS * (1 << (SINGLE_SAMPLE_SIZE_BYTES - 1)))
#endif

K_MEM_SLAB_DEFINE(i2sBufferTx, BLOCK_SIZE_BYTES , CONFIG_NRFX_I2S_TX_BLOCK_COUNT, 4);

struct i2s_config i2sConfigTx = {
	.word_size = SINGLE_SAMPLE_SIZE_BYTES * 8,
	.channels = NB_OF_CHANNELS,
	.format = I2S_FMT_DATA_FORMAT_I2S,
	.options = 0,
	.frame_clk_freq = FRAME_CLOCK_FREQUENCY_HZ,
	.mem_slab = &i2sBufferTx,
	.block_size = BLOCK_SIZE_BYTES,
	.timeout = TRANSFER_BLOCK_TIME_MS
};

K_MEM_SLAB_DEFINE(i2sBufferRx, BLOCK_SIZE_BYTES, CONFIG_NRFX_I2S_RX_BLOCK_COUNT, 4);

struct i2s_config i2sConfigRx = {
	.word_size = SINGLE_SAMPLE_SIZE_BYTES * 8,
	.channels = NB_OF_CHANNELS,
	.format = I2S_FMT_DATA_FORMAT_I2S,
	.options = 0,
	.frame_clk_freq = FRAME_CLOCK_FREQUENCY_HZ,
	.mem_slab = &i2sBufferRx,
	.block_size = BLOCK_SIZE_BYTES,
	.timeout = TRANSFER_BLOCK_TIME_MS
};

#define PREPARE_BUFFER(buf, siz)				\
		{						\
			static u8_t current_val;		\
			u8_t *d = buf;				\
			for (u32_t i = 0; i < siz; i++) {	\
				d[i] = current_val++;		\
			}					\
		}

int i2s_basic(void)
{
	struct device *dev;
	int ret = 0;
	u8_t *my_tx_buf, *my_rx_buf;
	size_t rcv_size = 0;

	/* Configure */
	dev = device_get_binding(I2S_DEV);
	if (dev == NULL) {
		LOG_ERR("Error getting device binding");
		return 1;
	}

	ret = i2s_configure(dev, I2S_DIR_TX, &i2sConfigTx);
	if (ret != 0) {
		LOG_ERR("Error configuring device, error code : %d", ret);
		return 2;	
	}
	ret = i2s_configure(dev, I2S_DIR_RX, &i2sConfigRx);
	if (ret != 0) {
		LOG_ERR("Error configuring device, error code : %d", ret);
		return 3;	
	}
	
	/* Write */
	my_tx_buf = NULL;
	ret = k_mem_slab_alloc(i2sConfigTx.mem_slab, (void **)&my_tx_buf, K_NO_WAIT);
	if (ret == 0) {
		PREPARE_BUFFER(my_tx_buf, BLOCK_SIZE_BYTES);
		for(u8_t i = 0; i < WRITES_NUMBER; i++) {
			ret = i2s_write(dev, my_tx_buf, BLOCK_SIZE_BYTES);
			if (ret != 0) {
				LOG_WRN("Error during i2s_write");
			}
			LOG_INF("Block size: %d", BLOCK_SIZE_BYTES);
		}
	}
	else {
		LOG_ERR("Unable to allocate slab, code: %d", ret);
		return 4;
	}
	
	/* Trigger TX */
	if (i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_START)) {
		LOG_WRN("Error TX TRIGGER START");
		return 6;
	}
	else {
		LOG_INF("TX TRIGGER START");
	}

	/* Trigger RX */
	if (i2s_trigger(dev, I2S_DIR_RX, I2S_TRIGGER_START)) {
		LOG_WRN("Error RX TRIGGER START");
		return 7;
	}
	else {
		LOG_INF("RX TRIGGER START");
	}

	/* Read */
	ret = i2s_read(dev, (void **)&my_rx_buf, &rcv_size);
	LOG_INF("Receive size: %x", rcv_size);
	LOG_HEXDUMP_INF(my_rx_buf, rcv_size, "rx buffer");
	if (ret != 0) {
		LOG_WRN("Error during i2s_read: %d", ret);
	}

	/* Skip initial zeros */
	uint8_t skip = 0;
	while(my_rx_buf[skip+1] == 0)
	{
		skip++;
	}

	LOG_INF("Skipped %d", skip);

	/* Compare */
	for(u32_t i = 1; i < (rcv_size - skip); i++)
	{
		if(my_rx_buf[i+skip] != (i % 256)){
			LOG_ERR("Failed comparison at %d: was %x should be %x",
				i, my_rx_buf[i+skip], (i%256));
			return 8;
		}
	}

	k_mem_slab_free(i2sConfigRx.mem_slab, (void **)&my_rx_buf);

	LOG_INF("I2S Basic test OK");
	return 0;
}

int main(void)
{
	i2s_basic();
	return 0;
}
