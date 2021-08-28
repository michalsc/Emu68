/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"

/* Line9 is one large SUBX/SUB/SUBA */

uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* SUBA */
    if ((opcode & 0xf0c0) == 0x90c0)
    {
        uint8_t ext_words = 0;
        uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
        uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
        uint8_t tmp = 0xff;
        RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

        if (size == 2)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
#ifdef __aarch64__
        if (size == 2)
            *ptr++ = sxth(tmp, tmp);

        *ptr++ = sub_reg(reg, reg, tmp, LSL, 0);
#else
        if (size == 2)
            *ptr++ = sxth(tmp, tmp, 0);

        *ptr++ = sub_reg(reg, reg, tmp, 0);
#endif
        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;
    }
    /* SUBX */
    else if ((opcode & 0xf130) == 0x9100)
    {
        uint8_t size = (opcode >> 6) & 3;
        /* Move negated C flag to ARM flags */
#ifdef __aarch64__
        uint8_t cc = RA_GetCC(&ptr);
        if (size == 2) {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = mvn_reg(tmp, cc, ROR, 7);
            *ptr++ = set_nzcv(tmp);

            RA_FreeARMRegister(&ptr, tmp);
        } else {
            *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
        }
#else
        M68K_GetCC(&ptr);
        *ptr++ = tst_immed(REG_SR, SR_X);
#endif
        /* Register to register */
        if ((opcode & 0x0008) == 0)
        {
            uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

            switch (size)
            {
                case 0: /* Byte */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 24);
                    *ptr++ = subs_reg(tmp, tmp, regx, LSL, 24);
                    *ptr++ = bfxil(regy, tmp, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 24);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, tmp, tmp, 0x401);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 24);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(regy, tmp, 0, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#endif
                    break;
                case 1: /* Word */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 16);
                    *ptr++ = subs_reg(tmp, tmp, regx, LSL, 16);
                    *ptr++ = bfxil(regy, tmp, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 16);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, tmp, tmp, 0x801);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 16);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(regy, tmp, 0, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#endif
                    break;
                case 2: /* Long */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = sbcs(regy, regy, regx);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = sub_cc_immed(ARM_CC_NE, regy, regy, 1);
                    *ptr++ = subs_reg(regy, regy, regx, 0);
#endif
                    break;
            }
        }
        /* memory to memory */
        else
        {
            uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            uint8_t dest = RA_AllocARMRegister(&ptr);
            uint8_t src = RA_AllocARMRegister(&ptr);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            switch (size)
            {
                case 0: /* Byte */
                    *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                    *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
#ifdef __aarch64__
                    uint8_t tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 24);
                    *ptr++ = subs_reg(dest, dest, src, LSL, 24);
                    *ptr++ = lsr(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 0x401);
                    *ptr++ = rsbs_reg(dest, src, dest, 24);
                    *ptr++ = lsr_immed(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
#endif
                    break;
                case 1: /* Word */
                    *ptr++ = ldrh_offset_preindex(regx, src, -2);
                    *ptr++ = ldrh_offset_preindex(regy, dest, -2);
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = csetm(tmp, A64_CC_NE);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 16);
                    *ptr++ = subs_reg(dest, dest, src, LSL, 16);
                    *ptr++ = lsr(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 0x801);
                    *ptr++ = rsbs_reg(dest, src, dest, 16);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
#endif
                    break;
                case 2: /* Long */
                    *ptr++ = ldr_offset_preindex(regx, src, -4);
                    *ptr++ = ldr_offset_preindex(regy, dest, -4);
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = sbcs(dest, dest, src);
                    *ptr++ = str_offset(regy, dest, 0);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = sub_cc_immed(ARM_CC_NE, dest, dest, 1);
                    *ptr++ = subs_reg(dest, dest, src, 0);
                    *ptr++ = str_offset(regy, dest, 0);
#endif
                    break;
            }

            RA_FreeARMRegister(&ptr, dest);
            RA_FreeARMRegister(&ptr, src);
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVnCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CC);
            }
        }
    }
    /* SUB */
    else if ((opcode & 0xf000) == 0x9000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;
#ifdef __aarch64__
        uint8_t tmp = 0xff;
#endif
        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0xff;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            if (size == 4)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            switch(size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = subs_reg(dest, dest, src, LSL, 0);
#else
                    *ptr++ = subs_reg(dest, dest, src, 0);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 16);
                    *ptr++ = subs_reg(src, tmp, src, LSL, 16);
                    *ptr++ = bfxil(dest, src, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = rsbs_reg(src, src, dest, 16);
                    *ptr++ = lsr_immed(src, src, 16);
                    *ptr++ = bfi(dest, src, 0, 16);
#endif
                    break;
                case 1:
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 24);
                    *ptr++ = subs_reg(src, tmp, src, LSL, 24);
                    *ptr++ = bfxil(dest, src, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = rsbs_reg(src, src, dest, 24);
                    *ptr++ = lsr_immed(src, src, 24);
                    *ptr++ = bfi(dest, src, 0, 8);
#endif
                    break;
            }

            RA_FreeARMRegister(&ptr, src);
        }
        else
        {
            uint8_t dest = 0xff;
            uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t mode = (opcode & 0x0038) >> 3;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

            /* Fetch data into temporary register, perform add, store it back */
            switch (size)
            {
            case 4:
                if (mode == 4)
                {
                    *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldr_offset(dest, tmp, 0);

                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = subs_reg(tmp, tmp, src, 0);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, tmp, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, tmp, 0);
                break;
            case 2:
                if (mode == 4)
                {
                    *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrh_offset(dest, tmp, 0);
                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = lsl(tmp, tmp, 16);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
                *ptr++ = lsr(tmp, tmp, 16);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = subs_reg(tmp, tmp, src, 16);
                *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, tmp, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, tmp, 0);
                break;
            case 1:
                if (mode == 4)
                {
                    *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrb_offset(dest, tmp, 0);

                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = lsl(tmp, tmp, 24);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
                *ptr++ = lsr(tmp, tmp, 24);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = subs_reg(tmp, tmp, src, 24);
                *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, tmp, 0);
                break;
            }

            RA_FreeARMRegister(&ptr, dest);
            RA_FreeARMRegister(&ptr, tmp);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVnCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CC);
            }
        }
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }


    return ptr;
}
/*
1st octet Rn

2nd octet
[0|1|2] = SUB including Dn,Dn
[3] = SUBA.W
[4|5|6] = SUB Dn,Dn range been cannibalized for SUBX
[7] = SUBA.L

3rd octet mode select

4th octet reg/mode
*/
/*
static EMIT_Function JumpTable[4096] = {
	[00000 ... 00007] = EMIT_SUB_reg,  //D0 Destination
	[00020 ... 00047] = EMIT_SUB_mem,
	[00050 ... 00074] = EMIT_SUB_ext,
	[00100 ... 00107] = EMIT_SUB_reg,
	[00120 ... 00147] = EMIT_SUB_mem,
	[00150 ... 00174] = EMIT_SUB_ext,
	[00200 ... 00207] = EMIT_SUB_reg,
	[00220 ... 00247] = EMIT_SUB_mem,
	[00250 ... 00274] = EMIT_SUB_ext,

	[00300 ... 00317] = EMIT_SUBA_reg,
	[00320 ... 00347] = EMIT_SUBA_mem,
	[00350 ... 00374] = EMIT_SUBA_ext, //Word

	[00400 ... 00407] = EMIT_SUBX_reg,
	[00410 ... 00417] = EMIT_SUBX_mem, //R0
	[00500 ... 00507] = EMIT_SUBX_reg,
	[00510 ... 00517] = EMIT_SUBX_mem,
	[00600 ... 00607] = EMIT_SUBX_reg,
	[00610 ... 00617] = EMIT_SUBX_mem,

	[00420 ... 00447] = EMIT_SUB_mem,
	[00450 ... 00471] = EMIT_SUB_ext,  //D0 Source
	[00520 ... 00547] = EMIT_SUB_mem,
	[00550 ... 00571] = EMIT_SUB_ext,
	[00620 ... 00647] = EMIT_SUB_mem,
	[00650 ... 00671] = EMIT_SUB_ext,

	[00700 ... 00717] = EMIT_SUBA_reg,
	[00720 ... 00747] = EMIT_SUBA_mem,
	[00750 ... 00774] = EMIT_SUBA_ext, //Long

[01000 ... 01007] = EMIT_SUB,  //D1 Destination
[01020 ... 01074] = EMIT_SUB,
[01100 ... 01107] = EMIT_SUB,
[01120 ... 01174] = EMIT_SUB,
[01200 ... 01207] = EMIT_SUB,
[01220 ... 01274] = EMIT_SUB,

[01300 ... 01374] = EMIT_SUBA, //Word

[01400 ... 01417] = EMIT_SUBX, //R1
[01500 ... 01517] = EMIT_SUBX,
[01600 ... 01617] = EMIT_SUBX,

[01420 ... 01471] = EMIT_SUB,  //D1 Source
[01520 ... 01571] = EMIT_SUB,
[01620 ... 01671] = EMIT_SUB,

[01700 ... 01774] = EMIT_SUBA, //Long

[02000 ... 02007] = EMIT_SUB,  //D2 Destination
[02020 ... 02074] = EMIT_SUB,
[02100 ... 02107] = EMIT_SUB,
[02120 ... 02174] = EMIT_SUB,
[02200 ... 02207] = EMIT_SUB,
[02220 ... 02274] = EMIT_SUB,

[02300 ... 02374] = EMIT_SUBA, //Word

[02400 ... 02417] = EMIT_SUBX, //R2
[02500 ... 02517] = EMIT_SUBX,
[02600 ... 02617] = EMIT_SUBX,

[02420 ... 02471] = EMIT_SUB,  //D2 Source
[02520 ... 02571] = EMIT_SUB,
[02620 ... 02671] = EMIT_SUB,

[02700 ... 02774] = EMIT_SUBA, //Long

[03000 ... 03007] = EMIT_SUB,  //D3 Destination
[03020 ... 03074] = EMIT_SUB,
[03100 ... 03107] = EMIT_SUB,
[03120 ... 03174] = EMIT_SUB,
[03200 ... 03207] = EMIT_SUB,
[03220 ... 03274] = EMIT_SUB,

[03300 ... 03374] = EMIT_SUBA, //Word

[03400 ... 03417] = EMIT_SUBX, //R3
[03500 ... 03517] = EMIT_SUBX,
[03600 ... 03617] = EMIT_SUBX,

[03420 ... 03471] = EMIT_SUB,  //D3 Source
[03520 ... 03571] = EMIT_SUB,
[03620 ... 03671] = EMIT_SUB,

[03700 ... 03774] = EMIT_SUBA, //Long

[04000 ... 04007] = EMIT_SUB,  //D4 Destination
[04020 ... 04074] = EMIT_SUB,
[04100 ... 04107] = EMIT_SUB,
[04120 ... 04174] = EMIT_SUB,
[04200 ... 04207] = EMIT_SUB,
[04220 ... 04274] = EMIT_SUB,

[04300 ... 04374] = EMIT_SUBA, //Word

[04400 ... 04417] = EMIT_SUBX, //R4
[04500 ... 04517] = EMIT_SUBX,
[04600 ... 04617] = EMIT_SUBX,

[04420 ... 04471] = EMIT_SUB,  //D4 Source
[04520 ... 04571] = EMIT_SUB,
[04620 ... 04671] = EMIT_SUB,

[04700 ... 04774] = EMIT_SUBA, //Long

[05000 ... 05007] = EMIT_SUB,  //D5 Destination
[05020 ... 05074] = EMIT_SUB,
[05100 ... 05107] = EMIT_SUB,
[05120 ... 05174] = EMIT_SUB,
[05200 ... 05207] = EMIT_SUB,
[05220 ... 05274] = EMIT_SUB,

[05300 ... 05374] = EMIT_SUBA, //Word

[05400 ... 05417] = EMIT_SUBX, //R5
[05500 ... 05517] = EMIT_SUBX,
[05600 ... 05617] = EMIT_SUBX,

[05420 ... 05471] = EMIT_SUB,  //D5 Source
[05520 ... 05571] = EMIT_SUB,
[05620 ... 05671] = EMIT_SUB,

[05700 ... 05774] = EMIT_SUBA, //Long

[06000 ... 06007] = EMIT_SUB,  //D6 Destination
[06020 ... 06074] = EMIT_SUB,
[06100 ... 06107] = EMIT_SUB,
[06120 ... 06174] = EMIT_SUB,
[06200 ... 06207] = EMIT_SUB,
[06220 ... 06274] = EMIT_SUB,

[06300 ... 06374] = EMIT_SUBA, //Word

[06400 ... 06417] = EMIT_SUBX, //R6
[06500 ... 06517] = EMIT_SUBX,
[06600 ... 06617] = EMIT_SUBX,

[06420 ... 06471] = EMIT_SUB,  //D6 Source
[06520 ... 06571] = EMIT_SUB,
[06620 ... 06671] = EMIT_SUB,

[06700 ... 06774] = EMIT_SUBA, //Long

[07000 ... 07007] = EMIT_SUB,  //D7 Destination
[07020 ... 07074] = EMIT_SUB,
[07100 ... 07107] = EMIT_SUB,
[07120 ... 07174] = EMIT_SUB,
[07200 ... 07207] = EMIT_SUB,
[07220 ... 07274] = EMIT_SUB,

[07300 ... 07374] = EMIT_SUBA, //Word

[07400 ... 07417] = EMIT_SUBX, //R7
[07500 ... 07517] = EMIT_SUBX,
[07600 ... 07617] = EMIT_SUBX,

[07420 ... 07471] = EMIT_SUB,  //D7 Source
[07520 ... 07571] = EMIT_SUB,
[07620 ... 07671] = EMIT_SUB,

[07700 ... 07774] = EMIT_SUBA, //Long
}
*/
