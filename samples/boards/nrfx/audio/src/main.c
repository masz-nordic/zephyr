/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/gpio.h>
#include <nrfx_pdm.h>
#include <i2s.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* I2S defines */
#define NB_OF_SAMPLES			256
#define NB_OF_CHANNELS			2
#define SINGLE_SAMPLE_SIZE_BYTES	2
#define FRAME_CLOCK_FREQUENCY_HZ	8000
#define WORD_SIZE_BITS			(SINGLE_SAMPLE_SIZE_BYTES * 8)
#define TRANSFER_BLOCK_TIME_US		((NB_OF_SAMPLES * 1000000) / FRAME_CLOCK_FREQUENCY_HZ)
#define TRANSFER_TIMEOUT_MS		((TRANSFER_BLOCK_TIME_US > 1000) ? (1 + TRANSFER_BLOCK_TIME_US / 1000) : (1))

#if (SINGLE_SAMPLE_SIZE_BYTES == 1)
typedef u8_t i2s_buf_t;
#elif (SINGLE_SAMPLE_SIZE_BYTES == 2)
typedef u16_t i2s_buf_t;
#elif (SINGLE_SAMPLE_SIZE_BYTES == 3)
typedef u32_t i2s_buf_t;
#else
#error Not supported
#endif

#define BLOCK_SIZE_BYTES	(NB_OF_SAMPLES * NB_OF_CHANNELS * (1 << (SINGLE_SAMPLE_SIZE_BYTES - 1)))

/* PDM defines */
#define PDM_CLK_PIN	DT_NORDIC_NRF_PDM_PDM_0_CLK_PIN
#define PDM_DIN_PIN	DT_NORDIC_NRF_PDM_PDM_0_DIN_PIN
#define PDM_BUFFER_SIZE	(NB_OF_SAMPLES * NB_OF_CHANNELS * sizeof(i2s_buf_t))
#define PDM_BUFFER_NUM	50

#define WORK_QUEUE_STACK	512
#define WORK_QUEUE_PRIORITY	5

/* PDM structs */
K_THREAD_STACK_DEFINE(wq_stack, WORK_QUEUE_STACK);
struct k_work_q pdm_wq;

struct device *gpio_dev;
u8_t tgl;

struct pdm_work {
	struct k_work work;
	struct k_sem sem;
	u16_t	buffer[PDM_BUFFER_SIZE];
} pdm_item[PDM_BUFFER_NUM];

/* I2S structs */
K_MEM_SLAB_DEFINE(i2sBufferTx, BLOCK_SIZE_BYTES,
		  CONFIG_NRFX_I2S_TX_BLOCK_COUNT, 4);

struct i2s_config i2sConfigTx = {
	.word_size = WORD_SIZE_BITS,
	.channels = NB_OF_CHANNELS,
	.format = I2S_FMT_DATA_FORMAT_I2S,
	.options = 0,
	.frame_clk_freq = FRAME_CLOCK_FREQUENCY_HZ,
	.mem_slab = &i2sBufferTx,
	.block_size = BLOCK_SIZE_BYTES,
	.timeout = TRANSFER_TIMEOUT_MS
};

bool tx_running;
int alloc_res;
int write_res;
void *my_tx_buf;
struct device *i2s_dev;
u8_t alloced;

static void pdm_work(struct k_work *wk);

static void pdm_handler(nrfx_pdm_evt_t const * const evt)
{
	if (evt->error) {
		LOG_ERR("Overflow");
	}

	if (evt->buffer_requested) {
		LOG_DBG("Buffer requested");

		/* Find first unused buffer */
		u8_t idx;
		
		for (idx = 0; (k_sem_take(&pdm_item[idx].sem, K_NO_WAIT) != 0) &&
			      (idx < PDM_BUFFER_NUM); idx++) {
			;
		}

		if (idx >= PDM_BUFFER_NUM) {
			LOG_ERR("No available buffer");
			return;
		}

		LOG_DBG("Giving buffer: %d", idx);
		nrfx_err_t err = nrfx_pdm_buffer_set(&pdm_item[idx].buffer[0], PDM_BUFFER_SIZE);

		if (err != NRFX_SUCCESS) {
			LOG_ERR("Buffer[%d] set error: %d", idx, err);
			return;
		}

		if (idx == 0) {
			gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_1_GPIOS_PIN, 1);
		} else if (idx == 1) {
			gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_2_GPIOS_PIN, 1);
		} else if (idx == 2) {
			gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_3_GPIOS_PIN, 1);
		}
	}

	if (evt->buffer_released) {
		LOG_DBG("Buffer released: %p", evt->buffer_released);

		struct pdm_work *item = CONTAINER_OF(evt->buffer_released,
						     struct pdm_work, buffer);
		k_work_submit(&item->work);
	}
}

