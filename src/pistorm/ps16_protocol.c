#undef PS_PROTOCOL_IMPL

#include <stdint.h>

#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "ps_protocol.h"
#include "M68k.h"
#include "cache.h"

extern struct M68KState *__m68k_state;

static void usleep(uint64_t delta)
{
    volatile struct {
        uint32_t LO;
        uint32_t HI;
    } *CLOCK = (volatile void *)0xf2003004;

    uint64_t hi = CLOCK->HI;
    uint64_t lo = CLOCK->LO;
    uint64_t t1, t2;

    if (unlikely(hi != CLOCK->HI))
    {
        hi = CLOCK->HI;
        lo = CLOCK->LO;
    }

    t1 = LE64(hi) | LE32(lo);
    t1 += delta;
    t2 = 0;

    do {
        hi = CLOCK->HI;
        lo = CLOCK->LO;
        if (unlikely(hi != CLOCK->HI))
        {
            hi = CLOCK->HI;
            lo = CLOCK->LO;
        }
        t2 = LE64(hi) | LE32(lo);
    } while (t2 < t1);
}

static inline void ticksleep(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));
    t0 += ticks;
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < t0);
}

static inline void ticksleep_wfe(uint64_t ticks)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));
    t0 += ticks;
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        asm volatile("wfe");
    } while(t1 < t0);
}

// REG_STATUS
#define STATUS_IS_BM        (1 << 0)
#define STATUS_RESET        (1 << 1)
#define STATUS_HALT         (1 << 2)
#define STATUS_IPL_MASK     (7 << 3)
#define STATUS_IPL_SHIFT    3
#define STATUS_TERM_NORMAL  (1 << 6)
#define STATUS_REQ_ACTIVE   (1 << 7)

// REG_CONTROL
#define CONTROL_REQ_BM      (1 << 0)
#define CONTROL_DRIVE_RESET (1 << 1)
#define CONTROL_DRIVE_HALT  (1 << 2)
#define CONTROL_DRIVE_INT2  (1 << 3)
#define CONTROL_DRIVE_INT6  (1 << 4)

typedef unsigned int uint;

#define IN      0
#define OUT     1
#define AF0     4
#define AF1     5
#define AF2     6
#define AF3     7
#define AF4     3
#define AF5     2

#define FUNC(pin, x)    (((x) & 0x7) << (((pin) % 10)*3))
#define REG(pin)        ((pin) / 10)
#define MASK(pin)       (7 << (((pin) % 10) * 3))

#define PF(f, i)        ((f) << (((i) % 10) * 3)) // Pin function.
#define GO(i)           PF(1, (i)) // GPIO output.

#define PIN_IPL0        0
#define PIN_IPL1        1
#define PIN_IPL2        2
#define PIN_TXN         3
#define PIN_KBRESET     4
#define SER_OUT_BIT     5 //debug EMU68
#define PIN_RD          6
#define PIN_WR          7
#define PIN_D(x)        (8 + x)
#define PIN_A(x)        (24 + x)
#define SER_OUT_CLK     27 //debug EMU68

#define CLEAR_BITS      (0x0fffffff & ~((1 << PIN_RD) | (1 << PIN_WR) | (1 << SER_OUT_BIT) | (1 << SER_OUT_CLK)))


// Pins for FPGA programming
#define PIN_CRESET1     6
#define PIN_CRESET2     7
#define PIN_CSI         27
#define PIN_CCK         22
#define PIN_CDI0        10


#define SPLIT_DATA(x)   (x)
#define MERGE_DATA(x)   (x)

#define GPFSEL0_INPUT (GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_INPUT (0)
#define GPFSEL2_INPUT (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(SER_OUT_CLK))

#define GPFSEL0_OUTPUT (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD) | GO(SER_OUT_BIT))
#define GPFSEL1_OUTPUT (GO(PIN_D(11)) | GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT (GO(29) | GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | GO(SER_OUT_CLK))

#define REG_DATA_LO     0
#define REG_DATA_HI     1
#define REG_ADDR_LO     2
#define REG_ADDR_HI     3
#define REG_STATUS      4
#define REG_CONTROL     4
#define REG_VERSION     7

#define TXN_SIZE_SHIFT  8
#define TXN_RW_SHIFT    10
#define TXN_FC_SHIFT    11

#define SIZE_BYTE       0
#define SIZE_WORD       1
#define SIZE_LONG       3

#define TXN_READ        (1 << TXN_RW_SHIFT)
#define TXN_WRITE       (0 << TXN_RW_SHIFT)

