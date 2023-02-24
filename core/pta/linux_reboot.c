#include <kernel/boot.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <io.h>
#include <imx.h>
#include <malloc.h>
#include <mm/core_memprot.h>
#include <sm/psci.h>
#include <string.h>
#include <tee/tee_supp_plugin_rpc.h>

#include <linux_reboot.h>

#define GICC_EOIR (0x010)

#define FS_LOAD_PLUGIN_UUID                                                    \
	{                                                                      \
		0xb9b4b4c7, 0x983f, 0x497a,                                    \
		{                                                              \
			0x9f, 0xdc, 0xf5, 0xdf, 0x86, 0x4f, 0x29, 0xc4         \
		}                                                              \
	}
static const TEE_UUID fs_load_uuid = FS_LOAD_PLUGIN_UUID;

#define FS_LOAD_KERNEL_IMAGE 0

static struct boot_args {
	uint32_t nsec_entry;
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

static enum itr_return turn_cpu_off(struct itr_handler *h);

static struct itr_handler handler = {
	.it = 139,
	.handler = turn_cpu_off,
};
DECLARE_KEEP_PAGER(handler);
bool interrupt_registered = false;

static enum itr_return turn_cpu_off(struct itr_handler *h __unused)
{
	vaddr_t gicc_base;

	asm volatile("cpsid i" ::: "memory", "cc");

	gicc_base = core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC, 1);
	io_write32(gicc_base + GICC_EOIR, handler.it);

	reset_threads();

	psci_cpu_off();

	return ITRR_HANDLED;
}

static uint8_t image_mem_pool[10 * 1024 * 1024];

TEE_Result update_image(void **img, size_t *size)
{
	TEE_Result res;

	IMSG("Getting kernel image size from normal world.\n");

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 NULL, 0, size);
	if (res != TEE_ERROR_SHORT_BUFFER) {
		EMSG("Error invoking fs-load: %x\n", res);
		return res;
	}

	if (!*img) {
		malloc_add_pool(image_mem_pool, sizeof(image_mem_pool));
		*img = malloc(*size);
	} else {
		free(*img);
		*img = malloc(*size);
	}

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 *img, *size, size);
	if (res != TEE_SUCCESS) {
		EMSG("Error invoking fs-load: %x\n", res);
		return res;
	}

	IMSG("Loaded kernel image in secure memory.\n");
	return TEE_SUCCESS;
}

TEE_Result prepare_normal_world(void *img, size_t size)
{
	int i;
	uint32_t src;
	vaddr_t src_base = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC, 1);

	IMSG("Current core: %x", __get_core_pos());

	IMSG("Disabling IRQ\n");
	asm volatile("cpsid if" ::: "memory", "cc");

	IMSG("Disabling all other cpus.");
	if (!interrupt_registered) {
		itr_add(&handler);
		itr_set_priority(handler.it, 1);
		interrupt_registered = true;
	}
	itr_enable(handler.it);

	// Checking if all cpus have shut down
	for (i = 1; i <= 3; i++) {
		IMSG("Disabling core %d", i);

		itr_set_affinity(handler.it, 1 << i);
		itr_raise_pi(handler.it);
		while (io_read32(src_base + SRC_GPR1 + i * 8 + 4) != UINT32_MAX)
			;

		imx_set_src_gpr(i, 0);
		src = io_read32(src_base + SRC_SCR);
		src &= ~BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + i - 1);
		src |= BIT32(SRC_SCR_CORE1_RST_OFFSET + i - 1);
		io_write32(src_base + SRC_SCR, src);
	}

	itr_disable(handler.it);

	reset_threads();

	IMSG("Copying kernel image to normal world memory.\n");
	vaddr_t kernel = (vaddr_t)core_mmu_add_mapping(
		MEM_AREA_RAM_NSEC, boot_args.nsec_entry, size);
	if (!kernel) {
		EMSG("Couldn't map kernel memory!\n");
		return TEE_ERROR_OUT_OF_MEMORY;
	}
	memcpy((void *)kernel, img, size);

	IMSG("Resetting normal world context in secure monitor.");
	struct sm_nsec_ctx *nsec_ctx;
	nsec_ctx = sm_get_nsec_ctx();
	memset(nsec_ctx, 0, sizeof(struct sm_nsec_ctx));

	IMSG("Initializing normal world context in secure monitor.");
	IMSG("  nsec_entry: 0x%x", boot_args.nsec_entry);
	init_sec_mon(boot_args.nsec_entry);

	return TEE_SUCCESS;
}

extern void reset_linux(unsigned long dt_addr, unsigned long args);

void restart_normal_world(void)
{
	IMSG("Executing reboot.");
	IMSG("  dt_addr   : 0x%x", boot_args.dt_addr);
	IMSG("  boot_args : %x", boot_args.args);
	reset_linux(boot_args.dt_addr, boot_args.args);
}