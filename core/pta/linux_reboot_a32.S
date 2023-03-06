#include <arm.h>
#include <generated/arm32_sysreg.S>
#include <asm.S>
#include <sm/optee_smc.h>
#include <sm/teesmc_opteed.h>
#include <sm/teesmc_opteed_macros.h>

/* void reset_linux(unsigned long dt_addr, unsigned long args) */
FUNC reset_linux , : , .identity_map
UNWIND( .cantunwind)
	push	{r0, r1}

        /* Change to monitor mode */
	mrs	r1, cpsr
	cps #CPSR_MODE_MON

	/* Change to non-secure mode */
	read_scr r0
        orr	r0, r0, #SCR_NS
        write_scr r0
	
	isb
	read_sctlr r2
	bic	r2, r2, #0x1 // Disable mmu
	write_sctlr r2
	isb

	// Switch back to secure mode
	bic	r0, r0, #SCR_NS
        write_scr r0

	// Switch back to original mode
	msr	cpsr, r1

	pop	{r0, r1}
        mov	r4, #0
	mov	r3, r0 /* device tree address */
        mov	r2, r1 /* boot args */
        mov	r1, #0
        mov	r0, #TEESMC_OPTEED_RETURN_ENTRY_DONE
        smc	#0
        b	.	/* SMC should not return */
END_FUNC reset_linux
