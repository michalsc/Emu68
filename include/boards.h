#ifndef _BOARDS_H
#define _BOARDS_H

#include <stdint.h>

struct ExpansionBoard {
    const void *    rom_file;
    uint32_t        rom_size;
    uint32_t        map_base;
    uint32_t        is_z3;
    uint32_t        enabled;
    void            (*map)(struct ExpansionBoard *);
};

#endif /* _BOARDS_H */
