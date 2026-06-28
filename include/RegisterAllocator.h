/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _REGISTER_ALLOCATOR_H
#define _REGISTER_ALLOCATOR_H

#include <stdint.h>
#include "M68k.h"

void RA_TouchM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_SetDirtyM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_InsertM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_RemoveM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_DiscardM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_FlushM68kRegs(struct TranslatorContext *ctx);
void RA_StoreDirtyM68kRegs(struct TranslatorContext *ctx);

uint16_t RA_GetChangedMask();
void RA_ClearChangedMask();

uint8_t RA_AllocARMRegister(struct TranslatorContext *);
void RA_FreeARMRegister(struct TranslatorContext *ctx, uint8_t arm_reg);
int RA_IsM68kRegister(uint8_t arm_reg);

uint8_t RA_GetMappedARMRegister(uint8_t m68k_reg);
uint8_t RA_IsARMRegisterMapped(uint8_t arm_reg);
void RA_AssignM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg, uint8_t arm_reg);
uint8_t RA_MapM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
uint8_t RA_MapM68kRegisterForWrite(struct TranslatorContext *ctx, uint8_t m68k_reg);
void RA_UnmapM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
uint8_t RA_CopyFromM68kRegister(struct TranslatorContext *ctx, uint8_t m68k_reg);
uint16_t RA_GetTempAllocMask();

void RA_ResetFPUAllocator();
uint8_t RA_AllocFPURegister(struct TranslatorContext *ctx);
void RA_FreeFPURegister(struct TranslatorContext *ctx, uint8_t arm_reg);
uint8_t RA_MapFPURegister(struct TranslatorContext *ctx, uint8_t fpu_reg);
uint8_t RA_MapFPURegisterForWrite(struct TranslatorContext *ctx, uint8_t fpu_reg);
void RA_SetDirtyFPURegister(struct TranslatorContext *ctx, uint8_t fpu_reg);
void RA_FlushFPURegs(struct TranslatorContext *ctx);
void RA_StoreDirtyFPURegs(struct TranslatorContext *ctx);

uint8_t RA_TryCTX(struct TranslatorContext *ctx);
uint8_t RA_GetCTX(struct TranslatorContext *ctx);
void RA_FlushCTX(struct TranslatorContext *ctx);
int RA_IsCCLoaded();
int RA_IsCCModified();
uint8_t RA_GetCC(struct TranslatorContext *ctx);
uint8_t RA_ModifyCC(struct TranslatorContext *ctx);
void RA_FlushCC(struct TranslatorContext *ctx);
void RA_StoreCC(struct TranslatorContext *ctx);
uint8_t RA_GetFPCR(struct TranslatorContext *ctx);
uint8_t RA_ModifyFPCR(struct TranslatorContext *ctx);
void RA_FlushFPCR(struct TranslatorContext *ctx);
void RA_StoreFPCR(struct TranslatorContext *ctx);
uint8_t RA_GetFPSR(struct TranslatorContext *ctx);
uint8_t RA_ModifyFPSR(struct TranslatorContext *ctx);
void RA_FlushFPSR(struct TranslatorContext *ctx);
void RA_StoreFPSR(struct TranslatorContext *ctx);

void EMIT_SaveRegFrame(struct TranslatorContext *ctx, uint32_t mask);
void EMIT_RestoreRegFrame(struct TranslatorContext *ctx, uint32_t mask);

#endif /* _REGISTER_ALLOCATOR_H */
