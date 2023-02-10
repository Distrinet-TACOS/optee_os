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

#define EPITCR 0x00
#define EPITSR 0x04
#define EPITLR 0x08
#define EPITCMPR 0x0c
#define EPITCNR 0x10

#define EPITCR_EN BIT(0)
#define EPITCR_ENMOD BIT(1)
#define EPITCR_OCIEN BIT(2)
#define EPITCR_RLD BIT(3)
#define EPITCR_PRESC(x) (((x)&0xfff) << 4)
#define EPITCR_SWR BIT(16)
#define EPITCR_IOVW BIT(17)
#define EPITCR_DBGEN BIT(18)
#define EPITCR_WAITEN BIT(19)
#define EPITCR_RES BIT(20)
#define EPITCR_STOPEN BIT(21)
#define EPITCR_OM_DISCON (0 << 22)
#define EPITCR_OM_TOGGLE (1 << 22)
#define EPITCR_OM_CLEAR (2 << 22)
#define EPITCR_OM_SET (3 << 22)
#define EPITCR_CLKSRC_OFF (0 << 24)
#define EPITCR_CLKSRC_PERIPHERAL (1 << c 24)
#define EPITCR_CLKSRC_REF_HIGH (2 << 24)
#define EPITCR_CLKSRC_REF_LOW (3 << 24)

#define EPITSR_OCIF BIT(0)

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
	struct task *t;

	CIRCLEQ_FOREACH(t, &sched.task_list, entries) {
		if (strcmp(t->name, name)) {
			return;
		}
	}

	t = malloc(sizeof(struct task));
	t->name = strdup(name);
	t->func = func;
	CIRCLEQ_INSERT_TAIL(&sched.task_list, t, entries);
}

void unregister_task(const char *name)
{
	struct task *t;
	CIRCLEQ_FOREACH(t, &sched.task_list, entries) {
		if (strcmp(t->name, name) == 0) {
			CIRCLEQ_REMOVE(&sched.task_list, t, entries);
			free(t);
			break;
		}
	}

}

static enum itr_return schedule_tasks(struct itr_handler *handler __unused)
{
	struct task *t;

	// Clearing EPIT interrupt.
	io_write32(sched.epit_base + EPITSR, 0x1);

	CIRCLEQ_FOREACH(t, &sched.task_list, entries)
	{
		t->func();
	}

	return ITRR_HANDLED;
}

static struct itr_handler schedule_itr = {
	.it = 88,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = schedule_tasks,
};
DECLARE_KEEP_PAGER(schedule_itr);

static TEE_Result scheduler_init(void)
{
	IMSG("Adding timer interrupt\n");

	sched.epit_base = (vaddr_t)phys_to_virt_io(0x20D0000, 1);
	uint32_t config = EPITCR_CLKSRC_REF_LOW | EPITCR_IOVW |
			  EPITCR_PRESC(32) | EPITCR_RLD | EPITCR_OCIEN |
			  EPITCR_ENMOD;

	io_write32(sched.epit_base + EPITCR, 0x0);
	io_write32(sched.epit_base + EPITCR, config);
	io_write32(sched.epit_base + EPITSR, 0x1);
	io_write32(sched.epit_base + EPITLR, 500);
	io_write32(sched.epit_base + EPITCMPR, 500);
	// Only enabling EPIT now as explained in 24.5.1 in manual.
	io_write32(sched.epit_base + EPITCR, io_read32(sched.epit_base + EPITCR) | EPITCR_EN);

	itr_add(&schedule_itr);
	itr_set_affinity(schedule_itr.it, 1);
	itr_set_priority(schedule_itr.it, 0);
	itr_enable(schedule_itr.it);

	IMSG("Added timer interrupt\n");

	return TEE_SUCCESS;
}

boot_final(scheduler_init);