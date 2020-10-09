/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "DuffCopy.h"
#include "ElfLoader.h"
#include <stdint.h>
#include <stddef.h>

#define D(x) /* x */

typedef uint32_t    Elf32_Addr;
typedef uint16_t    Elf32_Half;
typedef uint32_t    Elf32_Off;
typedef int32_t     Elf32_Sword;
typedef uint32_t    Elf32_Word;

#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_NIDENT   16

#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'

#define ELFCLASS32  1

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_REL  1
#define ET_EXEC 2

#define EM_68K  4

#define R_68K_NONE  0
#define R_68K_32    1
#define R_68K_16    2
#define R_68K_8     3
#define R_68K_PC32  4
#define R_68K_PC16  5
#define R_68K_PC8   6

#define EV_CURRENT  1

typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shstrndx;
} Elf32_Ehdr;

#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_HIRESERVE   0xffff

typedef struct {
    Elf32_Word      sh_name;
    Elf32_Word      sh_type;
    Elf32_Word      sh_flags;
    Elf32_Addr      sh_addr;
    Elf32_Off       sh_offset;
    Elf32_Word      sh_size;
    Elf32_Word      sh_link;
    Elf32_Word      sh_info;
    Elf32_Word      sh_addralign;
    Elf32_Word      sh_entsize;
} Elf32_Shdr;

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_LOPROC      0x70000000
#define SHT_HIPROC      0x7fffffff
#define SHT_LOUSER      0x80000000
#define SHT_HIUSER      0xffffffff

#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_MASKPROC    0xf0000000

#define STN_UNDEF       0

typedef struct {
    Elf32_Word      st_name;
    Elf32_Addr      st_value;
    Elf32_Word      st_size;
    unsigned char   st_info;
    unsigned char   st_other;
    Elf32_Half      st_shndx;
} Elf32_Sym;

#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2
#define STB_LOPROC  13
#define STB_HIPROC  15

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_LOPROC  13
#define STT_HIPROC  15

typedef struct {
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
} Elf32_Rel;

typedef struct {
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
    Elf32_Sword r_addend; 
} Elf32_Rela;

#define ELF32_R_SYM(i) ((i)>>8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))

