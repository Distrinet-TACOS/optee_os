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

uint32_t print1 = 1;
uint32_t print2 = 1;

TaskHandle_t test1_handler, test2_handler, test3_handler;

void raise88(void){
	IMSG("Raise");
	for(uint32_t i=0; i<10000; i++);
	itr_raise_pi(88);
}

void vPrintSMFIQ(){
	IMSG(" - Go to SW"); 
}

void vPrintSMFIQret(){
	IMSG(" -! Return to NW");
}

void currentMode(){
	uint32_t cpsr;

	asm volatile ("mrs	%[cpsr], cpsr" : [cpsr] "=r" (cpsr) );

	IMSG(" - Current mode : %x", cpsr);
}

void vPrintFIQ(){
	uint32_t cpsr;

	asm volatile ("mrs	%[cpsr], cpsr" : [cpsr] "=r" (cpsr) );

	IMSG(" - OPTEE Handler FIQ : %x", cpsr);
}

void vPrintIRQ(){
	uint32_t cpsr;

	asm volatile ("mrs	%[cpsr], cpsr" : [cpsr] "=r" (cpsr) );

	uint32_t iar;
	uint32_t id;

	uint32_t addr = phys_to_virt_io(configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET, 0x1);
	iar = io_read32(addr + 0x00C);;
	id = iar & 0x3ff;

	IMSG(" -! OPTEE Handler IRQ: %x, ID = %u", cpsr, id);
}

void vAssertInASM7(){
	IMSG("ASM7");
}

void vAssertInASM8(){
	IMSG("ASM8");
}

void vAssertInASMr0(uint32_t x){
	IMSG("Check r0 : %x", x);
}

static void test1_task(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	while(1){

		if(io_read32(&print1) == 1){
			io_write32(&print1, 0);
			io_write32(&print2, 1);
			IMSG("TEST TASK 1 -----");
		}
	}
}

static void test2_task(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	while(1){

		if(io_read32(&print2) == 1){
			io_write32(&print2, 0);
			io_write32(&print1 ,1);
			IMSG("----- TEST TASK 2");
		}
	}
}

void test3_task(void *pvParameters){

	/* Stop warnings. */
	( void ) pvParameters;
	uint32_t flag = 0;
	
	while(1){
		IMSG("--- TEST TASK 3 ---");

		if(flag == 1){
			print1 = 1;
			print2 = 1;
			vTaskResume(test1_handler);
			vTaskResume(test2_handler);

			flag = 0;
		}
		else{
			vTaskSuspend(test1_handler);
			vTaskSuspend(test2_handler);

			flag = 1;
		}

		vTaskDelay(3);
	}
}


#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

	StaticTask_t xTask1TCB, xTask2TCB, xTask3TCB;
	StackType_t xStack1[ configMINIMAL_STACK_SIZE ];
	StackType_t xStack2[ configMINIMAL_STACK_SIZE ];
	StackType_t xStack3[ configMINIMAL_STACK_SIZE ];

	void vCreateTestsTasks(void){

		if( configSUPPORT_STATIC_ALLOCATION == 1 ){

			test1_handler = xTaskCreateStatic(test1_task, "TASK1", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, xStack1, &xTask1TCB);

			if(test1_handler == NULL){
				IMSG("Cannot create task1");
				while(1);
			}

			test2_handler = xTaskCreateStatic(test2_task, "TASK2", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, xStack2, &xTask2TCB);

			if(test2_handler == NULL){
				IMSG("Cannot create task2");
				while(1);
			}

			test3_handler = xTaskCreateStatic(test3_task, "TASK3", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 2, xStack3, &xTask3TCB);

			if(test3_handler == NULL){
				IMSG("Cannot create task3");
				while(1);
			}
		}
	}
#endif

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1 && configSUPPORT_STATIC_ALLOCATION == 0)
	void vCreateTestsTasks(void){
		BaseType_t xReturn = pdFAIL;

		xReturn = xTaskCreate( test1_task, "TASK1", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, &test1_handler );      		

		if(xReturn != pdPASS){
			IMSG("Couldn't create test1_tasks");
			while(1);
		}	
		
		xReturn = xTaskCreate( test2_task, "TASK2", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 1, &test2_handler );      		

		if(xReturn != pdPASS){
			IMSG("Couldn't create test2_tasks");
			while(1);
		}

		xReturn = xTaskCreate( test3_task, "TASK3", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 2, &test2_handler );      		

		if(xReturn != pdPASS){
			IMSG("Couldn't create test2_tasks");
			while(1);
		}
	}
#endif

static enum itr_return Epit_Interrupt_Handler(void){
	
	static uint32_t delay = 20000/EPIT1_PERIODE_MS;

	if (delay == 1) {
		vTaskStartScheduler();
	}

	if (delay == 0) {
		FreeRTOS_Tick_Handler();
	}
	else{
		delay--;
	}

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
	io_write32(EPIT1_BASE_VA + EPITLR, EPIT1_PERIODE_MS);

	io_write32(EPIT1_BASE_VA + EPITCMPR, EPIT1_PERIODE_MS);
	/* Only enabling EPIT now as explained in 24.5.1 in manual. */
	io_write32(EPIT1_BASE_VA + EPITCR, io_read32(EPIT1_BASE_VA + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);

	IMSG("Enable EPIT TIMER");
	itr_enable(schedule_itr.it);

	vCreateTestsTasks();
	//vTaskStartScheduler();

	return TEE_SUCCESS;
}

boot_final(scheduler_init);
