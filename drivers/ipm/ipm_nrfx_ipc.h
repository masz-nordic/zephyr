/*
 * Copyright (c) 2019, Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <nrfx_ipc.h>

#define NRFX_IPC_ID_MAX_VALUE	 IPC_TASKS_NUM

/*
 * Group IPC signals, events and channels into message channels.
 * Message channels are one-way connections between cores.
 * 
 * For example Message Channel 0 is configured as TX on core 0
 * and as RX on core 1:
 *
 * [C0]			    [C1]
 * SIGNAL0 -> CHANNEL0 -> EVENT0
 *
 * Message Channel 1 is configured as RX on core 0 and as TX
 * on core 1:
 * [C0]			    [C1]
 * EVENT1 <- CHANNEL1 <- SIGNAL1
 */

#define IPC_EVENT_BIT(idx) \
	((IS_ENABLED(CONFIG_IPM_MSG_CH_##idx##_RX)) << idx)

#define IPC_EVENT_BITS		\
	(			\
	 IPC_EVENT_BIT(0)  |	\
	 IPC_EVENT_BIT(1)  |	\
	 IPC_EVENT_BIT(2)  |	\
	 IPC_EVENT_BIT(3)  |	\
	 IPC_EVENT_BIT(4)  |	\
	 IPC_EVENT_BIT(5)  |	\
	 IPC_EVENT_BIT(6)  |	\
	 IPC_EVENT_BIT(7)  |	\
	 IPC_EVENT_BIT(8)  |	\
	 IPC_EVENT_BIT(9)  |	\
	 IPC_EVENT_BIT(10) |	\
	 IPC_EVENT_BIT(11) |	\
	 IPC_EVENT_BIT(12) |	\
	 IPC_EVENT_BIT(13) |	\
	 IPC_EVENT_BIT(14) |	\
	 IPC_EVENT_BIT(15)	\
	)

static const nrfx_ipc_config_t ipc_cfg = {
	.tx_signals_channels_cfg =
	{
		[0] = NRFX_IPC_BITPOS(0),
		[1] = NRFX_IPC_BITPOS(1),
		[2] = NRFX_IPC_BITPOS(2),
		[3] = NRFX_IPC_BITPOS(3),
		[4] = NRFX_IPC_BITPOS(4),
		[5] = NRFX_IPC_BITPOS(5),
		[6] = NRFX_IPC_BITPOS(6),
		[7] = NRFX_IPC_BITPOS(7),
		[8] = NRFX_IPC_BITPOS(8),
		[9] = NRFX_IPC_BITPOS(9),
		[10] = NRFX_IPC_BITPOS(10),
		[11] = NRFX_IPC_BITPOS(11),
		[12] = NRFX_IPC_BITPOS(12),
		[13] = NRFX_IPC_BITPOS(13),
		[14] = NRFX_IPC_BITPOS(14),
		[15] = NRFX_IPC_BITPOS(15),
	},
	.rx_events_channels_cfg =
	{
		[0] = NRFX_IPC_BITPOS(0),
		[1] = NRFX_IPC_BITPOS(1),
		[2] = NRFX_IPC_BITPOS(2),
		[3] = NRFX_IPC_BITPOS(3),
		[4] = NRFX_IPC_BITPOS(4),
		[5] = NRFX_IPC_BITPOS(5),
		[6] = NRFX_IPC_BITPOS(6),
		[7] = NRFX_IPC_BITPOS(7),
		[8] = NRFX_IPC_BITPOS(8),
		[9] = NRFX_IPC_BITPOS(9),
		[10] = NRFX_IPC_BITPOS(10),
		[11] = NRFX_IPC_BITPOS(11),
		[12] = NRFX_IPC_BITPOS(12),
		[13] = NRFX_IPC_BITPOS(13),
		[14] = NRFX_IPC_BITPOS(14),
		[15] = NRFX_IPC_BITPOS(15),
	},
	.rx_events_enable_cfg = IPC_EVENT_BITS, 
};

