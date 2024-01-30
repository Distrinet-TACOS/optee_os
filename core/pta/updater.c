#include <kernel/pseudo_ta.h>
#include <linux_reboot.h>
#include <updater.h>
#include <string.h>
#include <stdio.h>

// static void *copy(void *dest, const void *src, size_t len)
// {
// 	size_t i = 0;
// 	size_t j = 0;
// 	const struct image *img = src;
// 	struct chunk *chunk = img->response;

// 	char *dst = dest;

// 	// Forward to chunk where image starts.
// 	while (i + chunk->size < img->img_start) {
// 		i += chunk->size;
// 		chunk = chunk->next;
// 	}
// 	// Forward index in chunk to start of image.
// 	i = img->img_start - i;

// 	while (j < len) {
// 		if (i < chunk->size) {
// 			dst[j++] = chunk->data[i++];
// 		} else if (chunk->next) {
// 			i = 0;
// 			chunk = chunk->next;
// 		} else {
// 			return 0;
// 		}
// 	}

// 	return dest;
// }

#include <kernel/scheduler.h>

static TEE_Result update(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_pt =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	DMSG("Entering updater.");
	size_t buffer_len = 80;
	char buffer[buffer_len + 1];
	uint8_t *image = params[0].value.a;
	size_t c;
	size_t i;
	for (i = 0; i < 20; i ++) {
		memset(buffer, 0, buffer_len + 1);
		for (c = 0; c < buffer_len; c += 3) {
			snprintf(&buffer[c], 4, "%02x ", *image++);
		}
		DMSG("%s", buffer);
	}

	DMSG("Preparing Normal World.");
	prepare_normal_world((void *)params[0].value.a, params[0].value.b);
	DMSG("Executing restart.");
	// notify_restart();

	restart_normal_world();

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void __unused *sess_ctx, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd_id) {
	case PTA_UPDATER_CMD_UPDATE:
		return update(param_types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

pseudo_ta_register(.uuid = PTA_UPDATER_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_SINGLE_INSTANCE,
		   .invoke_command_entry_point = invoke_command);