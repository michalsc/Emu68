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

uint8_t RA_MapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);
uint8_t RA_MapM68kRegisterForWrite(uint32_t **arm_stream, uint8_t m68k_reg);
void RA_UnmapM68kRegister(uint32_t **arm_stream, uint8_t m68k_reg);

#endif /* _REGISTER_ALLOCATOR_H */
