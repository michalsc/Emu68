/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _HUNKLOADER_H
#define _HUNKLOADER_H

#include <stdint.h>

struct SegList {
    uint32_t        h_Size;
    uint32_t        h_Next;
    uint8_t         h_Data[];
};

void * LoadHunkFile(void *buffer, void *base, void *virt);
uint32_t GetHunkFileSize(void *buffer);

#endif /* _HUNKLOADER_H */