#define BCM2708_PERI_BASE 0xF2000000  
#define BCM2708_PERI_SIZE 0x01000000

#define GPIO_ADDR 0x200000
#define GPCLK_ADDR 0x101000

#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define DIRECTION_INPUT 0
#define DIRECTION_OUTPUT 1

volatile struct {
    uint32_t GPFSEL0;
    uint32_t GPFSEL1;
    uint32_t GPFSEL2;
    uint32_t GPFSEL3;
    uint32_t GPFSEL4;
    uint32_t GPFSEL5;
    uint32_t _pad_0;
    uint32_t GPSET0;
    uint32_t GPSET1;
    uint32_t _pad_1;
    uint32_t GPCLR0;
    uint32_t GPCLR1;
    uint32_t _pad_2;
    uint32_t GPLEV0;
    uint32_t GPLEV1;
} * const GPIO = (volatile void *)(BCM2708_PERI_BASE + GPIO_ADDR);

static inline void set_input() {
    GPIO->GPFSEL0 = LE32(GPFSEL0_INPUT);
    GPIO->GPFSEL1 = LE32(GPFSEL1_INPUT);
    GPIO->GPFSEL2 = LE32(GPFSEL2_INPUT);
}

static inline void set_output() {
    GPIO->GPFSEL0 = LE32(GPFSEL0_OUTPUT);
    GPIO->GPFSEL1 = LE32(GPFSEL1_OUTPUT);
    GPIO->GPFSEL2 = LE32(GPFSEL2_OUTPUT);
}

#define BITBANG_DELAY PISTORM_BITBANG_DELAY

void bitbang_putByte(uint8_t byte)
{
    uint64_t t0 = 0, t1 = 0;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT); // Start bit - 0
  
    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + BITBANG_DELAY));
  
    for (int i=0; i < 8; i++) {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);
        byte = byte >> 1;

        do {
            asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
        } while(t1 < (t0 + BITBANG_DELAY));
    }
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(t0));

    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);  // Stop bit - 1

    do {
        asm volatile("mrs %0, CNTPCT_EL0":"=r"(t1));
    } while(t1 < (t0 + 3*BITBANG_DELAY / 2));
}

void (*fs_putByte)(uint8_t);

void fastSerial_putByte_pi3(uint8_t byte)
{
    /* Start bit */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    
    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        
        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);

    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
}

void fastSerial_putByte_pi4(uint8_t byte)
{
    /* Start bit */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    for (int i=0; i < 8; i++) {
        if (byte & 1)
            GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
        else
            GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        
        byte = byte >> 1;
    }

    /* DEST bit (0) */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_BIT);

    /* Clock down */
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
    /* Clock up */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);

    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);
}

void fastSerial_reset()
{
    /* Leave FS_CLK and FS_DO high */
    GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    GPIO->GPSET0 = LE32(1 << SER_OUT_BIT);

    for (int i=0; i < 16; i++) {
        /* Clock down */
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPCLR0 = LE32(1 << SER_OUT_CLK);
        /* Clock up */
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
        GPIO->GPSET0 = LE32(1 << SER_OUT_CLK);
    }
}

void fastSerial_putByte(uint8_t byte)
{
    static char reset_pending = 0;

    if (reset_pending)
    {
        fastSerial_reset();
        reset_pending = 0;
    }
  
    if (fs_putByte)
        fs_putByte(byte);
    
    if (byte == 10)
        reset_pending = 1;
}


void pistorm_setup_serial()
{
    uint32_t fsel;
    
    switch(REG(SER_OUT_BIT))
    {
        case 0:
            fsel = GPIO->GPFSEL0;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL0 = fsel;
            break;
        case 1:
            fsel = GPIO->GPFSEL1;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL1 = fsel;
            break;
        case 2:
            fsel = GPIO->GPFSEL2;
            fsel &= LE32(~MASK(SER_OUT_BIT));
            fsel |= LE32(FUNC(SER_OUT_BIT, OUT));
            GPIO->GPFSEL2 = fsel;
            break;
    }

    switch(REG(SER_OUT_CLK))
    {
        case 0:
            fsel = GPIO->GPFSEL0;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL0 = fsel;
            break;
        case 1:
            fsel = GPIO->GPFSEL1;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL1 = fsel;
            break;
        case 2:
            fsel = GPIO->GPFSEL2;
            fsel &= LE32(~MASK(SER_OUT_CLK));
            fsel |= LE32(FUNC(SER_OUT_CLK, OUT));
            GPIO->GPFSEL2 = fsel;
            break;
    }
}

