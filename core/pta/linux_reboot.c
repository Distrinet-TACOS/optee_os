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
#include <kernel/unwind.h>
#include <kernel/scheduler.h>
#include <sm/sm.h>

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
	.it = 139,
	.handler = turn_cpu_off,
};
DECLARE_KEEP_PAGER(handler);

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

size_t size;

// static void *imgg;

void restart_normal_world(void *img)
{
	void (*fptr)(unsigned long, unsigned long) = NULL;
	vaddr_t va;
	uint32_t val;
	int i;
	// vaddr_t pl310 = pl310_base();
	uint32_t cpsr;
	vaddr_t gicc_base =
		core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC, 1);

	// unregister_task("Linux watchdog");

	IMSG("Current core: %x", __get_core_pos());
	asm volatile(" mrs %0, cpsr" : "=r"(cpsr) :);
	IMSG("CPSR: 0x%x", cpsr);

	IMSG("Acknowledging and leaving FIQ interrupt mode.");
	// itr_disable(88);
	io_write32(gicc_base + GICC_EOIR, 88);
	asm volatile("cps #0x13" ::: "memory", "cc");
	
	IMSG("Disabling all other cpus.");
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

	reset_threads();

	IMSG("Disabled IRQ\n");
	asm volatile("cpsid i" ::: "memory", "cc");

	IMSG("Copying kernel image to normal world memory.\n");
	vaddr_t kernel = (vaddr_t)core_mmu_add_mapping(
		MEM_AREA_RAM_NSEC, boot_args.nsec_entry, size);
	if (!kernel) {
		EMSG("Couldn't map kernel memory!\n");
		return;
	}
	memcpy((void *)kernel, img, size);
	IMSG("0x12000000: %x", io_read32(kernel));

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
	// print_kernel_stack();

	IMSG("Resetting normal world context in secure monitor.");
	struct sm_nsec_ctx *nsec_ctx;
	nsec_ctx = sm_get_nsec_ctx();
	memset(nsec_ctx, 0, sizeof(struct sm_nsec_ctx));

	IMSG("Initializing normal world context in secure monitor.");
	IMSG("  nsec_entry: 0x%x", boot_args.nsec_entry);
	init_sec_mon(boot_args.nsec_entry);

	IMSG("Executing reboot.");
	IMSG("  dt_addr   : 0x%x", boot_args.dt_addr);
	IMSG("  boot_args : %x", boot_args.args);
	fptr = (void *)virt_to_phys(reset_linux);
	fptr(boot_args.dt_addr, boot_args.args);
}

TEE_Result update_image(void **img)
{
	TEE_Result res;

	IMSG("Getting kernel image size from normal world.\n");

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 NULL, 0, &size);
	if (res != TEE_ERROR_SHORT_BUFFER) {
		EMSG("Error invoking fs-load: %x\n", res);
		return res;
	}

	if (!*img) {
		malloc_add_pool(image, image_size);
		*img = malloc(size);
	} else {
		free(*img);
		*img = malloc(size);
	}
		// imgg = *img;

	res = tee_invoke_supp_plugin_rpc(&fs_load_uuid, FS_LOAD_KERNEL_IMAGE, 0,
					 *img, size, &size);
	if (res != TEE_SUCCESS) {
		EMSG("Error invoking fs-load: %x\n", res);
		return res;
	}

	IMSG("Loaded kernel image in secure memory.\n");
	return TEE_SUCCESS;
}