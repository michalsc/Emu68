#ifndef _TLSF_H
#define _TLSF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TLSF_MULTITHREADING 1

void* tlsf_init();
void tlsf_set_flags(void *handle, uint32_t flags);
void tlsf_add_memory(void* t, void* memory, uintptr_t size);

void* tlsf_malloc(void* handle, uintptr_t size);
void* tlsf_malloc_aligned(void* handle, uintptr_t size, uintptr_t align);
void tlsf_free(void* handle, void* ptr);
void* tlsf_realloc(void* handle, void* ptr, uintptr_t new_size);
void* tlsf_init_with_memory(void* memory, uintptr_t size);

uintptr_t tlsf_get_total_size(void* memory);
uintptr_t tlsf_get_free_size(void* memory);

#ifdef __cplusplus
}

namespace Emu68 {

class TLSF {
public:
    TLSF(void* memory, uintptr_t size) { handle = tlsf_init_with_memory(memory, size); }
    TLSF(void* handle) : handle(handle) {}

    void* malloc(uintptr_t size) { return tlsf_malloc(handle, size); }
    void* malloc_aligned(uintptr_t size, uintptr_t align) { return tlsf_malloc_aligned(handle, size, align); }
    void* realloc(void* ptr, uintptr_t new_size) { return tlsf_realloc(handle, ptr, new_size); }
    void free(void* ptr) { tlsf_free(handle, ptr); }
    uintptr_t total_size() { return tlsf_get_total_size(handle); }
    uintptr_t free_size() { return tlsf_get_free_size(handle); }

private:
    void *handle;
};

} // namespace Emu68

#endif

#endif /* _TLSF_H */
