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

void RA_TouchM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_SetDirtyM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_InsertM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_RemoveM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_DiscardM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_FlushM68kRegs(uint32_t **arm_stream);
void RA_StoreDirtyM68kRegs(uint32_t **arm_stream);

uint16_t RA_GetChangedMask();
void RA_ClearChangedMask();

uint8_t RA_AllocARMRegister(uint32_t **arm_stream);
void RA_FreeARMRegister(uint32_t **arm_stream, uint8_t arm_reg);

uint8_t RA_GetMappedARMRegister(uint8_t m68k_reg);
uint8_t RA_IsARMRegisterMapped(uint8_t arm_reg);
void RA_AssignM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg, uint8_t arm_reg);
uint8_t RA_MapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint8_t RA_MapM68kRegisterForWrite(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_UnmapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint8_t RA_CopyFromM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint16_t RA_GetTempAllocMask();

void RA_ResetFPUAllocator();
uint8_t RA_AllocFPURegister(uint32_t **arm_stream);
void RA_FreeFPURegister(uint32_t **arm_stream, uint8_t arm_reg);
uint8_t RA_MapFPURegister(uint32_t **arm_stream, uint8_t fpu_reg);
uint8_t RA_MapFPURegisterForWrite(uint32_t **arm_stream, uint8_t fpu_reg);
void RA_SetDirtyFPURegister(uint32_t **arm_stream, uint8_t fpu_reg);
void RA_FlushFPURegs(uint32_t **arm_stream);
void RA_StoreDirtyFPURegs(uint32_t **arm_stream);


#endif /* _REGISTER_ALLOCATOR_H */
