#include <stdio.h>

#include "RegisterAllocator.h"
#include "M68k.h"
#include "ARM.h"

uint16_t m68kcode[] = {
    0x0601, 0xef,
    0x0601, 0x05,
    0x7070,
    0xffff,
};
uint32_t armcode[1024];
uint32_t *armcodeptr = armcode;
uint16_t *m68kcodeptr = m68kcode;

uint32_t *EmitINSN(uint32_t *arm_ptr, uint16_t **m68k_ptr)
{
    uint32_t *ptr = arm_ptr;
    uint16_t opcode = (*m68k_ptr)[0];
    uint8_t group = opcode >> 12;

    if (group == 0)
    {
        ptr = EMIT_line0(arm_ptr, m68k_ptr);
    }
    else if ((group & 0xc) == 0 && (group & 3))
    {
        ptr = EMIT_move(arm_ptr, m68k_ptr);
    }
    else if (group == 7)
    {
        ptr = EMIT_moveq(arm_ptr, m68k_ptr);
    }

    /* No progress? Assume undefined instruction and emit udf to trigger exception */
    if (ptr == arm_ptr)
    {
        ptr = arm_ptr;
        *ptr++ = udf(opcode);
        (*m68k_ptr)++;
    }

    return ptr;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fprintf(stderr, "Test\n");
    uint32_t *p = armcodeptr;
    uint32_t *end = armcodeptr;

    while (*m68kcodeptr != 0xffff)
    {
        end = EmitINSN(end, &m68kcodeptr);
    }
    RA_FlushM68kRegs(&end);
    *end++ = bx_lr();

    while (p != end) {
        uint32_t insn = *p++;
        printf("    %02x %02x %02x %02x\n", insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
    }
        

    return 0;
}
