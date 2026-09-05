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
