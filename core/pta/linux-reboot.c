#include <kernel/pseudo_ta.h>
#include <tee/tee_supp_plugin_rpc.h>
#include <initcall.h>
#include <stdlib.h>
#include <malloc.h>
#include <kernel/thread.h>

#define PTA_NAME "linux-reboot.pta"

#define PTA_LINUX_REBOOT_UUID                                                  \
	{                                                                      \
		0x9c694c03, 0xdb3a, 0x4653,                                    \
		{                                                              \
			0xab, 0xc3, 0xe3, 0x95, 0x52, 0x8e, 0x71, 0x1c         \
		}                                                              \
	}

#define FS_LOAD_PLUGIN_UUID                                                    \
	{                                                                      \
		0xb9b4b4c7, 0x983f, 0x497a,                                    \
		{                                                              \
			0x9f, 0xdc, 0xf5, 0xdf, 0x86, 0x4f, 0x29, 0xc4         \
		}                                                              \
	}

static const TEE_UUID fs_load_uuid = FS_LOAD_PLUGIN_UUID;

#define PTA_LINUX_REBOOT_UPDATE 0
#define FS_LOAD_KERNEL_IMAGE 0

static struct mobj *mobj;
static void *img;
static uint8_t image[10 * 1024 * 1024];
static const size_t image_size = sizeof(image);

static TEE_Result update_image(uint32_t nParamTypes,
			       TEE_Param pParams[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	unsigned int res;
	size_t out_len;

	if (nParamTypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("Getting kernel image size from normal world.\n");

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 NULL, 0, &out_len);
	if (res != TEE_ERROR_SHORT_BUFFER) {
		EMSG("Error invoking fs-load: %x\n", res);
	}

	malloc_add_pool(image, image_size);

	img = malloc(out_len);
	IMSG("out_len: %zu\n", out_len);
	IMSG("img: %x\n", img);

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 img, out_len, &out_len);
	if (res != TEE_SUCCESS) {
		EMSG("Error invoking fs-load: %x\n", res);
	}

	IMSG("%x", ((uint8_t *)img)[0]);
	IMSG("%x", ((uint8_t *)img)[1]);
	IMSG("%x", ((uint8_t *)img)[2]);
	IMSG("%x", ((uint8_t *)img)[3]);
	IMSG("%x", ((uint8_t *)img)[4]);
	IMSG("%x", ((uint8_t *)img)[5]);
	IMSG("%x", ((uint8_t *)img)[6]);
	IMSG("%x", ((uint8_t *)img)[7]);
	IMSG("%x", ((uint8_t *)img)[8]);
	IMSG("%x", ((uint8_t *)img)[9]);

	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext __unused,
				 uint32_t nCommandID, uint32_t nParamTypes,
				 TEE_Param pParams[TEE_NUM_PARAMS])
{
	switch (nCommandID) {
	case PTA_LINUX_REBOOT_UPDATE:
		return update_image(nParamTypes, pParams);
	default:
		break;
	}

	return TEE_ERROR_NOT_IMPLEMENTED;
}

static TEE_Result init(void)
{
	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = PTA_LINUX_REBOOT_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_SINGLE_INSTANCE,
		   .invoke_command_entry_point = invoke_command,
		   .create_entry_point = init);
