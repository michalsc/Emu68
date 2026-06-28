#pragma pack(push,2)
#include <exec/types.h>
#include "powerpc/powerpc.h"
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

/* On AArch64 we do not have so many constraints regarding the alignment */
void L_CopyMemPPC(struct PPCBase *, void * source, void * dest, ULONG size)
{
    DFUNC(kprintf("[PPC] powerpc.library/CopyMemPPC(%08x, %08x, %d)\n", source, dest, size));
    
    union {
        ULONG *u32;
        UBYTE *u8;
        VOID *v;
    } src, dst;

    ULONG size_4L = size >> 4;
    ULONG size_L = (size >> 2) & 3;
    ULONG size_B = size & 3;

    src.v = source;
    dst.v = dest;

    switch(size_L) {
        case 3: 
            *dst.u32++ = *src.u32++;
            // Fallthrough
        case 2: 
            *dst.u32++ = *src.u32++;
            // Fallthrough
        case 1:
            *dst.u32++ = *src.u32++;
            break;
    }

    while(size_4L--) {
        ULONG t1, t2, t3, t4;

        t1 = *src.u32++;
        t2 = *src.u32++;
        t3 = *src.u32++;
        t4 = *src.u32++;

        *dst.u32++ = t1;
        *dst.u32++ = t2;
        *dst.u32++ = t3;
        *dst.u32++ = t4;
    }

    switch(size_B) {
        case 3: 
            *dst.u8++ = *src.u8++;
            // Fallthrough
        case 2: 
            *dst.u8++ = *src.u8++;
            // Fallthrough
        case 1:
            *dst.u8++ = *src.u8++;
            break;
    }
}
