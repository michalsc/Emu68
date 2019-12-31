/*
    Copyright � 1995-2014, The AROS Development Team. All rights reserved.
    $Id$
*/

#define _GNU_SOURCE

#include "support.h"
#include "tlsf.h"

#undef USE_MACROS

/*
 * Minimal alignment as required by AROS. In contrary to the default
 * TLSF implementation, we do not allow smaller blocks here.
 * Size needs to be aligned to at least 8, see THIS_FREE_MASK comment.
 */
#define SIZE_ALIGN      32

/*
 * Settings for TLSF allocator:
 * MAX_LOG2_SLI - amount of bits used for the second level list
 * MAX_FLI      - maximal allowable allocation size - 2^32 should be enough on 32bit systems
 *                64bit systems use 512GB limit.
 */
#define MAX_LOG2_SLI    (5)
#define MAX_SLI         (1 << MAX_LOG2_SLI)
#if __WORDSIZE == 64
#define MAX_FLI         (32+7)
#else
#define MAX_FLI         (32)
#endif
#define FLI_OFFSET      (6)
#define SMALL_BLOCK     (2 << FLI_OFFSET)

#define REAL_FLI        (MAX_FLI - FLI_OFFSET)

#define ROUNDUP(x)      (((x) + SIZE_ALIGN - 1) & ~(SIZE_ALIGN - 1))
#define ROUNDDOWN(x)    ((x) & ~(SIZE_ALIGN - 1))

/* Fields used in the block header length field to identify busy/free blocks */
#define THIS_FREE_MASK (uintptr_t)1
#define THIS_FREE   (uintptr_t)1
#define THIS_BUSY   (uintptr_t)0

#define PREV_FREE_MASK (uintptr_t)2
#define PREV_FREE   (uintptr_t)2
#define PREV_BUSY   (uintptr_t)0

#define SIZE_MASK   (~(THIS_FREE_MASK | PREV_FREE_MASK))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Size of additional memory needed to manage new block */
#define HEADERS_SIZE (((3 * ROUNDUP(sizeof(hdr_t))) + ROUNDUP(sizeof(tlsf_area_t))))

/* free node links together all free blocks if similar size */
typedef struct free_node_s {
    struct bhdr_s *    prev;
    struct bhdr_s *    next;
} free_node_t;

/* block header in front of each block - both free and busy */
typedef struct hdr_s {
    struct bhdr_s * prev;
    uintptr_t       length;
} hdr_t;

/*
 * Each block is defined by bhdr_t structure. Free blocks contain only
 * the header which allows us to go through all memory blocks in the system.
 * The free blocks contain additionally the node which chains them in one
 * of the free block lists
 */
typedef struct bhdr_s {
    union {
        hdr_t       header;
        uint8_t     __min_align[SIZE_ALIGN];
    };
    union {
        uint8_t         mem[1];
        free_node_t     free_node;
    };
} bhdr_t;

/* Memory area within the TLSF pool */
typedef struct tlsf_area_s {
    struct tlsf_area_s *    next;       // Next memory area
    bhdr_t *                end;        // Pointer to "end-of-area" block header
} tlsf_area_t;

typedef struct {
    tlsf_area_t *       memory_area;

    uintptr_t           total_size;
    uintptr_t           free_size;

    uint32_t            flbitmap;
    uint32_t            slbitmap[REAL_FLI];

    bhdr_t *            matrix[REAL_FLI][MAX_SLI];
} tlsf_t;

static inline __attribute__((always_inline)) int LS(uintptr_t i)
{
    if (sizeof(uintptr_t) == 4)
        return __builtin_ffs(i) - 1;
    else
        return __builtin_ffsl(i) - 1;
}

static inline __attribute__((always_inline)) int MS(uintptr_t i)
{
    if (sizeof(uintptr_t) == 4)
        return 31 - __builtin_clz(i);
    else
        return 63 - __builtin_clzl(i);
}

static inline __attribute__((always_inline)) void SetBit(int nr, uint32_t *ptr)
{
    ptr[nr >> 5] |= (1 << (nr & 31));
}

static inline __attribute__((always_inline)) void ClrBit(int nr, uint32_t *ptr)
{
    ptr[nr >> 5] &= ~(1 << (nr & 31));
}