static int checkHeader(Elf32_Ehdr *eh)
{
    if (
        eh->e_ident[EI_MAG0] != ELFMAG0 ||
        eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 ||
        eh->e_ident[EI_MAG3] != ELFMAG3
    )
    {
        return 0;
    }

    if (
        eh->e_ident[EI_CLASS] != ELFCLASS32 ||
        eh->e_ident[EI_VERSION] != EV_CURRENT ||
        !(eh->e_type == ET_REL || eh->e_type == ET_EXEC) ||
        eh->e_ident[EI_DATA] != ELFDATA2MSB ||
        eh->e_machine != EM_68K
    )
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int GetElfSize(void *file, uint32_t *size_rw, uint32_t *size_ro)
{
    Elf32_Ehdr *eh = (Elf32_Ehdr *)file;
    uint32_t s_ro = 0;
    uint32_t s_rw = 0;

    D(kprintf("[ELF] getElfSize(%p)", eh));

    if (checkHeader(eh))
    {
        Elf32_Shdr *sh = (Elf32_Shdr *)((intptr_t)eh + eh->e_shoff);
        int i;

        for (i = 0; i < eh->e_shnum; i++)
        {
            /* Does the section require memoy allcation? */
            if (sh[i].sh_flags & SHF_ALLOC)
            {
                uint32_t size = (sh[i].sh_size + sh[i].sh_addralign - 1) & ~(sh[i].sh_addralign - 1);

                /*
                    * I extend the section size according to the alignment requirement. However, also the already
                    * measured size has to be aligned poperly. It is so, because the loader has to align the load address later on.
                    */
                if (sh[i].sh_flags & SHF_WRITE)
                {
                    s_rw = (s_rw + sh[i].sh_addralign - 1) & ~(sh[i].sh_addralign - 1);
                    s_rw += size;
                }
                else
                {
                    s_ro = (s_ro + sh[i].sh_addralign - 1) & ~(sh[i].sh_addralign - 1);
                    s_ro += size;
                }
            }
        }
    }
    else
    {
        return 0;
    }
    
    D(kprintf(": ro=%p, rw=%p\n", s_ro, s_rw));

    if (size_ro)
        *size_ro = s_ro;
    if (size_rw)
        *size_rw = s_rw;

    return 1;
}

static uintptr_t ptr_ro;
static uintptr_t ptr_rw;

static int loadHunk(void *mem, Elf32_Shdr *sh)
{
    void *ptr = (void *)0;
        
    /* empty chunk? Who cares :) */
    if (!sh->sh_size) 
        return 1;
        
    /* Allocate a chunk with write access */
    if (sh->sh_flags & SHF_WRITE)
    {       
        ptr_rw = (ptr_rw + (uintptr_t)sh->sh_addralign - 1) & ~((uintptr_t)sh->sh_addralign - 1);
        ptr = (void *)ptr_rw;
        ptr_rw = ptr_rw + sh->sh_size;
    }
    else    
    {       
        /* Read-Only mode? Get the memory from the kernel space, align it accorting to the demand */
        ptr_ro = (ptr_ro + (uintptr_t)sh->sh_addralign - 1) & ~((uintptr_t)sh->sh_addralign - 1);
        ptr = (void *)ptr_ro;
        ptr_ro = ptr_ro + sh->sh_size;
    }
        
    sh->sh_addr = (uintptr_t)ptr;
        
    /* copy block of memory from ELF file if it exists */
    if (sh->sh_type != SHT_NOBITS)
    {       
        memcpy(ptr, (void*)((uintptr_t)mem + sh->sh_offset), sh->sh_size);
    }
    else
    {
        bzero(ptr, sh->sh_size);
    }

    return 1;
}

/* Perform relocations of given section */
static int relocate(Elf32_Ehdr *eh, Elf32_Shdr *sh, int shrel_idx)
{
    Elf32_Shdr *shrel = &sh[shrel_idx];
    Elf32_Shdr *shsymtab = &sh[shrel->sh_link];
    Elf32_Shdr *toreloc = &sh[shrel->sh_info];
        
    int is_exec = (eh->e_type == ET_EXEC);

    Elf32_Sym *symtab = (Elf32_Sym *)(uintptr_t)shsymtab->sh_addr;
    Elf32_Rela *rel = (Elf32_Rela *)(uintptr_t)shrel->sh_addr;
    uintptr_t section = toreloc->sh_addr;

    D(kprintf("[ELF] sh_size=%d, sh_entsize=%d\n", shrel->sh_size, shrel->sh_entsize));

    unsigned int numrel = (unsigned long)shrel->sh_size
                        / (unsigned long)shrel->sh_entsize;
    unsigned int i;

    Elf32_Sym *SysBase_sym = (void *)0;

    (void)is_exec;

    for (i = 0; i < numrel; i++, rel++)
    {
        Elf32_Sym *sym = &symtab[ELF32_R_SYM(rel->r_info)];
        uint32_t *p = (uint32_t *)(section + rel->r_offset);
        uint32_t s;

        switch (sym->st_shndx)
        {
            case SHN_UNDEF:
                kprintf("[ELF] Undefined symbol '%s' in section '%s'\n",
                                        (char *)(uintptr_t)(sh[shsymtab->sh_link].sh_addr) +
                                        sym->st_name,
                                        (char *)(uintptr_t)(sh[eh->e_shstrndx].sh_addr) +
                                        toreloc->sh_name);
                continue; //return 0;
                
            case SHN_COMMON:
                kprintf("[ELF] COMMON symbol '%s' in section '%s'\n",
                                        (char *)(uintptr_t)(sh[shsymtab->sh_link].sh_addr) +
                                        sym->st_name,
                                        (char *)(uintptr_t)(sh[eh->e_shstrndx].sh_addr) +
                                        toreloc->sh_name);
                        
                return 0;
                
            case SHN_ABS:
                if (SysBase_sym == (void *)0) {
                    if (strcmp((char *)(uintptr_t)(sh[shsymtab->sh_link].sh_addr) + sym->st_name, "SysBase") == 0) {
                        SysBase_sym = sym;
                        goto SysBase_yes;
                    } else
                        goto SysBase_no;
                } else if (SysBase_sym == sym) {
                    SysBase_yes:    s = (uint32_t)4UL;
                } else
                    SysBase_no:     s = sym->st_value;
                break;
            default:
                s = (uint32_t)sh[sym->st_shndx].sh_addr + sym->st_value;
        }

        D(kprintf("[ELF] Relocating symbol %s, type ", sym->st_name ? (char*)(uintptr_t)(sh[shsymtab->sh_link].sh_addr) + sym->st_name : "<unknown>"));
        switch (ELF32_R_TYPE(rel->r_info))
        {
            case R_68K_32:
                D(kprintf("R_68K_32"));
                *p = s + rel->r_addend;
                break;

            case R_68K_PC32:
                D(kprintf("R_68K_PC32"));
                *p = s + rel->r_addend - (uintptr_t)p;
                break;

            case R_68K_NONE:
                D(kprintf("R_68K_NONE"));
                break;

            default:
                kprintf("[ELF] Unknown relocation #%d type %ld\n", i, (long)ELF32_R_TYPE(rel->r_info));
                return 0;
        }
        D(kprintf(" -> %08x\n", (intptr_t)BE32(*p)));
    }
    return 1;
}

void * LoadELFFile(void *mem, void *load_address)
{
    Elf32_Ehdr *elf = (Elf32_Ehdr *)mem;
    uint32_t size_rw;
    uint32_t size_ro;

    if (GetElfSize(elf, &size_rw, &size_ro))
    {
        Elf32_Shdr *sh = (Elf32_Shdr *)((intptr_t)mem + elf->e_shoff);
        ptr_ro = (uintptr_t)load_address;
        ptr_rw = ptr_ro + ((size_ro + 4095) & ~4095);

        for (int i = 0; i < elf->e_shnum; i++)
        {
            /* Load the symbol and string tables */
            if (sh[i].sh_type == SHT_SYMTAB || sh[i].sh_type == SHT_STRTAB)
            {
                sh[i].sh_addr = ((uintptr_t)elf + sh[i].sh_offset);
            }
            /* Does the section require memoy allcation? */
            else if (sh[i].sh_flags & SHF_ALLOC)
            {
                /* Yup, it does. Load the hunk */
                if (!loadHunk(mem, &sh[i]))
                {
                        return 0;
                }
                else
                {
                    if (sh[i].sh_size)
                    {
                        D(kprintf("[ELF] %s section loaded at %08x\n", 
                                  sh[i].sh_flags & SHF_WRITE ? "RW":"RO",
                                  (void*)(intptr_t)(sh[i].sh_addr)));
                    }
                }
            }
        }

        /* For every loaded section perform the relocations */
        for (int i = 0; i < elf->e_shnum; i++)
        {
            if (sh[i].sh_type == SHT_RELA && sh[sh[i].sh_info].sh_addr)
            {
                sh[i].sh_addr = ((intptr_t)elf + sh[i].sh_offset);
                if (!sh[i].sh_addr || !relocate(elf, sh, i))
                {
                    return 0;
                }
            }
        }
    }

    return load_address;
}