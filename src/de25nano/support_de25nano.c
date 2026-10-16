#include <stdarg.h>
#include <stdint.h>

#include "support.h"

uintptr_t UART_BASE = 0xffffffa010C02100UL;
volatile unsigned char print_lock = 0;

void putByte(void *, char c) {
    volatile uint32_t *u = (volatile uint32_t *)UART_BASE;
    while (!(u[0x14/4] & LE32(1u << 5))) ;
    u[0] = LE32((uint8_t)c);
}

void kprintf(const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);

    while(__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("yield");
    vkprintf_pc(putByte, (void*)0, format, v);
    __atomic_clear(&print_lock, __ATOMIC_RELEASE);

    va_end(v);
}


void __printf_chk(int, const char *restrict format, ...)
{
    va_list v;
    va_start(v, format);

    while (__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE))
        __asm__ volatile("yield");

    vkprintf_pc(putByte, (void*)0, format, v);

    __atomic_clear(&print_lock, __ATOMIC_RELEASE);

    va_end(v);
}

void __fprintf_chk(void *, int, const char *restrict format, ...)
{
    (void)format;

    va_list v;
    va_start(v, format);

    while (__atomic_test_and_set(&print_lock, __ATOMIC_ACQUIRE))
        __asm__ volatile("yield");

    vkprintf_pc(putByte, (void*)0, format, v);

    __atomic_clear(&print_lock, __ATOMIC_RELEASE);

    va_end(v);
}

int pitch;
int fb_width;
int fb_height;
void *framebuffer;

bool is_serial_up() { return true; }