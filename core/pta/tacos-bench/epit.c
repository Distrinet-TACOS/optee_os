#include <drivers/imx_ccm.h>
#include <imx-regs.h>
#include <io.h>
#include <kernel/interrupt.h>
#include <kernel/notif.h>
#include <mm/core_memprot.h>
#include <trace.h>

#include "bench.h"

static const uint32_t cmp = 0xffffffff;
static const uint32_t rst = 0xffffff;
static vaddr_t epit_base;
static enum bench_cmd cmd;
static uint32_t notif_value;
static volatile bool busy;

static void init_epit(void)
{
	uint32_t config = EPITCR_CLKSRC_PERIPHERAL | EPITCR_IOVW |
			  EPITCR_PRESC(0) | EPITCR_OCIEN;

	epit_base = (vaddr_t)phys_to_virt_io(0x20D0000, 1);

	ungate_clk(CCM_CCGR1, BS_CCM_CCGR1_EPIT1S);

	io_write32(epit_base + EPITCR, 0x0);
	io_write32(epit_base + EPITCR, config);
	io_write32(epit_base + EPITSR, 0x1);
	io_write32(epit_base + EPITLR, rst);
	io_write32(epit_base + EPITCMPR, cmp);
	// Only enabling EPIT now as explained in 24.5.1 in manual.
	io_write32(epit_base + EPITCR,
		   io_read32(epit_base + EPITCR) | EPITCR_EN);
}

static inline void reset_epit(void)
{
	io_write32(epit_base + EPITSR, 0x1);
	io_write32(epit_base + EPITLR, rst);
}

static void disable_epit(void)
{
	io_write32(epit_base + EPITCR, 0x0);
}

static enum itr_return epit_cb(struct itr_handler *handler __unused);

static struct itr_handler epit_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = epit_cb,
};
DECLARE_KEEP_PAGER(epit_itr);

static enum itr_return epit_cb(struct itr_handler *handler __unused)
{
	uint32_t cur = io_read32(epit_base + EPITCNR);
	bench.stamps.stamps[bench.stamps.counter++] = cmp - cur;

	if (bench.stamps.counter >= MAX_STAMPS) {
		IMSG("Disabling interrupt.\n");
		itr_disable(epit_itr.it);
		disable_epit();
		_print_stamps(cmd);
		if (cmd == EPIT_SW) {
			busy = false;
		}
		else if (cmd == EPIT_NW) {
			notif_send_async(notif_value);
		}
		return ITRR_HANDLED;
	}

	if (bench.stamps.counter == 1) {
		IMSG("Counter: %2d", bench.stamps.counter - 1);
	} else {
		IMSG("\033[2K\033[1ACounter: %2d", bench.stamps.counter - 1);
	}
	reset_epit();

	return ITRR_HANDLED;
}

static  void epit(void)
{
	uint32_t start;
	uint32_t end;
	uint32_t epit_start;
	uint32_t epit_end;
	volatile int i;

	// Determine epit frequency.
	init_epit();

	stamp(start);
	epit_start = io_read32(epit_base + EPITCNR);
	for (i = 0; i < 10000000; i++)
		;
	stamp(end);
	epit_end = io_read32(epit_base + EPITCNR);
	IMSG("### Average ticks ###\n");
	IMSG("Ticks      : %u\n", end - start);
	IMSG("Start epit : %u\n", epit_start);
	IMSG("End epit   : %u\n", epit_end);
	IMSG("\n");

	reset_epit();
	itr_enable(epit_itr.it);
}

TEE_Result epit_sw(void)
{
	cmd = EPIT_SW;
	busy = true;
	epit();

	while (busy)
		;

	return TEE_SUCCESS;
}

TEE_Result epit_nw(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	notif_alloc_async_value(&notif_value);
	params[0].value.a = notif_value;

	cmd = EPIT_NW;
	epit();

	return TEE_SUCCESS;
}

void add_epit_itr(void)
{
	itr_add(&epit_itr);
	itr_set_affinity(epit_itr.it, 1);
	itr_set_priority(epit_itr.it, 0);
}