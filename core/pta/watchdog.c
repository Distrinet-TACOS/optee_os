#include <kernel/scheduler.h>
#include <tee_internal_api.h>
#include <kernel/pseudo_ta.h>
#include <kernel/notif.h>

#define PTA_NAME "watchdog.pta"

#define PTA_WATCHDOG_UUID                                                      \
	{                                                                      \
		0xfc93fda1, 0x6bd2, 0x4e6a,                                    \
		{                                                              \
			0x89, 0x3c, 0x12, 0x2f, 0x6c, 0x3c, 0x8e, 0x33         \
		}                                                              \
	}

#define PTA_WATCHDOG_UPDATE 0
#define PTA_WATCHDOG_GET_NOTIF_VALUE 1

static bool alive;
static int32_t notif_value = 10;

static TEE_Result update(void)
{
	alive = true;

	return TEE_SUCCESS;
}

// static TEE_Result get_notif_value(uint32_t ptypes,
// 				  TEE_Param params[TEE_NUM_PARAMS])
// {
// 	uint32_t expected_ptypes =
// 		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
// 				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
// 				TEE_PARAM_TYPE_NONE);

// 	if (ptypes != expected_ptypes)
// 		return TEE_ERROR_BAD_PARAMETERS;

// 	if (notif_value < 0)
// 		return TEE_ERROR_BAD_STATE;

// 	params[0].value.a = (uint32_t)notif_value;
// 	return TEE_SUCCESS;
// }

static TEE_Result invoke_command(void *pSessionContext __unused,
				 uint32_t nCommandID, uint32_t nParamTypes,
				 TEE_Param pParams[TEE_NUM_PARAMS])
{
	switch (nCommandID) {
	// case PTA_WATCHDOG_GET_NOTIF_VALUE:
	// 	return get_notif_value(nParamTypes, pParams);
	// 	break;
	case PTA_WATCHDOG_UPDATE:
		return update();
	default:
		break;
	}

	return TEE_ERROR_NOT_IMPLEMENTED;
}

static void task(void)
{
	if (alive) {
		alive = false;
	} else {
		IMSG("Linux has crashed!!!");
	}

	notif_send_async(notif_value);
}

static TEE_Result init(void)
{
	alive = false;

	if (!notif_async_is_enabled() || !notif_async_is_started()) {
		EMSG("Notifications are not yet available!!!\n");
		return TEE_ERROR_BAD_STATE;
	}

	register_task("Linux watchdog", task);

	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = PTA_WATCHDOG_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_SINGLE_INSTANCE,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = init);