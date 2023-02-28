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

#include <generated/asm-defines.h>

	.eabi_attribute Tag_ABI_align_preserved, 1
	.text
	.arm

	.set MODE_MASK,	0x1f
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
	CPS	#SYS_MODE
	PUSH	{R0-R12, R14}

	/* Push the critical nesting count. */
	LDR	R2, ulCriticalNestingConst
	LDR	R1, [R2]
	PUSH	{R1}

	/* Does the task have a floating point context that needs saving?  If
	ulPortTaskHasFPUContext is 0 then no. */
	LDR	R2, ulPortTaskHasFPUContextConst
	LDR	R3, [R2]
	CMP	R3, #0

	/* Save the floating point context, if any. */
	FMRXNE  R1,  FPSCR
	VPUSHNE {D0-D15}
	VPUSHNE	{D16-D31}
	PUSHNE	{R1}

	/* Save ulPortTaskHasFPUContext itself. */
	PUSH	{R3}

	/* Save the stack pointer in the TCB. */
	LDR	R0, pxCurrentTCBConst
	LDR	R1, [R0]
	STR	SP, [R1]
	
	cmp	sp, #0x4e000000
	Blt	.

	.endm

; /**********************************************************************/

.macro portRESTORE_CONTEXT

	/* Set the SP to point to the stack of the task being restored. */
	LDR	R0, pxCurrentTCBConst
	LDR	R1, [R0]
	LDR	SP, [R1]

	/* Is there a floating point context to restore?  If the restored
	ulPortTaskHasFPUContext is zero then no. */
	LDR	R0, ulPortTaskHasFPUContextConst
	POP	{R1}
	STR	R1, [R0]
	CMP	R1, #0

	/* Restore the floating point context, if any. */
	POPNE 	{R0}
	VPOPNE	{D16-D31}
	VPOPNE	{D0-D15}
	VMSRNE  FPSCR, R0

	/* Restore the critical section nesting depth. */
	LDR	R0, ulCriticalNestingConst
	POP	{R1}
	STR	R1, [R0]

	/* Ensure the priority mask is correct for the critical nesting depth. */
	LDR	R2, ulICCPMRConst
	LDR	R2, [R2]
	CMP	R1, #0
	MOVEQ	R4, #255
	LDRNE	R4, ulMaxAPIPriorityMaskConst
	LDRNE	R4, [R4]
	STR	R4, [R2]

	/* Restore all system mode registers other than the SP (which is already
	being used). */
	POP	{R0-R12, R14}

	push 	{r0}
	ldr	r0, [sp, #4]
	cmp	r0, #0x4e000000
	Blt	.
	pop	{r0}
	cmp	sp, #0x4e000000
	Blt	.
	
	/* Return to the task code, loading CPSR on the way. */
	RFEIA	sp!

	.endm

; /**********************************************************************/

.macro portSAVE_OPTEE_REG

	// Get control block
	push	{r0}
	ldr	r0, pxOpteeBottomOfStackConst
	ldr	r0, [r0]

	// Store r0-r12
	stmfd	r0!, {r1-r12}
	pop	{r1}
	stmfd	r0!, {r1}

	// Save SPSR, sp & lr
	mrs	r1, spsr
	stmfd	r0!, {r1, sp, lr}

	// Save CPSR
	mrs	r1, cpsr
	stmfd	r0!, {r1}

	// Save control block end & set system sp to that end
	ldr	r1, pxOpteeTopOfStackConst
	str	r0, [r1]

	.endm

; /**********************************************************************/

.macro portRESTORE_OPTEE_REG

	// Retrieve control block end
	ldr	r0, pxOpteeTopOfStackConst
	ldr	r0, [r0]

	// Restore CPSR
	ldmfd	r0!, {r1}
	msr	cpsr, r1

	// Restore SPSR, sp & lr
	ldmfd	r0!, {r1, sp, lr}
	msr	spsr, r1

	// Restore r0-r12
	ldmfd	r0, {r0-r12}

	// Return from interrupt handler
	bx	lr

	.endm

; /**********************************************************************/

/******************************************************************************
 * SVC handler is used to start the scheduler.
 *****************************************************************************/
.align 4
.type FreeRTOS_SVC_Handler, %function
FreeRTOS_SVC_Handler:
	/* Restore saved register in __thread_svc_handler */
	POP	{r0}
	POP	{r0-r7, lr}

	/* Save the context of the current task and select a new task to run. */
	portSAVE_CONTEXT
	
	LDR 	r0, vTaskSwitchContextConst
	BLX	r0

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
/* void FreeRTOS_FIQ_Handler(uint32_t iar, struct thread_fiq_regs *itr_regs) */
FreeRTOS_FIQ_Handler:
	/* Save used registers */
	PUSH	{r0-r4, lr}

	/* Write the value read from ICCIAR to ICCEOIR. */
	LDR	r1, ulICCEOIRConst
	LDR	r1, [r1]
	STR	r0, [r1]

	/* Check status of scheduler */
	LDR	r1, =xSchedulerRunning
	LDR	r1, [r1]
	CMP	r1, #1
	BNE	FreeRTOS_exit

	/* Determine if called while FreeRTOS tasks were running */
	LDR	r1, =uRunningFreeRTOS
	LDR	r2, [r1]
	CMP	r2, #1
	BEQ	from_freertos
	
from_optee:
	/* Set uRunningFreeRTOS */
	MOV	r2, #1
	STR	r2, [r1]

	/* Restore OPTEE registers and save context */
	POP	{r0-r4, lr}
	portSAVE_OPTEE_REG


	/* Did the interrupt request a context switch?  r1 holds the address of
	ulPortYieldRequired and r0 the value of ulPortYieldRequired for future
	use. */
	LDR	r1, =ulPortYieldRequired
	LDR	r0, [r1]
	CMP	r0, #1
	BNE	restore

	/* A context switch is to be performed.  Clear the context switch pending
	flag. */
	MOV	r0, #0
	STR	r0, [r1]
	B	switch_and_restore

from_freertos:
	/* Did the interrupt request a context switch?  r1 holds the address of
	ulPortYieldRequired and r0 the value of ulPortYieldRequired for future
	use. */
	LDR	r1, =ulPortYieldRequired
	LDR	r0, [r1]
	CMP	r0, #1
	BNE	FreeRTOS_exit

	/* A context switch is to be performed.  Clear the context switch pending
	flag. */
	MOV	r0, #0
	STR	r0, [r1]

	/* Restore used registers before saving the context to the task stack. */
	POP	{r0-r4, lr}

	/* Reset registers which were saved in native_intr_handler */
	add	sp, r1, #THREAD_FIQ_REG_R0
	pop	{r0-r12, lr}
	add	sp, sp, #4
	
	portSAVE_CONTEXT

switch_and_restore:
	cmp	sp, #0x4e000000
	Blt	.

	/* Call the function that selects the new task to execute.
	vTaskSwitchContext() if vTaskSwitchContext() uses LDRD or STRD
	instructions, or 8 byte aligned stack allocated data.  LR does not need
	saving as a new LR will be loaded by portRESTORE_CONTEXT anyway. */
	LDR	r0, vTaskSwitchContextConst
	BLX	r0
	
restore:
	/* Restore the context of, and branch to, the task selected to execute
	next. Ensure system is in SYS mode. */
	CPS	#SYS_MODE
	portRESTORE_CONTEXT

FreeRTOS_exit:
	POP	{r0-r4, pc}


/******************************************************************************
 * portRESTORE_OPTEE_CONTEXT is called when we have no ready task left other 
 * than Idle task, and will leave FreeRTOS et go back to OPTEE code 
 *****************************************************************************/
.align 4
.type portRESTORE_OPTEE_CONTEXT, %function
portRESTORE_OPTEE_CONTEXT:
	/* Reset uRunningFreeRTOS flag to save OPTEE context at the next interrupt */
	LDR	r0, =uRunningFreeRTOS
	MOV	r1, #0
	STR	r1, [r0]

	/* Restore saved register in __thread_svc_handler */
	POP	{r0}
	POP	{r0-r7, lr}

	/* Save FreeRTOS context */
	portSAVE_CONTEXT

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
	ORR	R1, R1, #0x40000000
	VMSR	FPEXC, R1

	FMRX	R1, FPSCR

	VPUSH	{D0-D15}
	VPUSH	{D16-D31}
	PUSH	{R1}
	
	LDR	R1, vApplicationFPUSafeIRQHandlerConst
	BLX	R1

	POP	{R0}
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
