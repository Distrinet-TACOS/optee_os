#include <tee_api_types.h>
#include <kernel/pseudo_ta.h>
#include <kernel/notif.h>
#include <tee/uuid.h>
#include <sys/queue.h>
#include <trace.h>

#include <drivers/secure_controller.h>
#include <drivers/secure_controller_public.h>

#define PTA_NAME "Secure controller"
#define SEC_CONT_UUID                                                          \
	{                                                                      \
		UUID1, UUID2, UUID3,                                           \
		{                                                              \
			UUID4, UUID5, UUID6, UUID7, UUID8, UUID9, UUID10,      \
				UUID11                                         \
		}                                                              \
	}

static uint32_t notif_value = 0;

struct app {
	TEE_UUID uuid;
	bool notify;
	SLIST_ENTRY(app) list;
};
static SLIST_HEAD(app_head, app) apps = SLIST_HEAD_INITIALIZER(apps);

// TEE_Result ssp_register(TEE_UUID uuid)
// {
// 	struct app *entry = malloc(sizeof(struct app));
// 	entry->uuid = uuid;
// 	SLIST_INSERT_HEAD(&apps, entry, list);
// 	return TEE_SUCCESS;
// }

TEE_Result ssp_notify(TEE_UUID uuid)
{
	if (notif_value == 0) {
		return TEE_ERROR_BAD_STATE;
	}

	struct app *entry = malloc(sizeof(struct app));
	entry->uuid = uuid;
	SLIST_INSERT_HEAD(&apps, entry, list);

	notif_send_async(notif_value);

	return TEE_SUCCESS;
}

static TEE_Result get_notif_value(uint32_t ptypes,
				  TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].value.a = notif_value;
	return TEE_SUCCESS;
}

static TEE_Result get_notifying_uuid(uint32_t ptypes,
				     TEE_Param params[TEE_NUM_PARAMS])
{
	int i;
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
				TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	uint8_t *current = params[0].memref.buffer;
	struct app *first;

	// Memory allocated for ALLOCATED_UUIDS apps. Filling these first.
	for (i = 0; i < ALLOCATED_UUIDS && (first = SLIST_FIRST(&apps)); i++) {
		tee_uuid_to_octets(current, &first->uuid);
		SLIST_REMOVE_HEAD(&apps, list);
		free(first);
		current = &current[sizeof(TEE_UUID)];
	}
	params[1].value.a = i;

	// If more apps are present, let the calling application know.
	if (!SLIST_EMPTY(&apps)) {
		params[1].value.b = true;
	} else {
		params[1].value.b = false;
	}
	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext __unused, uint32_t cmd,
				 uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case GET_NOTIF_VALUE:
		return get_notif_value(ptypes, params);
		break;
	case GET_NOTIFYING_UUID:
		return get_notifying_uuid(ptypes, params);
		break;
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
}

static TEE_Result create_entry_point(void)
{
	// Try to get async notification value. Return error if failed.
	if (!notif_async_is_enabled() || !notif_async_is_started()) {
		EMSG("Notifications are not yet available!!!\n");
		return TEE_ERROR_BAD_STATE;
	}

	TEE_Result res = notif_alloc_async_value(&notif_value);
	if (res != TEE_SUCCESS)
		return TEE_ERROR_OUT_OF_MEMORY;
	else {
		IMSG("Notification value: %d.\n", notif_value);
		return TEE_SUCCESS;
	}
}

pseudo_ta_register(.uuid = SEC_CONT_UUID, .name = PTA_NAME,
		   .flags = PTA_MANDATORY_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = create_entry_point);