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

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

/****************************************************************************/
/*	1000xxx011xxxxxx - DIVU.W												*/
/****************************************************************************/
/*	DIVU.W <ea>,Dn 		32/16 → 16r-16q										*/
/*																			*/
/*	Operation: dest ÷ src → dest											*/
/*		 X|N|Z|V|C															*/
/*	CC: (-|*|*|*|0)															*/
/*	SR_NZ are undefined if overflow or divide by 0 occures					*/
/*																			*/
/*	Description: Divides the dest operand by the src operant and stores		*/
/*	the result in the dest. This instruction divides a long by a word. The	*/
/*	result is a quotient in the lower word(LSB) and a remainder in the upper*/
/*	word(MSB).																*/
/*																			*/
/*	Exceptions:																*/
/*			1.	Division By Zero, this cause a Trap, Exception vector 0x14	*/
/*			2.	Overflow may be detected and set before the operation		*/
/*				completes. If the instruction detects an overflow, it sets	*/
/*				SR_V flag, and the operands are uneffected.					*/
/****************************************************************************/

uint32_t *EMIT_DIVU_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_DIVU_mem")))
uint32_t *EMIT_DIVU_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_DIVU_reg")))
uint32_t *EMIT_DIVU_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
	ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
}

/****************************************************************************/
/*	1000xxx111xxxxxx - DIVS.W												*/
/****************************************************************************/
/*	DIVS.W <ea>,Dn 		32/16 → 16r-16q										*/
/*																			*/
/*	Operation: dest ÷ src → dest											*/
/*		 X|N|Z|V|C															*/
/*	CC: (-|*|*|*|0)															*/
/*	SR_NZ are undefined if overflow or divide by 0 occures					*/
/*																			*/
/*	Description: Divides the dest operand by the src operant and stores		*/
/*	the result in the dest. This instruction divides a long by a word. The	*/
/*	result is a quotient in the lower word(LSB) and a remainder in the upper*/
/*	word(MSB).																*/
/*																			*/
/*	Exceptions:																*/
/*			1.	Division By Zero, this cause a Trap, Exception vector 0x14	*/
/*			2.	Overflow may be detected and set before the operation		*/
/*				completes. If the instruction detects an overflow, it sets	*/
/*				SR_V flag, and the operands are uneffected.					*/
/****************************************************************************/

uint32_t *EMIT_DIVS_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_DIVS_mem")))
uint32_t *EMIT_DIVS_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_DIVS_reg")))
uint32_t *EMIT_DIVS_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
		ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
}

/****************************************************************************/
/*	1000xxx100001xxx - SBCD (An)											*/
/****************************************************************************/
/*	SBCD -(Ax),-(Ay)														*/
/*																			*/
/*	Operation: dest₁₀ - src₁₀ - SR_X → dest₁₀								*/
/*		 X|N|Z|V|C															*/
/*	CC: (*|U|*|U|*)															*/
/*	SR_Z is cleared if nonzero; otherwise unaltered							*/
/*																			*/
/*	Description: Predecrements src operand address before fetching operant	*/
/*	subtracting it and SR_X from the dest operand which is fetched also		*/
/*	from a predecremented address, before storing it back into dest address.*/
/*																			*/
/*	this operation operates on bytes only!									*/
/****************************************************************************/

uint32_t *EMIT_SBCD_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_SBCD_reg")))

/****************************************************************************/
/*	1000xxx100000xxx - SBCD Dn												*/
/****************************************************************************/
/*	SBCD Dx,Dy																*/
/*																			*/
/*	Operation: dest₁₀ - src₁₀ - SR_X → dest₁₀								*/
/*		 X|N|Z|V|C															*/
/*	CC: (*|U|*|U|*)															*/
/*	SR_Z is cleared if nonzero; otherwise unaltered							*/
/*																			*/
/*	Description: Subtracts the src operand and SR_X from dest operand and	*/
/*	stores the result in the dest location. the subtraction is performed	*/
/*	following BCD arithmatic; the operants are stored in BCD format.		*/
/*																			*/
/*	this operation operates on bytes only!									*/
/****************************************************************************/
/*
	SBCD  a NAÏve attempt 
	dest - ((SR_X + src) & mask) if dest <= -1 (overflow) add 0x9, carry 1; else  do nothing, repeat on the second nyble0.

	uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
	uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
	uint8_t mask = RA_AllocARMRegister(&ptr);
	uint8_t tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
	uint8_t cc = RA_GetCC(&ptr);
#endif
	/* first iteration */
