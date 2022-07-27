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
#include <FreeRTOS/timers.h>

#define EPIT_BASE_VA	((uint32_t) phys_to_virt_io(EPIT_BASE_PA, 0x1))

extern void FreeRTOS_IRQ_Handler();

void vAssertInASM0(){
	uint32_t cpsr;

	asm volatile ("mrs	%[cpsr], cpsr"
			: [cpsr] "=r" (cpsr)
	);

	IMSG("Before switch sys mode : Current mode : %x", cpsr);
}

void vAssertInASM(){

	uint32_t cpsr;

	asm volatile ("mrs	%[cpsr], cpsr"
			: [cpsr] "=r" (cpsr)
	);

	IMSG("Current mode : %x", cpsr);
}

void vAssertInASM1(){
	IMSG(" - Call Normal World from vector_fiq_entry");
}

void vAssertInASM2(){
	IMSG(" -! Return Normal World to vector_fiq_entry");
}

void vAssertInASM3(){
	IMSG(" - Call Secure World from native_intr_handler");
}
void vAssertInASM4(){
	IMSG(" -! Return Secure World to native_intr_handler");
}

void vAssertInASM5(){
	IMSG("From ASM 5");
}

void vAssertInASM6(){
	IMSG("From ASM 6");
}
void vAssertInASM7(){
	IMSG("From ASM 7");
}
void vAssertInASM8(){
	IMSG("From ASM 8");
}

void vAssertInASMr0(uint32_t x){
	IMSG("Check r0 : %x", x);
}

TaskHandle_t test1_handler, test2_handler;
static void test1_tasks(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	while(1){
		IMSG("TEST 1 TASKS\n");
		
		vTaskDelay(1);	// 1 ticks
	}
}

static void test2_tasks(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	volatile TickType_t tick; 
	while(1){

		IMSG("----- TEST 2 TASKS\n");
		// tick = xTaskGetTickCount();
		// IMSG("	Count of ticks since scheduler started : %u\n", tick);
		
		vTaskDelay(2);	// 2 ticks
	}
}

void vCreateTestsTasks(void){

	BaseType_t xReturn = pdFAIL;

	// xReturn = xTaskCreate( test1_tasks, "TEST1", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, &test1_handler );      		

	// if(xReturn != pdPASS){
	// 	IMSG("Couldn't create test1_tasks");
	// 	while(1);
	// }	
	
	xReturn = xTaskCreate( test2_tasks, "TEST2", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, &test2_handler );      		

	if(xReturn != pdPASS){
		IMSG("Couldn't create test2_tasks");
		while(1);
	}
}

extern volatile BaseType_t xSchedulerRunning;

void vClearEpitInterrupt(void){
	/*	Clearing interrupt	*/
	io_write32(EPIT_BASE_VA + EPITSR, 0x1);
}

static enum itr_return vTickInterrupt(void){
	
	static uint32_t delay = 6; // 25s delai

	IMSG("FreeRTOS TickInterrupt");

	switch (delay){
		case 0:
			FreeRTOS_Tick_Handler();
			break;
		case 1:
			vCreateTestsTasks();
			vTaskStartScheduler();
		default:
			delay--;
			IMSG("delay = %u", delay);
			vClearEpitInterrupt();
			break;
	}

	return ITRR_HANDLED;
}

static struct itr_handler schedule_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = vTickInterrupt,
};
DECLARE_KEEP_PAGER(schedule_itr);

static TEE_Result scheduler_init(void){

	uint32_t config = EPITCR_CLKSRC_REF_LOW | EPITCR_IOVW |
			  EPITCR_PRESC(32) | EPITCR_RLD | EPITCR_OCIEN |
			  EPITCR_ENMOD;

	io_write32(EPIT_BASE_VA + EPITCR, 0x0);
	io_write32(EPIT_BASE_VA + EPITCR, config);
	io_write32(EPIT_BASE_VA + EPITSR, 0x1);
	io_write32(EPIT_BASE_VA + EPITLR, 5000);

	io_write32(EPIT_BASE_VA + EPITCMPR, 5000);
	// Only enabling EPIT now as explained in 24.5.1 in manual.
	io_write32(EPIT_BASE_VA + EPITCR, io_read32(EPIT_BASE_VA + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);

	IMSG("Enable EPIT TIMER");
	itr_enable(schedule_itr.it);

	return TEE_SUCCESS;
}

boot_final(scheduler_init);