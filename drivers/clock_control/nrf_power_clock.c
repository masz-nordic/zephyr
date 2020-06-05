/*
 * Copyright (c) 2016-2019 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include "nrf_clock_calibration.h"
#include <logging/log.h>
#if defined(CONFIG_SOC_SERIES_NRF53X)
#include <hal/nrf_usbreg.h>
#else
#include <hal/nrf_power.h>
#endif

LOG_MODULE_REGISTER(clock_control, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

#define DT_DRV_COMPAT nordic_nrf_clock

/* Helper logging macros which prepends subsys name to the log. */
#ifdef CONFIG_LOG
#define CLOCK_LOG(lvl, dev, subsys, ...) \
	LOG_##lvl("%s: " GET_ARG1(__VA_ARGS__), \
		get_sub_config(dev, (enum clock_control_nrf_type)subsys)->name \
		COND_CODE_0(NUM_VA_ARGS_LESS_1(__VA_ARGS__),\
				(), (, GET_ARGS_LESS_1(__VA_ARGS__))))
#else
#define CLOCK_LOG(...)
#endif

#define ERR(dev, subsys, ...) CLOCK_LOG(ERR, dev, subsys, __VA_ARGS__)
#define WRN(dev, subsys, ...) CLOCK_LOG(WRN, dev, subsys, __VA_ARGS__)
#define INF(dev, subsys, ...) CLOCK_LOG(INF, dev, subsys, __VA_ARGS__)
#define DBG(dev, subsys, ...) CLOCK_LOG(DBG, dev, subsys, __VA_ARGS__)

/* Clock subsys structure */
struct nrf_clock_control_sub_data {
	sys_slist_t list;	/* List of users requesting callback */
	u8_t ref;		/* Users counter */
	bool started;		/* Indicated that clock is started */
};

/* Clock subsys static configuration */
struct nrf_clock_control_sub_config {
	nrf_clock_event_t started_evt;	/* Clock started event */
	nrf_clock_task_t start_tsk;	/* Clock start task */
	nrf_clock_task_t stop_tsk;	/* Clock stop task */
#ifdef CONFIG_LOG
	const char *name;
#endif
};

struct nrf_clock_control_data {
	struct nrf_clock_control_sub_data subsys[CLOCK_CONTROL_NRF_TYPE_COUNT];
};

struct nrf_clock_control_config {
	struct nrf_clock_control_sub_config
					subsys[CLOCK_CONTROL_NRF_TYPE_COUNT];
};

enum usbd_power_int {
#if defined(CONFIG_SOC_SERIES_NRF53X)
	USBD_PWR_INT_DETECTED = NRF_USBREG_INT_USBDETECTED,
	USBD_PWR_INT_REMOVED  = NRF_USBREG_INT_USBREMOVED,
	USBD_PWR_INT_READY    = NRF_USBREG_INT_USBPWRRDY,
#else
	USBD_PWR_INT_DETECTED = NRF_POWER_INT_USBDETECTED_MASK,
	USBD_PWR_INT_REMOVED  = NRF_POWER_INT_USBREMOVED_MASK,
	USBD_PWR_INT_READY    = NRF_POWER_INT_USBPWRRDY_MASK,
#endif
};

enum usbd_power_event_type {
#if defined(CONFIG_SOC_SERIES_NRF53X)
	USBD_PWR_EVT_DETECTED = NRF_USBREG_EVENT_USBDETECTED,
	USBD_PWR_EVT_REMOVED  = NRF_USBREG_EVENT_USBREMOVED,
	USBD_PWR_EVT_READY    = NRF_USBREG_EVENT_USBPWRRDY,
#else
	USBD_PWR_EVT_DETECTED = NRF_POWER_EVENT_USBDETECTED,
	USBD_PWR_EVT_REMOVED  = NRF_POWER_EVENT_USBREMOVED,
	USBD_PWR_EVT_READY    = NRF_POWER_EVENT_USBPWRRDY,
#endif
};

static void clkstarted_handle(struct device *dev,
			      enum clock_control_nrf_type type);

/* Return true if given event has enabled interrupt and is triggered. Event
 * is cleared.
 */
static bool clock_event_check_and_clean(nrf_clock_event_t evt, u32_t intmask)
{
	bool ret = nrf_clock_event_check(NRF_CLOCK, evt) &&
			nrf_clock_int_enable_check(NRF_CLOCK, intmask);

	if (ret) {
		nrf_clock_event_clear(NRF_CLOCK, evt);
	}

	return ret;
}