/*
	*ptr++ = mov_immed_u16(mask, 0xf, 0);
#ifdef __aarch64__ //get SR_X for SBCD
	*ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
#else
	M68K_GetCC(&ptr);
	*ptr++ = tst_immed(REG_SR, SR_X);
#endif
	*ptr++ = 
	*ptr++ = add_reg(tmp, src, mask, LSL, 0);
	*ptr++ = subs_reg(tmp, dest, mask, LSL, 0);
	*ptr++ = addlt_immed(tmp, #9);
}

*/
uint32_t *EMIT_SBCD_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
	ptr = EMIT_InjectDebugString(ptr, "[JIT] SBCD at %08x not implemented\n", *m68k_ptr - 1);
	ptr = EMIT_InjectPrintContext(ptr);
	*ptr++ = udf(opcode);
}

/****************************************************************************/
/*	1000xxx101000xxx - PACK Dn												*/
/****************************************************************************/
/*	PACK Dx,Dy,#<adjustment>												*/
/*																			*/
/*	Operation: src(Unpacked BCD) + Adjustment → dest(Packed BCD)			*/
/*																			*/
/*	Description: Adjusts the lower nybles of each byte into a single byte.	*/
/*	The adjustment is added to the value contained in the src reg. Bits		*/
/*	11:8 and 3:0 of the intermediate result are concatenated and placed in	*/
/*	bits 7:0 of the dest reg. The remainder of the dest reg is unaffected.	*/
/*																			*/
/****************************************************************************/

uint32_t *EMIT_PACK_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint8_t reg){
#ifdef __aarch64__
	uint16_t	addend = BE16((*m68k_ptr)[0]); //for ANSII & EBCDIC this value will be 0x0, other values are valid!
	uint8_t		tmp = RA_CopyFromM68kRegister(&ptr, opcode & 7);
	uint8_t		dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
	
	if (addend & 0xfff) //will never trigger if used modi operandi
		*ptr++ = add_immed(tmp, tmp, addend & 0xfff);

	if (addend & 0xf000) //will never trigger if used modi operandi
		*ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);

	*ptr++ = bfi(tmp, tmp, 4, 4);

	RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
	*ptr++ = bfxil(dest, tmp, 4, 8);


	(*m68k_ptr)++;
	ptr = EMIT_AdvancePC(ptr, 4);

	RA_FreeARMRegister(&ptr, tmp);
#else
	ptr = EMIT_InjectDebugString(ptr, "[JIT] PACK at %08x not implemented\n", *m68k_ptr - 1);
	ptr = EMIT_InjectPrintContext(ptr);
	*ptr++ = udf(opcode);
#endif
}

/****************************************************************************/
/*	1000xxx101001xxx - PACK (An)											*/
/****************************************************************************/
/*	PACK -(Ax),-(Ay),#<adjustment>											*/
/*																			*/
/*	Operation: src(Unpacked BCD) + Adjustment → dest(Packed BCD)			*/
/*																			*/
/*	Description: Adjusts the lower nybles of each byte into a single byte.	*/
/*	The adjustment is added to the value contained in the src address. Bits	*/
/*	11:8 and 3:0 of the intermediate result are concatenated and placed in	*/
/*	bits 7:0 of the dest adress. The remainder of the dest address is		*/
/*	unaffected.																*/
/****************************************************************************/

uint32_t *EMIT_PACK_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
#ifdef __aarch64__
	uint16_t addend = BE16((*m68k_ptr)[0]); //for ANSII & EBCDIC this value will be 0x0, other values are valid!
	uint8_t tmp = RA_AllocARMRegister(&ptr);
	uint8_t an_src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
	uint8_t dest = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

	*ptr++ = ldrsh_offset_preindex(an_src, tmp, -2);

	RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));

	if (addend & 0xfff) //will never trigger if used modi operandi
		*ptr++ = add_immed(tmp, tmp, addend & 0xfff);

	if (addend & 0xf000) //will never trigger if used modi operandi
		*ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);

	*ptr++ = bfi(tmp, tmp, 4, 4);

	RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

	*ptr++ = lsr(tmp, tmp, 4);

	if (((opcode >> 9) & 7) == 7)
		*ptr++ = strb_offset_preindex(dest, tmp, -2);

	else
		*ptr++ = strb_offset_preindex(dest, tmp, -1);

	(*m68k_ptr)++;
	ptr = EMIT_AdvancePC(ptr, 4);

	RA_FreeARMRegister(&ptr, tmp);
#else
	ptr = EMIT_InjectDebugString(ptr, "[JIT] PACK at %08x not implemented\n", *m68k_ptr - 1);
	ptr = EMIT_InjectPrintContext(ptr);
	*ptr++ = udf(opcode);
