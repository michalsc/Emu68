#ifndef _HUNKLOADER_H
#define _HUNKLOADER_H

#include <stdint.h>

struct SegList {
    uint32_t        h_Size;
    struct SegList *h_Next;
    uint8_t         h_Data[];
};

void * LoadHunkFile(void *buffer);

#endif /* _HUNKLOADER_H */
