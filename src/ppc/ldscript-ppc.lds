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
    } > ROM = 0

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    } > ROM

    .rodata1 : { *(.rodata1) } > ROM

    .sdata2 : { *(.sdata2) *(.sdata2.*) *(.gnu.linkonce.s2.*) } > ROM

    .trailer :
    {
        . = ALIGN(0x1000) - 4;
        LONG(-1)
    } > ROM = -1

    .bss : { *(.bss) } > INV
    .data : { *(.data) } > INV
}