#endif
}

/****************************************************************************/
/*	1000xxx110000xxx - UNPK Dn												*/
/****************************************************************************/
/*	UNPK Dy,Dx 																*/
/*																			*/
/*	Operation: dest ⋁ src(Packed BCD) → dest(Unpacked BCD)					*/
/*																			*/
/*	Description: Divides the dest operand by the src operant and stores		*/
/*	the result in the dest. This instruction divides a long by a word. The	*/
/*	result is a quotient in the lower word(LSB) and a remainder in the upper*/
/*	word(MSB).																*/
/*																			*/
/*	Exceptions:																*/
/*			1.	Division By Zero, this cause a Trap, Exception vector 0x14	*/
/*			2.	Overflow may be detected and set before the operation		*/
/*				completes. If the instruction detects an overflow, it sets	*/
/*				SR_V flag, and the operands are uneffected.					*/
/****************************************************************************/

uint32_t EMIT_UNPK_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
#ifdef __aarch64__
	uint16_t addend = BE16((*m68k_ptr)[0]); //Constants used for, ANCII 0x3030; EBCDIC 0xf0f0; EMCA-1 0x1010.
	uint8_t tmp = RA_AllocARMRegister(&ptr); //Helper register
	uint8_t mask = RA_AllocARMRegister(&ptr); //This is an const why do we use a register here?
	uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
	uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);

	*ptr++ = mov_immed_u16(mask, 0x0f0f, 0); //This is loading a const var
	*ptr++ = orr_reg(tmp, src, src, LSL, 4);
	*ptr++ = and_reg(tmp, tmp, mask, LSL, 0);

	if (addend & 0xfff) //This will always trigger when the instruction ran modi operandi
		*ptr++ = add_immed(tmp, tmp, addend & 0xfff);

	if (addend & 0xf000) //This will always trigger when the instruction ran modi operandi
		*ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);

	RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
	*ptr++ = bfi(dest, tmp, 0, 16);

	(*m68k_ptr)++;
	ptr = EMIT_AdvancePC(ptr, 4);

	RA_FreeARMRegister(&ptr, tmp);
	RA_FreeARMRegister(&ptr, mask);
	RA_FreeARMRegister(&ptr, src);
#else
	ptr = EMIT_InjectDebugString(ptr, "[JIT] UNPK at %08x not implemented\n", *m68k_ptr - 1);
	ptr = EMIT_InjectPrintContext(ptr);
	*ptr++ = udf(opcode);
#endif

/* 1000xxx110001xxx - UNPK (An) */

uint32_t *EMIT_UNPK_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
#ifdef __aarch64__
	uint16_t addend = BE16((*m68k_ptr)[0]); //const used for, ANCII 0x3030; EBCDIC 0xf0f0.
	uint8_t tmp = RA_AllocARMRegister(&ptr);
	uint8_t mask = RA_AllocARMRegister(&ptr);
	uint8_t src = RA_AllocARMRegister(&ptr);
	uint8_t an_src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
	uint8_t dest = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

	*ptr++ = mov_immed_u16(mask, 0x0f0f, 0);

	if ((opcode & 7) == 7)
		*ptr++ = ldrsb_offset_preindex(an_src, src, -2);
	else
		*ptr++ = ldrsb_offset_preindex(an_src, src, -1);

	RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));

	*ptr++ = orr_reg(tmp, src, src, LSL, 4);
	*ptr++ = and_reg(tmp, tmp, mask, LSL, 0);

	if (addend & 0xfff) //this will always trigger when this instruction is used modi operandi
		*ptr++ = add_immed(tmp, tmp, addend & 0xfff);
		
	if (addend & 0xf000) //this will always trigger when this instruction is used modi operandi
		*ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);

	RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
	*ptr++ = strh_offset_preindex(dest, tmp, -2);

	(*m68k_ptr)++;
	ptr = EMIT_AdvancePC(ptr, 4);

	RA_FreeARMRegister(&ptr, tmp);
	RA_FreeARMRegister(&ptr, mask);
	RA_FreeARMRegister(&ptr, src);
#else
	ptr = EMIT_InjectDebugString(ptr, "[JIT] UNPK at %08x not implemented\n", *m68k_ptr - 1);
	ptr = EMIT_InjectPrintContext(ptr);
	*ptr++ = udf(opcode);
#endif
}

/* 1000xxx0xx000xxx - OR Dn */