static inline __attribute__((always_inline)) void MAPPING_INSERT(uintptr_t r, int *fl, int *sl)
{
    if (r < SMALL_BLOCK)
    {
        *fl = 0;
        *sl = (int)(r / (SMALL_BLOCK / MAX_SLI));
    }
    else
    {
        *fl = MS(r);
        *sl = (int)(((uintptr_t)r >> (*fl - MAX_LOG2_SLI)) - MAX_SLI);
        *fl -= FLI_OFFSET;
    }
}

static inline __attribute__((always_inline)) void MAPPING_SEARCH(uintptr_t *r, int *fl, int *sl)
{
    if (*r < SMALL_BLOCK)
    {
        *fl = 0;
        *sl = (int)(*r / (SMALL_BLOCK / MAX_SLI));
    }
    else
    {
        uintptr_t tmp = ((uintptr_t)1 << (MS(*r) - MAX_LOG2_SLI)) - 1;
        uintptr_t tr = *r + tmp;

        *fl = MS(tr);
        *sl = (int)(((uintptr_t)tr >> (*fl - MAX_LOG2_SLI)) - MAX_SLI);
        *fl -= FLI_OFFSET;
        *r = tr & ~tmp;
    }
}

static inline __attribute__((always_inline)) bhdr_t * FIND_SUITABLE_BLOCK(tlsf_t *tlsf, int *fl, int *sl)
{
    uintptr_t bitmap_tmp = tlsf->slbitmap[*fl] & ((uintptr_t)~0 << *sl);
    bhdr_t *b = NULL;

    if (bitmap_tmp)
    {
        *sl = LS(bitmap_tmp);
        b = tlsf->matrix[*fl][*sl];
    }
    else
    {
        bitmap_tmp = tlsf->flbitmap & ((uintptr_t)~0 << (*fl + 1));
        if (likely(bitmap_tmp != 0))
        {
            *fl = LS(bitmap_tmp);
            *sl = LS(tlsf->slbitmap[*fl]);
            b = tlsf->matrix[*fl][*sl];
        }
    }

    return b;
}


#ifdef USE_MACROS

#define GET_SIZE(b) ({ IPTR size = b->header.length & SIZE_MASK; size; })
#define GET_FLAGS(b) ({ IPTR flags = b->header.length & (THIS_FREE_MASK | PREV_FREE_MASK); flags; })
#define SET_SIZE(b, size) do{ b->header.length = GET_FLAGS(b) | (size); }while(0)
#define SET_FLAGS(b, flags) do{ b->header.length = GET_SIZE(b) | (flags); }while(0)
#define SET_SIZE_AND_FLAGS(b, size, flags) do{b->header.length = (size) | (flags);}while(0)
#define FREE_BLOCK(b) ((b->header.length & THIS_FREE_MASK) == THIS_FREE)
#define SET_FREE_BLOCK(b) do{b->header.length = (b->header.length & ~THIS_FREE_MASK) | THIS_FREE;}while(0)
#define SET_BUSY_BLOCK(b) do{b->header.length = (b->header.length & ~THIS_FREE_MASK) | THIS_BUSY;}while(0)
#define SET_FREE_PREV_BLOCK(b) do{b->header.length = (b->header.length & ~PREV_FREE_MASK) | PREV_FREE;}while(0)
#define SET_BUSY_PREV_BLOCK(b) do{b->header.length = (b->header.length & ~PREV_FREE_MASK) | PREV_BUSY;}while(0)
#define FREE_PREV_BLOCK(b) ((b->header.length & PREV_FREE_MASK) == PREV_FREE)
#define GET_NEXT_BHDR(hdr, size) ({ bhdr_t * __b = (bhdr_t *)((UBYTE *)&hdr->mem[0] + (size)); __b; })
#define MEM_TO_BHDR(ptr) ({ bhdr_t * b = (bhdr_t*)((void*)(ptr) - offsetof(bhdr_t, mem)); b; })

#define REMOVE_HEADER(tlsf, b, fl, sl) do{ \
        if (b->free_node.next)                                          \
            b->free_node.next->free_node.prev = b->free_node.prev;      \
        if (b->free_node.prev)                                          \
            b->free_node.prev->free_node.next = b->free_node.next;      \
        if (tlsf->matrix[fl][sl] == b) {                                \
            tlsf->matrix[fl][sl] = b->free_node.next;                   \
            if (!tlsf->matrix[fl][sl])                                  \
                ClrBit(sl, &tlsf->slbitmap[fl]);                        \
            if (!tlsf->slbitmap[fl])                                    \
                ClrBit(fl, &tlsf->flbitmap);                            \
        } } while(0)

