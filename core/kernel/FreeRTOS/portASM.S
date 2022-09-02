/*
 * FreeRTOS Kernel V10.4.3 LTS Patch 2
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 * 1 tab == 4 spaces!
 */

	.eabi_attribute Tag_ABI_align_preserved, 1
	.text
	.arm

	.set SYS_MODE,	0x1f
	.set SVC_MODE,	0x13
	.set FIQ_MODE,	0x11

	/* Hardware registers. */
	.extern ulICCEOIR
	.extern ulICCPMR

	/* Variables and functions. */
	.extern ulMaxAPIPriorityMask
	.extern _freertos_vector_table
	.extern pxCurrentTCB
	.extern pxOpteeTopOfStack
	.extern pxOpteeBottomOfStack
	.extern vTaskSwitchContext
	.extern ulPortInterruptNesting
	.extern ulPortTaskHasFPUContext
	.extern vEnableFloatingPointExtensions

	.global FreeRTOS_FIQ_Handler
	.global FreeRTOS_SVC_Handler
	.global vPortRestoreTaskContext
	.global vApplicationIdleHook
	.global	portRESTORE_OPTEE_CONTEXT

.macro portSAVE_CONTEXT
	/* Save the LR and SPSR onto the system mode stack before switching to
	system mode to save the remaining system mode registers. */
	SRSDB	sp!, #SYS_MODE
	CPS		#SYS_MODE
	PUSH	{R0-R12, R14}

	/* Push the critical nesting count. */
	LDR		R2, ulCriticalNestingConst
	LDR		R1, [R2]
	PUSH	{R1}

	/* Does the task have a floating point context that needs saving?  If
	ulPortTaskHasFPUContext is 0 then no. */
	LDR		R2, ulPortTaskHasFPUContextConst
	LDR		R3, [R2]
	CMP		R3, #0

	/* Save the floating point context, if any. */
	FMRXNE  R1,  FPSCR
	VPUSHNE {D0-D15}
	VPUSHNE	{D16-D31}
	PUSHNE	{R1}

	/* Save ulPortTaskHasFPUContext itself. */
	PUSH	{R3}

	/* Save the stack pointer in the TCB. */
	LDR		R0, pxCurrentTCBConst
	LDR		R1, [R0]
	STR		SP, [R1]

	.endm

; /**********************************************************************/

.macro portRESTORE_CONTEXT

	/* Set the SP to point to the stack of the task being restored. */
	LDR		R0, pxCurrentTCBConst
	LDR		R1, [R0]
	LDR		SP, [R1]

	/* Is there a floating point context to restore?  If the restored
	ulPortTaskHasFPUContext is zero then no. */
	LDR		R0, ulPortTaskHasFPUContextConst
	POP		{R1}
	STR		R1, [R0]
	CMP		R1, #0

	/* Restore the floating point context, if any. */
	POPNE 	{R0}
	VPOPNE	{D16-D31}
	VPOPNE	{D0-D15}
	VMSRNE  FPSCR, R0

	/* Restore the critical section nesting depth. */
	LDR		R0, ulCriticalNestingConst
	POP		{R1}
	STR		R1, [R0]

	/* Ensure the priority mask is correct for the critical nesting depth. */
	LDR		R2, ulICCPMRConst
	LDR		R2, [R2]
	CMP		R1, #0
	MOVEQ	R4, #255
	LDRNE	R4, ulMaxAPIPriorityMaskConst
	LDRNE	R4, [R4]
	STR		R4, [R2]

	/* Restore all system mode registers other than the SP (which is already
	being used). */
	POP		{R0-R12, R14}
	
	/* Return to the task code, loading CPSR on the way. */
	RFEIA	sp!

	.endm

; /**********************************************************************/

.macro portRESTORE_FREERTOS_CONTEXT
	/* Push used registers */
	PUSH	{r0-r4, lr}

	/* Increment nesting count.  r3 holds the address of ulPortInterruptNesting
	for future use.  r1 holds the original ulPortInterruptNesting value for
	future use. */
	LDR		r3, ulPortInterruptNestingConst
	LDR		r1, [r3]
	ADD		r4, r1, #1
	STR		r4, [r3]

#ifdef USE_APPLICATION_HANDLER
	/* Be careful of using a hook function in the FreeRTOS handler on the imx6 with OPTEE
	 * - IRQ shouldn't be enalable during the execution
	 * - Be careful of the sp_mode if enabeling FIQ interrupt
	 * - Check if your interrupt handler is the right one triggered */

	/* Ensure bit 2 of the stack pointer is clear.  r2 holds the bit 2 value for
	future use.  _RB_ Does this ever actually need to be done provided the start
	of the stack is 8-byte aligned? */
	MOV		r2, sp
	AND		r2, r2, #4
	SUB		sp, sp, r2

	/* Call the interrupt handler.  r4 pushed to maintain alignment. */
	PUSH	{r0-r4, lr}
	LDR		r1, vApplicationFIQHandlerConst
	BLX		r1
	POP		{r0-r4, lr}
	ADD		sp, sp, r2
