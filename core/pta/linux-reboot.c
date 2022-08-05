#include <kernel/pseudo_ta.h>
#include <tee/tee_supp_plugin_rpc.h>
#include <malloc.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <kernel/misc.h>
#include <imx.h>
#include <sm/psci.h>
#include <kernel/interrupt.h>

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

#define GICC_EOIR (0x010)

static void *img;
static uint8_t image[10 * 1024 * 1024];
static const size_t image_size = sizeof(image);

static enum itr_return turn_cpu_off(struct itr_handler *h);

static struct itr_handler handler = {
	.it = 11,
	.handler = turn_cpu_off,
};
DECLARE_KEEP_PAGER(handler);

static enum itr_return turn_cpu_off(struct itr_handler *h)
{
	vaddr_t gicc_base;

	asm volatile("cpsid i" ::: "memory", "cc");

	gicc_base = core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC, 1);
	io_write32(gicc_base + GICC_EOIR, handler.it);

	psci_cpu_off();
}

static TEE_Result update_image(uint32_t nParamTypes,
			       TEE_Param pParams[TEE_NUM_PARAMS] __unused)
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	unsigned int res;
	size_t out_len;
	vaddr_t va;
	uint32_t val;
	int i;

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
	IMSG("img: %px\n", img);

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

	IMSG("Current core: %x", __get_core_pos());

	itr_add(&handler);
	itr_raise_sgi(handler.it, 0b1110);

	va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC, 1);

	for (i = 1; i <= 3; i++) {
		while (io_read32(va + SRC_GPR1 + i * 8 + 4) != UINT32_MAX)
			;
		IMSG("Disabling core %d", i);
		val = io_read32(va + SRC_SCR);
		val &= ~BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + i - 1);
		val |= BIT32(SRC_SCR_CORE1_RST_OFFSET + i - 1);
		io_write32(va + SRC_SCR, val);
		imx_set_src_gpr(i, 0);
	}

	itr_disable(handler.it);

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