#define INSERT_FREE_BLOCK(tlsf, b) do {                     \
    int fl, sl; MAPPING_INSERT(GET_SIZE(b), &fl, &sl);      \
    b->free_node.prev = NULL;                               \
    b->free_node.next = tlsf->matrix[fl][sl];               \
    if (tlsf->matrix[fl][sl])                               \
        tlsf->matrix[fl][sl]->free_node.prev = b;           \
    tlsf->matrix[fl][sl] = b;                               \
    SetBit(fl, &tlsf->flbitmap);                            \
    SetBit(sl, &tlsf->slbitmap[fl]); }while(0)

#else

static inline __attribute__((always_inline)) uintptr_t GET_SIZE(bhdr_t *b)
{
    return b->header.length & SIZE_MASK;
}

static inline __attribute__((always_inline)) uintptr_t GET_FLAGS(bhdr_t *b)
{
    return b->header.length & (THIS_FREE_MASK | PREV_FREE_MASK);
}

static inline __attribute__((always_inline)) void SET_SIZE(bhdr_t *b, uintptr_t size)
{
    b->header.length = GET_FLAGS(b) | size;
}

static inline __attribute__((always_inline)) void SET_SIZE_AND_FLAGS(bhdr_t *b, uintptr_t size, uintptr_t flags)
{
    b->header.length = size | flags;
}

static inline __attribute__((always_inline)) int FREE_BLOCK(bhdr_t *b)
{
    return ((b->header.length & THIS_FREE_MASK) == THIS_FREE);
}

static inline __attribute__((always_inline)) void SET_FREE_BLOCK(bhdr_t *b)
{
    b->header.length = (b->header.length & ~THIS_FREE_MASK) | THIS_FREE;
}

static inline __attribute__((always_inline)) void SET_BUSY_BLOCK(bhdr_t *b)
{
    b->header.length = (b->header.length & ~THIS_FREE_MASK) | THIS_BUSY;
}

static inline __attribute__((always_inline)) void SET_FREE_PREV_BLOCK(bhdr_t *b)
{
    b->header.length = (b->header.length & ~PREV_FREE_MASK) | PREV_FREE;
}

static inline __attribute__((always_inline)) void SET_BUSY_PREV_BLOCK(bhdr_t *b)
{
    b->header.length = (b->header.length & ~PREV_FREE_MASK) | PREV_BUSY;
}

static inline __attribute__((always_inline)) int FREE_PREV_BLOCK(bhdr_t *b)
{
    return ((b->header.length & PREV_FREE_MASK) == PREV_FREE);
}

static inline __attribute__((always_inline)) bhdr_t * GET_NEXT_BHDR(bhdr_t *hdr, uintptr_t size)
{
    return (bhdr_t *)((uint8_t *)&hdr->mem[0] + size);
}

static inline __attribute__((always_inline)) bhdr_t * MEM_TO_BHDR(void *ptr)
{
    return (bhdr_t *)((uintptr_t)ptr - __builtin_offsetof(bhdr_t, mem));
}

static inline __attribute__((always_inline)) void REMOVE_HEADER(tlsf_t *tlsf, bhdr_t *b, int fl, int sl)
{
    if (b->free_node.next)
        b->free_node.next->free_node.prev = b->free_node.prev;
    if (b->free_node.prev)
        b->free_node.prev->free_node.next = b->free_node.next;

    if (tlsf->matrix[fl][sl] == b)
    {
        tlsf->matrix[fl][sl] = b->free_node.next;
        if (!tlsf->matrix[fl][sl])
            ClrBit(sl, &tlsf->slbitmap[fl]);
        if (!tlsf->slbitmap[fl])
            ClrBit(fl, &tlsf->flbitmap);
    }
}

static inline __attribute__((always_inline)) void INSERT_FREE_BLOCK(tlsf_t *tlsf, bhdr_t *b)
{
    int fl, sl;

    MAPPING_INSERT(GET_SIZE(b), &fl, &sl);

    b->free_node.prev = NULL;
    b->free_node.next = tlsf->matrix[fl][sl];

    if (tlsf->matrix[fl][sl])
        tlsf->matrix[fl][sl]->free_node.prev = b;

    tlsf->matrix[fl][sl] = b;

    SetBit(fl, &tlsf->flbitmap);
    SetBit(sl, &tlsf->slbitmap[fl]);
}

