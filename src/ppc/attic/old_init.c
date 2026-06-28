


asm(
"delay_loop:    \n"
"    mtctr   %r3 \n"     /* number of iterations in r3 */
"1:  bdnz+    1b \n"
"    blr");

void delay_loop(uint32_t count);

double GetBogoMIPS(uint32_t count)
{
    uint32_t Begin_Time, End_Time;

    *(uint32_t *)0 = 0xdead;

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    delay_loop(count);
    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));

    kprintf("%d loop cycles in %d us -> %d BogoMIPS\n", count, End_Time - Begin_Time, count / (End_Time - Begin_Time));
    
    return (double)count / (double)(End_Time - Begin_Time);
}

void PPC_C_Init(uint16_t *framebuffer, uint32_t fb_width, uint32_t fb_height, uint32_t pitch)
{
    uint32_t Begin_Time, End_Time;
    uint32_t Begin_Cycles, End_Cycles;

    kprintf("Hello, PPC\n");
    kprintf("Here is Emu68, %s, speaking ;)\n", "or maybe EmuPPC");
    kprintf("Testing literals:\n");
    kprintf("  %%s: %s\n", "this is a text");
    kprintf("  %%c: %c\n", 'A');
    kprintf("  %%d: %d\n", 1536);
    kprintf("  %%x: %x\n", 0xdeadbeef);

    asm volatile("mtdec %0"::"r"(5000000));

    GetBogoMIPS(1000000);

    uint32_t start, end;

    asm volatile("mfspr %0, 900":"=r"(start));

    asm volatile("lwbrx %0, 0, %1":"=r"(Begin_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(Begin_Cycles));

    const uint32_t w = 400;
    const uint32_t h = 300;
    const uint32_t start_x = (fb_width - w) / 2;
    const uint32_t start_y = (fb_height - h) / 2;
    (void)pitch;
    uint32_t c = 0;
    
    for (int i=0; i < 10000; i++)
    {
        uint16_t *ptr = framebuffer + start_y * fb_width + start_x;

        for (unsigned y = 0; y < h; y++) {
            for (unsigned x=0; x < w; x++) {
                ptr[x] = c++;
            }
            ptr += fb_width;
        }
    }
    asm volatile("icbi 0, %r0");
    kprintf("%d\n", c);

    asm volatile("lwbrx %0, 0, %1":"=r"(End_Time):"r"(0xf2003004));
    asm volatile("mftbl %0":"=r"(End_Cycles));

    asm volatile("mfspr %0, 900":"=r"(end));

    kprintf("Test loop time: %d us\n", End_Time - Begin_Time);
    kprintf("Test loop cycles: %d\n", End_Cycles - Begin_Cycles);
    kprintf("Test loop instructions: %u\n", end - start);
    uint32_t speed = (((end - start) / ((End_Time - Begin_Time) / 1000)) ) / 100;
    kprintf("Test loop speed: %u.%u MIPS\n", speed / 10, speed % 10);

    

    uint32_t dec;
    asm volatile("mfdec %0":"=r"(dec));
    kprintf("Decrementer: %d\n", dec);
    GetBogoMIPS(100000000);
    asm volatile("mfdec %0":"=r"(dec));
    kprintf("Decrementer: %d\n", dec);
    GetBogoMIPS(100000000);
    asm volatile("mfdec %0":"=r"(dec));
    kprintf("Decrementer: %d\n", dec);

    register double d asm("fr4") = GetBogoMIPS(100000000);

    asm volatile("mtctr %0; bctr"::"r"(0),"f"(d));
}
