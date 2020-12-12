#ifndef _PISTORM_H
#define _PISTORM_H

/*
    -- Description:
    -- Address     Function
    -- 0x0        Write 16
    -- 0x1        Read 16
    -- 0x2        Write 8
    -- 0x3        Read 8
    -- 0x4        Status
    -- 0x5
    -- 0x6        Write 32 -- not used for 68000, handeld in Musashi
    -- 0x7        Read 32 -- not used for 68000, handeld in Musash
    --
    -- Status Register
    -- 15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0
    -- ro ro ro wo wo wo ro ro                   rw rw
    -- I  I  I  F  F  F  B  B                    R  I
    -- P  P  P  C  C  C  E  G                    E  N
    -- L  L  L  2  1  0  R                       S  I
    -- 2  1  0           R                       E  T
*/

/* Commands which can be send to the fpga. WRITE32/READ32 not used for 68000 interface! */
#define PISTORM_CMD_WRITE16     0x00
#define PISTORM_CMD_READ16      0x01
#define PISTORM_CMD_WRITE8      0x02
#define PISTORM_CMD_READ8       0x03
#define PISTORM_CMD_STATUS      0x04
#define PISTORM_CMD_WRITE32     0x06
#define PISTORM_CMD_READ32      0x07

/* Status bits of M68k bus */
#define PISTORM_STAT_IPL_MASK   0xe000
#define PISTORM_STAT_IPL_SHIFT  13
#define PISTORM_STAT_IPL(x)     (((x) << PISTORM_STAT_IPL_SHIFT) & PISTORM_STAT_IPL_MASK)
#define PISTORM_STAT_FC_MASK    0x1c00
#define PISTORM_STAT_FC_SHIFT   10
#define PISTORM_STAT_FC(x)      (((x) << PISTORM_STAT_FC_SHIFT) & PISTORM_STAT_FC_MASK)
#define PISTORM_STAT_BERR       0x0200
#define PISTORM_STAT_BG         0x0100
#define PISTORM_STAT_RESET      0x0002
#define PISTORM_STAT_INIT       0x0001

#endif /* _PISTORM_H */
