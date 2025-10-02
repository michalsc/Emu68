void  __attribute__((used,aligned(256),section(".vectors"))) __stub_vectors() {
__asm__(
"       .section .vectors               \n"

"       .org 0x100,0                    \n"
"       .globl SystemReset              \n"
"SystemReset:                           \n"
"       lis %r1, 0xfff0                 \n"
"       bl PPC_C_Init                   \n"
"1:     b 1b                            \n"

"       .org 0x200,0                    \n"
"       .globl MachineCheck             \n"
"MachineCheck:                          \n"
"1:     b 1b                            \n"

"       .org 0x300,0                    \n"
"       .globl DSI                      \n"
"DSI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x400,0                    \n"
"       .globl ISI                      \n"
"ISI:                                   \n"
"1:     b 1b                            \n"

"       .org 0x500,0                    \n"
"       .globl ExternalInt              \n"
"ExternalInt:                           \n"
"1:     b 1b                            \n"

"       .org 0x600,0                    \n"
"       .globl Alignment                \n"
"Alignment:                             \n"
"1:     b 1b                            \n"

"       .org 0x700,0                    \n"
"       .globl Program                  \n"
"Program:                               \n"
"1:     b 1b                            \n"

);
}

int PPC_C_Init(int a, int b)
{
    return a + 123; (void)b;
}
