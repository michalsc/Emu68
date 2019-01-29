#ifndef _REGISTER_ALLOCATOR_H
#define _REGISTER_ALLOCATOR_H

#include <stdint.h>

uint8_t RA_SelectNewSlot();
void RA_UpdateSlot(uint8_t slot);
uint8_t RA_MapM68kRegister(uint8_t m68k_reg)

#endif /* _REGISTER_ALLOCATOR_H */