static void clock_irqs_disable(void)
{
#if defined(CONFIG_SOC_SERIES_NRF53X)
	nrf_clock_int_disable(NRF_CLOCK,
			(NRF_CLOCK_INT_HF_STARTED_MASK |
			 NRF_CLOCK_INT_LF_STARTED_MASK));
#if CONFIG_USB_NRFX
	nrf_usbreg_int_disable(NRF_USBREGULATOR,
			(NRF_USBREG_INT_USBDETECTED |
			 NRF_USBREG_INT_USBREMOVED  |
			 NRF_USBREG_INT_USBPWRRDY));
#endif
#else
	nrf_clock_int_disable(NRF_CLOCK,
			(NRF_CLOCK_INT_HF_STARTED_MASK |
			 NRF_CLOCK_INT_LF_STARTED_MASK |
			 COND_CODE_1(CONFIG_USB_NRFX,
				(NRF_POWER_INT_USBDETECTED_MASK |
				 NRF_POWER_INT_USBREMOVED_MASK  |
				 NRF_POWER_INT_USBPWRRDY_MASK),
				(0))));
#endif
}

static void clock_irqs_enable(void)
{
#if defined(CONFIG_SOC_SERIES_NRF53X)
	nrf_clock_int_enable(NRF_CLOCK,
			(NRF_CLOCK_INT_HF_STARTED_MASK |
			 NRF_CLOCK_INT_LF_STARTED_MASK));
#if CONFIG_USB_NRFX
	nrf_usbreg_int_enable(NRF_USBREGULATOR,
			(NRF_USBREG_INT_USBDETECTED |
			 NRF_USBREG_INT_USBREMOVED  |
			 NRF_USBREG_INT_USBPWRRDY));
#endif
#else
	nrf_clock_int_enable(NRF_CLOCK,
			(NRF_CLOCK_INT_HF_STARTED_MASK |
			 NRF_CLOCK_INT_LF_STARTED_MASK |
			 COND_CODE_1(CONFIG_USB_NRFX,
				(NRF_POWER_INT_USBDETECTED_MASK |
				 NRF_POWER_INT_USBREMOVED_MASK  |
				 NRF_POWER_INT_USBPWRRDY_MASK),
				(0))));
#endif
}

static struct nrf_clock_control_sub_data *get_sub_data(struct device *dev,
					      enum clock_control_nrf_type type)
{
	struct nrf_clock_control_data *data = dev->driver_data;

	return &data->subsys[type];
}

static const struct nrf_clock_control_sub_config *get_sub_config(
					struct device *dev,
					enum clock_control_nrf_type type)
{
	const struct nrf_clock_control_config *config =
						dev->config_info;

	return &config->subsys[type];
}

static enum clock_control_status get_status(struct device *dev,
					    clock_control_subsys_t subsys)
{
	enum clock_control_nrf_type type = (enum clock_control_nrf_type)subsys;
	struct nrf_clock_control_sub_data *data;

	__ASSERT_NO_MSG(type < CLOCK_CONTROL_NRF_TYPE_COUNT);
	data = get_sub_data(dev, type);
	if (data->started) {
		return CLOCK_CONTROL_STATUS_ON;
	}

	if (data->ref > 0) {
		return CLOCK_CONTROL_STATUS_STARTING;
	}

	return CLOCK_CONTROL_STATUS_OFF;
}

static int clock_stop(struct device *dev, clock_control_subsys_t subsys)
{
	enum clock_control_nrf_type type = (enum clock_control_nrf_type)subsys;
	const struct nrf_clock_control_sub_config *config;
	struct nrf_clock_control_sub_data *data;
	int err = 0;
	int key;

	__ASSERT_NO_MSG(type < CLOCK_CONTROL_NRF_TYPE_COUNT);
	config = get_sub_config(dev, type);
	data = get_sub_data(dev, type);

	key = irq_lock();
	if (data->ref == 0) {
		err = -EALREADY;
		goto out;
	}
	data->ref--;
	if (data->ref == 0) {
		DBG(dev, subsys, "Stopping");
		sys_slist_init(&data->list);

		if (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)
			&& (subsys == CLOCK_CONTROL_NRF_SUBSYS_LF)) {
			z_nrf_clock_calibration_lfclk_stopped();
		}

		nrf_clock_task_trigger(NRF_CLOCK, config->stop_tsk);

		data->started = false;
	}

out:
	irq_unlock(key);

	return err;
}

static bool is_in_list(sys_slist_t *list, sys_snode_t *node)
{
	sys_snode_t *item = sys_slist_peek_head(list);

	do {
		if (item == node) {
			return true;
		}

		item = sys_slist_peek_next(item);
	} while (item);

	return false;
}

static void list_append(sys_slist_t *list, sys_snode_t *node)
{
	int key;

	key = irq_lock();
	sys_slist_append(list, node);
	irq_unlock(key);
}

static struct clock_control_async_data *list_get(sys_slist_t *list)
{
	struct clock_control_async_data *async_data;
	sys_snode_t *node;
	int key;

