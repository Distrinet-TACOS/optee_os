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

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/timers.h>
#include <FreeRTOS/task.h>

#include <kernel/scheduler.h>

TimerHandle_t xTimers;

// ---
TaskHandle_t test_handler;
static void test_tasks(void *pvParameters){
	IMSG("TEST TASKS\n");
}
// ---

void vAssertCalled(){
	//taskDISABLE_INTERRUPTS();
    IMSG("Assertion called");
    //while(true);
}

void vAssertCalled_int(unsigned int x){
    IMSG("Assertion int called : %x", x);
}

void vAssertCalled_imsg(char* s){
    IMSG("Assertion imsg called : %s", s);
}

struct task {
	char *name;
	void (*func)(void);
	CIRCLEQ_ENTRY(task) entries;
};

static struct scheduler {
	CIRCLEQ_HEAD(task_list_head, task) task_list;
	vaddr_t epit_base;
} sched = { .task_list = CIRCLEQ_HEAD_INITIALIZER(sched.task_list) };

void register_task(const char *name, void (*func)(void))
{
	struct task *t = malloc(sizeof(struct task));
	t->name = strdup(name);
	t->func = func;

	CIRCLEQ_INSERT_TAIL(&sched.task_list, t, entries);
}

static int count = 0;

// void vTimerCallback( TimerHandle_t xTimer ){

// 	uint32_t ulCount;

// 	/* The number of times this timer has expired is saved as the
//     timer's ID.  Obtain the count. */
//     ulCount = ( uint32_t ) pvTimerGetTimerID( xTimer );

// 	IMSG("Before : ulCount = %u", ulCount);

//     /* Increment the count, then test to see if the timer has expired
//     ulMaxExpiryCountBeforeStopping yet. */
//     ulCount++;

// 	vTimerSetTimerID( xTimer, ( void * ) ulCount );

// 	ulCount = ( uint32_t ) pvTimerGetTimerID( xTimer );

// 	IMSG("After : ulCount = %u", ulCount);
// }

// vConfigureTickInterrupt
static enum itr_return schedule_tasks(struct itr_handler *handler __unused){
	
	IMSG("I'm interrupting");
	struct task *t;

	// Clearing interrupt.
	io_write32(sched.epit_base + EPITSR, 0x1);

	CIRCLEQ_FOREACH(t, &sched.task_list, entries)
	{
		IMSG("Executing task %s, call no. %d\n", t->name, count);
		t->func();
		count++;
	}

	return ITRR_HANDLED;
}

static struct itr_handler schedule_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = schedule_tasks,
};
DECLARE_KEEP_PAGER(schedule_itr);

static void lol(void)
{
	IMSG("---");
	TickType_t temp = xTaskGetTickCount();
	IMSG("LOL func = %u", temp);
}

// static void pop(void)
// {	
// 	IMSG("---");
// 	if( xTimerIsTimerActive( xTimers ) != pdFALSE ){
// 		TickType_t expiryTime = xTimerGetExpiryTime(xTimers);
// 	IMSG("expiryTime = %u", expiryTime);
// 	}
// 	else{
// 		// If xTimers isn't active, we start it
// 		IMSG("xTimers start...");
// 		if( xTimerStart( xTimers, 0 ) != pdPASS )
// 		{
// 			IMSG("The timer Timer_test could not be set into the Active state.");
// 		}
// 	}
// }

// void xTimers_init(void){

// 	xTimers = xTimerCreate( "Timer_test", 10, pdTRUE, ( void * ) 0, vTimerCallback);

// 	if( xTimers == NULL ){
// 		IMSG("The timer Timer_test was not created.");
// 	}
// }

static TEE_Result scheduler_init(void)
{
	IMSG("Start Adding timer interrupt\n");
	sched.epit_base = (vaddr_t)phys_to_virt_io(EPIT_BASE_PA, 0x1);
	uint32_t config = EPITCR_CLKSRC_REF_LOW | EPITCR_IOVW |
			  EPITCR_PRESC(32) | EPITCR_RLD | EPITCR_OCIEN |
			  EPITCR_ENMOD;

	io_write32(sched.epit_base + EPITCR, 0x0);
	io_write32(sched.epit_base + EPITCR, config);
	io_write32(sched.epit_base + EPITSR, 0x1);
	io_write32(sched.epit_base + EPITLR, 3000);

	io_write32(sched.epit_base + EPITCMPR, 3000);
	// Only enabling EPIT now as explained in 24.5.1 in manual.
	io_write32(sched.epit_base + EPITCR, io_read32(sched.epit_base + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);

	IMSG("End Added timer interrupt\n");

	register_task("test", lol);
	// register_task("test2", pop);

	/* --- */
	IMSG("Start scheduler\n");
	vTaskStartScheduler();
	IMSG("End Start scheduler\n");
	/* -- */

	return TEE_SUCCESS;
}

void vConfigureTickInterrupt(void){

	static int is_enable = 0;
	
	if(!is_enable){
		IMSG("---");
		IMSG("Enable EPIT TIMER");
		is_enable = 1;
		register_task("FreeRTOS_timer", vConfigureTickInterrupt);
		itr_enable(schedule_itr.it);
		IMSG("End enable EPIT TIMER");
		IMSG("---");
	}
	else{
		IMSG("-- I'm interrupting for FreeRTOS --");
	}
}

boot_final(scheduler_init);