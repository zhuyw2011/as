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
#define AS_LOG_ELF64 1
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static boolean ELF64_GetDYNVirtualAddress(void* elfFile, Elf64_Addr *vstart_addr, Elf64_Addr *vend_addr)
{
	uint32_t i;
	boolean has_vstart;
	Elf64_Ehdr *fileHdr = elfFile;
	Elf64_Phdr *phdr = elfFile + fileHdr->e_phoff;

	has_vstart = FALSE;
	for(i=0; i < fileHdr->e_phnum; i++)
	{
		if(phdr[i].p_type != PT_LOAD)
		{
			continue;
		}
		ASLOG(ELF64, ("LOAD segment: %d,  0x%08X,  0x%08x\n",
				i, phdr[i].p_vaddr, phdr[i].p_memsz));
		if(phdr[i].p_memsz < phdr[i].p_filesz)
		{
			ASLOG(ERROR, ("invalid elf: segment %d: p_vaddr: %d, p_memsz: %d\n",
					i, phdr[i].p_vaddr, phdr[i].p_memsz));
			has_vstart = FALSE;
			break;
		}
		if (!has_vstart)
		{
			*vstart_addr = phdr[i].p_vaddr;
			*vend_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
			has_vstart = TRUE;
			if (*vend_addr < *vstart_addr)
			{
				ASLOG(ERROR, ("invalid elf: segment %d: p_vaddr: %d, p_memsz: %d\n",
						i, phdr[i].p_vaddr, phdr[i].p_memsz));
				has_vstart = FALSE;
				break;
			}
		}
		else
		{
			if (phdr[i].p_vaddr < *vend_addr)
			{
				ASLOG(ERROR, ("invalid elf: segment should be sorted and not overlapped\n"));
				has_vstart = FALSE;
				break;
			}
			if (phdr[i].p_vaddr > *vend_addr + 16)
			{
				/* There should not be too much padding in the object files. */
				ASWARNING(("too much padding before segment %d\n", i));
			}

			*vend_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
			if (*vend_addr < phdr[i].p_vaddr)
			{
				ASLOG(ERROR, ("invalid elf: "
						"segment %d address overflow\n", i));
				has_vstart = FALSE;
				break;
			}
		}
	}

	if(*vstart_addr >= *vend_addr)
	{
		ASLOG(ERROR, ("invalid eld: start=%08X end=%08X\n",
				*vstart_addr, *vend_addr));
		has_vstart = FALSE;
	}

	return has_vstart;
}

static boolean ELF64_LoadDYNObject(void* elfFile,ELF_ObjectType* elfObj)
{
	boolean r = TRUE;
	uint32_t i;
	Elf64_Ehdr *fileHdr = elfFile;
	Elf64_Phdr *phdr = elfFile + fileHdr->e_phoff;
	Elf64_Shdr *shdr = elfFile + fileHdr->e_shoff;

	for(i=0; i < fileHdr->e_phnum; i++)
	{
		if(PT_LOAD == phdr[i].p_type)
		{
			memcpy(elfObj->space + phdr[i].p_vaddr - (unsigned long)elfObj->vstart_addr,
					elfFile + phdr[i].p_offset, phdr[i].p_filesz);
		}
	}

	elfObj->entry = elfObj->space + fileHdr->e_entry - (unsigned long)elfObj->vstart_addr;
	ASLOG(ELF64, ("entry is %p\n", elfObj->entry));
	/* handle relocation section */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		uint32_t j, nr_reloc;
		Elf64_Sym *symtab;
		Elf64_Rel *rel;
		uint8_t *strtab;

		if(SHT_REL == shdr[i].sh_type)
		{
			/* get relocate item */
			rel = (Elf64_Rel *)(elfFile + shdr[i].sh_offset);
			/* locate .rel.plt and .rel.dyn section */
			symtab = (Elf64_Sym *)(elfFile + shdr[shdr[i].sh_link].sh_offset);
			strtab = (uint8_t *)(elfFile +
					shdr[shdr[shdr[i].sh_link].sh_link].sh_offset);
			nr_reloc = (uint32_t)(shdr[i].sh_size / sizeof(Elf64_Rel));
			/* relocate every items */
			for (j = 0; j < nr_reloc; j ++)
			{
				Elf64_Sym *sym = &symtab[ELF64_R_SYM(rel->r_info)];

				ASLOG(ELF64, ("relocate symbol %s shndx %d\n",
						strtab + sym->st_name, sym->st_shndx));
				if ((sym->st_shndx != SHT_NULL) ||
					(ELF64_ST_BIND(sym->st_info) == STB_LOCAL) ||
					((ELF64_ST_BIND(sym->st_info) == STB_GLOBAL) &&
					 (ELF64_ST_TYPE(sym->st_info) == STT_OBJECT)) )
				{
					ELF64_Relocate(elfObj, rel,
								(Elf64_Addr)(elfObj->space
										+ sym->st_value
										- (unsigned long)elfObj->vstart_addr));
				}
				else
				{
					Elf64_Addr addr;

					/* need to resolve symbol in kernel symbol table */
					addr = (Elf64_Addr)ELF_FindSymbol((const char *)(strtab + sym->st_name));
					if (addr == 0)
					{
						ASLOG(ERROR,("ELF: can't find %s in kernel symbol table\n",
								strtab + sym->st_name));
						r = FALSE;
					}
					else
					{
						ELF64_Relocate(elfObj, rel, addr);
					}

				}
				rel ++;
			}
		}
	}

	return r;
}

