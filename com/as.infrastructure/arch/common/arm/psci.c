/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2019 AS <parai@foxmail.com>
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
/* ============================ [ INCLUDES  ] ====================================================== */
#include <stdio.h>
#include "asdebug.h"
#include "psci.h"
#include "smp.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
__attribute__((noinline))
int psci_invoke(unsigned long function_id, unsigned long arg0,
		unsigned long arg1, unsigned long arg2)
{
	asm volatile(
		"hvc #0"
	: "+r" (function_id)
	: "r" (arg0), "r" (arg1), "r" (arg2));
	return function_id;
}

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
#ifdef __AARCH64__
	return psci_invoke(PSCI_0_2_FN64_CPU_ON, cpuid, entry_point, 0);
#else
	return psci_invoke(PSCI_0_2_FN_CPU_ON, cpuid, entry_point, 0);
#endif
}

#define PSCI_POWER_STATE_TYPE_POWER_DOWN (1U << 16)
void psci_cpu_die(void)
{
	int err = psci_invoke(PSCI_0_2_FN_CPU_OFF,
			PSCI_POWER_STATE_TYPE_POWER_DOWN, 0, 0);
	ASLOG(ERROR, ("CPU%d unable to power off (error = %d)\n",  smp_processor_id(), err));
}

void psci_sys_reset(void)
{
	psci_invoke(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}
