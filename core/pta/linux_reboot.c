#include <kernel/pseudo_ta.h>
#include <tee/tee_supp_plugin_rpc.h>
#include <malloc.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <kernel/misc.h>
#include <imx.h>
#include <sm/psci.h>
#include <kernel/interrupt.h>
#include <string.h>
#include <trace.h>
#include <linux_reboot.h>
#include <kernel/tz_ssvce_pl310.h>
#include <kernel/boot.h>
#include <console.h>

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

static uint8_t image[10 * 1024 * 1024];
static const size_t image_size = sizeof(image);

static enum itr_return turn_cpu_off(struct itr_handler *h);

static struct itr_handler handler = {
	.it = 122,
	.handler = turn_cpu_off,
};
DECLARE_KEEP_PAGER(handler);

static enum itr_return turn_cpu_off(struct itr_handler *h __unused)
{
	vaddr_t gicc_base;

	asm volatile("cpsid i" ::: "memory", "cc");

	gicc_base = core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC, 1);
	io_write32(gicc_base + GICC_EOIR, handler.it);

	psci_cpu_off();
}

static struct boot_args {
	unsigned long nsec_entry;
	uint32_t dt_addr;
	uint32_t args;
} boot_args;

void set_nsec_entry_reboot(unsigned long nsec_entry, unsigned long dt_addr,
			   unsigned long args)
{
	boot_args.nsec_entry = nsec_entry;
	boot_args.dt_addr = dt_addr;
	boot_args.args = args;
}

static void restart_normal_world(void *img, size_t size)
{
	void (*fptr)(unsigned long, unsigned long) = NULL;
	vaddr_t pl310 = pl310_base();

	asm volatile("cpsid i" ::: "memory", "cc");
	IMSG("Disabled IRQ\n");

	// Copy image to normal world
	vaddr_t kernel = (vaddr_t)core_mmu_add_mapping(
		MEM_AREA_RAM_NSEC, boot_args.nsec_entry, size);
	if (!kernel) {
		EMSG("Couldn't map kernel memory!\n");
		return;
	}
	if (virt_to_phys(kernel) != 0x12000000) {
		EMSG("Wrong mapping of kernel memory!\n");
		EMSG("Requested %x\n", boot_args.nsec_entry);
		return;
	}
	IMSG("img (va): %x", img);
	IMSG("img (pa): %x", virt_to_phys(img));
	IMSG("kernel (va): %x", kernel);
	IMSG("kernel (pa): %x", virt_to_phys(kernel));

	char *src = (char *)img;
	char *dst = (char *)kernel;

	while (size--) {
		if (size % 0x100000 == 0) {
			IMSG("size: %zu", size);
			IMSG("src (va): %x", src);
			IMSG("src (pa): %x", virt_to_phys(src));
			IMSG("dst (va): %x", dst);
			IMSG("dst (pa): %x", virt_to_phys(dst));
		}
		*dst++ = *src++;
	}

	IMSG("0x12000000: %x", io_read32(kernel));

	// memcpy_print(kernel, img, size, true);

	/* Disable L2 cache (is this really necessary?)*/

	// // Disable full-line-of-zeros
	// write_actlr(read_actlr() & ~(BIT(3) | BIT(2) | BIT(1)));
	// IMSG("Disabled full-line-of-zeros.\n");

	// // Clean & disable L2 cache
	// cache_op_outer(DCACHE_CLEAN_INV, NULL, 0);
	// io_write32(pl310 + PL310_CTRL, 0);
	// asm volatile("dsb st");
	// IMSG("Cleaned & disabled L2 cache\n");

	// /*  Flush & turn of caching */
	// cache_op_inner(DCACHE_CLEAN_INV, NULL, 0);
	// IMSG("Invoking print function at linux-reboot.c:9b478b\n");
	// // uint32_t cpsr = change_to_normal_world();
	// asm volatile(
	// 	/* Change to monitor mode */
	// 	"mrs	r2, cpsr\n\t"
	// 	"cps 	#0x16\n\t"

	// 	/* Change to non-secure mode */
	// 	"mrc p15, 0, r1, c1, c1, 0\n\t"
	// 	"orr	r1, r1, 0b1\n\t"
	// 	"mcr p15, 0, r1, c1, c1, 0\n\t"

	// 	"mrc	p15, 0, r0, c1, c0, 0	\n\t"
	// 	"bic	r0, r0, #0x1000		\n\t" // SCTLR.I
	// 	"bic	r0, r0, #0x0006		\n\t" // SCTLR.C & SCTLR.A
	// 	"mcr	p15, 0, r0, c1, c0, 0	\n\t"

	// 	// Switch back to secure mode
	// 	"bic	r1, r1, 0b1\n\t"
	// 	"mrc p15, 0, r1, c1, c1, 0\n\t"

	// 	// Switch back to original mode
	// 	"msr	cpsr, r2\n\t"
	// );
	// // change_to_secure_world(cpsr);
	// IMSG("Invoking print function at linux-reboot.c:6c175a\n");
	// cache_op_inner(DCACHE_CLEAN_INV, NULL, 0);
	// IMSG("Cleaned and disabled chache.\n");

	// Do the actual reset
	init_sec_mon(boot_args.nsec_entry);
	fptr = (void *)virt_to_phys(reset_linux);
	fptr(boot_args.dt_addr, boot_args.args);
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
	static void *img;

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
	itr_enable(handler.it);

	va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC, 1);

	// Checking if all cpus have shut down
	for (i = 1; i <= 3; i++) {
		itr_set_affinity(handler.it, 1 << i);
		itr_raise_pi(handler.it);

		while (io_read32(va + SRC_GPR1 + i * 8 + 4) != UINT32_MAX)
			;
		IMSG("Disabling core %d", i);
		imx_set_src_gpr(i, 0);
		val = io_read32(va + SRC_SCR);
		val &= ~BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + i - 1);
		val |= BIT32(SRC_SCR_CORE1_RST_OFFSET + i - 1);
		io_write32(va + SRC_SCR, val);
	}

	itr_disable(handler.it);

	restart_normal_world(img, out_len);

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
