MEMORY
{
    ROM (rx) : ORIGIN = 0xfff00000, LENGTH = 64K
    INV (rwx) : ORIGIN = 0xdeadbeef, LENGTH = 0
}

SECTIONS
{
    .text :
    {
        *(.vectors)
        *(.startup)
        *(.text)
        *(.text.*)
        *(.stub)
        . = ALIGN(4096);
    } > ROM = 0

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4096);
    } > ROM

    .rodata1 : { *(.rodata1) } > ROM

    .sdata2 : { *(.sdata2) *(.sdata2.*) *(.gnu.linkonce.s2.*) } > ROM

    .bss : { *(.bss) } > INV
    .data : { *(.data)             } > INV
}
