#include <kernel/scheduler.h>
#include <tee_internal_api.h>
#include <kernel/pseudo_ta.h>
#include <kernel/notif.h>
#include <console.h>
#include <linux_reboot.h>
#include <kernel/interrupt.h>

#define PTA_NAME "watchdog.pta"

#define PTA_WATCHDOG_UUID                                                      \
	{                                                                      \
		0xfc93fda1, 0x6bd2, 0x4e6a,                                    \
		{                                                              \
			0x89, 0x3c, 0x12, 0x2f, 0x6c, 0x3c, 0x8e, 0x33         \
		}                                                              \
	}

#define PTA_WATCHDOG_SETUP 0
#define PTA_WATCHDOG_UPDATE 1

static bool alive;
static int32_t notif_value = 10;
static void *img;
static int count = -1;
static bool first = false;

static void task(void)
{
	const char *p;
	const char *alive1 = "\0337\
\033[1;1H\033[38;5;2m████    ████    ████    \
\033[2;1H\033[38;5;2m████    ████    ████    \
\033[3;1H\033[38;5;2m                    ████\
\033[4;1H\033[38;5;2m          Linux     ████\
\033[5;1H\033[38;5;2m████    is alive!!      \
\033[6;1H\033[38;5;2m████                    \
\033[7;1H\033[38;5;2m    ████    ████    ████\
\033[8;1H\033[38;5;2m    ████    ████    ████\
\033[0m\0338";
	const char *alive2 = "\0337\
\033[1;1H\033[38;5;2m    ████    ████    ████\
\033[2;1H\033[38;5;2m    ████    ████    ████\
\033[3;1H\033[38;5;2m████                    \
\033[4;1H\033[38;5;2m████      Linux         \
\033[5;1H\033[38;5;2m        is alive!!  ████\
\033[6;1H\033[38;5;2m                    ████\
\033[7;1H\033[38;5;2m████    ████    ████    \
\033[8;1H\033[38;5;2m████    ████    ████    \
\033[0m\0338";
	const char *dead1 = "\0337\
\033[1;1H\033[38;5;1m████    ████    ████    \
\033[2;1H\033[38;5;1m████    ████    ████    \
\033[3;1H\033[38;5;1m                    ████\
\033[4;1H\033[38;5;1m          Linux     ████\
\033[5;1H\033[38;5;1m████     is dead!       \
\033[6;1H\033[38;5;1m████                    \
\033[7;1H\033[38;5;1m    ████    ████    ████\
\033[8;1H\033[38;5;1m    ████    ████    ████\
\033[0m\0338";
	const char *dead2 = "\0337\
\033[1;1H\033[38;5;1m    ████    ████    ████\
\033[2;1H\033[38;5;1m    ████    ████    ████\
\033[3;1H\033[38;5;1m████                    \
\033[4;1H\033[38;5;1m████      Linux         \
\033[5;1H\033[38;5;1m         is dead!   ████\
\033[6;1H\033[38;5;1m                    ████\
\033[7;1H\033[38;5;1m████    ████    ████    \
\033[8;1H\033[38;5;1m████    ████    ████    \
\033[0m\0338";

	if (alive) {
		alive = false;
		if (first)
			p = alive1;
		else
			p = alive2;
		if (count >= 0)
			count = 0;
	} else {
		if (first)
			p = dead1;
		else
			p = dead2;
		if (count >= 5) {
			count = -1;
			restart_normal_world(img);
		} else if (count >= 0) {
			count++;
		}
	}
	first = !first;
	for (p; *p; p++) {
		console_putc(*p);
	}
	console_flush();

	notif_send_async(notif_value);
}

static bool registered = false;

static TEE_Result setup(void)
{
	TEE_Result res;
	res = update_image(&img);
	if (res != TEE_SUCCESS) {
		EMSG("Error update image: %x\n", res);
		return res;
	}

	if (!registered) {
		register_task("Linux watchdog", task);
		registered = true;
	}

	alive = true;
	return TEE_SUCCESS;
}

static TEE_Result update(void)
{
	alive = true;
	if (count == -1) {
		count = 0;
	}

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext __unused,
				 uint32_t nCommandID,
				 uint32_t nParamTypes __unused,
				 TEE_Param pParams[TEE_NUM_PARAMS] __unused)
{
	switch (nCommandID) {
	case PTA_WATCHDOG_SETUP:
		return setup();
	case PTA_WATCHDOG_UPDATE:
		return update();
	default:
		break;
	}

	return TEE_ERROR_NOT_IMPLEMENTED;
}

static TEE_Result init(void)
{
	alive = false;

	if (!notif_async_is_enabled() || !notif_async_is_started()) {
		EMSG("Notifications are not yet available!!!\n");
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = PTA_WATCHDOG_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_MULTI_SESSION,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = init);