#endif

	/* Restore the old nesting count. */
	STR		r1, [r3]

	/* A context switch is never performed if the nesting count is not 0. */
	CMP		r1, #0
	BNE		exit_without_switch_from_optee

	/* Did the interrupt request a context switch?  r1 holds the address of
	ulPortYieldRequired and r0 the value of ulPortYieldRequired for future
	use. */
	LDR		r0, =ulPortYieldRequired
	LDR		r1, [r0]
	CMP		r1, #0
	BNE		switch_before_exit_from_optee

exit_without_switch_from_optee:
	/* No context switch.  Restore used registers, LR and SPSR before
	returning. */
	POP		{r0-r4, lr}
	
	CPS		#SYS_MODE
	/* Restore first task context */
	portRESTORE_CONTEXT

switch_before_exit_from_optee:
	/* A context switch is to be performed.  Clear the context switch pending
	flag. */
	MOV		r1, #0
	STR		r1, [r0]

	/* Restore used registers, LR-irq and SPSR before saving the context
	to the task stack. */
	POP		{r0-r4, lr}

	CPS		#SYS_MODE
	
	/* Call the function that selects the new task to execute.
	vTaskSwitchContext() if vTaskSwitchContext() uses LDRD or STRD
	instructions, or 8 byte aligned stack allocated data.  LR does not need
	saving as a new LR will be loaded by portRESTORE_CONTEXT anyway. */
	LDR		r0, vTaskSwitchContextConst
	BLX		r0

	/* Restore the context of, and branch to, the task selected to execute
	next. */
	portRESTORE_CONTEXT

	.endm

; /**********************************************************************/

.macro portSAVE_OPTEE_REG

	/* Set sp of OPTEE Stack */
	CPS 	#SYS_MODE
	LDR		sp, pxOpteeTopOfStackConst
	LDR		sp, [sp]
	CPS		#SVC_MODE

	/* Store lr and SPSR, we will overwrite the SPSR by the CPSR just after*/
	SRSDB	sp, #SYS_MODE
	CPS 	#SYS_MODE
	
	/* Save the right CPSR, because we change */
	MRS		lr, CPSR
	BIC		lr, lr, #0x1f
	ORR		lr, lr,	#SVC_MODE 
	PUSH	{lr}
	SUB		sp, sp, #4

	/* Save registers */
	PUSH	{r0-r12, lr}

	/* Save the sp */
	LDR		r1, pxOpteeBottomOfStackConst
	STR		sp, [r1]

	.endm

; /**********************************************************************/

.macro portRESTORE_OPTEE_REG

	/* Set sp of OPTEE Stack */
	LDR		sp, pxOpteeBottomOfStackConst
	LDR		sp, [sp]
	
	/* Restore registers */
	POP		{r0-r12, lr}

	/* Return to the task code, loading CPSR on the way. */
	RFEIA 	sp

	.endm

; /**********************************************************************/

/******************************************************************************
 * SVC handler is used to start the scheduler.
 *****************************************************************************/
.align 4
.type FreeRTOS_SVC_Handler, %function
FreeRTOS_SVC_Handler:
	/* Restore saved register in __thread_svc_handler */
	POP		{r0}
	POP		{r0-r7, lr}

	/* Save the context of the current task and select a new task to run. */
	portSAVE_CONTEXT
	
	LDR 	r0, vTaskSwitchContextConst
	BLX		r0

	portRESTORE_CONTEXT

/******************************************************************************
 * vPortRestoreTaskContext is used to start the scheduler.
 *****************************************************************************/
.type vPortRestoreTaskContext, %function
vPortRestoreTaskContext:
	/* Switch to system mode. */
	CPS		#SYS_MODE
	portRESTORE_CONTEXT

.align 4
.type FreeRTOS_FIQ_Handler, %function
FreeRTOS_FIQ_Handler:
	/* Save used registers */
	PUSH	{r0-r4, lr}
	
	/* Check status of scheduler */
	LDR		r1, =xSchedulerRunning
	LDR		r1, [r1]
	CMP		r1, #1

	/* Write the value read from ICCIAR to ICCEOIR. */
	LDRNE 	r2, ulICCEOIRConst
	LDRNE	r2, [r2]
	/* r0 hold the ICCIAR passed in parameter in gic_it_handle */
	STRNE	r0, [r2]
	/* Return if scheduler isn't running */		
	POPNE	{r0-r4, pc}

	/* If called from OPTEE Secure Monitor, save OPTEE context for futur 
	   context switching between FreeRTOS and OPTEE */
	LDR		r1, =uSwitchFromOPTEE
	LDR		r2, [r1]
	CMP		r2, #1
	BNE		keep_running
	
