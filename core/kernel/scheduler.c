// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, imec-DistriNet
 */

#include <io.h>
#include <mm/core_memprot.h>
#include <kernel/interrupt.h>
#include <initcall.h>
#include <sys/queue.h>
#include <string.h>
#include <tee_api_types.h>
#include <trace.h>
#include <kernel/notif.h>
#include <kernel/scheduler.h>

#include <FreeRTOS/FreeRTOSConfig.h>
#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/portmacro.h>
#include <FreeRTOS/task.h>

static enum itr_return Epit_Interrupt_Handler(void){
	
	FreeRTOS_Tick_Handler();

	/*	Clearing interrupt	*/
	io_write32(EPIT1_BASE_VA + EPITSR, 0x1);

	return ITRR_HANDLED;
}

static struct itr_handler schedule_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = Epit_Interrupt_Handler,
};
DECLARE_KEEP_PAGER(schedule_itr);

static TEE_Result scheduler_init(void){

	uint32_t config = EPITCR_CLKSRC_REF_LOW | EPITCR_IOVW |
			  EPITCR_PRESC(32) | EPITCR_RLD | EPITCR_OCIEN |
			  EPITCR_ENMOD;

	io_write32(EPIT1_BASE_VA + EPITCR, 0x0);
	io_write32(EPIT1_BASE_VA + EPITCR, config);
	io_write32(EPIT1_BASE_VA + EPITSR, 0x1);
	io_write32(EPIT1_BASE_VA + EPITLR, EPIT1_PERIOD_TICKS);

	io_write32(EPIT1_BASE_VA + EPITCMPR, EPIT1_PERIOD_TICKS);
	/* Only enabling EPIT now as explained in 24.5.1 in manual. */
	io_write32(EPIT1_BASE_VA + EPITCR, io_read32(EPIT1_BASE_VA + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);

	IMSG("Enable EPIT TIMER");
	itr_enable(schedule_itr.it);
	vInitVariableForFreeRTOS();
	vTaskStartScheduler();

	return TEE_SUCCESS;
}

boot_final(scheduler_init);
