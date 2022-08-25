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

uint32_t print = 0;

TaskHandle_t test1_handler, test2_handler, test3_handler;

static void test1_task(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	while(1){

		if(io_read32(&print) == 1){
			io_write32(&print, 0);
			IMSG("TEST TASK 1 -----");
		}
	}
}

static void test2_task(void *pvParameters){
	/* Stop warnings. */
	( void ) pvParameters;

	while(1){

		if(io_read32(&print) == 0){
			io_write32(&print, 1);
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

		xReturn = xTaskCreate( test3_task, "TASK3", configMINIMAL_STACK_SIZE, ( void * ) NULL, ( UBaseType_t ) 2, &test3_handler );      		

		if(xReturn != pdPASS){
			IMSG("Couldn't create test2_tasks");
			while(1);
		}
	}
#endif

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
	vInitVariableForFreeRTOS();
	vTaskStartScheduler();

	return TEE_SUCCESS;
}

boot_final(scheduler_init);
