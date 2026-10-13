/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
