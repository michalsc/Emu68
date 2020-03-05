#ifndef __SUPPORT_H
#define __SUPPORT_H

#include <stdarg.h>
#include <stdint.h>

static inline double sqrt(double a)
{
    double ret;

    asm volatile("fsqrt.x %1, %0":"=f"(ret):"f"(a));

    return ret;
}

static inline double fabs(double a)
{
    double ret;

    asm volatile("fabs.x %1, %0":"=f"(ret):"f"(a):"cc");

    return ret;
}

static inline double cos(double a)
{
    double ret;

    asm volatile("fcos.x %1, %0":"=f"(ret):"f"(a):"cc");

    return ret;
}

static inline double sin(double a)
{
    double ret;

    asm volatile("fsin.x %1, %0":"=f"(ret):"f"(a):"cc");

    return ret;
}

#define M_PI 3.14159265358979323846
#define M_1_PI 0.31830988618379067154

static inline __attribute__((always_inline)) uint64_t BE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[0]) << 56) | ((uint64_t)(tmp.u[1]) << 48) | ((uint64_t)(tmp.u[2]) << 40) | ((uint64_t)(tmp.u[3]) << 32) |
        (tmp.u[4] << 24) | (tmp.u[5] << 16) | (tmp.u[6] << 8) | (tmp.u[7]);
}

static inline __attribute__((always_inline)) uint64_t LE64(uint64_t x)
{
    union {
        uint64_t v;
        uint8_t u[8];
    } tmp;

    tmp.v = x;

    return ((uint64_t)(tmp.u[7]) << 56) | ((uint64_t)(tmp.u[6]) << 48) | ((uint64_t)(tmp.u[5]) << 40) | ((uint64_t)(tmp.u[4]) << 32) |
        (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline __attribute__((always_inline)) uint32_t BE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 24) | (tmp.u[1] << 16) | (tmp.u[2] << 8) | (tmp.u[3]);
}

static inline __attribute__((always_inline)) uint32_t LE32(uint32_t x)
{
    union {
        uint32_t v;
        uint8_t u[4];
    } tmp;

    tmp.v = x;

    return (tmp.u[3] << 24) | (tmp.u[2] << 16) | (tmp.u[1] << 8) | (tmp.u[0]);
}

static inline __attribute__((always_inline)) uint16_t BE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[0] << 8) | (tmp.u[1]);
}

static inline __attribute__((always_inline)) uint16_t LE16(uint16_t x)
{
    union {
        uint16_t v;
        uint8_t u[2];
    } tmp;

    tmp.v = x;

    return (tmp.u[1] << 8) | (tmp.u[0]);
}

extern "C" {

double copysign (double x, double y);
double scalbn (double x, int n);
double pow(double x, double y);
double my_erand48(unsigned short *Xi);
void do_global_ctors(void);
void do_global_dtors(void);
void *memcpy(void *d, const void *s, long unsigned int l);
void *memset(void *d, int c, long unsigned int l);
void vkprintf(const char * format, va_list args);
int strcmp(const char *s1, const char *s2);
char * strcpy(char *s1, const char *s2);
void kprintf(const char * format, ...);
void init_screen(uint16_t *fb, uint32_t p, uint32_t w, uint32_t h);
uint16_t *get_fb();
uint32_t get_pitch();
uint32_t get_width();
uint32_t get_height();
void silence(int s);

}

#endif /* __SUPPORT_H */