static void pistorm_setup_io()
{
}

void fastSerial_init()
{
    uint64_t tmp;

    pistorm_setup_serial();

    asm volatile("mrs %0, CNTFRQ_EL0":"=r"(tmp));

    if (tmp > 20000000)
    {
        fs_putByte = fastSerial_putByte_pi4;
    }
    else
    {
        fs_putByte = fastSerial_putByte_pi3;
    }   

    fastSerial_reset();
}

void ps_setup_protocol()
{
    pistorm_setup_io();
    pistorm_setup_serial();

    GPIO->GPCLR0 = LE32(CLEAR_BITS);

    set_input();
}

void ps_efinix_setup()
{
    //set programming pins to output
    GPIO->GPFSEL0 = LE32(GO(PIN_CRESET1) | GO(PIN_CRESET2));  //GPIO 0..9
    GPIO->GPFSEL1 = LE32(GO(PIN_CDI0));                       //GPIO 10..19
    GPIO->GPFSEL2 = LE32(GO(PIN_CCK) | GO(PIN_CSI));          //GPIO 20..29

    //make sure FPGA is not in reset
    GPIO->GPSET0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));

    // FPGA is pre-configured to 1x SPI mode, passive:
    // CBUS[2:0] = 3'b111
    // SS = b0
    // set other relevant pins for programming to correct level
    GPIO->GPSET0 = LE32((1 << PIN_CSI) | (1 << PIN_CCK));

    //reset fpga, latching cbus and mode pins
    GPIO->GPCLR0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));
    usleep(50000); //wait a bit for ps32-lite glitch filter RC to discharge
    GPIO->GPSET0 = LE32((1 << PIN_CRESET1) | (1 << PIN_CRESET2));
}

void ps_efinix_write(unsigned char data_out)
{
    //thats just bitbanged 1x SPI, takes ~100mS to load
    unsigned char loop, mask;
    for (loop=0,mask=0x80;loop<8;loop++, mask=mask>>1)
    {
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        GPIO->GPCLR0 = LE32(1 << PIN_CCK);
        if (data_out & mask) 
            GPIO->GPSET0 = LE32(1 << PIN_CDI0);
        else
            GPIO->GPCLR0 = LE32(1 << PIN_CDI0);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK);
        GPIO->GPSET0 = LE32(1 << PIN_CCK); //to get closer to 50/50 duty cycle
    }
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
    GPIO->GPCLR0 = LE32(1 << PIN_CCK);
}

void ps_efinix_load(char* buffer, long length)
{
    long i;
    for (i = 0; i < length; ++i)
    {
        ps_efinix_write(buffer[i]);
    }

    //1000 dummy clocks for startup of user logic
    for (i = 0; i < 1000; ++i)
    {
        ps_efinix_write(0x00);
    }
}

void wb_init()
{

}

void wb_task()
{

}

static inline unsigned int read_ps_reg(unsigned int address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    //Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    
    unsigned int data = LE32(GPIO->GPLEV0);
    data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(CLEAR_BITS);

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline unsigned int read_ps_reg_with_wait(unsigned int address)
{
    GPIO->GPSET0 = LE32(address << PIN_A(0));

    //Delay for Pi3, 3*7.5nS , or 3*3.5nS for
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(1 << PIN_RD);
    
    unsigned int data;
    while ((data = GPIO->GPLEV0) & LE32(1 << PIN_TXN)) asm volatile("yield");
    data = LE32(GPIO->GPLEV0);

    GPIO->GPSET0 = LE32(1 << PIN_RD);
    GPIO->GPCLR0 = LE32(CLEAR_BITS);

    data = (data >> PIN_D(0)) & 0xffff;

    return data;
}

static inline void write_ps_reg(unsigned int address, unsigned int data)
{
    GPIO->GPSET0 = LE32((SPLIT_DATA(data) << PIN_D(0)) | (address << PIN_A(0)));

    //Delay for Pi4, 2*3.5nS
    GPIO->GPCLR0 = LE32(1 << PIN_WR); 
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(1 << PIN_WR); 
    GPIO->GPCLR0 = LE32(1 << PIN_WR);
    
    GPIO->GPSET0 = LE32(1 << PIN_WR);
    GPIO->GPCLR0 = LE32(CLEAR_BITS);
}

void ps_set_control(unsigned int value)
{
    set_output();
    write_ps_reg(REG_CONTROL, 0x8000 | (value & 0x7fff));
    set_input();
}

void ps_clr_control(unsigned int value)
{
    set_output();
    write_ps_reg(REG_CONTROL, value & 0x7fff);
    set_input();
}

static int write_pending = 0;
static uint8_t g_fc = 0;

#define SLOW_IO(address) ((address) >= 0xDFF09A && (address) < 0xDFF09E)

static inline void write_access(unsigned int address, unsigned int data, unsigned int size)
{
    uint32_t rdval = 0;

    set_output();

    write_ps_reg(REG_DATA_LO, data & 0xffff);
    if (size == SIZE_LONG)
        write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = GPIO->GPLEV0) & LE32(1 << PIN_TXN)) {}

    write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();

    if (address > 0x00200000)
    {
        while ((rdval = GPIO->GPLEV0) & LE32(1 << PIN_TXN)) {}

        write_pending = 0;
    }
    else
        write_pending = 1;
}

