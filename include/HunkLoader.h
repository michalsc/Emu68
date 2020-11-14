#ifndef _HUNKLOADER_H
#define _HUNKLOADER_H

#include <stdint.h>

struct SegList {
    uint32_t        h_Size;
    uint32_t        h_Next;
    uint8_t         h_Data[];
};

void * LoadHunkFile(void *buffer, void *base);

#endif /* _HUNKLOADER_H */
