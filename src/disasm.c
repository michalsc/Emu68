#include <capstone/capstone.h>
#include <tlsf.h>
#include <support.h>
#include <stdarg.h>

extern void *tlsf;

void *my_malloc(size_t size)
{
    return tlsf_malloc(tlsf, size);
}

void *my_calloc(size_t nmemb, size_t size)
{
    void *ptr = tlsf_malloc(tlsf, size * nmemb);
    bzero(ptr, size * nmemb);
    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    return tlsf_realloc(tlsf, ptr, size);
}

void my_free(void *ptr)
{
    tlsf_free(tlsf, ptr);
}

struct putc_data {
    char *buffer;
    size_t written;
    size_t limit;
};

void my_putc(void *data, const char c)
{
    struct putc_data *p = data;

    if (p->written < p->limit)
    {
        p->buffer[p->written] = c;
        p->buffer[p->written+1] = 0;
    }

    p->written++;
}

int my_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    struct putc_data p;
    p.buffer = str;
    p.limit = size;
    p.written = 0;

    vkprintf_pc(my_putc, &p, format, ap);

    return p.written;
}

void disasm_init()
{
    cs_opt_mem setup;

    setup.malloc = my_malloc;
    setup.calloc = my_calloc;
    setup.realloc = my_realloc;
    setup.free = my_free;
    setup.vsnprintf = my_vsnprintf;

    if (!cs_option(0, CS_OPT_MEM, (uintptr_t)&setup)) {
        kprintf("[BOOT] Disassembler set up\n");
    } else {
        kprintf("[BOOT] Disassembler init error\n");
    }
}

csh h_m68k;
csh h_arm;

void disasm_open()
{
    cs_err err;
    err = cs_open(CS_ARCH_M68K, CS_MODE_M68K_040, &h_m68k);
#ifdef __aarch64__
    err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &h_arm);
#else
    err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, h_arm);
#endif
}

void disasm_close()
{
    cs_close(&h_m68k);
    cs_close(&h_arm);
}

void disasm_print(uint16_t *m68k_addr, size_t m68k_size, uint32_t *arm_addr, size_t arm_size, uint32_t *arm_start)
{
    cs_insn *insn_m68k;
    cs_insn *insn_arm;
    size_t count_m68k = 0;
    size_t count_arm = 0;
    size_t max_count = 0;

    if (m68k_addr)
        count_m68k = cs_disasm(h_m68k, (const uint8_t *)m68k_addr, m68k_size, (uintptr_t)m68k_addr, 1, &insn_m68k);
    if (arm_addr)
        count_arm = cs_disasm(h_arm, (const uint8_t *)arm_addr, arm_size, (uintptr_t)arm_addr - (uintptr_t)arm_start, 0, &insn_arm);

    max_count = (count_arm > count_m68k) ? count_arm : count_m68k;

    for (size_t i=0; i < max_count; i++)
    {
        if (i < count_m68k)
        {
            kprintf("[JIT] %08x: %7s %21s", insn_m68k[i].address, insn_m68k[i].mnemonic, insn_m68k[i].op_str);
        }
        else
            kprintf("[JIT]                                        ");

        if (i < count_arm)
        {
            kprintf("-> %08x: %7s %s\n", insn_arm[i].address, insn_arm[i].mnemonic, insn_arm[i].op_str);
        }
        else
            kprintf("\n");
    }

    if (count_m68k)
        cs_free(insn_m68k, count_m68k);
    if (count_arm)
        cs_free(insn_arm, count_arm);
}