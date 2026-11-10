/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU_LOGO_H
#define _EMU_LOGO_H

#include <stdint.h>

struct EmuLogo {
    uint16_t el_Width;
    uint16_t el_Height;
    uint8_t  *el_Data;
};

enum LogoTheme {
    GRAY = 0,
    BLACK,
    PURPLE,
    CLAY,
    REGAL,
    REEF,
    ORCHID,
    DUNE,
    EMBER,
    MOSS,
    BONE,
    WB13,
    WB20,
    WB20i
};

extern struct EmuLogo EmuLogo;

#endif /* _EMU_LOGO_H */
