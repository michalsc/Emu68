/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _INTC_H
#define _INTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define GIC_PPI_LEGACY_IRQ      31
#define GIC_PPI_NPTIMER         30
#define GIC_PPI_SPTIMER         29
#define GIC_PPI_LEGACY_FIQ      28
#define GIC_PPI_VTIMER          27
#define GIC_PPI_HTIMER          26
#define GIC_PPI_VMI             25

#define GIC_SPI(n)              (32 + (n))

int gic_available();
void gic_local_init();
void gic_local_disable();
void gic_irq_eanble(unsigned int id);
void gic_irq_disable(unsigned int id);
void gic_set_priority(unsigned int id, uint8_t prio);
uint32_t gic_read_iar();
void gic_write_eoir(uint32_t id);

void intc_global_init();

#ifdef __cplusplus
}
#endif

#endif /* _INTC_H */
