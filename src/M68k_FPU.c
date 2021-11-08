/****************************************************/
/* FPU Instruction opcodes and condition fields.	*/
/*													*/
/* This part holds all tables,						*/
/* Header according to the following format.		*/
/* Table Name.										*/
/* Assumed depth.									*/
/* The bits used [x:y]								*/
/*  												*/
/****************************************************/

#include "M68k.h"

/* 
Index

	EA decoding & format,
	D0
	D1
	D2
	D3
	D4
	D5
	D6
	D7
	(A0)
	(A1)
	(A2)
	(A3)
	(A4)
	(A5)
	(A6)
	(A7)
	(A0)+
	(A1)+
	(A2)+
	(A3)+
	(A4)+
	(A5)+
	(A6)+
	(A7)+
	-(A0)
	-(A1)
	-(A2)
	-(A3)
	-(A4)
	-(A5)
	-(A6)
	-(A7)
	(d16,A0)
	(d16,A1)
	(d16,A2)
	(d16,A3)
	(d16,A4)
	(d16,A5)
	(d16,A6)
	(d16,A7)
	(A0,Xn)
	(A1,Xn)
	(A2,Xn)
	(A3,Xn)
	(A4,Xn)
	(A5,Xn)
	(A6,Xn)
	(A7,Xn)
	(xxx).W
	(xxx).L
	(d16,PC)
	(PC,Xn)
	#<data>

	FPU Instructions,
	CC Instructions,
	Branch,
	Misc tests
	IEEE Aware tests
	IEEE Nonaware tests
*/