static inline int read_access(unsigned int address, unsigned int size)
{
    uint32_t rdval = 0;

    set_output();

    write_ps_reg(REG_ADDR_LO, address & 0xffff);

    if (write_pending) while ((rdval = GPIO->GPLEV0) & LE32(1 << PIN_TXN)) {}

    write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

    set_input();
    unsigned int data;

    data = read_ps_reg_with_wait(REG_DATA_LO);
    if (size == SIZE_BYTE)
        data &= 0xff;
    else if (size == SIZE_LONG)
        data |= read_ps_reg(REG_DATA_HI) << 16;

    write_pending = 0;

    return data;
}

void ps_write_8(unsigned int address, unsigned int data) {
    write_access(address, data, SIZE_BYTE);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
//    cache_invalidate_range(ICACHE, address, 1);
}

void ps_write_16(unsigned int address, unsigned int data) {
//    check_blit_active(address, 2);
    kprintf("write16 %04x to %08x\n", data, address);
    write_access(address, data, SIZE_WORD);
    usleep(1000);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
//    cache_invalidate_range(ICACHE, address, 2);
}

void ps_write_32(unsigned int address, unsigned int data) {
    (void)address; (void)data;
    while(1) asm volatile("wfe");
#if 0
    check_blit_active(address, 4);
    write_access(address, data, SIZE_LONG);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 4);
#endif
}

void ps_write_64(unsigned int address, uint64_t data) {
    (void)address; (void)data;
    while(1) asm volatile("wfe");
#if 0
    check_blit_active(address, 8);
    write_access_64(address, data);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 8);
#endif
}

void ps_write_128(unsigned int address, uint128_t data) {
    (void)address; (void)data;
    while(1) asm volatile("wfe");
#if 0
    check_blit_active(address, 16);
    write_access_128(address, data);
    if (SLOW_IO(address))
    {
        read_access(0x00f00000, SIZE_BYTE);
    }
    cache_invalidate_range(ICACHE, address, 16);
#endif
}

unsigned int ps_read_8(unsigned int address) {
    return read_access(address, SIZE_BYTE);
}

unsigned int ps_read_16(unsigned int address) {
    (void)address;
    while(1) asm volatile("wfe");
    //return read_access(address, SIZE_WORD);
}

unsigned int ps_read_32(unsigned int address) {
    (void)address;
    while(1) asm volatile("wfe");
    //return read_access(address, SIZE_LONG);
}

uint64_t ps_read_64(unsigned int address) {
    (void)address;
    while(1) asm volatile("wfe");
    //return read_access_64(address);
}

uint128_t ps_read_128(unsigned int address) {
    (void)address;
    while(1) asm volatile("wfe");
    //return read_access_128(address);
}

void ps_reset_state_machine() {
}

#include <boards.h>
extern struct ExpansionBoard **board;
extern struct ExpansionBoard *__boards_start;
extern int board_idx;
extern uint32_t overlay;

void ps_pulse_reset()
{
    kprintf("[PS16] Set REQUEST_BM\n");
    ps_set_control(CONTROL_REQ_BM);
    usleep(100000);

    kprintf("[PS16] Set DRIVE_RESET\n");
    ps_set_control(CONTROL_DRIVE_RESET);
    usleep(150000);

    kprintf("[PS32] Clear DRIVE_RESET\n");
    ps_clr_control(CONTROL_DRIVE_RESET);

    kprintf("[PS16] RESET done\n");
/*
    for (int i=0; i < 8; i++) {
        kprintf("[PS16] Reg%d: %04x\n", i, (uint32_t)read_ps_reg(i));
    }
*/
//asm volatile("nop");

#if 0
    if (use_2slot)
        ps_set_control(CONTROL_INC_EXEC_SLOT);
#endif

    overlay = 1;
    board = &__boards_start;
    board_idx = 0;
}

