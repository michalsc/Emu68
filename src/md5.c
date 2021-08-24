#include <stdint.h>
#include "md5.h"
#include "support.h"

static const uint8_t s[] = { 7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21 };

static const uint32_t K[] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

static inline void prefetch(void *from, void *to)
{
    uint64_t t1, t2;

    asm volatile("ldp %0, %1, [%2, #0]; stp %0, %1, [%3, #0]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #16]; stp %0, %1, [%3, #16]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #32]; stp %0, %1, [%3, #32]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
    asm volatile("ldp %0, %1, [%2, #48]; stp %0, %1, [%3, #48]":"=r"(t1), "=r"(t2):"r"(from), "r"(to));
}

struct MD5 CalcMD5(void *_start, void *_end)
{
    struct MD5 md = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

    uint32_t *M = (uint32_t*)_start;
    uint32_t *E = (uint32_t*)((intptr_t)_start + (((intptr_t)_end - (intptr_t)_start + 63) & ~63));
    uint32_t count = 1 + (E - M) / 16;
    uint32_t pad[16];

    prefetch(M, pad);

    while(count--)
    {
        uint32_t A = md.a;
        uint32_t B = md.b;
        uint32_t C = md.c;
        uint32_t D = md.d;

        for (int i=0; i < 64; i++)
        {
            uint32_t F, g;

            if (i <= 15)
            {
                F = (B & C) | ((~B) & D);
                g = i;
            }
            else if (i <= 31)
            {
                F = (D & B) | ((~D) & C);
                g = (5*i + 1) & 15;
            }
            else if (i <= 47)
            {
                F = B ^ C ^ D;
                g = (3*i + 5) & 15;
            }
            else
            {
                F = C ^ (B | ~D);
                g = (7*i) & 15;
            }

            F = F + A + K[i] + pad[g];
            A = D;
            D = C;
            C = B;
            B = B + ((F << s[i]) | (F >> (32 - s[i])));
        }

        md.a += A;
        md.b += B;
        md.c += C;
        md.d += D;

        M = M + 16;
        if (M == E)
        {
            pad[0] = BE32(0x80000000);
            for (int i=1; i < 14; i++)
                pad[i] = 0;
            pad[14] = (uintptr_t)_start;
            pad[15] = (uintptr_t)_end - (uintptr_t)_start;
        }
        else
        {
            prefetch(M, pad);
        }
    }

    return md;
}

uint32_t CalcCRC32(void *_start, void *_end)
{
    intptr_t s = (intptr_t)_start;
    intptr_t e = (intptr_t)_end;

    uint32_t crc = 0xffffffff;

    while((e - s) >= 16) {
        uint64_t val1; uint64_t val2;
        asm volatile("ldp %0, %1, [%2]":"=r"(val1), "=r"(val2):"r"(s));
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val1));
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val2));
        s += 16;
    }
    if ((e - s) >= 8) {
        uint64_t val = *(uint64_t *)s;
        asm volatile("crc32x %w0, %w0, %2":"=r"(crc):"0"(crc),"r"(val));
        s += 8;
    }
    if ((e - s) >= 4) {
        uint32_t val = *(uint32_t *)s;
        asm volatile("crc32w %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
        s += 4;
    }
    if ((e - s) >= 2) {
        uint16_t val = *(uint16_t *)s;
        asm volatile("crc32h %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
        s += 2;
    }
    if (e != s) {
        uint16_t val = *(uint16_t *)s;
        asm volatile("crc32b %w0, %w0, %w2":"=r"(crc):"0"(crc),"r"(val));
    }

    return crc;
}