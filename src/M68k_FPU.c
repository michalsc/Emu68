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

	Dn
	An
	(An)
	(An)+
	-(An)
	(d16,An)
	(d16,PC)

	FPU Instructions,
*/

/* 1st opcode, Table bits [8:0]*/
static EMIT_Function FPU[512] = {
	
/* EA Decoding */
	[0000 ... 0007]	= EMIT_NEXT,
	[0010 ... 0017] = EMIT_NEXT,
	[0020 ... 0027] = EMIT_NEXT,
	[0030 ... 0037] = EMIT_NEXT,
	[0040 ... 0047] = EMIT_NEXT,
	[0050 ... 0071] = EMIT_NEXT,
	[0072 ... 0074] = EMIT_NEXT,

/* CC instructions */
	[0100 ... 0107] = EMIT_FSCC_reg,
	[0110 ... 0117] = EMIT_FDBCC,
	[0120 ... 0147] = EMIT_FSCC,
	[0150 ... 0171] = EMIT_FSCC,
	[0172]			= EMIT_FTRAPCC,
	[0173]			= EMIT_FTRAPCC,
	[0174]			= EMIT_FTRAPCC,

/* Branch */
	//Misc tests
	[0200]			= EMIT_FBF,
	[0217]			= EMIT_FBT,
	[0220]			= EMIT_FBSF,
	[0221]			= EMIT_FBSEQ,
	[0236]			= EMIT_FBSNE,
	[0237]			= EMIT_FBST,
	//IEEE Aware tests
	[0201]			= EMIT_FBEQ,
	[0202]			= EMIT_FBOGT,
	[0203]			= EMIT_FBOGE,
	[0204]			= EMIT_FBOLT,
	[0205]			= EMIT_FBOLE,
	[0206]			= EMIT_FBOGL,
	[0207]			= EMIT_FBOR,
	[0210]			= EMIT_FBUN,
	[0211]			= EMIT_FBUEQ,
	[0212]			= EMIT_FBUGT,
	[0213]			= EMIT_FBUGE,
	[0214]			= EMIT_FBULT,
	[0215]			= EMIT_FBULE,
	[0216]			= EMIT_FBNE,
	//IEEE Nonaware tests
	[0222]			= EMIT_FBGT,
	[0223]			= EMIT_FBGE,
	[0224]			= EMIT_FBLT,
	[0225]			= EMIT_FBLE,
	[0226]			= EMIT_FBGL,
	[0227]			= EMIT_FBGLE,
	[0230]			= EMIT_FBNGLE,
	[0231]			= EMIT_FBNGL,
	[0232]			= EMIT_FBNLE,
	[0233]			= EMIT_FBNLT,
	[0234]			= EMIT_FBNGE,
	[0235]			= EMIT_FBNGT,

	[0420 ... 0427] = EMIT_FSAVE,
	[0440 ... 0447] = EMIT_FSAVE,
	[0450 ... 0471] = EMIT_FSAVE,

	[0520 ... 0537] = EMIT_FRESTORE,
	[0550 ... 0574] = EMIT_FRESTORE,
}

/* 2nd opcode, Tables; bits[15:10] */
static EMIT_Function JumpTableDn[64] = { //Dn
	[000 ... 007] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },			//FPm,FPn
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPn
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },			//Dn,FPn
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },			//Dn,FPn
	[027]		  = { { EMIT_FMOVECR }, NULL, 0, 0, 2, 0, 8 }, 			//#<ccc>,FPn //This should be an Extended format, but for sanity sake!
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },			//FPn,Dn
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },			//FPn,Dn
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },			//FPn,Dn
	[041 ... 042] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FP(IAR|SR)
	[044]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[051 ... 052] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FP(IAR|SR),Dn
	[054]		  = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn
}

static EMIT_Function JumpTableAn[64] = { //FMOVE.L An,FPIAR and FMOVE.L FPIAR,An 
	[041]			  = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A0,FPIAR 
	[051]			  = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE>L FPIAR,A0
}

