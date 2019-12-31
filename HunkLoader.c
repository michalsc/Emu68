/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "DuffCopy.h"
#include "HunkLoader.h"
#include "ARM.h"

#define D(x) /* */

#ifdef RASPI

char * pool = (char *)0x000ffff8;
void * _my_malloc(size_t size)
{
    void *ptr = pool;
    pool += (size + 31) & ~31;
    return ptr;
}
#define malloc(s) _my_malloc(s)

#endif

void * LoadHunkFile(void *buffer)
{
    uint32_t *words = buffer;
    struct SegList * hunks = NULL;
    void * prevhunk = NULL;
    struct SegList *h = NULL;
    uint32_t first_to_load = 0;
    uint32_t last_to_load = 0;
    uint32_t current_block = 0;
    intptr_t ref_base = 0;
    intptr_t base = 0;

    kprintf("[HUNK] Loading Hunk file from address %p\n", buffer);

    if (BE32(words[0]) != 0x3f3 || BE32(words[1]) != 0)
    {
        kprintf("[HUNK] FAILURE: Wrong header %08x:%08x.\n", BE32(words[0]), BE32(words[1]));
        return NULL;
    }

    /* Parse header */
    first_to_load = BE32(words[3]);
    last_to_load = BE32(words[4]);

    D(kprintf("[HUNK] Pre-allocating segments %d to %d\n", first_to_load, last_to_load));

    words = &words[5];

    /* Pre-allocate memory for all loadable hunks */
    for (unsigned i = 0; i < last_to_load - first_to_load + 1; i++)
    {
        uint32_t size = 4 * (BE32(*words++) & 0x3fffffff);
        struct SegList *h = malloc(size + sizeof(struct SegList));
        bzero(h, size + sizeof(struct SegList));

        h->h_Next = 0;
        h->h_Size = size;
        if (hunks == NULL)
            hunks = h;
        if (prevhunk != NULL)
            *(uint32_t*)prevhunk = (uint32_t)((intptr_t)&h->h_Next);
        prevhunk = &h->h_Next;
    }

    if (1)
    {
        void *segments = &hunks->h_Next;

        kprintf("[HUNK] Dumping hunk list:\n");
        while (segments != NULL)
        {
            kprintf("[HUNK]   Hunk %08x, size %d, next %08x\n",
                segments, ((uint32_t*)segments)[-1], *(uint32_t*)segments);
            segments = (void*)(intptr_t)(*(uint32_t*)segments);
        }
    }

    D(kprintf("[HUNK] Pulling the hunk data\n"));

    /* Now pull file hunk by hunk and eventually apply relocations */
    h = hunks;

    do
    {
        switch (BE32(*words))
        {
            case 0x3e9:
                if (current_block >= first_to_load)
                {
                    D(kprintf("[HUNK] Loading block %d (code hunk) to %08x with size of %d words\n",
                        current_block, (void*)&h->h_Data, BE32(words[1])));
                    DuffCopy((void*)&h->h_Data, &words[2], BE32(words[1]));
                }
                else {
                    D(kprintf("[HUNK] Skipping block %d\n", current_block));
                }
                words += 2 + BE32(words[1]);
                break;

            case 0x3ea:
                if (current_block >= first_to_load)
                {
                    D(kprintf("[HUNK] Loading block %d (data hunk) to %08x with size of %d words\n",
                        current_block, (void*)h->h_Data, BE32(words[1])));
                    DuffCopy((void*)&h->h_Data, &words[2], BE32(words[1]));
                }
                else {
                    D(kprintf("[HUNK] Skipping block %d\n", current_block));
                }
                words += 2 + BE32(words[1]);
                break;

            case 0x3eb:
                if (current_block >= first_to_load)
                {
                    D(kprintf("[HUNK] Block %d (bss hunk) with size of %d words\n",
                        current_block, BE32(words[1])));
                }
                else {
                    D(kprintf("[HUNK] Skipping block %d\n", current_block));
                }
                words += 2;
                break;

            case 0x3ec:
                if (current_block >= first_to_load) {
                    D(kprintf("[HUNK] Applying relocations to previous section\n"));
                } else {
                    D(kprintf("[HUNK] Skipping relocations for block %d\n", current_block));
                }
                ref_base = 0;
                base = (intptr_t)&h->h_Data;
                words++;
                while(BE32(words[0]) != 0)
                {
                    uint32_t count = BE32(words[0]);
                    uint32_t refcnt = BE32(words[1]);
                    if (current_block >= first_to_load)
                    {
                        void *segments = &hunks->h_Next;
                        for (unsigned i=0; i < refcnt; i++)
                        {
                            segments = (void *)(intptr_t)*(uint32_t*)segments;
                        }
                        ref_base = (intptr_t)segments + 4;
                        words += 2;

                        D(kprintf("[HUNK]   section %d (base %08x):\n", refcnt, ref_base));

                        while(count--)
                        {
                            uint32_t off = BE32(*words++);
                            D(kprintf("[HUNK]    at offset %08x\n", off));
                            *(uint32_t*)(base + off) = BE32(
                                BE32(*(uint32_t*)(base+off)) + ref_base
                            );
                        }
                    }
                    else
                        words += 2 + count;
                }
                words++;
                break;

            case 0x3fd:
                if (current_block >= first_to_load) {
                    D(kprintf("[HUNK] Applying PC-relative relocations to previous section\n"));
                } else {
                    D(kprintf("[HUNK] Skipping relocations for block %d\n", current_block));
                }
                ref_base = 0;
                base = (intptr_t)&h->h_Data;
                words++;
                while(BE32(words[0]) != 0)
                {
                    uint32_t count = BE32(words[0]);
                    uint32_t refcnt = BE32(words[1]);
                    if (current_block >= first_to_load)
                    {
                        void *segments = &hunks->h_Next;
                        for (unsigned i=0; i < refcnt; i++)
                        {
                            segments = (void*)(intptr_t)(*(uint32_t*)segments);
                        }

                        ref_base = (intptr_t)segments + 4;
                        ref_base -= (intptr_t)base;
                        words += 2;

                        D(kprintf("[HUNK]   section %d (base %08x):\n", refcnt, ref_base));

                        while(count--)
                        {
                            uint32_t off = BE32(*words++);
                            D(kprintf("[HUNK]    at offset %08x\n", off));
                            *(uint32_t*)(base + off) = BE32(
                                BE32(*(uint32_t*)(base+off)) + ref_base
                            );
                        }
                    }
                    else
                        words += 2 + count;
                }
                words++;
                break;

            case 0x3f0:
                D(kprintf("[HUNK] Symbols. Skipping...\n"));
                words++;
                while(BE32(words[0]) != 0)
                {
                    words += BE32(words[0]) + 2;
                }
                words++;
                break;

            case 0x3f2:
                D(kprintf("[HUNK] End of block\n"));
                words++;
                current_block++;
                h = (struct SegList *)((uintptr_t)h->h_Next - __builtin_offsetof(struct SegList, h_Next));
                break;

            default:
                kprintf("[HUNK] Unknown hunk type %08x at %08x\n", BE32(*words), words);
                words += 2 + BE32(words[1]);
                break;
        }
    } while(current_block <= last_to_load);




    return &hunks->h_Next;
}
