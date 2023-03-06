// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, imec-DistriNet
 */

#include <io.h>
#include <mm/core_memprot.h>
#include <kernel/interrupt.h>
#include <initcall.h>
#include <string.h>
#include <tee_api_types.h>
#include <trace.h>
#include <kernel/notif.h>
#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <linux_reboot.h>

#include <kernel/scheduler.h>


static uint32_t base;
/* FreeRTOS IRQ Handler */
extern void FreeRTOS_FIQ_Handler(uint32_t iar, struct thread_fiq_regs *itr_regs);
extern volatile uint32_t uRunningFreeRTOS;
static bool should_restart;

void notify_restart(void) {
	should_restart = true;
}

static enum itr_return epit_interrupt_handler(struct itr_handler *h)
{
	/* FreeRTOS tick handler */
	FreeRTOS_Tick_Handler();
	/* Clearing EPIT interrupt source */
	io_write32(base + EPITSR, 0x1);
	/* Write EOIR register in FreeRTOS_FIQ_Handler from portASM.S */
	FreeRTOS_FIQ_Handler(h->it, h->itr_regs);

	if (should_restart && !uRunningFreeRTOS) {
		should_restart = false;
		restart_normal_world();
	}

	return ITRR_HANDLED_HARDWARE;
}

static struct itr_handler schedule_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = epit_interrupt_handler,
};
DECLARE_KEEP_PAGER(schedule_itr);

static TEE_Result scheduler_init(void)
{
	uint32_t config = EPITCR_CLKSRC_REF_LOW | EPITCR_IOVW |
			  EPITCR_PRESC(32) | EPITCR_RLD | EPITCR_OCIEN |
			  EPITCR_ENMOD;

	base = (uint32_t)phys_to_virt_io(EPIT1_BASE_PA, EPIT_SIZE);

	io_write32(base + EPITCR, 0x0);
	io_write32(base + EPITCR, config);
	io_write32(base + EPITSR, 0x1);
	io_write32(base + EPITLR, EPIT1_PERIOD_TICKS);

	io_write32(base + EPITCMPR, EPIT1_PERIOD_TICKS);
	/* Only enabling EPIT now as explained in 24.5.1 in manual. */
	io_write32(base + EPITCR, io_read32(base + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);

	IMSG("Enable FreeRTOS");
	vInitVariableForFreeRTOS();
	vTaskStartScheduler();
	itr_enable(schedule_itr.it);

	return TEE_SUCCESS;
}

boot_final(scheduler_init);
