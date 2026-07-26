#ifndef _REGLOCK_H
#define _REGLOCK_H

#include <arm_neon.h>

register uint64x2_t reserved_reg_q19 asm("q19");
register uint64x2_t reserved_reg_q20 asm("q20");
register uint64x2_t reserved_reg_q21 asm("q21");

#endif /* _REGLOCK_H */