save_optee_context:
	/* Reset uSwitchFromOPTEE */
	MOV		r2, #0
	STR		r2, [r1]

	/* Write the value read from ICCIAR to ICCEOIR. */
	LDR 	r1, ulICCEOIRConst
	LDR		r1, [r1]
	/* r0 hold the ICCIAR passed in parameter in gic_it_handle */
	STR		r0, [r1]

	/* Restore OPTEE registers */
	POP		{r0-r4, lr}

	portSAVE_OPTEE_REG

	/* SRestoring FreeRTOS context */
	portRESTORE_FREERTOS_CONTEXT

keep_running:
	/* Restore used registers */
	POP		{r0-r4, lr}

#ifdef USE_APPLICATION_HANDLER
	/* Be careful of using a hook function in the FreeRTOS handler on the imx6 with OPTEE
	 * - IRQ shouldn't be enalable during the execution
	 * - Be careful of the sp_mode if enabeling FIQ interrupt
	 * - Check if your interrupt handler is the right one triggered */

	/* Push the return address and SPSR. */
	PUSH	{lr}
	MRS		lr, SPSR
	PUSH	{lr}

	/*	PUSH register into the stack in FIQ mode */
	/* Change to supervisor mode to allow reentry. */
	CPS		#SVC_MODE

	/* FIQ and SVC share the same stack so we need do adjust the sp 
	   by number of elemt pushed * 4 */
	SUB		sp, sp, #48
#endif
	
	/* Push used registers. */
	PUSH	{r0-r4, r12}

	/* Increment nesting count.  r3 holds the address of ulPortInterruptNesting
	for future use.  r1 holds the original ulPortInterruptNesting value for
	future use. */
	LDR		r3, ulPortInterruptNestingConst
	LDR		r1, [r3]
	ADD		r4, r1, #1
	STR		r4, [r3]

#ifdef USE_APPLICATION_HANDLER
	/* Be careful of using a hook function in the FreeRTOS handler on the imx6 with OPTEE
	 * - IRQ shouldn't be enalable during the execution
	 * - Be careful of the sp_mode if enabeling FIQ interrupt
	 * - Check if your interrupt handler is the right one triggered */

	/* Ensure bit 2 of the stack pointer is clear.  r2 holds the bit 2 value for
	future use.  _RB_ Does this ever actually need to be done provided the start
	of the stack is 8-byte aligned? */
	MOV		r2, sp
	AND		r2, r2, #4
	SUB		sp, sp, r2

	/* Call the interrupt handler.  r4 pushed to maintain alignment. */
	PUSH	{r0-r4, lr}
	LDR		r1, vApplicationFIQHandlerConst
	BLX		r1
	POP		{r0-r4, lr}
	ADD		sp, sp, r2
#endif

	/* Write the value read from ICCIAR to ICCEOIR. */
	LDR 	r2, ulICCEOIRConst
	LDR		r2, [r2]
	/* r0 hold the ICCIAR passed in parameter in gic_it_handle */
	STR		r0, [r2]

	/* Restore the old nesting count. */
	STR		r1, [r3]

	/* A context switch is never performed if the nesting count is not 0. */
	CMP		r1, #0
	BNE		exit_without_switch

	/* Did the interrupt request a context switch?  r1 holds the address of
	ulPortYieldRequired and r0 the value of ulPortYieldRequired for future
	use. */
	LDR		r1, =ulPortYieldRequired
	LDR		r0, [r1]
	CMP		r0, #0

	BNE		switch_before_exit

exit_without_switch:

	/* No context switch.  Restore used registers, LR_irq and SPSR before
	returning. */
	POP		{r0-r4, r12}

#ifdef USE_APPLICATION_HANDLER
	/* Be careful of using a hook function in the FreeRTOS handler on the imx6 with OPTEE
	 * - IRQ shouldn't be enalable during the execution
	 * - Be careful of the sp_mode if enabeling FIQ interrupt
	 * - Check if your interrupt handler is the right one triggered */

	/* Restore original SVC sp */
	ADD		sp, sp, #48

	CPS		#FIQ_MODE

	POP		{lr}
	MSR		SPSR_cxsf, lr
	POP		{lr}
#endif

	/* POP register which are are push in native_intr_handler */
	POP		{r0-r3, r8-r12, lr}

	MOVS	pc, lr