void main(void)
{
	nrfx_err_t err;

	/* Connect PDM IRQ to nrfx_pdm_irq_handler */
	IRQ_CONNECT(DT_NORDIC_NRF_PDM_PDM_0_IRQ_0,
		    DT_NORDIC_NRF_PDM_PDM_0_IRQ_0_PRIORITY,
		    nrfx_isr, nrfx_pdm_irq_handler, 0);

	/* Configure CLK and DIN pins in GPIO */
	gpio_dev = device_get_binding(DT_ALIAS_GPIO_0_LABEL);
	gpio_pin_configure(gpio_dev, PDM_CLK_PIN, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, PDM_DIN_PIN, GPIO_DIR_IN);
	gpio_pin_configure(gpio_dev, DT_GPIO_LEDS_LED_0_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, DT_GPIO_LEDS_LED_1_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, DT_GPIO_LEDS_LED_2_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, DT_GPIO_LEDS_LED_3_GPIOS_PIN, GPIO_DIR_OUT);

	/* Initialize PDM */
	nrfx_pdm_config_t config = NRFX_PDM_DEFAULT_CONFIG(PDM_CLK_PIN, PDM_DIN_PIN);
	config.edge = NRF_PDM_EDGE_LEFTRISING;
	config.mode = NRF_PDM_MODE_STEREO;

	err = nrfx_pdm_init(&config, pdm_handler);

	if (err != NRFX_SUCCESS) {
		LOG_ERR("PDM init error: %d", err);
		return;
	}

	/* Start data handling thread */
	k_work_q_start(&pdm_wq, wq_stack, K_THREAD_STACK_SIZEOF(wq_stack),
		       WORK_QUEUE_PRIORITY);
	
	for (u32_t i = 0; i < PDM_BUFFER_NUM; i++) {
		k_work_init(&pdm_item[i].work, pdm_work);
		k_sem_init(&pdm_item[i].sem, 1, 1);
	}

	/* Start sampling */
	err = nrfx_pdm_start();

	if (err != NRFX_SUCCESS) {
		LOG_ERR("Start error: %d", err);
		return;
	}

	LOG_INF("PDM started");

	/* Get I2S device */
	i2s_dev = device_get_binding("I2S_0");
	if (!i2s_dev) {
		LOG_ERR("I2S device not found!");
		return;
	}

	/* Configure I2S */
	int ret = i2s_configure(i2s_dev, I2S_DIR_TX, &i2sConfigTx);
	if (ret != 0) {
		LOG_ERR("I2S configuration failed");
		return;
	}
}

static void pdm_work(struct k_work *wk)
{
	LOG_DBG("PDM workqueue");
	struct pdm_work *item = CONTAINER_OF(wk, struct pdm_work, work);

	LOG_DBG("Data[0]: %x", item->buffer[0]);

	gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_0_GPIOS_PIN, tgl%2);
	tgl++;

	alloc_res = k_mem_slab_alloc(i2sConfigTx.mem_slab, &my_tx_buf,
		    (tx_running) ? (TRANSFER_TIMEOUT_MS) : (K_NO_WAIT));
	if (alloc_res == 0) {
		memcpy(my_tx_buf, &item->buffer[0], PDM_BUFFER_SIZE);
		write_res = i2s_write(i2s_dev, my_tx_buf, BLOCK_SIZE_BYTES);
		if (write_res < 0) {
			LOG_ERR("i2s_write() returned error");
			return;
		} else {
			alloced++;
			LOG_DBG("I2S data sent");
		}
	} else {
		LOG_WRN("Unable to allocate memory slab");
	}

	if ((alloced > 2 && !tx_running)) {
		if (i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START) != 0) {
			LOG_ERR("TX trigger START returned error");
			return;
		}
		tx_running = true;
		LOG_INF("I2S Starting TX");
	}


	/* Release the buffer */
	k_sem_give(&item->sem);
	
	if (item == &pdm_item[0]) {
		gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_1_GPIOS_PIN, 0);
	} else if (item == &pdm_item[1]) {
		gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_2_GPIOS_PIN, 0);
	} else if (item == &pdm_item[2]) {
		gpio_pin_write(gpio_dev, DT_GPIO_LEDS_LED_3_GPIOS_PIN, 0);
	}
}

