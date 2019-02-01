#ifndef _TLSF_H
#define _TLSF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *tlsf_init();
void tlsf_add_memory(void *t, void *memory, uintptr_t size);

void *tlsf_malloc(void *handle, uintptr_t size);
void *tlsf_malloc_aligned(void *handle, uintptr_t size, uintptr_t align);
void tlsf_free(void *handle, void *ptr);
void *tlsf_realloc(void *handle, void *ptr, uintptr_t new_size);

uintptr_t tlsf_avail(void *tlsf, uint32_t requirements);

#ifdef __cplusplus
}
#endif

#endif /* _TLSF_H */
