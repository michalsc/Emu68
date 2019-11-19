#include <stdint.h>

extern void SmallPT();
extern uint16_t *framebuffer;
extern uint32_t pitch;
void do_global_ctors(void);

void init_screen(uint16_t *fb, uint32_t p, uint32_t w, uint32_t h);
void put_char(char);

void _start(uint32_t p asm("d0"), uint16_t *fb asm("a0"), uint32_t w asm("d1"), uint32_t h asm("d2"))
{
    init_screen(fb, p, w, h);
    do_global_ctors();
    SmallPT();
}