static void ELF64_ConstructSymbolTable(void* elfFile, const char* byname, ELF_ObjectType* elfObj)
{
	uint32_t i;
	Elf64_Ehdr *fileHdr = elfFile;
	Elf64_Shdr *shdr = elfFile + fileHdr->e_shoff;

	/* construct module symbol table */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		/* find .dynsym section */
		uint8_t *shstrab;
		shstrab = elfFile + shdr[fileHdr->e_shstrndx].sh_offset;
		if (0 == strcmp((const char *)(shstrab + shdr[i].sh_name), byname))
			break;
	}
	/* found .dynsym section */
	if (i != fileHdr->e_shnum)
	{
		uint32_t j, count = 0;
		Elf64_Sym  *symtab = NULL;
		uint8_t *strtab = NULL;
		void* strpool;

		symtab = elfFile + shdr[i].sh_offset;
		strtab = elfFile + shdr[shdr[i].sh_link].sh_offset;

		strpool = elfObj->symtab + elfObj->nsym*sizeof(ELF_SymtabType);

		for (j = 0, count = 0; j < shdr[i].sh_size / sizeof(Elf64_Sym); j++)
		{
			size_t length;

			if ((ELF64_ST_BIND(symtab[j].st_info) != STB_GLOBAL) ||
				(ELF64_ST_TYPE(symtab[j].st_info) != STT_FUNC))
				continue;

			length = strlen((const char *)(strtab + symtab[j].st_name)) + 1;

			elfObj->symtab[count].addr =
				(void *)(elfObj->space + symtab[j].st_value);
			elfObj->symtab[count].name = strpool;
			strpool += length;
			memset((void *)elfObj->symtab[count].name, 0, length);
			memcpy((void *)elfObj->symtab[count].name,
					  strtab + symtab[j].st_name,
					  length);
			ASLOG(ELF64, ("symtab[%d] %s %p\n", count,
						elfObj->symtab[count].name,
						elfObj->symtab[count].addr));
			count ++;
		}
	}
	else
	{
		elfObj->symtab = NULL;
		elfObj->nsym = 0;
	}
}

static uint32_t ELF64_GetSymbolTableSize(void* elfFile, const char* byname, uint32_t *symtabCount)
{
	uint32_t i,sz;
	Elf64_Ehdr *fileHdr = elfFile;
	Elf64_Shdr *shdr = elfFile + fileHdr->e_shoff;

	/* construct module symbol table */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		/* find .dynsym section */
		uint8_t *shstrab;
		shstrab = elfFile + shdr[fileHdr->e_shstrndx].sh_offset;
		if (0 == strcmp((const char *)(shstrab + shdr[i].sh_name), byname))
			break;
	}
	/* found .dynsym section */
	if (i != fileHdr->e_shnum)
	{
		uint32_t j, count = 0;
		Elf64_Sym  *symtab = NULL;
		uint8_t *strtab = NULL;

		symtab = elfFile + shdr[i].sh_offset;
		strtab = elfFile + shdr[shdr[i].sh_link].sh_offset;

		for (j = 0; j < shdr[i].sh_size / sizeof(Elf64_Sym); j++)
		{
			if ((ELF64_ST_BIND(symtab[j].st_info) == STB_GLOBAL) &&
				(ELF64_ST_TYPE(symtab[j].st_info) == STT_FUNC))
				count ++;
		}

		sz = count * sizeof(ELF_SymtabType);
		*symtabCount = count;

		for (j = 0, count = 0; j < shdr[i].sh_size / sizeof(Elf64_Sym); j++)
		{
			size_t length;

			if ((ELF64_ST_BIND(symtab[j].st_info) != STB_GLOBAL) ||
				(ELF64_ST_TYPE(symtab[j].st_info) != STT_FUNC))
				continue;

			sz += strlen((const char *)(strtab + symtab[j].st_name)) + 1;
			count ++;
		}
	}
	else
	{
		sz = 0;
		*symtabCount = 0;
	}

	return sz;
}

static ELF_ObjectType* ELF64_LoadSharedObject(void* elfFile)
{
	ELF_ObjectType* elfObj = NULL;
	Elf64_Addr vstart_addr, vend_addr;
	uint32_t elf_size;
	uint32_t symtab_size;
	uint32_t nsym;

	if(ELF64_GetDYNVirtualAddress(elfFile, &vstart_addr, &vend_addr))
	{
		elf_size = vend_addr - vstart_addr;

		symtab_size = ELF64_GetSymbolTableSize(elfFile, ELF_DYNSYM, &nsym);

		elfObj = malloc(sizeof(ELF_ObjectType)+elf_size+symtab_size);
		if(NULL != elfObj)
		{
			elfObj->magic = ELF64_MAGIC;
			elfObj->space = &elfObj[1];
			elfObj->symtab = ((void*)&elfObj[1]) + elf_size;
			elfObj->nsym   = nsym;
			elfObj->size  = elf_size;
			elfObj->vstart_addr = (void*)vstart_addr;
			memset(elfObj->space, 0, elf_size);
			if(FALSE == ELF64_LoadDYNObject(elfFile, elfObj))
			{
				free(elfObj);
				elfObj = NULL;
			}
			else
			{
				ELF64_ConstructSymbolTable(elfFile, ELF_DYNSYM, elfObj);
			}
		}
	}

	return elfObj;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void* ELF64_Load(void* elfFile)
{
	void* elf = NULL;
	switch(((Elf64_Ehdr*)elfFile)->e_type)
	{
		case ET_DYN:
			elf = ELF64_LoadSharedObject(elfFile);
			break;
		default:
		break;
	}
	return elf;
}
#endif