switch_before_exit:
	/* A context switch is to be performed.  Clear the context switch pending
	flag. */
	MOV		r0, #0
	STR		r0, [r1]

	/* Restore used registers, LR-fiq and SPSR before saving the context
	to the task stack. */
	POP		{r0-r4, r12}

#ifdef USE_APPLICATION_HANDLER
	/* Be careful of using a hook function in the FreeRTOS handler on the imx6 with OPTEE
	 * - IRQ shouldn't be enalable during the execution
	 * - Be careful of the sp_mode if enabeling FIQ interrupt
	 * - Check if your interrupt handler is the right one triggered */

	/* Restore original SVC sp */
	ADD		sp, sp, #48

	CPS		#FIQ_MODE

	POP		{lr}
	MSR		SPSR_cxsf, lr
	POP		{lr}
#endif

	/* POP register which are are push in native_intr_handler */
	POP		{r0-r3, r8-r12, lr}
	
	portSAVE_CONTEXT
	/* Call the function that selects the new task to execute.
	vTaskSwitchContext() if vTaskSwitchContext() uses LDRD or STRD
	instructions, or 8 byte aligned stack allocated data.  LR does not need
	saving as a new LR will be loaded by portRESTORE_CONTEXT anyway. */
	LDR		r0, vTaskSwitchContextConst
	BLX		r0
	
	/* Restore the context of, and branch to, the task selected to execute
	next. */
	portRESTORE_CONTEXT

/******************************************************************************
 * portRESTORE_OPTEE_CONTEXT is called when we have no ready task left other 
 * than Idle task, and will leave FreeRTOS et go back to OPTEE code 
 *****************************************************************************/
.align 4
.type portRESTORE_OPTEE_CONTEXT, %function
portRESTORE_OPTEE_CONTEXT:
	/* Reset uSwitchFromOPTEE flag to save OPTEE context at the next interrupt */
	LDR		r0, =uSwitchFromOPTEE
	LDR		r1, [r0]
	MOV		r2, #1
	STR		r2, [r0]

	/* Restore saved register in __thread_svc_handler */
	POP		{r0}
	POP		{r0-r7, lr}

	/* Save FreeRTOS context */
	portSAVE_CONTEXT

	/* Restore processor mode */
	CPS		#SYS_MODE

	portRESTORE_OPTEE_REG

/******************************************************************************
 * If the application provides an implementation of vApplicationFIQHandler(),
 * then it will get called directly without saving the FPU registers on
 * interrupt entry, and this weak implementation of
 * vApplicationFIQHandler() will not get called.
 *
 * If the application provides its own implementation of
 * vApplicationFPUSafeIRQHandler() then this implementation of
 * vApplicationFIQHandler() will be called, save the FPU registers, and then
 * call vApplicationFPUSafeIRQHandler().
 *
 * Therefore, if the application writer wants FPU registers to be saved on
 * interrupt entry their IRQ handler must be called
 * vApplicationFPUSafeIRQHandler(), and if the application writer does not want
 * FPU registers to be saved on interrupt entry their IRQ handler must be
 * called vApplicationFIQHandler().
 *****************************************************************************/

.align 4
.weak vApplicationFIQHandler
.type vApplicationFIQHandler, %function
vApplicationFIQHandler:
	PUSH	{LR}

	VMRS	R1, FPEXC
	ORR		R1, R1, #0x40000000
	VMSR	FPEXC, R1

	FMRX	R1,  FPSCR

	VPUSH	{D0-D15}
	VPUSH	{D16-D31}
	PUSH	{R1}
	
	LDR		R1, vApplicationFPUSafeIRQHandlerConst
	BLX		R1

	POP		{R0}
	VPOP	{D16-D31}
	VPOP	{D0-D15}
	VMSR	FPSCR, R0

	POP 	{PC}

ulICCEOIRConst:	.word ulICCEOIR
ulICCPMRConst: .word ulICCPMR
pxCurrentTCBConst: .word pxCurrentTCB
pxOpteeTopOfStackConst: .word pxOpteeTopOfStack
pxOpteeBottomOfStackConst: .word pxOpteeBottomOfStack
ulCriticalNestingConst: .word ulCriticalNesting
ulPortTaskHasFPUContextConst: .word ulPortTaskHasFPUContext
ulMaxAPIPriorityMaskConst: .word ulMaxAPIPriorityMask
vTaskSwitchContextConst: .word vTaskSwitchContext
portRESTORE_OPTEE_CONTEXTConst:	.word portRESTORE_OPTEE_CONTEXT
vApplicationFIQHandlerConst: .word vApplicationFIQHandler
ulPortInterruptNestingConst: .word ulPortInterruptNesting
vApplicationFPUSafeIRQHandlerConst: .word vApplicationFPUSafeIRQHandler

.end
