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
#include <stdint.h>
#include "spinlock.h"
#include "smp.h"
#include "mmu.h"
/* ============================ [ MACROS    ] ====================================================== */

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void spin_lock(spinlock_t *lock)
{
	uint32_t val, fail;

#ifdef __AARCH64__
	do {
		asm volatile(
		"1:	ldaxr	%w0, [%2]\n"
		"	cbnz	%w0, 1b\n"
		"	mov	%0, #1\n"
		"	stxr	%w1, %w0, [%2]\n"
		: "=&r" (val), "=&r" (fail)
		: "r" (&lock->v)
		: "cc" );
	} while (fail);
#else
	do {
		asm volatile(
		"1:	ldrex	%0, [%2]\n"
		"	teq	%0, #0\n"
		"	bne	1b\n"
		"	mov	%0, #1\n"
		"	strex	%1, %0, [%2]\n"
		: "=&r" (val), "=&r" (fail)
		: "r" (&lock->v)
		: "cc" );
	} while (fail);
#endif
	smp_mb();
}

void spin_unlock(spinlock_t *lock)
{
	smp_mb();
	lock->v = 0;
}