uint32_t *EMIT_OR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint8_t reg){
	uint8_t size = 1 << ((opcode >> 6) & 3); //This makes a bit mask where only 1 bit is valid
	uint8_t ext_words = 0;
	uint8_t test_register = 0xff; //Used only in __aarch64__ code for now.
	uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
	uint8_t src = 0xff;

	test_register = dest;

	RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
	if (size == 4)
		ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
	else
		ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

	switch (size){
#ifdef __aarch64__
	case 4:
		*ptr++ = orr_reg(dest, dest, src, LSL, 0);
		break;
	case 2:
		*ptr++ = orr_reg(src, src, dest, LSL, 0);
		*ptr++ = bfi(dest, src, 0, 16);
		break;
	case 1:
		*ptr++ = orr_reg(src, src, dest, LSL, 0);
		*ptr++ = bfi(dest, src, 0, 8);
		break;
#else //on __aarch32__ this shift used to set SR_N propperly
	case 4:
		*ptr++ = orrs_reg(dest, dest, src, 0);
		break;
	case 2:
		*ptr++ = lsl_immed(src, src, 16);
		*ptr++ = orrs_reg(src, src, dest, 16);
		*ptr++ = lsr_immed(src, src, 16);
		*ptr++ = bfi(dest, src, 0, 16);
		break;
	case 1:
		*ptr++ = lsl_immed(src, src, 24);
		*ptr++ = orrs_reg(src, src, dest, 24);
		*ptr++ = lsr_immed(src, src, 24);
		*ptr++ = bfi(dest, src, 0, 8);
		break;
#endif
	}
	RA_FreeARMRegister(&ptr, src);

	ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
	(*m68k_ptr) += ext_words;

	if (update_mask){
#ifdef __aarch64__
		switch(size){
			case 4:
				*ptr++ = cmn_reg(31, test_register, LSL, 0);
				break;
			case 2:
				*ptr++ = cmn_reg(31, test_register, LSL, 16);
				break;
			case 1:
				*ptr++ = cmn_reg(31, test_register, LSL, 24);
				break;
		}
#endif
		uint8_t cc = RA_ModifyCC(&ptr);
		ptr = EMIT_GetNZ00(ptr, cc, &update_mask); //This might be silly, but if SR_Z is true, then SR_N can never be true. And vice versa.

		if (update_mask & SR_Z)
			ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
		if (update_mask & SR_N)
			ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
	}

	RA_FreeARMRegister(&ptr, test_register);
}

/* 1000xxxxxxxxxxxx - OR <ea> */

