/****************************************************/
/* FPU Instruction opcodes and condition fields.	*/
/*													*/
/* This part holds all tables,						*/
/* Header according to the following format.		*/
/* Table Name.										*/
/*													*/
/****************************************************/

#include "M68k.h"

/* Index

	EA Decoding
	CC Instructions,
	Branch,
		Misc tests
		IEEE Aware tests
		IEEE Nonaware tests
	Long
		Misc tests
		IEEE Aware tests
		IEEE Nonaware tests
	FSAVE

	Dn
	An
	(An)
	(An)+
	-(An)
	(d16,An)
	(d16,PC)

	FPU Instructions
*/

/* 1st opcode, Table bits [8:0] */
static EMIT_Function FPU[512] = {
	
/* EA Decoding */
	[0000 ... 0007]	= { { EMIT_Dn }, NULL, 0, 0, 0, 0, 0},
	[0010 ... 0017] = { { EMIT_An }, NULL, 0, 0, 0, 0, 0},
	[0020 ... 0027] = { { EMIT_AnIndir }, NULL, 0, 0, 0, 0, 0 },
	[0030 ... 0037] = { { EMIT_Incr }, NULL, 0, 0, 0, 0, 0 },
	[0040 ... 0047] = { { EMIT_Decr }, NULL, 0, 0, 0, 0, 0 },
	[0050 ... 0071] = { { EMIT_EA }, NULL, 0, 0, 0, 0, 0 },
	[0072 ... 0074] = { { EMIT_PC }, NULL, 0, 0, 0, 0, 0 },

/* CC instructions */ //cc is IEEE nonaware & NAN is set, Set BSUN and IOP to 1.
	[0100 ... 0107] = { { EMIT_FSCC_reg }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 1 },
	[0110 ... 0117] = { { EMIT_FDBCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0120 ... 0147] = { { EMIT_FSCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 1 },
	[0150 ... 0171] = { { EMIT_FSCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 1, 1 },
	[0172]			= { { EMIT_FTRAPCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0173]			= { { EMIT_FTRAPCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 4, 0, 0 },
	[0174]			= { { EMIT_FTRAPCC }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },

/* Branch */
	//Misc tests
	[0200]			= { { EMIT_FBF }, NULL, FPCC, 0, 2, 0, 0 },
	[0217]			= { { EMIT_FBT }, NULL, FPCC, 0, 2, 0, 0 },
	[0220]			= { { EMIT_FBSF }, NULL, FPCC, 0, 2, 0, 0 },
	[0221]			= { { EMIT_FBSEQ }, NULL, FPCC, 0, 2, 0, 0 },
	[0236]			= { { EMIT_FBSNE }, NULL, FPCC, 0, 2, 0, 0 },
	[0237]			= { { EMIT_FBST }, NULL, FPCC, 0, 2, 0, 0 },
	//IEEE Aware tests
	[0201]			= { { EMIT_FBEQ }, NULL, FPCC, 0, 2, 0, 0 },
	[0202]			= { { EMIT_FBOGT }, NULL, FPCC, 0, 2, 0, 0 },
	[0203]			= { { EMIT_FBOGE }, NULL, FPCC, 0, 2, 0, 0 },
	[0204]			= { { EMIT_FBOLT }, NULL, FPCC, 0, 2, 0, 0 },
	[0205]			= { { EMIT_FBOLE }, NULL, FPCC, 0, 2, 0, 0 },
	[0206]			= { { EMIT_FBOGL }, NULL, FPCC, 0, 2, 0, 0 },
	[0207]			= { { EMIT_FBOR }, NULL, FPCC, 0, 2, 0, 0 },
	[0210]			= { { EMIT_FBUN }, NULL, FPCC, 0, 2, 0, 0 },
	[0211]			= { { EMIT_FBUEQ }, NULL, FPCC, 0, 2, 0, 0 },
	[0212]			= { { EMIT_FBUGT }, NULL, FPCC, 0, 2, 0, 0 },
	[0213]			= { { EMIT_FBUGE }, NULL, FPCC, 0, 2, 0, 0 },
	[0214]			= { { EMIT_FBULT }, NULL, FPCC, 0, 2, 0, 0 },
	[0215]			= { { EMIT_FBULE }, NULL, FPCC, 0, 2, 0, 0 },
	[0216]			= { { EMIT_FBNE }, NULL, FPCC, 0, 2, 0, 0 },
	//IEEE Nonaware tests
	[0222]			= { { EMIT_FBGT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0223]			= { { EMIT_FBGE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0224]			= { { EMIT_FBLT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0225]			= { { EMIT_FBLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0226]			= { { EMIT_FBGL }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0227]			= { { EMIT_FBGLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0230]			= { { EMIT_FBNGLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0231]			= { { EMIT_FBNGL }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0232]			= { { EMIT_FBNLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0233]			= { { EMIT_FBNLT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0234]			= { { EMIT_FBNGE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
	[0235]			= { { EMIT_FBNGT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 2, 0, 0 },
/* Long */
	//Misc tests
	[0300]			= { { EMIT_FBF }, NULL, FPCC, 0, 3, 0, 0 },
	[0317]			= { { EMIT_FBT }, NULL, FPCC, 0, 3, 0, 0 },
	[0320]			= { { EMIT_FBSF }, NULL, FPCC, 0, 3, 0, 0 },
	[0321]			= { { EMIT_FBSEQ }, NULL, FPCC, 0, 3, 0, 0 },
	[0336]			= { { EMIT_FBSNE }, NULL, FPCC, 0, 3, 0, 0 },
	[0337]			= { { EMIT_FBST }, NULL, FPCC, 0, 3, 0, 0 },
	//IEEE Aware tests
	[0301]			= { { EMIT_FBEQ }, NULL, FPCC, 0, 3, 0, 0 },
	[0302]			= { { EMIT_FBOGT }, NULL, FPCC, 0, 3, 0, 0 },
	[0303]			= { { EMIT_FBOGE }, NULL, FPCC, 0, 3, 0, 0 },
	[0304]			= { { EMIT_FBOLT }, NULL, FPCC, 0, 3, 0, 0 },
	[0305]			= { { EMIT_FBOLE }, NULL, FPCC, 0, 3, 0, 0 },
	[0306]			= { { EMIT_FBOGL }, NULL, FPCC, 0, 3, 0, 0 },
	[0307]			= { { EMIT_FBOR }, NULL, FPCC, 0, 3, 0, 0 },
	[0310]			= { { EMIT_FBUN }, NULL, FPCC, 0, 3, 0, 0 },
	[0311]			= { { EMIT_FBUEQ }, NULL, FPCC, 0, 3, 0, 0 },
	[0312]			= { { EMIT_FBUGT }, NULL, FPCC, 0, 3, 0, 0 },
	[0313]			= { { EMIT_FBUGE }, NULL, FPCC, 0, 3, 0, 0 },
	[0314]			= { { EMIT_FBULT }, NULL, FPCC, 0, 3, 0, 0 },
	[0315]			= { { EMIT_FBULE }, NULL, FPCC, 0, 3, 0, 0 },
	[0316]			= { { EMIT_FBNE }, NULL, FPCC, 0, 3, 0, 0 },
	//IEEE Nonaware tests
	[0322]			= { { EMIT_FBGT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0323]			= { { EMIT_FBGE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0324]			= { { EMIT_FBLT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0325]			= { { EMIT_FBLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0326]			= { { EMIT_FBGL }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0327]			= { { EMIT_FBGLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0330]			= { { EMIT_FBNGLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0331]			= { { EMIT_FBNGL }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0332]			= { { EMIT_FBNLE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0333]			= { { EMIT_FBNLT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0334]			= { { EMIT_FBNGE }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
	[0335]			= { { EMIT_FBNGT }, NULL, FPCC, FPSR_BSUN | FPSR_IOP, 3, 0, 0 },
/* FSAVE */
	[0420 ... 0427] = { { EMIT_FSAVE }, NULL, SR_S, 0, 1, 0, 0 },
	[0440 ... 0447] = { { EMIT_FSAVE }, NULL, SR_S, 0, 1, 0, 0 },
	[0450 ... 0471] = { { EMIT_FSAVE }, NULL, SR_S, 0, 1, 1, 0 },

	[0520 ... 0537] = { { EMIT_FRESTORE }, NULL, SR_S, 0, 1, 0, 0 },
	[0550 ... 0574] = { { EMIT_FRESTORE }, NULL, SR_S, 0, 1, 1, 0 },
}

uint32_t *EMIT_FPU(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed) {

	uint16_t opcode = BE16((*m68k_ptr)[0]);
	uint16_t opcode2 = BE16((*m68k_ptr)[1]);
	uint8_t ext_count = 1;
	(*m68k_ptr)++;
	*insn_consumed = 1;

	if (FPU[opcode & 0777].od_Emit) {
		ptr = FPU[opcode & 0777].od_Emit(ptr, opcode, opcode2, m68k_ptr, insn_consumed);
	}
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr -1);
		ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);//What should be passed to this exception?
		*ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr;
}

/* opcode2, Tables; bits[15:10] */
static EMIT_Function JumpTableDn[64] = { //Dn
	[000 ... 007] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },	//FPm,FPn
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },	//Dn,FPn
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },	//Dn,FPn
	[027]		  = { { EMIT_FMOVECR }, NULL, 0, 0, 2, 0, 8 }, 	//#<ccc>,FPn //This should be an Extended format, but for sanity sake!
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,Dn
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,Dn
	[041 ... 042] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[044]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPCR
	[051 ... 052] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[054]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FPCR,Dn
}

uint32_t *EMIT_Dn(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableDn[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableDn[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTableAn[64] = { //FMOVE.L An,FPIAR and FMOVE.L FPIAR,An 
	[041]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A0,FPIAR 
	[051]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FMOVE>L FPIAR,A0
}

uint32_t *EMIT_An(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableAn[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableAn[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTableAnIndir[64] = { //(An)
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT ], NULL, 0, 0, 2, 0, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[037]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[041 ... 047] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },
	[051 ... 057] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },
	[064]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//static, postincrement
	[066]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//dynamic. postincrement
	[074]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//static, postincrement
	[076]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//dynamic. postincrement
}

uint32_t *EMIT_AnIndir(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableAnIndir[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableAnIndir[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTableIncr[64] = { //(An)+
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[037]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[041 ... 047] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },
	[064]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//static, postincrement
	[066]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//dynamic. postincrement
}

uint32_t *EMIT_Incr(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableIncr[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableIncr[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTableDecr[64] = { //-(An)
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[037]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[051 ... 057] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },
	[070]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//static, predecrement
	[072]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 0, 12 },	//dynamic, predecrement
}

uint32_t *EMIT_Decr(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableDecr[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableDecr[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTableEA[64] = { //(d16,An);(An,Xn);(xxx).(W|L)
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 1, 4 },	//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 1, 12 },	//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 1, 2 },	//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 1, 8 },	//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 1, 1 },	//FPn,<ea>
	[037]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[041 ... 047] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 1, 4 },
	[051 ... 057] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 1, 4 },
	[064]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//static, postincrement
	[066]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//dynamic. postincrement
	[074]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//static, postincrement
	[076]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//dynamic. postincrement
}

uint32_t *EMIT_EA(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTableEA[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTableEA[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

static EMIT_Function JumpTablePC[64] = { //(d16,PC);(PC,Xn);#<data>
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 1 },
	[041 ... 047] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 1, 4 },
	[064]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//static, postincrement
	[066]		  = { { EMIT_FMOVEM }, NULL, 0, 0, 2, 1, 12 },	//dynamic. postincrement
}

uint32_t *EMIT_PC(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {
	if (JumpTablePC[opcode2 & 0xfc00].od_Emit)
		ptr = JumpTablePC[opcode2 & 0xfc00].od_Emit(ptr, opcode, opcode2, m68k_ptr);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

/* FPU Instructions, Table 2; bits[5:0]*///bit 6 always assumed 0
static EMIT_Function JumpTableOp[128] = {
	[0x00]			= { { EMIT_FMOVE },		NULL, FPCR_RND, FPEB }, // the Format should give the last 3 values
	[0x01]			= { { EMIT_FINT },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x02]			= { { EMIT_FSINH },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x03]			= { { EMIT_FINTRZ },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x04]			= { { EMIT_FSQRT },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x06]			= { { EMIT_FLOGNP1 },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x08]			= { { EMIT_FETOXM1 },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x09]			= { { EMIT_FTANH },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x0A]			= { { EMIT_FATAN },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x0B]			= { { EMIT_FINTRN },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x0C]			= { { EMIT_FASIN },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x0D]			= { { EMIT_FATANH },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x0E]			= { { EMIT_FSIN },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x0F]			= { { EMIT_FTAN },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x10]			= { { EMIT_FETOX },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x11]			= { { EMIT_FTWOTOX },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x12]			= { { EMIT_FTENTOX },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x13]			= { { EMIT_FINTRP },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x14]			= { { EMIT_FLOGN }, 	NULL, FPCR_PREC, FPCC | FPEB },
	[0x15]			= { { EMIT_FLOG10 },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x16]			= { { EMIT_FLOG2 },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x18]			= { { EMIT_FABS },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x19]			= { { EMIT_FCOSH },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x1A]			= { { EMIT_FNEG },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x1B]			= { { EMIT_FINTRM },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x1C]			= { { EMIT_FACOS },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x1D]			= { { EMIT_FCOS },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x1E]			= { { EMIT_FGETEXP },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x1F]			= { { EMIT_FGETMAN },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x20]			= { { EMIT_FDIV },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x21]			= { { EMIT_FMOD },		NULL, FPCR_PREC, FPSR_ALL },
	[0x22]			= { { EMIT_FADD },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x23]			= { { EMIT_FMUL },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x24]			= { { EMIT_FSGLDIV },	NULL, 0, FPCC | FPEB },		//single precision
	[0x25]			= { { EMIT_FREM },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x26]			= { { EMIT_FSCALE },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x27]			= { { EMIT_FSGMUL },	NULL, 0, FPCC | FPEB },		//single precision
	[0x28]			= { { EMIT_FSUB },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x2D]			= { { EMIT_FMOD },		NULL, FPCR_PREC, FPSR_ALL },
	[0x30 ... 0x37]	= { { EMIT_FSINCOS },	NULL, FPCR_PREC, FPCC | FPEB },
	[0x38]			= { { EMIT_FCMP },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x3A]			= { { EMIT_FTST },		NULL, FPCR_PREC, FPCC | FPEB },
	[0x40]			= { { EMIT_FMOVE_S_dst }, NULL, FPCR_RND, FPEB },	//rounded to single
	[0x41]			= { { EMIT_FSQRT_S },	NULL, 0, FPCC | FPEB },		//rounded to single
	[0x44]			= { { EMIT_FMOVE_D_dst }, NULL, FPCR_RND, FPEB },	//rounded to double
	[0x45]			= { { EMIT_FSQRT_D },	NULL, 0, FPCC | FPEB },		//rounded to double
	[0x58]			= { { EMIT_FABS_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x5A]			= { { EMIT_FNEG_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x5C]			= { { EMIT_FABS_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
	[0x5E]			= { { EMIT_FNEG_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
	[0x60]			= { { EMIT_FDIV_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x62]			= { { EMIT_FADD_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x63]			= { { EMIT_FMUL_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x64]			= { { EMIT_FDIV_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
	[0x66]			= { { EMIT_FADD_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
	[0x67]			= { { EMIT_FMUL_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
	[0x68]			= { { EMIT_FSUB_S },	NULL, 0, FPCC | FPEB0 },	//rounded to single
	[0x6C]			= { { EMIT_FSUB_D },	NULL, 0, FPCC | FPEB0 },	//rounded to double
}
/* Any format function should preload specified registers according to format and jump to FPU Instruction table. */
uint32_t *EMIT_FORMAT(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint16_t *insn_consumed) {

	uint8_t fmt = 1 << ((opcode2 & 0x1c00 >> 10) // this should be a value between 0 and 128

	if (JumpTableOp[opcode2 & 0x7f].od_Emit)
		ptr = JumpTableOp[opcode2 & 0x7f].od_Emit(ptr, opcode, opcode2, m68k_ptr, fmt);
	else {
		ptr = EMIT_FlushPC(ptr);
		ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_LINE_F, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
	}
	return ptr
}

uint32_t *EMIT_FMOVECR(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){ /* FMOVECR only pulls extended-precision constants to a FP register */
	
}

/* any and all FMOVE instructions, this can 2 or 3 nested jumps depending on the encoding */
uint32_t *EMIT_FMOVE(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){
	
}

uint32_t *EMIT_FMOVE_S_dest(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){
	
}
uint32_t *EMIT_FMOVE_D_dest(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){
	
}
//this instruction should not update FPIAR when executed, this is always Long
uint32_t *EMIT_FMOVEM_L(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t direction = ((opcode2 >> 10) & 1);
}
//this instruction should not update FPIAR when executed, this is always extended
uint32_t *EMIT_FMOVEM_static(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	
}
//this instruction should not update FPIAR when executed, this is always extended
uint32_t *EMIT_FMOVEM_dynamic(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	
}

/* FPU Monadic Operations */
uint32_t *EMIT_FABS(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FABS_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FABS_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FACOS(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FASIN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FATAN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FCOS(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FCOSH(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FETOX(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FETOXM1(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FGETEXP(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FGETMAN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FINT(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FINTRZ(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FLOGN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FLOGNP1(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FLOG10(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FLOG2(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FNEG(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FNEG_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FNEG_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSIN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSINH(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSQRT(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FSQRT_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FSQRT_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FTAN(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FTANH(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FTENTOX(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FTWOTOX(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

/* FPU Dyadic Operations */
uint32_t *EMIT_FADD(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FADD_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FADD_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FCMP(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FDIV(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FDIV_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FDIV_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FMOD(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FMUL(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FMUL_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FMUL_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FREM(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSCALE(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSUB(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FSUB_S(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
uint32_t *EMIT_FSUB_D(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSGLDIV(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)

uint32_t *EMIT_FSGMUL(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **m68k_ptr, uint8_t fmt)