#endif /* USE_MACROS */

void * tlsf_malloc(void *t, uintptr_t size)
{
    tlsf_t *tlsf = t;
    int fl, sl;
    bhdr_t *b = NULL;

    size = ROUNDUP(size);

    if (unlikely(!size)) return NULL;

    /* Find the indices fl and sl for given size */
    MAPPING_SEARCH(&size, &fl, &sl);

    /* Find block of either the right size or larger */
    b = FIND_SUITABLE_BLOCK(tlsf, &fl, &sl);

    /* No block found? Either failure or tlsf will get more memory. */
    if (unlikely(!b))
    {
        return NULL;
    }

    /* Next header */
    bhdr_t *next = GET_NEXT_BHDR(b, GET_SIZE(b));

    /* Remove the found block from the free list */
    REMOVE_HEADER(tlsf, b, fl, sl);

    /* Is this block larger then requested? Try to split it then */
    if (likely(GET_SIZE(b) > (size + ROUNDUP(sizeof(hdr_t)))))
    {
        /* New split block */
        bhdr_t *sb = GET_NEXT_BHDR(b, size);
        sb->header.prev = b;

        /* Set size, this free and previous busy */
        SET_SIZE_AND_FLAGS(sb, GET_SIZE(b) - size - ROUNDUP(sizeof(hdr_t)), THIS_FREE | PREV_BUSY);

        /* The next header points to free block now */
        next->header.prev = sb;

        /* previous block (sb) is free */
        SET_FREE_PREV_BLOCK(next);

        /* Allocated block size truncated */
        SET_SIZE(b, size);

        /* Free block is inserted to free list */
        INSERT_FREE_BLOCK(tlsf, sb);
    }
    else
    {
        /* The block was of right size. Set it just busy in next pointer */
        SET_BUSY_PREV_BLOCK(next);
    }

    /* The allocated block is busy */
    SET_BUSY_BLOCK(b);

    /* Clear the pointers just in case */
    b->free_node.next = NULL;
    b->free_node.prev = NULL;

    /* Update counters */
    tlsf->free_size -= GET_SIZE(b);

    /* And return memory */
    return &b->mem[0];
}

static inline __attribute__((always_inline)) void MERGE(bhdr_t *b1, bhdr_t *b2)
{
    /* Merging adjusts the size - it's sum of both sizes plus size of block header */
    SET_SIZE(b1, GET_SIZE(b1) + GET_SIZE(b2) + ROUNDUP(sizeof(hdr_t)));
}

static inline __attribute__((always_inline)) bhdr_t * MERGE_PREV(tlsf_t *tlsf, bhdr_t *block)
{
    /* Is previous block free? */
    if (FREE_PREV_BLOCK(block))
    {
        int fl, sl;
        bhdr_t *prev = block->header.prev;

        /* Calculate index for removal */
        MAPPING_INSERT(GET_SIZE(prev), &fl, &sl);

        /* Do remove the header from the list */
        REMOVE_HEADER(tlsf, prev, fl, sl);

        /* Merge */
        MERGE(prev, block);

        return prev;
    }
    else
        return block;
}

static inline __attribute__((always_inline)) bhdr_t * MERGE_NEXT(tlsf_t *tlsf, bhdr_t *block)
{
    bhdr_t *next = GET_NEXT_BHDR(block, GET_SIZE(block));

    /* Is next block free? */
    if (FREE_BLOCK(next))
    {
        int fl, sl;

        /* Calculate index for removal */
        MAPPING_INSERT(GET_SIZE(next), &fl, &sl);

        /* Remove the header from the list */
        REMOVE_HEADER(tlsf, next, fl, sl);

        /* merge blocks */
        MERGE(block, next);
    }

    return block;
}

