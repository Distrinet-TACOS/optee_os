#include <arm.h>
#include <generated/arm32_sysreg.S>
#include <asm.S>
#include <sm/optee_smc.h>
#include <sm/teesmc_opteed.h>
#include <sm/teesmc_opteed_macros.h>

/* uint32_t change_to_normal_world(void) */
FUNC change_to_normal_world , :
UNWIND( .cantunwind)
	/* Change to monitor mode */
	mrs	r0, cpsr
	cps #CPSR_MODE_MON

	/* Change to non-secure mode */
	read_scr r1
        orr	r1, r1, #SCR_NS
        write_scr r1
	bx	lr
END_FUNC change_to_normal_world

/* void change_to_secure_world(uint32_t cpsr) */
FUNC change_to_secure_world , :
UNWIND( .cantunwind)
	// Switch back to secure mode
	bic	r1, r1, #SCR_NS
        write_scr r1

	// Switch back to original mode
	msr	cpsr, r0
	bx	lr
END_FUNC change_to_secure_world

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

	/* Write 0xc54078 to sctlr */
	isb
	read_sctlr r2
	@ movw	r2, #0x4078
	@ movt	r2, #0xc5
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