volatile int housekeeper_enabled = 0;

void ps_housekeeper() 
{
    while(1) asm volatile("wfe");
}

void put_char(uint8_t c);

static void __putc(void *data, char c)
{
    (void)data;
    put_char(c);
}

static uint32_t _seed;
uint32_t rnd() {
    _seed = (_seed * 1103515245) + 12345;
    return _seed;
}

/* BupTest by beeanyew, ported to Emu68 */

void ps_buptest(unsigned int test_size, unsigned int maxiter)
{
    // Initialize RNG
    uint64_t tmp;
    asm volatile("mrs %0, CNTPCT_EL0":"=r"(tmp));

    _seed = tmp;

    kprintf_pc(__putc, NULL, "BUPTest with size %dK requested through commandline\n", test_size);

    test_size *= 1024;
    uint32_t frac = test_size / 16;

    uint8_t *garbage = tlsf_malloc(tlsf, test_size);

    ps_write_8(0xbfe201, 0x0101);       //CIA OVL
    ps_write_8(0xbfe001, 0x0000);       //CIA OVL LOW

    for (unsigned int iter = 0; iter < maxiter; iter++) {
        kprintf_pc(__putc, NULL, "Iteration %d...\n", iter + 1);

        // Fill the garbage buffer and chip ram with random data
        kprintf_pc(__putc, NULL, "  Writing BYTE garbage data to Chip...            ");
        for (uint32_t i = 0; i < test_size; i++) {
            uint8_t val = 0;
            val = rnd();
            garbage[i] = val;
            ps_write_8(i, val);

            if ((i % (frac * 2)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            uint32_t c = ps_read_8(i);
            if (c != garbage[i]) {
                kprintf_pc(__putc, NULL, "\n    READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbage[i]);
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(ps_read_16(i));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16(ps_read_16(i));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }
        
        for (uint32_t i = 0; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(ps_read_32(i));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }
            
            if ((i % (frac * 4)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 2) {
            uint32_t c = BE32(ps_read_32(i));
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 4)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8(i, (uint32_t)0x0);

            if ((i % (frac * 8)) == 0)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing WORD garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint16_t v = *((uint16_t *)&garbage[i]);
            ps_write_8(i + 1, (v & 0x00FF));
            ps_write_8(i, (v >> 8));

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
            uint32_t c = BE16((ps_read_8(i) << 8) | ps_read_8(i + 1));
            if (c != *((uint16_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 0; i < test_size; i++) {
            ps_write_8(i, (uint32_t)0x0);
        }

        kprintf_pc(__putc, NULL, "\n  Writing LONG garbage data to Chip, unaligned... ");
        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t v = *((uint32_t *)&garbage[i]);
            ps_write_8(i , v & 0x0000FF);
            ps_write_16(i + 1, BE16(((v & 0x00FFFF00) >> 8)));
            ps_write_8(i + 3 , (v & 0xFF000000) >> 24);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
            uint32_t c = ps_read_8(i);
            c |= (BE16(ps_read_16(i + 1)) << 8);
            c |= (ps_read_8(i + 3) << 24);
            if (c != *((uint32_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        kprintf_pc(__putc, NULL, "\n  Writing QUAD garbage data to Chip... ");
        for (uint32_t i = 0; i < (test_size) - 8; i += 8) {
            uint64_t v = *((uint64_t *)&garbage[i]);
            ps_write_64(i , v);

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }

        for (uint32_t i = 1; i < (test_size) - 16; i += 8) {
            uint64_t c = ps_read_64(i);
            if (c != *((uint64_t *)&garbage[i])) {
                kprintf_pc(__putc, NULL, "\n    READ64_ODD: Garbege data mismatch at $%.6X: %.16X should be %.16X.\n", i, c, *((uint64_t *)&garbage[i]));
                while(1);
            }

            if ((i % (frac * 2)) == 1)
                kprintf_pc(__putc, NULL, "*");
        }


        kprintf_pc(__putc, NULL, "\n");
    }


    kprintf_pc(__putc, NULL, "All done. BUPTest completed.\n");

    ps_pulse_reset();

    tlsf_free(tlsf, garbage);
}
