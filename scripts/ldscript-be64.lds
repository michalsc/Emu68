
SECTIONS
{
    . = 0xffffff8000080000;

    __bootstrap_start = . ;

    .text :
    {
        *(.startup)
        PROVIDE(g_note_build_id = .);
        *(.note.gnu.build-id)
        *(.text)
        *(.text.*)
        *(.stub)
        *(.gnu.warning)
        *(.gnu.linkonce.t.*)
        . = ALIGN(2048);
        __vectors_start = . ;
        *(.vectors)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)

        . = ALIGN(32);
        __init_start = .;
        *(.init)
        QUAD(0)

        . = ALIGN(32);
        __boards_start = .;
        *(.boards.z2)
        *(.boards.z3)
        *(.boards)
        QUAD(0)
    }
    .rodata1 : { *(.rodata1) }

    .sdata2 : { *(.sdata2) *(.sdata2.*) *(.gnu.linkonce.s2.*) }

    .data ALIGN(0x1000) :
    {
        *core.bin.o(.data)
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    }

    .data1              : { *(.data1) }
    .eh_frame           : { KEEP (*(.eh_frame)) }
    .gcc_except_table   : { *(.gcc_except_table) }

    .sdata ALIGN(0x1000) :
    {
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
    }

    .trailer ALIGN(4) :
    {
    LONG(0)
    LONG(0)
    LONG(0x01000000)    /* DTOK = 1, please give us device tree */
    LONG(0x04000000)
    LONG(0x44544f4b)    /* DTOK */
    LONG(0x20000000)
    LONG(0x04000000)
    LONG(0x5250544c)
    }

    /* Begin of non-loaded empty sections */
    .mmu ALIGN(0x1000) (NOLOAD) :
    {
        __mmu_start = . ;

        *(.mmu)
    }
    __mmu_end = . ;

    __bss_start = . ;

    .sbss ALIGN(0x1000) :
    {
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
    }

    .sbss2  : { *(.sbss2) *(.sbss2.*) *(.gnu.linkonce.sb2.*) }
    
    .bss ALIGN(0x1000) :
    {
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
    }
    . = ALIGN(0x1000);

    __bss_end = . ;

    _end = . ;

    __bootstrap_end = . ;
}

__mmu_size = (__mmu_end - __mmu_start);
__bss_size = (__bss_end - __bss_start);