uint32_t *EMIT_OR_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68kptr)
__attribute__((alias("EMIT_OR_mem")));
uint32_t *EMIT_OR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr){
	uint8_t size = 1 << ((opcode >> 6) & 3);
	uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
	uint8_t ext_words = 0;
	uint8_t test_register = 0xff; //only used in __aarch64__ code for now

	if (direction == 0){
		uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
		uint8_t src = 0xff;

		test_register = dest;

		RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
		if (size == 4)
			ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
		else
			ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

		switch (size){
#ifdef __aarch64__
		case 4:
			*ptr++ = orr_reg(dest, dest, src, LSL, 0);
			break;
		case 2:
			*ptr++ = orr_reg(src, src, dest, LSL, 0);
			*ptr++ = bfi(dest, src, 0, 16);
			break;
		case 1:
			*ptr++ = orr_reg(src, src, dest, LSL, 0);
			*ptr++ = bfi(dest, src, 0, 8);
			break;
#else
		case 4:
			*ptr++ = orrs_reg(dest, dest, src, 0);
			break;
		case 2:
			*ptr++ = lsl_immed(src, src, 16);
			*ptr++ = orrs_reg(src, src, dest, 16);
			*ptr++ = lsr_immed(src, src, 16);
			*ptr++ = bfi(dest, src, 0, 16);
			break;
		case 1:
			*ptr++ = lsl_immed(src, src, 24);
			*ptr++ = orrs_reg(src, src, dest, 24);
			*ptr++ = lsr_immed(src, src, 24);
			*ptr++ = bfi(dest, src, 0, 8);
			break;
#endif
		}

		RA_FreeARMRegister(&ptr, src);
	}
	else{
		uint8_t dest = 0xff;
		uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
		uint8_t tmp = RA_AllocARMRegister(&ptr);
		uint8_t mode = (opcode & 0x0038) >> 3;

		test_register = tmp;

		if (mode == 4 || mode == 3)
			ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
		else
			ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

		/* Fetch data into temporary register, perform operation, store it back */

		switch (size){
		case 4:
			if (mode == 4){
				*ptr++ = ldr_offset_preindex(dest, tmp, -4);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = ldr_offset(dest, tmp, 0);

			/* Perform calcualtion */

#ifdef __aarch64__
			*ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
			*ptr++ = orrs_reg(tmp, tmp, src, 0);
#endif

			/* Store back */

			if (mode == 3){
				*ptr++ = str_offset_postindex(dest, tmp, 4);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = str_offset(dest, tmp, 0);
			break;
		case 2:
			if (mode == 4){
				*ptr++ = ldrh_offset_preindex(dest, tmp, -2);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = ldrh_offset(dest, tmp, 0);

			/* Perform calcualtion */

#ifdef __aarch64__
			*ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
			*ptr++ = lsl_immed(tmp, tmp, 16);
			*ptr++ = orrs_reg(tmp, tmp, src, 16);
			*ptr++ = lsr_immed(tmp, tmp, 16);
#endif

			/* Store back */

			if (mode == 3){
				*ptr++ = strh_offset_postindex(dest, tmp, 2);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = strh_offset(dest, tmp, 0);
			break;
		case 1:
			if (mode == 4){
				*ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = ldrb_offset(dest, tmp, 0);

			/* Perform calcualtion */

#ifdef __aarch64__
			*ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
			*ptr++ = lsl_immed(tmp, tmp, 24);
			*ptr++ = orrs_reg(tmp, tmp, src, 24);
			*ptr++ = lsr_immed(tmp, tmp, 24);
#endif

			/* Store back */

			if (mode == 3){
				*ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
				RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
			}
			else
				*ptr++ = strb_offset(dest, tmp, 0);
			break;
		}

		RA_FreeARMRegister(&ptr, dest);
	}

	ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
	(*m68k_ptr) += ext_words;

	if (update_mask){
#ifdef __aarch64__
		switch(size){
			case 4:
				*ptr++ = cmn_reg(31, test_register, LSL, 0);
				break;
			case 2:
				*ptr++ = cmn_reg(31, test_register, LSL, 16);
				break;
			case 1:
				*ptr++ = cmn_reg(31, test_register, LSL, 24);
				break;
		}
#endif
		uint8_t cc = RA_ModifyCC(&ptr);
		ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

		if (update_mask & SR_Z)
			ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
		if (update_mask & SR_N)
			ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
	}

	RA_FreeARMRegister(&ptr, test_register);
}

static EMIT_Function JumpTable[512] = {
	[0000 ... 0007] = EMIT_OR_reg,    //Dn Destination
	[0020 ... 0047] = EMIT_OR_mem,
	[0050 ... 0074] = EMIT_OR_ext,
	[0100 ... 0107] = EMIT_OR_reg,
	[0120 ... 0147] = EMIT_OR_mem,
	[0150 ... 0174] = EMIT_OR_ext,
	[0200 ... 0207] = EMIT_OR_reg,
	[0220 ... 0247] = EMIT_OR_mem,
	[0250 ... 0274] = EMIT_OR_ext,
	
	[0300 ... 0307] = EMIT_DIVU_reg,  //Dn Destination, DIVU.W
	[0320 ... 0347] = EMIT_DIVU_mem,
	[0350 ... 0374] = EMIT_DIVU_ext,
	
	[0400 ... 0407] = EMIT_SBCD_reg,
	[0410 ... 0417] = EMIT_SBCD_mem,  //Rn Destination
	[0420 ... 0447] = EMIT_OR_mem,
	[0450 ... 0474] = EMIT_OR_ext,    //Dn Source
	
	[0500 ... 0507] = EMIT_PACK_reg,
	[0510 ... 0517] = EMIT_PACK_mem,  //_ext,//Rn Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
	[0520 ... 0547] = EMIT_OR_mem, 
	[0550 ... 0574] = EMIT_OR_ext,
	
	[0600 ... 0607] = EMIT_UNPK_reg,
	[0610 ... 0617] = EMIT_UNPK_mem,  //_ext,//Rn Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
	[0620 ... 0647] = EMIT_OR_mem, 
	[0650 ... 0674] = EMIT_OR_ext,
	
	[0700 ... 0707] = EMIT_DIVS_reg,  //Dn Destination, DIVS.W
	[0720 ... 0747] = EMIT_DIVS_mem,
	[0750 ... 0774] = EMIT_DIVS_ext,
}

uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
	uint16_t opcode = BE16((*m68k_ptr)[0]);
	(*m68k_ptr)++;
	*insn_consumed = 1;

	/* Line8 */

	if (JumpTable[opcode & 0777]){
		ptr = JumpTable[opcode & 0777](ptr, opcode, m68k_ptr);
	}
	else{
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
		ptr = EMIT_InjectPrintContext(ptr);
		*ptr++ = udf(opcode);
	}
	return ptr;
}
