// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017, Linaro Limited
 */
#include <stdint.h>
#include <kernel/notif.h>
#include <kernel/pseudo_ta.h>
#include <trace.h>

#include "bench.h"

#define TA_NAME "Benchmarker"
#define TA_UUID                                                                \
	{                                                                      \
		0x6ce85888, 0xdb4e, 0x41f9,                                    \
		{                                                              \
			0xb4, 0x4f, 0x03, 0x6e, 0x21, 0x4d, 0xdf, 0x3a         \
		}                                                              \
	}

struct bench bench = { .notif_value = 9, .stamps = { .counter = 0 } };

void _print_stamps(enum bench_cmd cmd)
{
	uint32_t c;

	switch (cmd) {
	case RTT:
	case RTT_MEM:
	case ASYNC:
		IMSG("i: PTA\n");
		break;
	case EPIT_SW:
	case EPIT_NW:
		IMSG("i: Latency\n");
		break;
	default:
		IMSG("i: Stamp");
		break;
	}
	for (c = 0; c < bench.stamps.counter; c++) {
		IMSG("%-2d: %-10u\n", c, bench.stamps.stamps[c]);
	}

	bench.stamps.counter = 0;
}

static TEE_Result print_stamps(uint32_t ptypes,
			       TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	_print_stamps(params[0].value.a);

	return 0;
}

static TEE_Result __attribute__((optimize("O0")))
performance(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	uint32_t start;
	uint32_t end;

	uint32_t average = 0;
	int j;
	uint32_t p;
	uint32_t i;
	uint32_t count = 0;
	uint32_t x = 1000;

	for (j = 0; j < 10; j++) {
		stamp(start);
		for (p = 3; p <= x; p += 2) {
			for (i = 3; i * i <= p; i += 2) {
				if (p % i == 0) {
					count++;
					break;
				}
			}
		}
		stamp(end);
		average += end - start;
	}

	params[0].value.a = average / 10;

	return TEE_SUCCESS;
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

static TEE_Result async(void)
{
	if (bench.stamps.counter >= MAX_STAMPS) {
		return TEE_ERROR_SHORT_BUFFER;
	}

	stamp(bench.stamps.stamps[bench.stamps.counter++]);

	notif_send_async(9);

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session_ctx __unused, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd_id) {
	case PERF:
		return performance(param_types, params);
	case RTT:
		return rtt(param_types, params);
	case RTT_MEM:
		return rtt_mem();
	case ASYNC:
		return async();
	case EPIT_SW:
		return epit_sw();
	case EPIT_NW:
		return epit_nw(param_types, params);
	case PRINT_MEM:
		return print_stamps(param_types, params);
	default:
		break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

static TEE_Result create(void)
{
	if (!notif_async_is_enabled() || !notif_async_is_started()) {
		EMSG("Notifications are not yet available!!!\n");
		return TEE_ERROR_BAD_STATE;
	}

	// add_epit_itr();

	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = TA_UUID, .name = TA_NAME, .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = create);