void * tlsf_malloc_aligned(void *t, uintptr_t size, uintptr_t align)
{
    tlsf_t * tlsf = t;
    void * ptr;
    bhdr_t *b;

    size = ROUNDUP(size);

    /* Adjust align to the top nearest power of two */
    align = 1 << MS(align);

    ptr = tlsf_malloc(tlsf, size + align);

    if (!ptr)
    {
        return NULL;
    }

    b = MEM_TO_BHDR(ptr);

    if (align > SIZE_ALIGN)
    {
        void *aligned_ptr = (void *)(((uintptr_t)ptr + align - 1) & ~(align - 1));
        bhdr_t *aligned_bhdr = MEM_TO_BHDR(aligned_ptr);
        uintptr_t diff_begin = (uintptr_t)aligned_bhdr - (uintptr_t)b;
        uintptr_t diff_end = (uintptr_t)GET_NEXT_BHDR(b, GET_SIZE(b)) - (uintptr_t)GET_NEXT_BHDR(aligned_bhdr, size);

        SET_SIZE(aligned_bhdr, size);

        if (aligned_ptr != ptr)
        {
            if (diff_begin > 0)
            {
                SET_SIZE(b, diff_begin - ROUNDUP(sizeof(hdr_t)));

                tlsf->free_size += GET_SIZE(b);

                aligned_bhdr->header.prev = b;
                SET_FREE_PREV_BLOCK(aligned_bhdr);
                SET_FREE_BLOCK(b);

                b = MERGE_PREV(tlsf, b);

                /* Insert free block into the proper list */
                INSERT_FREE_BLOCK(tlsf, b);
            }

            ptr = &aligned_bhdr->mem[0];
        }

        if (diff_end > 0)
        {
            bhdr_t *b1 = GET_NEXT_BHDR(aligned_bhdr, GET_SIZE(aligned_bhdr));
            bhdr_t *next;

            b1->header.prev = aligned_bhdr;

            SET_SIZE(b1, diff_end - ROUNDUP(sizeof(hdr_t)));
            SET_BUSY_PREV_BLOCK(b1);
            SET_FREE_BLOCK(b1);

            next = GET_NEXT_BHDR(b1, GET_SIZE(b1));
            next->header.prev = b1;
            SET_FREE_PREV_BLOCK(next);

            b1 = MERGE_NEXT(tlsf, b1);

            INSERT_FREE_BLOCK(tlsf, b1);
        }
    }

    return ptr;
}

void tlsf_free(void *t, void *ptr)
{
    tlsf_t *tlsf = t;
    bhdr_t *fb;
    bhdr_t *next;

    if (unlikely(!ptr))
        return;

    fb = MEM_TO_BHDR(ptr);

    /* Mark block as free */
    SET_FREE_BLOCK(fb);

    /* adjust free size field on tlsf */
    tlsf->free_size += GET_SIZE(fb);

    /* Try to merge with previous and next blocks (if free) */
    fb = MERGE_PREV(tlsf, fb);
    fb = MERGE_NEXT(tlsf, fb);

    /* Tell next block that previous one is free. Also update the prev link in case it changed */
    next = GET_NEXT_BHDR(fb, GET_SIZE(fb));
    SET_FREE_PREV_BLOCK(next);
    next->header.prev = fb;

    /* Insert free block into the proper list */
    INSERT_FREE_BLOCK(tlsf, fb);
}

