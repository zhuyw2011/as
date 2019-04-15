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
/* ============================ [ INCLUDES  ] ====================================================== */
#include "elfinternal.h"
#ifdef USE_ELF64
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_ARM64ELF 0
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
int ELF64_Relocate(ELF_ObjectType *elfObj, Elf64_Rel *rel, Elf64_Addr sym_val)
{
	Elf64_Addr *where, tmp;
	Elf64_Sword addend, offset;
	uint32_t upper, lower, sign, j1, j2;

	where = (Elf64_Addr *)(elfObj->space
						   + rel->r_offset
						   - (unsigned long)elfObj->vstart_addr);
	asAssert(where < (Elf64_Addr *)(elfObj->space + elfObj->size));
	switch (ELF32_R_TYPE(rel->r_info))
	{
		case R_ARM_NONE:
			ASLOG(ARM64ELF, ("R_ARM_NONE\n"));
			break;
		default:
			ASLOG(ERROR, ("ARM64ELF: invalid relocate TYPE %d\n", ELF64_R_TYPE(rel->r_info)));
			return -1;
	}

	return 0;

}
#endif /* USE_ELF64 */