	key = irq_lock();
	node = sys_slist_get(list);
	irq_unlock(key);
	async_data = CONTAINER_OF(node,
		struct clock_control_async_data, node);

	return async_data;
}

static inline void anomaly_132_workaround(void)
{
#if (CONFIG_NRF52_ANOMALY_132_DELAY_US - 0)
	static bool once;

	if (!once) {
		k_busy_wait(CONFIG_NRF52_ANOMALY_132_DELAY_US);
		once = true;
	}
#endif
}

static int clock_async_start(struct device *dev,
			     clock_control_subsys_t subsys,
			     struct clock_control_async_data *data)
{
	enum clock_control_nrf_type type = (enum clock_control_nrf_type)subsys;
	const struct nrf_clock_control_sub_config *config;
	struct nrf_clock_control_sub_data *clk_data;
	int key;
	u8_t ref;

	__ASSERT_NO_MSG(type < CLOCK_CONTROL_NRF_TYPE_COUNT);
	config = get_sub_config(dev, type);
	clk_data = get_sub_data(dev, type);

	__ASSERT_NO_MSG((data == NULL) ||
			((data != NULL) && (data->cb != NULL)));

	/* if node is in the list it means that it is scheduled for
	 * the second time.
	 */
	if ((data != NULL)
	    && is_in_list(&clk_data->list, &data->node)) {
		return -EBUSY;
	}

	key = irq_lock();
	ref = ++clk_data->ref;
	__ASSERT_NO_MSG(clk_data->ref > 0);
	irq_unlock(key);

	if (data) {
		bool already_started;

		clock_irqs_disable();
		already_started = clk_data->started;
		if (!already_started) {
			list_append(&clk_data->list, &data->node);
		}
		clock_irqs_enable();

		if (already_started) {
			data->cb(dev, subsys, data->user_data);
		}
	}

	if (ref == 1) {
		DBG(dev, subsys, "Triggering start task");

		if (IS_ENABLED(CONFIG_NRF52_ANOMALY_132_WORKAROUND) &&
			(subsys == CLOCK_CONTROL_NRF_SUBSYS_LF)) {
			anomaly_132_workaround();
		}

		nrf_clock_task_trigger(NRF_CLOCK, config->start_tsk);
	}

	return 0;
}

static int clock_start(struct device *dev, clock_control_subsys_t sub_system)
{
	return clock_async_start(dev, sub_system, NULL);
}

/* Note: this function has public linkage, and MUST have this
 * particular name.  The platform architecture itself doesn't care,
 * but there is a test (tests/kernel/arm_irq_vector_table) that needs
 * to find it to it can set it in a custom vector table.  Should
 * probably better abstract that at some point (e.g. query and reset
 * it by pointer at runtime, maybe?) so we don't have this leaky
 * symbol.
 */
void nrf_power_clock_isr(void *arg);

static int clk_init(struct device *dev)
{
	LOG_ERR("CLK INIT");
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    nrf_power_clock_isr, 0, 0);

	irq_enable(DT_INST_IRQN(0));

	IRQ_CONNECT(55, DT_INST_IRQ(0, priority),
		    nrf_power_clock_isr, 0, 0);

	irq_enable(55);

	nrf_clock_lf_src_set(NRF_CLOCK, CLOCK_CONTROL_NRF_K32SRC);

	if (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)) {
		z_nrf_clock_calibration_init(dev);
	}

	clock_irqs_enable();

	for (enum clock_control_nrf_type i = 0;
		i < CLOCK_CONTROL_NRF_TYPE_COUNT; i++) {
		sys_slist_init(&(get_sub_data(dev, i)->list));
	}

	return 0;
}
static const struct clock_control_driver_api clock_control_api = {
	.on = clock_start,
	.off = clock_stop,
	.async_on = clock_async_start,
	.get_status = get_status,
};

static struct nrf_clock_control_data data;

static const struct nrf_clock_control_config config = {
	.subsys = {
		[CLOCK_CONTROL_NRF_TYPE_HFCLK] = {
			.start_tsk = NRF_CLOCK_TASK_HFCLKSTART,
			.started_evt = NRF_CLOCK_EVENT_HFCLKSTARTED,
			.stop_tsk = NRF_CLOCK_TASK_HFCLKSTOP,
			IF_ENABLED(CONFIG_LOG, (.name = "hfclk",))
		},
		[CLOCK_CONTROL_NRF_TYPE_LFCLK] = {
			.start_tsk = NRF_CLOCK_TASK_LFCLKSTART,
			.started_evt = NRF_CLOCK_EVENT_LFCLKSTARTED,
			.stop_tsk = NRF_CLOCK_TASK_LFCLKSTOP,
			IF_ENABLED(CONFIG_LOG, (.name = "lfclk",))
		}
	}
};

