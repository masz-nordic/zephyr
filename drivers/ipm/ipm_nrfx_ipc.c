/*
 * Copyright (c) 2019, Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <ipm.h>
#include <nrfx_ipc.h>
#include "ipm_nrfx_ipc.h"

#define LOG_LEVEL CONFIG_IPM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ipm_nrfx_ipc);

struct ipm_nrf_data {
	ipm_callback_t callback;
	void *callback_ctx;
};

static struct ipm_nrf_data nrfx_ipm_data;

/* Common layer for single and multiple instance variants */
static void gipm_init(void);
static void gipm_send(u32_t id);
static void gpim_receive_events_set_enable(u32_t bitmask);
static void gpim_receive_events_set_disable(u32_t bitmask);
static void gpim_receive_event_enable(u8_t index);
static void gpim_receive_event_disable(u8_t index);

/* Single instance IPM */
#if IS_ENABLED(CONFIG_IPM_NRFX_SINGLE_INSTANCE)

static void nrfx_ipc_handler(uint8_t event_index, void * p_context)
{
	if (nrfx_ipm_data.callback) {
		nrfx_ipm_data.callback(nrfx_ipm_data.callback_ctx,
				       event_index,
				       NULL);
	}
}

static int ipm_nrf_send(struct device *dev, int wait, u32_t id,
			const void *data, int size)
{
	if (id > NRFX_IPC_ID_MAX_VALUE) {
		return -EINVAL;
	}

	if (size > 0) {
		LOG_WRN("nRF driver does not support sending data over IPM");
	}

	gipm_send(id);
	return 0;
}

