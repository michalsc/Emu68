#include <stdint.h>

extern void Buddha();
extern uint16_t *framebuffer;
extern uint32_t pitch;

void init_screen(uint16_t *fb, uint32_t p, uint32_t w, uint32_t h);
void put_char(char);
void silence(int);

void _start(uint16_t *fb, uint32_t w, uint32_t h, uint32_t p)
{
    silence(0);
    init_screen(fb, p, w, h);
    Buddha();
}