static EMIT_Function JumpTableAnIndir[64] = { //(An)
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT,
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },			//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },			//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },			//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },			//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },			//FPn,<ea>
	[041 ... 047] = { { EMIT_FMOVEM_L,
	[051 ... 057] = { { EMIT_FMOVEM_L,
	[060]		  = { { EMIT_FMOVEM,	//static, predecrement
	[062]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[064]		  = { { EMIT_FMOVEM,	//static, postincrement
	[066]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
	[070]		  = { { EMIT_FMOVEM,	//static, predecrement
	[072]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[074]		  = { { EMIT_FMOVEM,	//static, postincrement
	[076]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
}

static EMIT_Function JumpTableIncr[64] = { //(An)+
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[030 ... 031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[032 ... 033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[034]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[035]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[036]		  = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[041 ... 047] = { { EMIT_FMOVEM_L,
	[060]		  = { { EMIT_FMOVEM,	//static, predecrement
	[062]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[064]		  = { { EMIT_FMOVEM,	//static, postincrement
	[066]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
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
	[051 ... 057] = { { EMIT_FMOVEM_L,
	[070]		  = { { EMIT_FMOVEM,	//static, predecrement
	[072]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[074]		  = { { EMIT_FMOVEM,	//static, postincrement
	[076]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
}

static EMIT_Function JumpTableEA[64] = { //(d16,An);(An,Xn);(xxx).(W|L)
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
	[041 ... 047] = { { EMIT_FMOVEM_L,
	[051 ... 057] = { { EMIT_FMOVEM_L,
	[060]		  = { { EMIT_FMOVEM,	//static, predecrement
	[062]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[064]		  = { { EMIT_FMOVEM,	//static, postincrement
	[066]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
	[070]		  = { { EMIT_FMOVEM,	//static, predecrement
	[072]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[074]		  = { { EMIT_FMOVEM,	//static, postincrement
	[076]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
}

static EMIT_Function JumpTablePC[64] = { //(d16,PC);(PC,Xn);#<data>
	[020 ... 021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[022 ... 023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[024]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[025]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[026]		  = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[041 ... 047] = { { EMIT_FMOVEM_L,
	[060]		  = { { EMIT_FMOVEM,	//static, predecrement
	[062]		  = { { EMIT_FMOVEM,	//dynamic, predecrement
	[064]		  = { { EMIT_FMOVEM,	//static, postincrement
	[066]		  = { { EMIT_FMOVEM,	//dynamic. postincrement
}

/* FPU Instructions, Table 2; bits[5:0]*///bit 6 always assumed 0
static EMIT_Function JumpTableOp[64] = {
	[0x00]			= { { EMIT_FMOVE }, NULL, FPMC, FPEB, 0, 0, 0 }, // the Format should give the last 3 values
	[0x01]			= { { EMIT_FINT }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x02]			= { { EMIT_FSINH }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x03]			= { { EMIT_FINTRZ }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x04]			= { { EMIT_FSQRT }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x06]			= { { EMIT_FLOGNP1 }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x08]			= { { EMIT_FETOXM1 }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x09]			= { { EMIT_FTANH }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0A]			= { { EMIT_FATAN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0B]			= { { EMIT_FINTRN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0C]			= { { EMIT_FASIN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0D]			= { { EMIT_FATANH }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0E]			= { { EMIT_FSIN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x0F]			= { { EMIT_FTAN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x10]			= { { EMIT_FETOX }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x11]			= { { EMIT_FTWOTOX }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x12]			= { { EMIT_FTENTOX }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x13]			= { { EMIT_FINTRP }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x14]			= { { EMIT_FLOGN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x15]			= { { EMIT_FLOG10 }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x16]			= { { EMIT_FLOG2 }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x18]			= { { EMIT_FABS }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x19]			= { { EMIT_FCOSH }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1A]			= { { EMIT_FNEG }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1B]			= { { EMIT_FINTRM }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1C]			= { { EMIT_FACOS }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1D]			= { { EMIT_FCOS }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1E]			= { { EMIT_FGETEXP }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x1F]			= { { EMIT_FGETMAN }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x20]			= { { EMIT_FDIV }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x21]			= { { EMIT_FMOD }, NULL, 0, FPSR_ALL, 0, 0, 0 },
	[0x22]			= { { EMIT_FADD }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x23]			= { { EMIT_FMUL }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x24]			= { { EMIT_FSGLDIV }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//single precision
	[0x25]			= { { EMIT_FREM }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x26]			= { { EMIT_FSCALE }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x27]			= { { EMIT_FSGMUL }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//single precision
	[0x28]			= { { EMIT_FSUB }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x2D]			= { { EMIT_FMOD }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x30 ... 0x37] = { { EMIT_FSINCOS }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x38]			= { { EMIT_FCMP }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x3A]			= { { EMIT_FTST }, NULL, 0, FPCC | FPEB, 0, 0, 0 },
	[0x40]			= { { EMIT_FMOVE_S_dst }, NULL, FPMC, FPEB, 0, 0, 4 },	//rounded to single
	[0x41]			= { { EMIT_FSQRT_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x44]			= { { EMIT_FMOVE_D_dst }, NULL, FPMC, FPEB, 0, 0, 8 },	//rounded to double
	[0x45]			= { { EMIT_FSQRT_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x58]			= { { EMIT_FABS_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x5A]			= { { EMIT_FNEG_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x5C]			= { { EMIT_FABS_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x5E]			= { { EMIT_FNEG_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x60]			= { { EMIT_FDIV_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x62]			= { { EMIT_FADD_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x63]			= { { EMIT_FMUL_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x64]			= { { EMIT_FDIV_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x66]			= { { EMIT_FADD_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x67]			= { { EMIT_FMUL_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
	[0x68]			= { { EMIT_FSUB_S }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to single
	[0x6C]			= { { EMIT_FSUB_D }, NULL, 0, FPCC | FPEB, 0, 0, 0 },	//rounded to double
}

uint32_t *EMIT_FMOVECR(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){ /* FMOVECR only pulls extended-precision constants to a FP register */
	
}
/* Any format function should preload specified registers according to format and jump to FPU Instruction table. */
uint32_t *EMIT_FORMAT(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t fmt = 1 << ((opcode2 >> 10 ) & 7);	/* This should generate a single bit set in a byte. */
	uint32_t fpiar = ptr_m68k;
}

/* any and all FMOVE instructions, this can 2 or 3 nested jumps depending on the encoding */
uint32_t *EMIT_FMOVE(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t fmt = 1 << ((opcode2 >> 10 ) & 7);	/* This should generate a single bit set in a byte. */
	uint32_t fpiar = ptr_m68k;
}

uint32_t *EMIT_FMOVE_S_dest(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){
	
}
uint32_t *EMIT_FMOVE_D_dest(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){
	
}
//this instruction should not update FPIAR when executed, this is always Long
uint32_t *EMIT_FMOVEM_control(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t direction = ((opcode2 >> 10) & 1);
}
//this instruction should not update FPIAR when executed, this is always extended
uint32_t *EMIT_FMOVEM_static(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t direction = ((opcode2 >> 9) & 1);
}
//this instruction should not update FPIAR when executed, this is always extended
uint32_t *EMIT_FMOVEM_dynamic(uint32_t *ptr, uint16_t opcode, uint16_t opcode2, uint16_t **ptr_m68k){

	uint8_t direction = ((opcode2 >> 9) & 1);
}

/* FPU Monadic Operations */
uint32_t *EMIT_FABS(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FABS_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FABS_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FACOS(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FASIN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FATAN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FCOS(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FCOSH(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FETOX(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FETOXM1(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FGETEXP(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FGETMAN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FINT(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FINTRZ(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FLOGN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FLOGNP1(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FLOG10(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FLOG2(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FNEG(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FNEG_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FNEG_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSIN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSINH(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSQRT(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FSQRT_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FSQRT_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FTAN(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FTANH(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FTENTOX(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FTWOTOX(uint32_t *ptr, uint16_t **ptr_m68k)

/* FPU Dyadic Operations */
uint32_t *EMIT_FADD(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FADD_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FADD_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FCMP(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FDIV(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FDIV_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FDIV_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FMOD(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FMUL(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FMUL_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FMUL_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FREM(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSCALE(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSUB(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FSUB_S(uint32_t *ptr, uint16_t **ptr_m68k)
uint32_t *EMIT_FSUB_D(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSGLDIV(uint32_t *ptr, uint16_t **ptr_m68k)

uint32_t *EMIT_FSGMUL(uint32_t *ptr, uint16_t **ptr_m68k)