void *tlsf_realloc(void *t, void *ptr, uintptr_t new_size)
{
    tlsf_t *tlsf = t;
    bhdr_t *b;
    bhdr_t *bnext;
    int fl;
    int sl;

    /* NULL pointer? just allocate the memory */
    if (unlikely(!ptr))
        return tlsf_malloc(tlsf, new_size);

    /* size = 0? free memory */
    if (unlikely(!new_size))
    {
        tlsf_free(tlsf, ptr);
        return NULL;
    }

    new_size = ROUNDUP(new_size);

    b = MEM_TO_BHDR(ptr);

    if (unlikely(new_size == GET_SIZE(b)))
        return ptr;

    bnext = GET_NEXT_BHDR(b, GET_SIZE(b));

    /* Is new size smaller than the previous one? Try to split the block if this is the case */
    if (new_size <= (GET_SIZE(b)))
    {
        /* New header starts right after the current block b */
        bhdr_t * b1 = GET_NEXT_BHDR(b, new_size);

        /* Update pointer and size */
        b1->header.prev = b;
        SET_SIZE_AND_FLAGS(b1, GET_SIZE(b) - new_size - ROUNDUP(sizeof(hdr_t)), THIS_FREE | PREV_BUSY);

        /* Current block gets smaller */
        SET_SIZE(b, new_size);

        tlsf->free_size += GET_SIZE(b1);

        /* Try to merge with next block */
        b1 = MERGE_NEXT(tlsf, b1);

        /* Tell next block that previous one is free. Also update the prev link in case it changed */
        bnext = GET_NEXT_BHDR(b1, GET_SIZE(b1));
        SET_FREE_PREV_BLOCK(bnext);
        bnext->header.prev = b1;

        /* Insert free block into the proper list */
        INSERT_FREE_BLOCK(tlsf, b1);
    }
    else
    {
        /* Is next block free? Is there enough free space? */
        if (FREE_BLOCK(bnext) && new_size <= GET_SIZE(b) + GET_SIZE(bnext) + ROUNDUP(sizeof(hdr_t)))
        {
            bhdr_t *b1;
            uintptr_t rest_size = ROUNDUP(sizeof(hdr_t)) + GET_SIZE(bnext) + GET_SIZE(b) - new_size;

            MAPPING_INSERT(GET_SIZE(bnext), &fl, &sl);

            REMOVE_HEADER(tlsf, bnext, fl, sl);

            if (rest_size > ROUNDUP(sizeof(hdr_t)))
            {
                rest_size -= ROUNDUP(sizeof(hdr_t));

                SET_SIZE(b, new_size);

                b1 = GET_NEXT_BHDR(b, GET_SIZE(b));
                b1->header.prev = b;

                SET_SIZE_AND_FLAGS(b1, rest_size, THIS_FREE | PREV_BUSY);

                bnext = GET_NEXT_BHDR(b1, GET_SIZE(b1));
                bnext->header.prev = b1;
                SET_FREE_PREV_BLOCK(bnext);

                INSERT_FREE_BLOCK(tlsf, b1);
            }
            else
            {
                if (rest_size)
                    SET_SIZE(b, new_size + ROUNDUP(sizeof(hdr_t)));
                else
                    SET_SIZE(b, new_size);

                bnext = GET_NEXT_BHDR(b, GET_SIZE(b));
                bnext->header.prev = b;
                SET_BUSY_PREV_BLOCK(bnext);
            }
        }
        else
        {
            /* Next block was not free. Create new buffer and copy old contents there */
            void * p = tlsf_malloc(tlsf, new_size);
            if (p)
            {
                memcpy(p, ptr, GET_SIZE(b));
                tlsf_free(tlsf, ptr);
                b = MEM_TO_BHDR(p);
            }
        }
    }

    return b->mem;
}

/* Allocation of headers in memory:
 * hdr
 *  header      (ROUNDUP(sizeof(hdr_t))
 *  mem         (ROUNDUP(sizeof(tlst_area_t))
 * b
 *  header      (ROUNDUP(sizeof(hdr_t))
 *  free space  (size - HEADERS_SIZE)
 * bend
 *  header      (ROUNDUP(sizeof(hdr_t))
 */
tlsf_area_t * init_memory_area(void * memory, uintptr_t size)
{
    bhdr_t * hdr = (bhdr_t *)memory;
    bhdr_t * b;
    bhdr_t * bend;

    tlsf_area_t * area;

    size = ROUNDDOWN(size);

    /* Prepare first header, which protects the tlst_area_t header */
    hdr->header.length = ROUNDUP(sizeof(tlsf_area_t)) | THIS_BUSY | PREV_BUSY;
    hdr->header.prev = NULL;

    b = GET_NEXT_BHDR(hdr, ROUNDUP(sizeof(tlsf_area_t)));
    b->header.prev = hdr;
    b->header.length = (size - HEADERS_SIZE) | PREV_BUSY | THIS_BUSY;

    bend = GET_NEXT_BHDR(b, GET_SIZE(b));
    bend->header.length = 0 | THIS_BUSY | PREV_BUSY;
    bend->header.prev = b;

    area = (tlsf_area_t *)hdr->mem;
    area->end = bend;

    return area;
}

void tlsf_add_memory(void *t, void *memory, uintptr_t size)
{
    tlsf_t *tlsf = t;

    if (memory && size > HEADERS_SIZE)
    {
        tlsf_area_t *area = init_memory_area(memory, size);
        bhdr_t *b;

        area->next = tlsf->memory_area;
        tlsf->memory_area = area;

        b = MEM_TO_BHDR(area);
        b = GET_NEXT_BHDR(b, GET_SIZE(b));

        tlsf->total_size += size;

        /* Add the initialized memory */
        tlsf_free(tlsf, b->mem);
    }
}

static tlsf_t __tlsf;

void * tlsf_init()
{
    tlsf_t *tlsf = &__tlsf;

    if (tlsf)
    {
        bzero(tlsf, sizeof(tlsf_t));
    }

    return tlsf;
}
