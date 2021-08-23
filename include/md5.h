#ifndef _MD5_H
#define _MD5_H

#include <stdint.h>

struct MD5 {
    uint32_t a, b, c, d;
};

struct MD5 CalcMD5(void *_start, void *_end);
uint32_t CalcCRC32(void *_start, void *_end);

#endif /* _MD5_H */
