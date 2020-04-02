#include <stdint.h>
#if 0
asm("   .text\n"
"       .globl _start\n"
"_start: move.l %d2,-(%a7)\n"
"       move.l %d1,-(%a7)\n"
"       move.l %a0,-(%a7)\n"
"       move.l %d0,-(%a7)\n"
"       lea _c_start,%a5\n"
"       jsr (%a5)\n"
"       lea 16(sp),sp\n"
"       rts"
);
#endif
extern void Buddha();
extern uint16_t *framebuffer;
extern uint32_t pitch;

void init_screen(uint16_t *fb, uint32_t p, uint32_t w, uint32_t h);
void put_char(char);
void silence(int);

void _c_start(uint32_t p asm("d0"), uint16_t *fb asm("a0"), uint32_t w asm("d1"), uint32_t h asm("d2"))
//void c_start(uint32_t p, uint16_t *fb, uint32_t w, uint32_t h)
{
    silence(0);
    init_screen(fb, p, w, h);
    Buddha();
}