/* EA decoding & format, Table 1; bits[22:10] */
static EMIT_Funtion JumpTableCase0[4096] = {
	[00000 ... 00007] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },	//FPm,FPn

	//D0
	[00020 ... 00021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00027] = { { EMIT_FMOVECR }, NULL, 0, 0, 2, 0, 8 }, 			//#<ccc>,FPn //This should be an Extended format, but for sanity sake!
	[00030 ... 00031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00041 ... 00042] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00044] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00051 ... 00052] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00054] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D1
	[00120 ... 00121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00130 ... 00131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00141 ... 00142] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00144] = { { EMIT_FMOVEM_L } NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00151 ... 00152] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00154] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D2
	[00220 ... 00221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00230 ... 00231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00241 ... 00242] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00244] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00251 ... 00252] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00254] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D3
	[00320 ... 00321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00330 ... 00331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00341 ... 00342] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00344] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00351 ... 00352] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00354] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D4
	[00420 ... 00421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00430 ... 00431] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00434] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00436] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00441 ... 00442] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00444] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00451 ... 00452] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00454] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D5
	[00520 ... 00521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00524] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00526] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00530 ... [00531] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00534] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00536] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00541 ... 00542] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00544] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00551 ... 00552] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00554] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D6
	[00620 ... 00621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00630 ... 00631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00641 ... 00642] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00644] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00651 ... 00652] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00654] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//D7
	[00720 ... 00721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },	//Dn,FPn
	[00724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },				//Dn,FPn
	[00726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },				//Dn,FPn
	[00730 ... 00731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,Dn
	[00734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,Dn
	[00736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,Dn
	[00741 ... 00742] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//Dn,FP(IAR|SR)
	[00744] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//Dn,FPCR
	[00751 ... 00752] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },	//FP(IAR|SR),Dn
	[00754] = { { EMIT_FMOVEM_L }, NULL, 0, 0, 2, 0, 4 },			//FPCR,Dn

	//FMOVE.L An,FPIAR and FMOVE.L FPIAR,An 
	[01041] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A0,FPIAR 
	[01051] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE>L FPIAR,A0
	[01141] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A1,FPIAR
	[01151] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A1
	[01241] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A2,FPIAR
	[01251] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A2
	[01341] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A3,FPIAR
	[01351] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A3
	[01441] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A4,FPIAR
	[01451] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A4
	[01541] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A5,FPIAR
	[01551] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A5
	[01641] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A6,FPIAR
	[01651] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A6
	[01741] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L A7,FPIAR
	[01751] = { { EMIT_FMOVEM_control } NULL, 0, 0, 2, 0, 4 },	//FMOVE.L FPIAR,A7

	//(A0)
	[02020 ... 02021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02022 ... 02023] = { { EMIT_FORMAT,
	[02024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02030 ... 02031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02032 ... 02033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02041 ... 02047] = { { EMIT_FMOVEM_L,
	[02051 ... 02057] = { { EMIT_FMOVEM_L,
	[02060] = { { EMIT_FMOVEM,	//static, predecrement
	[02062] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02064] = { { EMIT_FMOVEM,	//static, postincrement
	[02066] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02070] = { { EMIT_FMOVEM,	//static, predecrement
	[02072] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02074] = { { EMIT_FMOVEM,	//static, postincrement
	[02076] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A1)
	[02120 ... 02121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02122 ... 02123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02130 ... 02131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02132 ... 02133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02141 ... 02147] = { { EMIT_FMOVEM_L,
	[02151 ... 02157] = { { EMIT_FMOVEM_L,
	[02160] = { { EMIT_FMOVEM,	//static, predecrement
	[02162] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02164] = { { EMIT_FMOVEM,	//static, postincrement
	[02166] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02170] = { { EMIT_FMOVEM,	//static, predecrement
	[02172] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02174] = { { EMIT_FMOVEM,	//static, postincrement
	[02176] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A2)
	[02220 ... 02221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02222 ... 02223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02230 ... 02231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02232 ... 02233] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02235] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02241 ... 02247] = { { EMIT_FMOVEM_L,
	[02251 ... 02257] = { { EMIT_FMOVEM_L,
	[02260] = { { EMIT_FMOVEM,	//static, predecrement
	[02262] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02264] = { { EMIT_FMOVEM,	//static, postincrement
	[02266] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02270] = { { EMIT_FMOVEM,	//static, predecrement
	[02272] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02274] = { { EMIT_FMOVEM,	//static, postincrement
	[02276] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A3)
	[02320 ... 02321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02322 ... 02323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02330 ... 02331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02332 ... 02333] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02335] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02341 ... 02347] = EMIT_FMOVEM_L,
	[02351 ... 02357] = EMIT_FMOVEM_L,
	[02360] = EMIT_FMOVEM,	//static, predecrement
	[02362] = EMIT_FMOVEM,	//dynamic, predecrement
	[02364] = EMIT_FMOVEM,	//static, postincrement
	[02366] = EMIT_FMOVEM,	//dynamic. postincrement
	[02370] = EMIT_FMOVEM,	//static, predecrement
	[02372] = EMIT_FMOVEM,	//dynamic, predecrement
	[02374] = EMIT_FMOVEM,	//static, postincrement
	[02376] = EMIT_FMOVEM,	//dynamic. postincrement

	//(A4)
	[02420 ... 02421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02422 ... 02423] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02425] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02430 ... 02431] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02432 ... 02433] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02434] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02435] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02436] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02441 ... 02447] = { { EMIT_FMOVEM_L,
	[02451 ... 02457] = { { EMIT_FMOVEM_L,
	[02460] = { { EMIT_FMOVEM,	//static, predecrement
	[02462] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02464] = { { EMIT_FMOVEM,	//static, postincrement
	[02466] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02470] = { { EMIT_FMOVEM,	//static, predecrement
	[02472] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02474] = { { EMIT_FMOVEM,	//static, postincrement
	[02476] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A5)
	[02520 ... 02521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02522 ... 02523] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02524] = { {EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02525] = { {EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02526] = { {EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02530 ... 02531] = { {EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },		//FPn,<ea>
	[02532 ... 02533] = { {EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02534] = { {EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02535] = { {EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02536] = { {EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02541 ... 02547] = { { EMIT_FMOVEM_L,
	[02551 ... 02557] = { { EMIT_FMOVEM_L,
	[02560] = { { EMIT_FMOVEM,	//static, predecrement
	[02562] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02564] = { { EMIT_FMOVEM,	//static, postincrement
	[02566] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02570] = { { EMIT_FMOVEM,	//static, predecrement
	[02572] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02574] = { { EMIT_FMOVEM,	//static, postincrement
	[02576] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A6)
	[02620 ... 02621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02622 ... 02623] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02625] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02630 ... 02631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02632 ... 02633] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02635] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02641 ... 02647] = { { EMIT_FMOVEM_L,
	[02651 ... 02657] = { { EMIT_FMOVEM_L,
	[02660] = { { EMIT_FMOVEM,	//static, predecrement
	[02662] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02664] = { { EMIT_FMOVEM,	//static, postincrement
	[02666] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02670] = { { EMIT_FMOVEM,	//static, predecrement
	[02672] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02674] = { { EMIT_FMOVEM,	//static, postincrement
	[02676] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A7)
	[02720 ... 02721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[02722 ... 02723] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[02724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[02725] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[02726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[02730 ... 02731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[02732 ... [02733] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[02734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[02735] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[02736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[02741 ... 02747] = { { EMIT_FMOVEM_L,
	[02751 ... 02757] = { { EMIT_FMOVEM_L,
	[02760] = { { EMIT_FMOVEM,	//static, predecrement
	[02762] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02764] = { { EMIT_FMOVEM,	//static, postincrement
	[02766] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[02770] = { { EMIT_FMOVEM,	//static, predecrement
	[02772] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[02774] = { { EMIT_FMOVEM,	//static, postincrement
	[02776] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A0)+
	[03020 ... 03021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03022 ... 03023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03030 ... 03031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03032 ... 03033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[03035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[03036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[03041 ... 03047] = { { EMIT_FMOVEM_L,
	[03060] = { { EMIT_FMOVEM,	//static, predecrement
	[03062] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03064] = { { EMIT_FMOVEM,	//static, postincrement
	[03066] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A1)+
	[03120 ... 03121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03122 ... 03123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03130 ... 03131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03132 ... 03133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },				//FPn,<ea>
	[03135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },				//FPn,<ea>
	[03136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },				//FPn,<ea>
	[03141 ... 03147] = { { EMIT_FMOVEM_L,
	[03160] = { { EMIT_FMOVEM,	//static, predecrement
	[03162] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03164] = { { EMIT_FMOVEM,	//static, postincrement
	[03166] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A2)+
	[03220 ... 03221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03222 ... 03223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03230 ... 03231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03232 ... 03233] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03235] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03241 ... 03247] = { { EMIT_FMOVEM_L,
	[03260] = { { EMIT_FMOVEM,	//static, predecrement
	[03262] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03264] = { { EMIT_FMOVEM,	//static, postincrement
	[03266] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A3)+
	[03320 ... 03321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03322 ... 03323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03330 ... 03331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03332 ... 03333] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03335] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03341 ... 03347] = { { EMIT_FMOVEM_L,
	[03360] = { { EMIT_FMOVEM,	//static, predecrement
	[03362] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03364] = { { EMIT_FMOVEM,	//static, postincrement
	[03366] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A4)+
	[03420 ... 03421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03422 ... 03423] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03425] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03430 ... 03431] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03432 ... 03433] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03434] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03435] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03436] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03441 ... 03447] = { { EMIT_FMOVEM_L,
	[03460] = { { EMIT_FMOVEM,	//static, predecrement
	[03462] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03464] = { { EMIT_FMOVEM,	//static, postincrement
	[03466] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A5)+
	[03520 ... 03521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03522 ... 03523] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03524] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03525] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03526] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03530 ... 03531] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03532 ... 03533] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03534] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03535] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03536] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03541 ... 03547] = { { EMIT_FMOVEM_L,
	[03560] = { { EMIT_FMOVEM,	//static, predecrement
	[03562] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03564] = { { EMIT_FMOVEM,	//static, postincrement
	[03566] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A6)+
	[03620 ... 03621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03622 ... 03623] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03625] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03630 ... 03631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03632 ... 03633] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03635] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03641 ... 03647] = { { EMIT_FMOVEM_L,
	[03660] = { { EMIT_FMOVEM,	//static, predecrement
	[03662] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03664] = { { EMIT_FMOVEM,	//static, postincrement
	[03666] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A7)+
	[03720 ... 03721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[03722 ... 03723] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[03724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[03725] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[03726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[03730 ... 03731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[03732 ... 03733] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[03734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[03735] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[03736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[03741 ... 03747] = { { EMIT_FMOVEM_L,
	[03760] = { { EMIT_FMOVEM,	//static, predecrement
	[03762] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[03764] = { { EMIT_FMOVEM,	//static, postincrement
	[03766] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A0)
	[04020 ... 04021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04022 ... 04023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04030 ... 04031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04032 ... 04033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04051 ... 04057] = { { EMIT_FMOVEM_L,
	[04070] = { { EMIT_FMOVEM,	//static, predecrement
	[04072] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04074] = { { EMIT_FMOVEM,	//static, postincrement
	[04076] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A1)
	[04120 ... 04121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04122 ... 04123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04130 ... 04131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04132 ... 04133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04151 ... 04157] = { { EMIT_FMOVEM_L,
	[04170] = { { EMIT_FMOVEM,	//static, predecrement
	[04172] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04174] = { { EMIT_FMOVEM,	//static, postincrement
	[04176] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A2)
	[04220 ... 04221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04222 ... 04223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04230 ... 04231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04232 ... 04233] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04235] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04251 ... 04257] = { { EMIT_FMOVEM_L,
	[04270] = { { EMIT_FMOVEM,	//static, predecrement
	[04272] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04274] = { { EMIT_FMOVEM,	//static, postincrement
	[04276] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A3)
	[04320 ... 04321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04322 ... 04323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04330 ... 04331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04332 ... 04333] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04335] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04351 ... 04357] = { { EMIT_FMOVEM_L,
	[04370] = { { EMIT_FMOVEM,	//static, predecrement
	[04372] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04374] = { { EMIT_FMOVEM,	//static, postincrement
	[04376] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A4)
	[04420 ... 04421] = EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04422 ... 04423] = EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04424] = EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04425] = EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04426] = EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04430 ... 04431] = EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04432 ... 04423] = EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04434] = EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04425] = EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04436] = EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04451 ... 04457] = EMIT_FMOVEM_L,
	[04470] = EMIT_FMOVEM,	//static, predecrement
	[04472] = EMIT_FMOVEM,	//dynamic, predecrement
	[04474] = EMIT_FMOVEM,	//static, postincrement
	[04476] = EMIT_FMOVEM,	//dynamic. postincrement

	//-(A5)
	[04520 ... 04521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04522 ... 04523] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04524] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04525] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04526] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04530 ... 04531] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04532 ... 04533] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04534] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04535] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04536] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04551 ... 04557] = { { EMIT_FMOVEM_L,
	[04570] = { { EMIT_FMOVEM,	//static, predecrement
	[04572] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04574] = { { EMIT_FMOVEM,	//static, postincrement
	[04576] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A6)
	[04620 ... 04621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04622 ... 04623] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04625] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04630 ... 04631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04632 ... 04633] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04635] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04651 ... 04657] = { { EMIT_FMOVEM_L,
	[04670] = { { EMIT_FMOVEM,	//static, predecrement
	[04672] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04674] = { { EMIT_FMOVEM,	//static, postincrement
	[04676] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//-(A7)
	[04720 ... 04721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[04722 ... 04723] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[04724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[04725] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[04726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[04730 ... 04731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[04732 ... 04733] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[04734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[04735] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[04736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[04751 ... 04757] = { { EMIT_FMOVEM_L,
	[04770] = { { EMIT_FMOVEM,	//static, predecrement
	[04772] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[04774] = { { EMIT_FMOVEM,	//static, postincrement
	[04776] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A0)
	[05020 ... 05021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05022 ... 05023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05030 ... 05031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05032 ... 05033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05041 ... 05047] = { { EMIT_FMOVEM_L,
	[05051 ... 05057] = { { EMIT_FMOVEM_L,
	[05060] = { { EMIT_FMOVEM,	//static, predecrement
	[05062] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05064] = { { EMIT_FMOVEM,	//static, postincrement
	[05066] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05070] = { { EMIT_FMOVEM,	//static, predecrement
	[05072] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05074] = { { EMIT_FMOVEM,	//static, postincrement
	[05076] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A1)
	[05120 ... 05121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05122 ... 05123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05130 ... 05131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05132 ... 05133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05141 ... 05147] = { { EMIT_FMOVEM_L,
	[05151 ... 05157] = { { EMIT_FMOVEM_L,
	[05160] = { { EMIT_FMOVEM,	//static, predecrement
	[05162] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05164] = { { EMIT_FMOVEM,	//static, postincrement
	[05166] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05170] = { { EMIT_FMOVEM,	//static, predecrement
	[05172] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05174] = { { EMIT_FMOVEM,	//static, postincrement
	[05176] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A2)
	[05220 ... 05221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05222 ... 05223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05230 ... 05231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05232 ... 05233] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05235] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05241 ... 05247] = { { EMIT_FMOVEM_L,
	[05251 ... 05257] = { { EMIT_FMOVEM_L,
	[05260] = { { EMIT_FMOVEM,	//static, predecrement
	[05262] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05264] = { { EMIT_FMOVEM,	//static, postincrement
	[05266] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05270] = { { EMIT_FMOVEM,	//static, predecrement
	[05272] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05274] = { { EMIT_FMOVEM,	//static, postincrement
	[05276] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A3)
	[05320 ... 05321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05322 ... 05323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05330 ... 05331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05332 ... 05333] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05335] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05341 ... 05347] = { { EMIT_FMOVEM_L,
	[05351 ... 05357] = { { EMIT_FMOVEM_L,
	[05360] = { { EMIT_FMOVEM,	//static, predecrement
	[05362] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05364] = { { EMIT_FMOVEM,	//static, postincrement
	[05366] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05370] = { { EMIT_FMOVEM,	//static, predecrement
	[05372] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05374] = { { EMIT_FMOVEM,	//static, postincrement
	[05376] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A4)
	[05420 ... 05421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05422 ... 05423] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05425] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05430 ... 05431] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05432 ... 05433] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05434] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05435] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05436] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05441 ... 05447] = { { EMIT_FMOVEM_L,
	[05451 ... 05457] = { { EMIT_FMOVEM_L,
	[05460] = { { EMIT_FMOVEM,	//static, predecrement
	[05462] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05464] = { { EMIT_FMOVEM,	//static, postincrement
	[05466] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05470] = { { EMIT_FMOVEM,	//static, predecrement
	[05472] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05474] = { { EMIT_FMOVEM,	//static, postincrement
	[05476] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A5)
	[05520 ... 05521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05522 ... 05523] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05524] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05525] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05526] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05530 ... 05531] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05532 ... 05533] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05534] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05535] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05536] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05541 ... 05547] = { { EMIT_FMOVEM_L,
	[05551 ... 05557] = { { EMIT_FMOVEM_L,
	[05560] = { { EMIT_FMOVEM,	//static, predecrement
	[05562] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05564] = { { EMIT_FMOVEM,	//static, postincrement
	[05566] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05570] = { { EMIT_FMOVEM,	//static, predecrement
	[05572] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05574] = { { EMIT_FMOVEM,	//static, postincrement
	[05576] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A6)
	[05620 ... 05621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05622 ... 05623] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05625] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05630 ... 05631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05632 ... 05633] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05635] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05641 ... 05647] = { { EMIT_FMOVEM_L,
	[05651 ... 05657] = { { EMIT_FMOVEM_L,
	[05660] = { { EMIT_FMOVEM,	//static, predecrement
	[05662] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05664] = { { EMIT_FMOVEM,	//static, postincrement
	[05666] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05670] = { { EMIT_FMOVEM,	//static, predecrement
	[05672] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05674] = { { EMIT_FMOVEM,	//static, postincrement
	[05676] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,A7)
	[05720 ... 05721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[05722 ... 05723] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[05724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[05725] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[05726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[05730 ... 05731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[05732 ... 05733] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[05734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[05735] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[05736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[05741 ... 05747] = { { EMIT_FMOVEM_L,
	[05751 ... 05757] = { { EMIT_FMOVEM_L,
	[05760] = { { EMIT_FMOVEM,	//static, predecrement
	[05762] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05764] = { { EMIT_FMOVEM,	//static, postincrement
	[05766] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[05770] = { { EMIT_FMOVEM,	//static, predecrement
	[05772] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[05774] = { { EMIT_FMOVEM,	//static, postincrement
	[05776] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A0,Xn)
	[06020 ... 06021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06022 ... 06023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06030 ... 06031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06032 ... 06033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06041 ... 06047] = { { EMIT_FMOVEM_L,
	[06051 ... 06057] = { { EMIT_FMOVEM_L,
	[06060] = { { EMIT_FMOVEM,	//static, predecrement
	[06062] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06064] = { { EMIT_FMOVEM,	//static, postincrement
	[06066] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06070] = { { EMIT_FMOVEM,	//static, predecrement
	[06072] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06074] = { { EMIT_FMOVEM,	//static, postincrement
	[06076] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A1,Xn)
	[06120 ... 06121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06122 ... 06123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06130 ... 06131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06132 ... 06133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06141 ... 06147] = { { EMIT_FMOVEM_L,
	[06151 ... 06157] = { { EMIT_FMOVEM_L,
	[06160] = { { EMIT_FMOVEM,	//static, predecrement
	[06162] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06164] = { { EMIT_FMOVEM,	//static, postincrement
	[06166] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06170] = { { EMIT_FMOVEM,	//static, predecrement
	[06172] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06174] = { { EMIT_FMOVEM,	//static, postincrement
	[06176] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A2,Xn)
	[06220 ... 06221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06222 ... 06223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06230 ... 06231] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06232 ... 06233] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06234] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06235] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06236] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06241 ... 06247] = { { EMIT_FMOVEM_L,
	[06251 ... 06257] = { { EMIT_FMOVEM_L,
	[06260] = { { EMIT_FMOVEM,	//static, predecrement
	[06262] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06264] = { { EMIT_FMOVEM,	//static, postincrement
	[06266] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06270] = { { EMIT_FMOVEM,	//static, predecrement
	[06272] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06274] = { { EMIT_FMOVEM,	//static, postincrement
	[06276] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A3,Xn)
	[06320 ... 06321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06322 ... 06323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06330 ... 06331] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06332 ... 06333] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06334] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06335] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06336] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06341 ... 06347] = { { EMIT_FMOVEM_L,
	[06351 ... 06357] = { { EMIT_FMOVEM_L,
	[06360] = { { EMIT_FMOVEM,	//static, predecrement
	[06362] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06364] = { { EMIT_FMOVEM,	//static, postincrement
	[06366] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06370] = { { EMIT_FMOVEM,	//static, predecrement
	[06372] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06374] = { { EMIT_FMOVEM,	//static, postincrement
	[06376] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A4,Xn)
	[06420 ... 06421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06422 ... 06423] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06425] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06430 ... 06431] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06432 ... 06433] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06434] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06435] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06436] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06441 ... 06447] = { { EMIT_FMOVEM_L,
	[06451 ... 06457] = { { EMIT_FMOVEM_L,
	[06460] = { { EMIT_FMOVEM,	//static, predecrement
	[06462] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06464] = { { EMIT_FMOVEM,	//static, postincrement
	[06466] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06470] = { { EMIT_FMOVEM,	//static, predecrement
	[06472] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06474] = { { EMIT_FMOVEM,	//static, postincrement
	[06476] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A5,Xn)
	[06520 ... 06521] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06522 ... 06523] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06524] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06525] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06526] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06530 ... 06531] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06532 ... 06533] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06534] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06535] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06536] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06541 ... 06547] = { { EMIT_FMOVEM_L,
	[06551 ... 06557] = { { EMIT_FMOVEM_L,
	[06560] = { { EMIT_FMOVEM,	//static, predecrement
	[06562] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06564] = { { EMIT_FMOVEM,	//static, postincrement
	[06566] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06570] = { { EMIT_FMOVEM,	//static, predecrement
	[06572] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06574] = { { EMIT_FMOVEM,	//static, postincrement
	[06576] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A6,Xn)
	[06620 ... 06621] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06622 ... 06623] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06624] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06625] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06626] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06630 ... 06631] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06632 ... 06633] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06634] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06635] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06636] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06641 ... 06647] = { { EMIT_FMOVEM_L,
	[06651 ... 06657] = { { EMIT_FMOVEM_L,
	[06660] = { { EMIT_FMOVEM,	//static, predecrement
	[06662] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06664] = { { EMIT_FMOVEM,	//static, postincrement
	[06666] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06670] = { { EMIT_FMOVEM,	//static, predecrement
	[06672] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06674] = { { EMIT_FMOVEM,	//static, postincrement
	[06676] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(A7,Xn)
	[06720 ... 06721] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[06722 ... 06723] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[06724] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[06725] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[06726] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[06730 ... 06731] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[06732 ... 06733] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[06734] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[06735] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[06736] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[06741 ... 06747] = { { EMIT_FMOVEM_L,
	[06751 ... 06757] = { { EMIT_FMOVEM_L,
	[06760] = { { EMIT_FMOVEM,	//static, predecrement
	[06762] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06764] = { { EMIT_FMOVEM,	//static, postincrement
	[06766] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[06770] = { { EMIT_FMOVEM,	//static, predecrement
	[06772] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[06774] = { { EMIT_FMOVEM,	//static, postincrement
	[06776] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(xxx).W
	[07020 ... 07021] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[07022 ... 07023] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[07024] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[07025] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[07026] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[07030 ... 07031] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[07032 ... 07033] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[07034] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[07035] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[07036] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[07041 ... 07047] = { { EMIT_FMOVEM_L,
	[07051 ... 07057] = { { EMIT_FMOVEM_L,
	[07060] = { { EMIT_FMOVEM,	//static, predecrement
	[07062] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07064] = { { EMIT_FMOVEM,	//static, postincrement
	[07066] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[07070] = { { EMIT_FMOVEM,	//static, predecrement
	[07072] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07074] = { { EMIT_FMOVEM,	//static, postincrement
	[07076] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(xxx).L
	[07120 ... 07121] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[07122 ... 07123] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[07124] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[07125] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[07126] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[07130 ... 07131] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 4 },	//FPn,<ea>
	[07132 ... 07133] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 12 },	//FPn,<ea>
	[07134] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 2 },	//FPn,<ea>
	[07135] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 8 },	//FPn,<ea>
	[07136] = { { EMIT_FMOVE }, NULL, 0, 0, 2, 0, 1 },	//FPn,<ea>
	[07141 ... 07147] = { { EMIT_FMOVEM_L,
	[07151 ... 07157] = { { EMIT_FMOVEM_L,
	[07160] = { { EMIT_FMOVEM,	//static, predecrement
	[07162] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07164] = { { EMIT_FMOVEM,	//static, postincrement
	[07166] = { { EMIT_FMOVEM,	//dynamic. postincrement
	[07170] = { { EMIT_FMOVEM,	//static, predecrement
	[07172] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07174] = { { EMIT_FMOVEM,	//static, postincrement
	[07176] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(d16,PC)
	[07220 ... 07221] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[07222 ... 07223] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[07224] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[07225] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[07226] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[07241 ... 07247] = { { EMIT_FMOVEM_L,
	[07260] = { { EMIT_FMOVEM,	//static, predecrement
	[07262] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07264] = { { EMIT_FMOVEM,	//static, postincrement
	[07266] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//(PC,Xn) & other relatives (further decoding of the  required!)
	[07320 ... 07321] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 4 },
	[07322 ... 07323] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 12 },
	[07324] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 2 },
	[07325] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 8 },
	[07326] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 1, 1 },
	[07341 ... 07347] = { { EMIT_FMOVEM_L,
	[07360] = { { EMIT_FMOVEM,	//static, predecrement
	[07362] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07364] = { { EMIT_FMOVEM,	//static, postincrement
	[07366] = { { EMIT_FMOVEM,	//dynamic. postincrement

	//#<data>
	[07420 ... 07421] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 4 },
	[07422 ... 07423] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 12 },
	[07424] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 2 },
	[07425] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 8 },
	[07426] = { { EMIT_FORMAT }, NULL, 0, 0, 2, 0, 1 },
	[07441 ... 07447] = { { EMIT_FMOVEM_L,
	[07460] = { { EMIT_FMOVEM,	//static, predecrement
	[07462] = { { EMIT_FMOVEM,	//dynamic, predecrement
	[07464] = { { EMIT_FMOVEM,	//static, postincrement
	[07466] = { { EMIT_FMOVEM,	//dynamic. postincrement
}

/* FPU Instructions, Table 2; bits[6:0]*///bit 6 assumed 0
static EMIT_Function JumpTableOp[64] = {
	[0x00] = { { EMIT_FMOVE },
	[0x01] = { { EMIT_FINT },
	[0x02] = { { EMIT_FSINH },
	[0x03] = { { EMIT_FINTRZ },
	[0x04] = { { EMIT_FSQRT },
	[0x06] = { { EMIT_FLOGNP1 },
	[0x08] = { { EMIT_FETOXM1 },
	[0x09] = { { EMIT_FTANH },
	[0x0A] = { { EMIT_FATAN },
	[0x0B] = { { EMIT_FINTRN },
	[0x0C] = { { EMIT_FASIN },
	[0x0D] = { { EMIT_FATANH },
	[0x0E] = { { EMIT_FSIN },
	[0x0F] = { { EMIT_FTAN },
	[0x10] = { { EMIT_FETOX },
	[0x11] = { { EMIT_FTWOTOX },
	[0x12] = { { EMIT_FTENTOX },
	[0x13] = { { EMIT_FINTRP },
	[0x14] = { { EMIT_FLOGN },
	[0x15] = { { EMIT_FLOG10 },
	[0x16] = { { EMIT_FLOG2 },
	[0x18] = { { EMIT_FABS },
	[0x19] = { { EMIT_FCOSH },
	[0x1A] = { { EMIT_FNEG },
	[0x1B] = { { EMIT_FINTRM },
	[0x1C] = { { EMIT_FACOS },
	[0x1D] = { { EMIT_FCOS },
	[0x1E] = { { EMIT_FGETEXP },
	[0x1F] = { { EMIT_FGETMAN },
	[0x20] = { { EMIT_FDIV },
	[0x21] = { { EMIT_FMOD },
	[0x22] = { { EMIT_FADD },
	[0x23] = { { EMIT_FMUL },
	[0x24] = { { EMIT_FSGLDIV },	//single precision
	[0x25] = { { EMIT_FREM },
	[0x26] = { { EMIT_FSCALE },
	[0x27] = { { EMIT_FSGMUL },	//single precision
	[0x28] = { { EMIT_FSUB },
	[0x2D] = { { EMIT_FMOD },
	[0x30 ... 0x37] = EMIT_FSINCOS },
	[0x38] = { { EMIT_FCMP },
	[0x3A] = { { EMIT_FTST },
	[0x40] = { { EMIT_FMOVE_S_dst }, NULL, 0, 0, 2, 0, 4 },	//rounded to single
	[0x41] = { { EMIT_FSQRT_S },	//rounded to single
	[0x44] = { { EMIT_FMOVE_D_dst }, NULL, 0, 0, 2, 0, 8 },	//rounded to double
	[0x45] = { { EMIT_FSQRT_D },	//rounded to double
	[0x58] = { { EMIT_FABS_S },	//rounded to single
	[0x5A] = { { EMIT_FNEG_S },	//rounded to single
	[0x5C] = { { EMIT_FABS_D },	//rounded to double
	[0x5E] = { { EMIT_FNEG_D },	//rounded to double
	[0x60] = { { EMIT_FDIV_S },	//rounded to single
	[0x62] = { { EMIT_FADD_S },	//rounded to single
	[0x63] = { { EMIT_FMUL_S },	//rounded to single
	[0x64] = { { EMIT_FDIV_D },	//rounded to double
	[0x66] = { { EMIT_FADD_D },	//rounded to double
	[0x67] = { { EMIT_FMUL_D },	//rounded to double
	[0x68] = { { EMIT_FSUB_S },	//rounded to single
	[0x6C] = { { EMIT_FSUB_D },	//rounded to double
}

/* CC instructions, Table 1; bits[5:0]*/
static EMIT_Function JumpTableCase1[64] = {
	[000 ... 007] = EMIT_FSCC_reg,
	[010 ... 017] = EMIT_FDBCC,
	[020 ... 047] = EMIT_FSCC,
	{050 ... 071] = EMIT_FSCC,
	[072 ... 074] = EMIT_FTRAPCC,
}

/* Branch, Table 1; bits [4:0]*///bit 5 of the CC is always 0
static EMIT_Function JumpTableCase2[32] = {
	//Misc tests
	[000] = EMIT_FBF,
	[017] = EMIT_FBT,
	[020] = EMIT_FBSF,
	[021] = EMIT_FBSEQ,
	[036] = EMIT_FBSNE,
	[037] = EMIT_FBST,
	//IEEE Aware tests
	[001] = EMIT_FBEQ,
	[002] = EMIT_FBOGT,
	[003] = EMIT_FBOGE,
	[004] = EMIT_FBOLT,
	[005] = EMIT_FBOLE,
	[006] = EMIT_FBOGL,
	[007] = EMIT_FBOR,
	[010] = EMIT_FBUN,
	[011] = EMIT_FBUEQ,
	[012] = EMIT_FBUGT,
	[013] = EMIT_FBUGE,
	[014] = EMIT_FBULT,
	[015] = EMIT_FBULE,
	[016] = EMIT_FBNE,
	//IEEE Nonaware tests
	[022] = EMIT_FBGT,
	[023] = EMIT_FBGE,
	[024] = EMIT_FBLT,
	[025] = EMIT_FBLE,
	[026] = EMIT_FBGL,
	[027] = EMIT_FBGLE,
	[030] = EMIT_FBNGLE,
	[031] = EMIT_FBNGL,
	[032] = EMIT_FBNLE,
	[033] = EMIT_FBNLT,
	[034] = EMIT_FBNGE,
	[035] = EMIT_FBNGT,
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