static int ipm_nrf_max_data_size_get(struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static u32_t ipm_nrf_max_id_val_get(struct device *dev)
{
	ARG_UNUSED(dev);

	return NRFX_IPC_ID_MAX_VALUE;
}

static void ipm_nrf_register_callback(struct device *dev,
				      ipm_callback_t cb,
				      void *context)
{
	nrfx_ipm_data.callback = cb;
	nrfx_ipm_data.callback_ctx = context;
}

static int ipm_nrf_set_enabled(struct device *dev, int enable)
{
	/* Enable configured channels */
	if (enable) {
		irq_enable(DT_INST_0_NORDIC_NRF_IPC_IRQ_0);
		gpim_receive_events_set_enable((uint32_t)IPC_EVENT_BITS);
	} else {
		irq_disable(DT_INST_0_NORDIC_NRF_IPC_IRQ_0);
		gpim_receive_events_set_disable((uint32_t)IPC_EVENT_BITS);
	}
	return 0;
}

static int ipm_nrf_init(struct device *dev)
{
	gipm_init();
	return 0;
}

static const struct ipm_driver_api ipm_nrf_driver_api = {
	.send = ipm_nrf_send,
	.register_callback = ipm_nrf_register_callback,
	.max_data_size_get = ipm_nrf_max_data_size_get,
	.max_id_val_get = ipm_nrf_max_id_val_get,
	.set_enabled = ipm_nrf_set_enabled
};

DEVICE_AND_API_INIT(ipm_nrf, DT_INST_0_NORDIC_NRF_IPC_LABEL,
		    ipm_nrf_init, NULL, NULL,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		    &ipm_nrf_driver_api);

#else
/* Multiple instances of IPM */

struct vipm_nrf_data {
	ipm_callback_t callback[NRFX_IPC_ID_MAX_VALUE];
	void *callback_ctx[NRFX_IPC_ID_MAX_VALUE];
	bool ipm_init;
	struct device *ipm_device;
};

static struct vipm_nrf_data nrfx_vipm_data;

static void vipm_dispatcher(uint8_t event_index, void * p_context)
{
	if(nrfx_vipm_data.callback[event_index] != NULL) {
		nrfx_vipm_data.callback[event_index]
			(nrfx_vipm_data.callback_ctx[event_index], 0, NULL);
	}
}

static int vipm_nrf_max_data_size_get(struct device *dev)
{
	return ipm_max_data_size_get(dev);
}

static u32_t vipm_nrf_max_id_val_get(struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

#define VIPM_DEVICE_1(_idx)									\
static int vipm_nrf_##_idx##_init(struct device *dev)						\
{												\
	if(!nrfx_vipm_data.ipm_init) {								\
		gipm_init();									\
		nrfx_vipm_data.ipm_init = true;							\
	}											\
	return 0;										\
}												\
												\
static int vipm_nrf_##_idx##_send(struct device *dev, int wait, u32_t id,			\
				  const void *data, int size)					\
{												\
	if (!IS_ENABLED(CONFIG_IPM_MSG_CH_##_idx##_ENABLE)) {					\
		LOG_ERR("IPM_" #_idx " is not enabled");					\
		return -EINVAL;									\
	}											\
												\
	if(!IS_ENABLED(CONFIG_IPM_MSG_CH_##_idx##_TX)) {					\
		LOG_ERR("IPM_" #_idx " is RX message channel");					\
		return -EINVAL;									\
	}											\
												\
	if (id > NRFX_IPC_ID_MAX_VALUE) {							\
		return -EINVAL;									\
	}											\
												\
	if (id != 0) {										\
		LOG_WRN("Passing message ID to IPM with predefined message ID");		\
	}											\
												\
	if (size > 0) {										\
		LOG_WRN("nRF driver does not support sending data over IPM");			\
	}											\
												\
	return gipm_send(_idx);									\
}												\
												\
static void vipm_nrf_##_idx##_register_callback(struct device *dev,				\
					       ipm_callback_t cb, void *context)		\
{												\
	if(IS_ENABLED(CONFIG_IPM_MSG_CH_##_idx##_RX)) {						\
		nrfx_vipm_data.callback[_idx] = cb;						\
		nrfx_vipm_data.callback_ctx[_idx] = context;					\
	} else {										\
		LOG_WRN("Trying to register a callback for TX channel IPM_" #_idx);		\
	}											\
}												\
												\
static int vipm_nrf_##_idx##_set_enabled(struct device *dev, int enable)			\
{												\
	if (!IS_ENABLED(CONFIG_IPM_MSG_CH_##_idx##_RX)) {					\
		LOG_ERR("IPM_" #_idx " is TX message channel");					\
		return -EINVAL;									\
	} else if (enable) {									\
		irq_enable(DT_INST_0_NORDIC_NRF_IPC_IRQ_0);					\
		gpim_receive_event_enable(_idx);						\
	} else if (!enable) {									\
		gpim_receive_event_disable(_idx);						\
	}											\
	return 0;										\
}												\
												\
static const struct ipm_driver_api vipm_nrf_##_idx##_driver_api = {				\
	.send = vipm_nrf_##_idx##_send,								\
	.register_callback = vipm_nrf_##_idx##_register_callback,				\
	.max_data_size_get = vipm_nrf_max_data_size_get,					\
	.max_id_val_get = vipm_nrf_max_id_val_get,						\
	.set_enabled = vipm_nrf_##_idx##_set_enabled						\
};												\
												\
DEVICE_AND_API_INIT(vipm_nrf_##_idx , "IPM_"#_idx,						\
		    vipm_nrf_##_idx##_init, NULL, NULL,						\
		    PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,				\
		    &vipm_nrf_##_idx##_driver_api);

#define VIPM_DEVICE(_idx, _)						\
	COND_CODE_1(IS_ENABLED(CONFIG_IPM_MSG_CH_##_idx##_ENABLE),	\
		    (VIPM_DEVICE_1(_idx)), ())

UTIL_LISTIFY(NRFX_IPC_ID_MAX_VALUE, VIPM_DEVICE, _);

#endif

static void gipm_init(void)
{
	/* Init IPC */
#if IS_ENABLED(CONFIG_IPM_NRFX_SINGLE_INSTANCE)
	nrfx_ipc_init(nrfx_ipc_handler, 0, (void *)&nrfx_ipm_data);
#else
	nrfx_ipc_init(vipm_dispatcher, 0, (void *)&nrfx_ipm_data);
#endif
	IRQ_CONNECT(DT_INST_0_NORDIC_NRF_IPC_IRQ_0,
		    DT_INST_0_NORDIC_NRF_IPC_IRQ_0_PRIORITY,
		    nrfx_isr, nrfx_ipc_irq_handler, 0);

	/* Set up signals and channels */
	nrfx_ipc_config_load(&ipc_cfg);
}

static void gipm_send(u32_t id)
{
	nrfx_ipc_signal(id);
}

static void gpim_receive_events_set_enable(u32_t bitmask)
{
	nrfx_receive_events_set_enable(u32_t bitmask);
}

static void gpim_receive_events_set_disable(u32_t bitmask)
{
	nrfx_receive_events_set_disable(u32_t bitmask);
}

static void gpim_receive_event_enable(u8_t index)
{
	nrfx_receive_event_enable(u8_t index);
}

static void gpim_receive_event_disable(u8_t index)
{
	nrfx_receive_event_disable(u8_t index);
}
