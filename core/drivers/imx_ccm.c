#include <imx-regs.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <stdint.h>
#include <trace.h>

#include <drivers/imx_ccm.h>

void read_clk_settings(void)
{
	vaddr_t ccm_base = (vaddr_t)phys_to_virt(CCM_BASE, MEM_AREA_IO_SEC, CCM_SIZE);

	IMSG("Printing clock settings for IPG_CLK_ROOT & PERCLK_CLK_ROOT\n");
	IMSG("  Specifically for EPIT:\n\n");
	IMSG("  CCSR[PLL3_SW_CLK_SEL]\n");
	IMSG("           (%x)\n", io_read32(ccm_base + CCM_CCSR) & BM_CCM_CCSR_PLL3_SW_CLK_SEL >> BS_CCM_CCSR_PLL3_SW_CLK_SEL);
	IMSG("            |\n");
	IMSG("  CBCMR[PERIPH_CLK2_SEL]        CBCMR[PRE_PERIPH_CLK_SEL]\n");
	IMSG("           (%x)                            (%x)\n", io_read32(ccm_base + CCM_CBCMR) & BM_CCM_CBCMR_PERIPH_CLK2_SEL >> BS_CCM_CBCMR_PERIPH_CLK2_SEL,
								  io_read32(ccm_base + CCM_CBCMR) & BM_CCM_CBCMR_PRE_PERIPH_CLK_SEL >> BS_CCM_CBCMR_PRE_PERIPH_CLK_SEL);
	IMSG("            |                               |\n");
	IMSG("  CBCDR[PERIPH_CLK2_PODF]                   |\n");
	IMSG("           (%x)                              |\n", io_read32(ccm_base + CCM_CBCDR) & BM_CCM_CBCDR_PERIPH_CLK2_PODF >> BS_CCM_CBCDR_PERIPH_CLK2_PODF);
	IMSG("            |                               |\n");
	IMSG("  CBCDR[PERIPH_CLK_SEL] --------------------┘\n");
	IMSG("           (%x)\n", io_read32(ccm_base + CCM_CBCDR) & BM_CCM_CBCDR_PERIPH_CLK_SEL >> BS_CCM_CBCDR_PERIPH_CLK_SEL);
	IMSG("            |\n");
	IMSG("    CBCDR[AHB_PODF] --------------------- AHB_CLK_ROOT\n");
	IMSG("           (%x)\n", io_read32(ccm_base + CCM_CBCDR) & BM_CCM_CBCDR_AHB_PODF >> BS_CCM_CBCDR_AHB_PODF);
	IMSG("            |\n");
	IMSG("    CBCDR[IPG_PODF] ----- CCGR1[CG6] ---- IPG_CLK_ROOT\n");
	IMSG("           (%x)              (%x)\n", io_read32(ccm_base + CCM_CBCDR) & BM_CCM_CBCDR_IPG_PODF >> BS_CCM_CBCDR_IPG_PODF,
						    io_read32(ccm_base + CCM_CCGR1) & BM_CCM_CCGR1_EPIT1S >> BS_CCM_CCGR1_EPIT1S);
	IMSG("            |\n");
	IMSG("  CSCMR1[PERCLK_PODF] --- CCGR1[CG6] ---- PERCLK_CLK_ROOT\n");
	IMSG("           (%x)              (%x)\n", io_read32(ccm_base + CCM_CSCMR1) & BM_CCM_CSCMR1_PERCLK_PODF >> BS_CCM_CSCMR1_PERCLK_PODF,
						    io_read32(ccm_base + CCM_CCGR1) & BM_CCM_CCGR1_EPIT1S >> BS_CCM_CCGR1_EPIT1S);
}

void ungate_clk(uint32_t offset, uint32_t shift) {
	vaddr_t ccm_base = (vaddr_t)phys_to_virt(CCM_BASE, MEM_AREA_IO_SEC, CCM_SIZE);

	uint32_t gate = io_read32(ccm_base + offset);
	gate |= 0b11 << shift;
	io_write32(ccm_base + offset, gate);
}