/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2019  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* NOTES: please make sure codes of this file all in the same PPAGE */
/* ============================ [ INCLUDES  ] ====================================================== */
#include "kernel_internal.h"
#include "asdebug.h"
#include "derivative.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_OS 1

#define disable_interrupt() asm sei
#define enable_interrupt()  asm cli
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
extern void StartOsTick(void);
/* ============================ [ DATAS     ] ====================================================== */
uint16 knl_taskindp;
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void Os_PortInit(void)
{
	StartOsTick();
}

void Os_PortActivate(void)
{
	/* get internal resource or NON schedule */
	RunningVar->priority = RunningVar->pConst->runPriority;

	ASLOG(OS, ("%s(%d) is running\n",  RunningVar->pConst->name,
				(uint32)RunningVar->pConst->initPriority));

	CallLevel = TCL_TASK;
	Irq_Enable();

	RunningVar->pConst->entry();

	/* Should not return here */
	TerminateTask();
}

void Os_PortResume(void)
{
	asm {
	pula
	staa $15 /* PPAGE */
	pula
	staa $16 /* RPAGE */
	pula
	staa $17 /* EPAGE */
	pula
	staa $10 /* GPAGE */
	rti
	}
}

void Os_PortInitContext(TaskVarType* pTaskVar)
{
	pTaskVar->context.sp = (void*)((uint32_t)pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize-4);
	pTaskVar->context.pc = Os_PortActivate;
}

void Os_PortIdle(void)
{
	RunningVar = NULL;
	CallLevel = TCL_ISR2;
	do{
		enable_interrupt();
		/* wait for a while */
		asm nop;asm nop;asm nop;asm nop;
		disable_interrupt();
	} while(NULL == ReadyVar);
}
#ifdef OS_USE_PRETASK_HOOK
void Os_PortCallPreTaskHook(void)
{
	unsigned int salvedLevel = CallLevel;
	CallLevel = TCL_PREPOST;
	PreTaskHook();
	CallLevel = salvedLevel;
}
#else
#define Os_PortCallPreTaskHook()
#endif

#ifdef OS_USE_POSTTASK_HOOK
void Os_PortCallPostTaskHook(void)
{
	unsigned int salvedLevel = CallLevel;
	CallLevel = TCL_PREPOST;
	PostTaskHook();
	CallLevel = salvedLevel;
}
#else
#define Os_PortCallPostTaskHook()
#endif

void Os_PortStartDispatch(void)
{
	disable_interrupt();
	if(NULL == ReadyVar)
	{
		Os_PortIdle();
	}

	RunningVar = ReadyVar;

	Os_PortCallPreTaskHook();

	asm {
	ldx RunningVar
	lds 0, x
	jmp [0x2, x]
	}
}

void Os_PortDispatch(void)
{
	asm swi
}


#pragma CODE_SEG __NEAR_SEG NON_BANKED
/* Dispatch Implementation
 * For an interrupt or exception, the stack looks like below
 *   SP+7: PC
 *   SP+5: Y
 *   SP+3: X
 *   SP+1: D
 *   SP  : CCR
 */
interrupt void Isr_SoftwareInterrupt(void)
{
	asm {
	ldaa $10 /* GPAGE */
	psha
	ldaa $17 /* EPAGE */
	psha
	ldaa $16 /* RPAGE */
	psha
	ldaa $15 /* PPAGE */
	psha
	ldx RunningVar
	sts 0, x
	}

	RunningVar->context.pc = Os_PortResume;

	Os_PortCallPostTaskHook();

	Os_PortStartDispatch();
}
#pragma CODE_SEG DEFAULT