DEVICE_AND_API_INIT(clock_nrf, DT_INST_LABEL(0),
		    clk_init, &data, &config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &clock_control_api);

static void clkstarted_handle(struct device *dev,
			      enum clock_control_nrf_type type)
{
	struct nrf_clock_control_sub_data *sub_data = get_sub_data(dev, type);
	struct clock_control_async_data *async_data;

	DBG(dev, type, "Clock started");
	sub_data->started = true;

	while ((async_data = list_get(&sub_data->list)) != NULL) {
		async_data->cb(dev, (clock_control_subsys_t)type,
				async_data->user_data);
	}
}

#if defined(CONFIG_USB_NRFX)

static bool power_event_check_and_clean(enum usbd_power_event_type evt, u32_t intmask)
{
#if defined(CONFIG_SOC_SERIES_NRF53X)
	bool ret = nrf_usbreg_event_check(NRF_USBREGULATOR, evt) &&
			nrf_usbreg_int_enable_check(NRF_USBREGULATOR, intmask);

	if (ret) {
		nrf_usbreg_event_clear(NRF_USBREGULATOR, evt);
	}

	return ret;
#else
	bool ret = nrf_power_event_check(NRF_POWER, evt) &&
			nrf_power_int_enable_check(NRF_POWER, intmask);

	if (ret) {
		nrf_power_event_clear(NRF_POWER, evt);
	}

	return ret;
#endif
}

#endif // defined(CONFIG_USB_NRFX)

static void usb_power_isr(void)
{
#if defined(CONFIG_USB_NRFX)
	extern void usb_dc_nrfx_power_event_callback(enum usbd_power_event_type event);

	if (power_event_check_and_clean(USBD_PWR_EVT_DETECTED,
					USBD_PWR_INT_DETECTED)) {
		LOG_ERR("DETECTED");
		usb_dc_nrfx_power_event_callback(USBD_PWR_EVT_DETECTED);
	}

	if (power_event_check_and_clean(USBD_PWR_EVT_READY,
					USBD_PWR_INT_READY)) {
		LOG_ERR("READY");
		usb_dc_nrfx_power_event_callback(USBD_PWR_EVT_READY);
	}

	if (power_event_check_and_clean(USBD_PWR_EVT_REMOVED,
					USBD_PWR_INT_REMOVED)) {
		LOG_ERR("REMOVED");
		usb_dc_nrfx_power_event_callback(USBD_PWR_EVT_REMOVED);
	}
#endif
}

void nrf_power_clock_isr(void *arg)
{
	LOG_ERR("CLK ISR");
	ARG_UNUSED(arg);
	struct device *dev = DEVICE_GET(clock_nrf);

	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_HFCLKSTARTED,
					NRF_CLOCK_INT_HF_STARTED_MASK)) {
		struct nrf_clock_control_sub_data *data =
				get_sub_data(dev, CLOCK_CONTROL_NRF_TYPE_HFCLK);

		/* Check needed due to anomaly 201:
		 * HFCLKSTARTED may be generated twice.
		 */
		if (!data->started) {
			clkstarted_handle(dev, CLOCK_CONTROL_NRF_TYPE_HFCLK);
		}
	}

	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_LFCLKSTARTED,
					NRF_CLOCK_INT_LF_STARTED_MASK)) {
		if (IS_ENABLED(
			CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)) {
			z_nrf_clock_calibration_lfclk_started();
		}
		clkstarted_handle(dev, CLOCK_CONTROL_NRF_TYPE_LFCLK);
	}

	usb_power_isr();

	if (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)) {
		z_nrf_clock_calibration_isr();
	}
}

#ifdef CONFIG_USB_NRFX
#if defined(CONFIG_SOC_SERIES_NRF53X)
void nrf5_power_usb_power_int_enable(bool enable)
{
	u32_t mask;

	mask = NRF_USBREG_INT_USBDETECTED |
	       NRF_USBREG_INT_USBREMOVED  |
	       NRF_USBREG_INT_USBPWRRDY;

	if (enable) {
		nrf_usbreg_int_enable(NRF_USBREGULATOR, mask);
		irq_enable(55);
	} else {
		nrf_usbreg_int_disable(NRF_USBREGULATOR, mask);
	}
}
#else
void nrf5_power_usb_power_int_enable(bool enable)
{
	u32_t mask;

	mask = NRF_POWER_INT_USBDETECTED_MASK |
	       NRF_POWER_INT_USBREMOVED_MASK |
	       NRF_POWER_INT_USBPWRRDY_MASK;

	if (enable) {
		nrf_power_int_enable(NRF_POWER, mask);
		irq_enable(DT_INST_IRQN(0));
	} else {
		nrf_power_int_disable(NRF_POWER, mask);
	}
}
#endif 
#endif // CONFIG_USB_NRFX
