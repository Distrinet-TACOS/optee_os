// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017, Linaro Limited
 */
#include <kernel/interrupt.h>
#include <kernel/pseudo_ta.h>
#include <drivers/serial.h>
#include <trace.h>

#include <kernel/notif.h>
#include <ssp_bench.h>
#include <io.h>

#define TA_NAME "Benchmarker"
#define TA_UUID                                                                \
	{                                                                      \
		0x6ce85888, 0xdb4e, 0x41f9,                                    \
		{                                                              \
			0xb4, 0x4f, 0x03, 0x6e, 0x21, 0x4d, 0xdf, 0x3a         \
		}                                                              \
	}

#define stamp(x) asm volatile("isb; mrc p15, 0, %0, c9, c13, 0" : "=r"(x))
#define gt_base(x) asm volatile("mrc p15, 4, %0, c15, c0, 0" : "=r"(x))

static enum bench_cmd { RTT, SINGLE, RTT_MEM, PRINT_MEM };
#define MAX_STAMPS 100

static struct timestamps {
	uint32_t counter;
	uint32_t stamps[MAX_STAMPS];
};

static struct bench {
	struct timestamps *mem;
	uint32_t size;
	struct serial_chip *serial_chip_con_split;
	uint32_t notif_value;
	bool itr_enabled;
	struct timestamps stamps;
	vaddr_t gt_addr;
} bench = { .notif_value = 9, .stamps = { .counter = 0 } };

void register_serial_chip_bench(struct serial_chip *chip)
{
	bench.serial_chip_con_split = chip;
}

static TEE_Result print_stamps(void)
{
	uint32_t c;

	IMSG("i: PTA\n");
	for (c = 0; c < bench.stamps.counter; c++) {
		IMSG("%d: %u\n", c, bench.stamps.stamps[c]);
	}

	bench.stamps.counter = 0;

	return 0;
}

static TEE_Result rtt(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	stamp(params[0].value.a);

	return TEE_SUCCESS;
}

static TEE_Result rtt_mem(void)
{
	if (bench.stamps.counter >= MAX_STAMPS) {
		return TEE_ERROR_SHORT_BUFFER;
	}

	stamp(bench.stamps.stamps[bench.stamps.counter++]);

	return TEE_SUCCESS;
}

static enum itr_return
console_split_itr_cb(struct itr_handler *handler __unused)
{
	struct serial_chip *chip = bench.serial_chip_con_split;

	while (chip->ops->have_rx_data(chip)) {
		chip->ops->getchar(chip);
		stamp(bench.stamps.stamps[bench.stamps.counter]);
		notif_send_async(bench.notif_value);
		IMSG("stamp: %u", bench.stamps.stamps[bench.stamps.counter++]);
	}

	return ITRR_HANDLED;
}

static struct itr_handler console_itr = {
	.it = 59,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = console_split_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);

static TEE_Result register_itr(void)
{
	if (!bench.itr_enabled) {
		itr_add(&console_itr);
		itr_set_affinity(console_itr.it, 1);
		itr_enable(console_itr.it);
	}

	return TEE_SUCCESS;
}

static TEE_Result single(void)
{
	if (bench.stamps.counter >= MAX_STAMPS) {
		return TEE_ERROR_SHORT_BUFFER;
	}

	stamp(bench.stamps.stamps[bench.stamps.counter++]);

	notif_send_async(bench.notif_value);

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session_ctx __unused, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd_id) {
	case RTT:
		return rtt(param_types, params);
	case SINGLE:
		return single();
	case RTT_MEM:
		return rtt_mem();
	case PRINT_MEM:
		return print_stamps();
	default:
		break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

static TEE_Result create_entry_point(void) {
	paddr_t base;

	// gt_base(base);
	// bench.gt_addr += core_mmu_get_va(base + 0x200, MEM_AREA_IO_SEC, 1);

	// vaddr_t control = bench.gt_addr + 0x8;

	// io_write32(control, 1);

	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = TA_UUID, .name = TA_NAME, .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = create_entry_point);