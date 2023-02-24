#include <kernel/pseudo_ta.h>
#include <kernel/interrupt.h>
#include <kernel/notif.h>
#include <kernel/scheduler.h>
#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <console.h>
#include <linux_reboot.h>

#define PTA_NAME "observer.pta"

#define PTA_OBSERVER_UUID                                                      \
	{                                                                      \
		0xfc93fda1, 0x6bd2, 0x4e6a,                                    \
		{                                                              \
			0x89, 0x3c, 0x12, 0x2f, 0x6c, 0x3c, 0x8e, 0x33         \
		}                                                              \
	}

#define PTA_OBSERVER_SETUP 0
#define PTA_OBSERVER_UPDATE 1

static uint32_t notif_value = 0;
static void *img;
static size_t img_size;
static int count = -1;

static int print_output(bool alive, int i)
{
	const char *p;
	const char *alive_str[] = { "\0337\
\033[1;1H\033[38;5;2m████    ████    ████    \
\033[2;1H\033[38;5;2m████    ████    ████    \
\033[3;1H\033[38;5;2m                    ████\
\033[4;1H\033[38;5;2m          Linux     ████\
\033[5;1H\033[38;5;2m████    is alive!!      \
\033[6;1H\033[38;5;2m████                    \
\033[7;1H\033[38;5;2m    ████    ████    ████\
\033[8;1H\033[38;5;2m    ████    ████    ████\
\033[0m\0338",
				    "\0337\
\033[1;1H\033[38;5;2m    ████    ████    ████\
\033[2;1H\033[38;5;2m    ████    ████    ████\
\033[3;1H\033[38;5;2m████                    \
\033[4;1H\033[38;5;2m████      Linux         \
\033[5;1H\033[38;5;2m        is alive!!  ████\
\033[6;1H\033[38;5;2m                    ████\
\033[7;1H\033[38;5;2m████    ████    ████    \
\033[8;1H\033[38;5;2m████    ████    ████    \
\033[0m\0338" };
	const char *dead_str[] = { "\0337\
\033[1;1H\033[38;5;1m████    ████    ████    \
\033[2;1H\033[38;5;1m████    ████    ████    \
\033[3;1H\033[38;5;1m                    ████\
\033[4;1H\033[38;5;1m          Linux     ████\
\033[5;1H\033[38;5;1m████     is dead!       \
\033[6;1H\033[38;5;1m████                    \
\033[7;1H\033[38;5;1m    ████    ████    ████\
\033[8;1H\033[38;5;1m    ████    ████    ████\
\033[0m\0338",
				   "\0337\
\033[1;1H\033[38;5;1m    ████    ████    ████\
\033[2;1H\033[38;5;1m    ████    ████    ████\
\033[3;1H\033[38;5;1m████                    \
\033[4;1H\033[38;5;1m████      Linux         \
\033[5;1H\033[38;5;1m         is dead!   ████\
\033[6;1H\033[38;5;1m                    ████\
\033[7;1H\033[38;5;1m████    ████    ████    \
\033[8;1H\033[38;5;1m████    ████    ████    \
\033[0m\0338" };

	if (alive) {
		p = alive_str[i];
	} else {
		p = dead_str[i];
	}

	for (p; *p; p++) {
		console_putc(*p);
	}
	console_flush();

	return (i + 1) % 2;
}

static void observer(void *pvParameters __unused)
{
	int i = 0;

	while (true) {
		i = print_output(count == 0, i);

		if (count >= 5) {
			count = -1;
			prepare_normal_world(img, img_size);
			notify_restart();
		} else if (count >= 0) {
			count++;
		}

		notif_send_async(notif_value);
		vTaskDelay(5);
	}
}

static bool registered = false;

static TEE_Result setup(void)
{
	TEE_Result res;
	BaseType_t xReturn = pdFAIL;

	res = update_image(&img, &img_size);
	if (res != TEE_SUCCESS) {
		EMSG("Error update image: %x\n", res);
		return res;
	}

	if (!registered) {
		xReturn = xTaskCreate(observer, "Linux observer",
				      configMINIMAL_STACK_SIZE, (void *)NULL,
				      (UBaseType_t)1, NULL);
		if (xReturn != pdPASS) {
			EMSG("Couldn't create Linux observer task.");
			return (TEE_Result)xReturn;
		}
		registered = true;
	}

	count = 0;
	return TEE_SUCCESS;
}

static TEE_Result update(void)
{
	count = 0;
	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext __unused,
				 uint32_t nCommandID,
				 uint32_t nParamTypes __unused,
				 TEE_Param pParams[TEE_NUM_PARAMS] __unused)
{
	switch (nCommandID) {
	case PTA_OBSERVER_SETUP:
		return setup();
	case PTA_OBSERVER_UPDATE:
		return update();
	default:
		break;
	}

	return TEE_ERROR_NOT_IMPLEMENTED;
}

static TEE_Result open_session(uint32_t nParamTypes,
			       TEE_Param pParams[TEE_NUM_PARAMS],
			       void **ppSessionContext __unused)
{
	TEE_Result res;

	if (nParamTypes !=
	    TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE,
			    TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
		return TEE_SUCCESS;

	if (!notif_value) {
		res = notif_alloc_async_value(&notif_value);
		if (res != TEE_SUCCESS) {
			EMSG("Couldn't allocate an async notification value: %x\n",
			     res);
			return res;
		}
	}
	pParams[0].value.a = notif_value;

	return TEE_SUCCESS;
}

static TEE_Result init(void)
{
	if (!notif_async_is_enabled() || !notif_async_is_started()) {
		EMSG("Notifications are not yet available!!!\n");
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = PTA_OBSERVER_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_MULTI_SESSION,
		   .invoke_command_entry_point = invoke_command,
		   .open_session_entry_point = open_session,
		   .create_entry_point = init);