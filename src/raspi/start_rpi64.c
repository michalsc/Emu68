/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdint.h>
#include "A64.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "mmu.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "RegisterAllocator.h"
#include "version.h"

void _start();
void _boot();
void move_kernel(intptr_t from, intptr_t to);
extern uint64_t mmu_user_L1[512];
extern uint64_t mmu_user_L2[4*512];

void M68K_StartEmu(void *addr);

uint16_t *framebuffer;
uint32_t pitch;
uint32_t fb_width;
uint32_t fb_height;

extern const uint32_t topaz8_charloc[];
extern const uint8_t topaz8_chardata[];

uint32_t text_x = 0;
uint32_t text_y = 0;
const int modulo = 192;
int purple = 0;
int black = 0;

void put_char(uint8_t c)
{
    if (framebuffer && pitch)
    {
        uint16_t *pos_in_image = (uint16_t*)((uintptr_t)framebuffer + (text_y * 16 + 5)* pitch);
        pos_in_image += 4 + text_x * 8;

        if (c == 10) {
            text_x = 0;
            text_y++;
        }
        else if (c >= 32) {
            uint32_t loc = (topaz8_charloc[c - 32] >> 16) >> 3;
            const uint8_t *data = &topaz8_chardata[loc];

            for (int y = 0; y < 16; y++) {
                const uint8_t byte = *data;

                for (int x=0; x < 8; x++) {
                    if (byte & (0x80 >> x)) {
                        if (purple) {
                            pos_in_image[x] = LE16(0xed51);
                        }
                        else if (black) {
                            pos_in_image[x] = LE16(0x630c);
                        }
                        else {
                            pos_in_image[x] = 0;
                        }
                    }
                }

                if (y & 1)
                    data += modulo;
                pos_in_image += pitch / 2;
            }
            text_x++;
        }
    }
}

static void __putc(void *data, char c)
{
    (void)data;
    put_char(c);
}

void display_logo()
{
    struct Size sz = get_display_size();
    uint32_t start_x, start_y;
    uint16_t *buff;
    int32_t pix_cnt = (uint32_t)EmuLogo.el_Width * (uint32_t)EmuLogo.el_Height;
    uint8_t *rle = EmuLogo.el_Data;
    int x = 0;
    of_node_t *e = NULL;

    e = dt_find_node("/chosen");
    if (e)
    {
        of_property_t * prop = dt_find_property(e, "bootargs");
        if (prop)
        {
            const char *tok;
            if ((tok = find_token(prop->op_value, "logo=")))
            {
                tok += 5;

                if (strncmp(tok, "purple", 6) == 0)
                    purple = 1;
                else if (strncmp(tok, "black", 5) == 0)
                    black = 1;
            }
        }
    }

    kprintf("[BOOT] Display size is %dx%d\n", sz.width, sz.height);
    fb_width = sz.width;
    fb_height = sz.height;
    init_display(sz, (void**)&framebuffer, &pitch);
    kprintf("[BOOT] Framebuffer @ %08x\n", framebuffer);

    start_x = (sz.width - EmuLogo.el_Width) / 2;
    start_y = (sz.height - EmuLogo.el_Height) / 2;

    kprintf("[BOOT] Logo start coordinate: %dx%d, size: %dx%d\n", start_x, start_y, EmuLogo.el_Width, EmuLogo.el_Height);

    /* Calculate text coordinate for version string */
    text_y = (fb_height - 16 - 5) / 16;
    text_x = (fb_width - strlen(&VERSION_STRING[6]) * 8 - 1) / 8;

    /* First clear the screen. Use color in top left corner of RLE image for that */
    {
        uint8_t gray = rle[0];
        uint16_t color;

        if (purple)
        {
            gray = 240 - gray;
            int r=-330,g=-343,b=-91;
            r += (gray * 848) >> 8;
            g += (gray * 768) >> 8;
            b += (gray * 341) >> 8;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            color = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
        }
        else if (black)
        {
            gray = 0;
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }
        else
        {
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }

        for (int i=0; i < sz.width * sz.height; i++)
            framebuffer[i] = LE16(color);
    }

    /* Now decode RLE and draw it on the screen */
    buff = (uint16_t *)((uintptr_t)framebuffer + pitch*start_y);
    buff += start_x;

    while(pix_cnt > 0) {
        uint8_t gray = *rle++;
        uint8_t cnt = *rle++;
        uint16_t color;

        if (purple)
        {
            gray = 240 - gray;
            int r=-330,g=-343,b=-91;
            r += (gray * 848) >> 8;
            g += (gray * 768) >> 8;
            b += (gray * 341) >> 8;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            color = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
        }
        else if (black)
        {
            int g = ((120 - (int)gray) * 5) / 4;
            if (g < 0)
                g = 0;
            if (g > 255)
                g = 255;

            color = (g >> 3) | ((g >> 2) << 5) | ((g >> 3) << 11);
        }
        else
        {
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }

        pix_cnt -= cnt;
        while(cnt--) {
            buff[x++] = LE16(color);
            /* If new line, advance the buffer by pitch and reset x counter */
            if (x >= EmuLogo.el_Width) {
                buff += pitch / 2;
                x = 0;
            }
        }
    }

    /* Print EMu68 version number and git sha. */
    kprintf_pc(__putc, NULL, &VERSION_STRING[6]);

    /* Reset test coordinates for further text printing (e.g. buptest) */
    text_x = 0;
    text_y = 0;
    #if 0
extern unsigned char pistorm_get_model();
    kprintf_pc(__putc, NULL, "PiStorm model: %d\n", pistorm_get_model());
extern void *firmware_file;
extern uint32_t firmware_size;
    kprintf_pc(__putc, NULL, "Firmware file: %p\n", firmware_file);
    kprintf_pc(__putc, NULL, "Firmware size: %d\n", firmware_size);
    #endif
}

uintptr_t top_of_ram;

#ifdef PISTORM_ANY_MODEL
#include "ps_protocol.h"

extern int block_c0;
#endif

void platform_init()
{
    of_node_t *e = NULL;

    /*
        Prepare mapping for peripherals. Use and update the data from device tree here
        All peripherals are mapped in the lower 4G address space so that they can be
        accessed from m68k.
    */
    e = dt_find_node("/soc");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "ranges");
        uint32_t *ranges = p->op_value;
        int32_t len = p->op_length;
        uint32_t start_map = 0xf20 / 2;

        int addr_cpu_len = dt_get_property_value_u32(e->on_parent, "#address-cells", 1, FALSE);
        int addr_bus_len = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int size_bus_len = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);

        int pos_abus = addr_bus_len - 1;
        int pos_acpu = pos_abus + addr_cpu_len;
        int pos_sbus = pos_acpu + size_bus_len;

        while (len > 0)
        {
            uint32_t addr_bus, addr_cpu;
            uint32_t addr_len;

            addr_bus = BE32(ranges[pos_abus]);
            addr_cpu = BE32(ranges[pos_acpu]);
            addr_len = BE32(ranges[pos_sbus]);

            (void)addr_bus;

            mmu_map(addr_cpu, start_map << 21, addr_len, 
                MMU_ACCESS | MMU_ALLOW_EL0 | MMU_ATTR_DEVICE, 0);

            kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

            ranges[pos_acpu] = BE32(start_map << 21);

            start_map += addr_len >> 21;

            len -= sizeof(int32_t) * (addr_bus_len + addr_cpu_len + size_bus_len);
            ranges += addr_bus_len + addr_cpu_len + size_bus_len;
        }
    }
}

void platform_post_init()
{
    void *base_vcmem;
    uint32_t size_vcmem;

    kprintf("[BOOT] Platform post init\n");

    if (get_max_clock_rate(3) != get_clock_rate(3)) {
        kprintf("[BOOT] Changing ARM clock from %d MHz to %d MHz\n", get_clock_rate(3)/1000000, get_max_clock_rate(3)/1000000);
        set_clock_rate(3, get_max_clock_rate(3));
    }
    kprintf("[BOOT] ARM Clock at %d MHz\n", get_clock_rate(3) / 1000000);  
    kprintf("[BOOT] CORE Clock at %d MHz\n", get_clock_rate(4) / 1000000);

    get_vc_memory(&base_vcmem, &size_vcmem);
    kprintf("[BOOT] VC4 memory: %p-%p\n", (intptr_t)base_vcmem, (intptr_t)base_vcmem + size_vcmem - 1);

    if (base_vcmem && size_vcmem)
    {
        mmu_map((uintptr_t)base_vcmem, (uintptr_t)base_vcmem, size_vcmem,
                MMU_ACCESS | MMU_OSHARE | MMU_ALLOW_EL0 | MMU_ATTR_WRITETHROUGH, 0);
    }

    display_logo();

#ifdef PISTORM_ANY_MODEL
    kprintf("[BOOT] sending RESET signal to Amiga\n");
    ps_pulse_reset();

    block_c0 = 0;

    ps_write_8_int(0xde1000, 0);
    if (ps_read_8_int(0xde1000) & 0x80)
    {
        if (ps_read_8_int(0xde1000) & 0x80)
        {
            if (!(ps_read_8_int(0xde1000) & 0x80))
            {
                if (ps_read_8_int(0xde1000) & 0x80)
                {
                    kprintf("[BOOT] Gayle appears to be present\n");
                    block_c0 = 1;
                }
            }
        }
    }

    if (block_c0 == 0)
    {
        kprintf("[BOOT] Gayle not detected\n");
    }

#endif

    //*(volatile uint32_t *)0xf3000034 = LE32((7680000) | 0x30000000);
}

#if defined(__BCM2708A0__)
   #define UNICAM_CTRL    0x000
   #define UNICAM_STA     0x004
   #define UNICAM_ANA     0x008
   #define UNICAM_PRI     0x00c
   #define UNICAM_CLK     0x010
   #define UNICAM_DAT0    0x014
   #define UNICAM_DAT1    0x018
   #define UNICAM_DAT2    0x01c
   #define UNICAM_DAT3    0x020
   #define UNICAM_CMP0    0x024
   #define UNICAM_CMP1    0x028
   #define UNICAM_CAP0    0x02c
   #define UNICAM_CAP1    0x030
   #define UNICAM_DBG0    0x0f0
   #define UNICAM_DBG1    0x0f4
   #define UNICAM_DBG2    0x0f8
   #define UNICAM_ICTL    0x100
   #define UNICAM_ISTA    0x104
   #define UNICAM_IDI     0x108
   #define UNICAM_IPIPE   0x10c
   #define UNICAM_IBSA    0x110
   #define UNICAM_IBEA    0x114
   #define UNICAM_IBLS    0x118
   #define UNICAM_IBWP    0x11c
   #define UNICAM_IHWIN   0x120
   #define UNICAM_IHSTA   0x124
   #define UNICAM_IVWIN   0x128
   #define UNICAM_IVSTA   0x12c
   #define UNICAM_DCS     0x200
   #define UNICAM_DBSA    0x204
   #define UNICAM_DBEA    0x208
   #define UNICAM_DBWP    0x20c
#else
   #define UNICAM_CTRL    0x000
   #define UNICAM_STA     0x004
   #define UNICAM_ANA     0x008
   #define UNICAM_PRI     0x00c
   #define UNICAM_CLK     0x010
   #define UNICAM_CLT     0x014
   #define UNICAM_DAT0    0x018
   #define UNICAM_DAT1    0x01c
   #define UNICAM_DAT2    0x020
   #define UNICAM_DAT3    0x024
   #define UNICAM_DLT     0x028
   #define UNICAM_CMP0    0x02c
   #define UNICAM_CMP1    0x030
   #define UNICAM_CAP0    0x034
   #define UNICAM_CAP1    0x038
   #define UNICAM_ICTL    0x100
   #define UNICAM_ISTA    0x104
   #define UNICAM_IDI0    0x108
   #define UNICAM_IPIPE   0x10c
   #define UNICAM_IBSA0   0x110
   #define UNICAM_IBEA0   0x114
   #define UNICAM_IBLS    0x118
   #define UNICAM_IBWP    0x11c
   #define UNICAM_IHWIN   0x120
   #define UNICAM_IHSTA   0x124
   #define UNICAM_IVWIN   0x128
   #define UNICAM_IVSTA   0x12c
   #define UNICAM_ICC     0x130
   #define UNICAM_ICS     0x134
   #define UNICAM_IDC     0x138
   #define UNICAM_IDPO    0x13c
   #define UNICAM_IDCA    0x140
   #define UNICAM_IDCD    0x144
   #define UNICAM_IDS     0x148
   #define UNICAM_DCS     0x200
   #define UNICAM_DBSA0   0x204
   #define UNICAM_DBEA0   0x208
   #define UNICAM_DBWP    0x20c
   #define UNICAM_DBCTL   0x300
   #define UNICAM_IBSA1   0x304
   #define UNICAM_IBEA1   0x308
   #define UNICAM_IDI1    0x30c
   #define UNICAM_DBSA1   0x310
   #define UNICAM_DBEA1   0x314
   #define UNICAM_MISC    0x400
#endif

/*
 * The following bitmasks are from the kernel released by Broadcom
 * for Android - https://android.googlesource.com/kernel/bcm/
 * The Rhea, Hawaii, and Java chips all contain the same VideoCore4
 * Unicam block as BCM2835, as defined in eg
 * arch/arm/mach-rhea/include/mach/rdb_A0/brcm_rdb_cam.h and similar.
 * Values reworked to use the kernel BIT and GENMASK macros.
 *
 * Some of the bit mnenomics have been amended to match the datasheet.
 */
/* UNICAM_CTRL Register */
#define UNICAM_CPE BIT(0)
#define UNICAM_MEM BIT(1)
#define UNICAM_CPR BIT(2)
#define UNICAM_CPM_MASK GENMASK(3, 3)
#define UNICAM_CPM_CSI2 0
#define UNICAM_CPM_CCP2 1
#define UNICAM_SOE BIT(4)
#define UNICAM_DCM_MASK GENMASK(5, 5)
#define UNICAM_DCM_STROBE 0
#define UNICAM_DCM_DATA 1
#define UNICAM_SLS BIT(6)
#define UNICAM_PFT_MASK GENMASK(11, 8)
#define UNICAM_OET_MASK GENMASK(20, 12)

/* UNICAM_STA Register */
#define UNICAM_SYN BIT(0)
#define UNICAM_CS BIT(1)
#define UNICAM_SBE BIT(2)
#define UNICAM_PBE BIT(3)
#define UNICAM_HOE BIT(4)
#define UNICAM_PLE BIT(5)
#define UNICAM_SSC BIT(6)
#define UNICAM_CRCE BIT(7)
#define UNICAM_OES BIT(8)
#define UNICAM_IFO BIT(9)
#define UNICAM_OFO BIT(10)
#define UNICAM_BFO BIT(11)
#define UNICAM_DL BIT(12)
#define UNICAM_PS BIT(13)
#define UNICAM_IS BIT(14)
#define UNICAM_PI0 BIT(15)
#define UNICAM_PI1 BIT(16)
#define UNICAM_FSI_S BIT(17)
#define UNICAM_FEI_S BIT(18)
#define UNICAM_LCI_S BIT(19)
#define UNICAM_BUF0_RDY BIT(20)
#define UNICAM_BUF0_NO BIT(21)
#define UNICAM_BUF1_RDY BIT(22)
#define UNICAM_BUF1_NO BIT(23)
#define UNICAM_DI BIT(24)

#define UNICAM_STA_MASK_ALL                                                    \
  (UNICAM_DL + UNICAM_SBE + UNICAM_PBE + UNICAM_HOE + UNICAM_PLE +             \
   UNICAM_SSC + UNICAM_CRCE + UNICAM_IFO + UNICAM_OFO + UNICAM_PS +            \
   UNICAM_PI0 + UNICAM_PI1)

/* UNICAM_ANA Register */
#define UNICAM_APD BIT(0)
#define UNICAM_BPD BIT(1)
#define UNICAM_AR BIT(2)
#define UNICAM_DDL BIT(3)
#define UNICAM_CTATADJ_MASK GENMASK(7, 4)
#define UNICAM_PTATADJ_MASK GENMASK(11, 8)

/* UNICAM_PRI Register */
#define UNICAM_PE BIT(0)
#define UNICAM_PT_MASK GENMASK(2, 1)
#define UNICAM_NP_MASK GENMASK(7, 4)
#define UNICAM_PP_MASK GENMASK(11, 8)
#define UNICAM_BS_MASK GENMASK(15, 12)
#define UNICAM_BL_MASK GENMASK(17, 16)

/* UNICAM_CLK Register */
#define UNICAM_CLE BIT(0)
#define UNICAM_CLPD BIT(1)
#define UNICAM_CLLPE BIT(2)
#define UNICAM_CLHSE BIT(3)
#define UNICAM_CLTRE BIT(4)
#define UNICAM_CLAC_MASK GENMASK(8, 5)
#define UNICAM_CLSTE BIT(29)

/* UNICAM_CLT Register */
#define UNICAM_CLT1_MASK GENMASK(7, 0)
#define UNICAM_CLT2_MASK GENMASK(15, 8)

/* UNICAM_DATn Registers */
#define UNICAM_DLE BIT(0)
#define UNICAM_DLPD BIT(1)
#define UNICAM_DLLPE BIT(2)
#define UNICAM_DLHSE BIT(3)
#define UNICAM_DLTRE BIT(4)
#define UNICAM_DLSM BIT(5)
#define UNICAM_DLFO BIT(28)
#define UNICAM_DLSTE BIT(29)

#define UNICAM_DAT_MASK_ALL (UNICAM_DLSTE + UNICAM_DLFO)

/* UNICAM_DLT Register */
#define UNICAM_DLT1_MASK GENMASK(7, 0)
#define UNICAM_DLT2_MASK GENMASK(15, 8)
#define UNICAM_DLT3_MASK GENMASK(23, 16)

/* UNICAM_ICTL Register */
#define UNICAM_FSIE BIT(0)
#define UNICAM_FEIE BIT(1)
#define UNICAM_IBOB BIT(2)
#define UNICAM_FCM BIT(3)
#define UNICAM_TFC BIT(4)
#define UNICAM_LIP_MASK GENMASK(6, 5)
#define UNICAM_LCIE_MASK GENMASK(28, 16)

/* UNICAM_IDI0/1 Register */
#define UNICAM_ID0_MASK GENMASK(7, 0)
#define UNICAM_ID1_MASK GENMASK(15, 8)
#define UNICAM_ID2_MASK GENMASK(23, 16)
#define UNICAM_ID3_MASK GENMASK(31, 24)

/* UNICAM_ISTA Register */
#define UNICAM_FSI BIT(0)
#define UNICAM_FEI BIT(1)
#define UNICAM_LCI BIT(2)

#define UNICAM_ISTA_MASK_ALL (UNICAM_FSI + UNICAM_FEI + UNICAM_LCI)

/* UNICAM_IPIPE Register */
#define UNICAM_PUM_MASK GENMASK(2, 0)
/* Unpacking modes */
#define UNICAM_PUM_NONE 0
#define UNICAM_PUM_UNPACK6 1
#define UNICAM_PUM_UNPACK7 2
#define UNICAM_PUM_UNPACK8 3
#define UNICAM_PUM_UNPACK10 4
#define UNICAM_PUM_UNPACK12 5
#define UNICAM_PUM_UNPACK14 6
#define UNICAM_PUM_UNPACK16 7
#define UNICAM_DDM_MASK GENMASK(6, 3)
#define UNICAM_PPM_MASK GENMASK(9, 7)
/* Packing modes */
#define UNICAM_PPM_NONE 0
#define UNICAM_PPM_PACK8 1
#define UNICAM_PPM_PACK10 2
#define UNICAM_PPM_PACK12 3
#define UNICAM_PPM_PACK14 4
#define UNICAM_PPM_PACK16 5
#define UNICAM_DEM_MASK GENMASK(11, 10)
#define UNICAM_DEBL_MASK GENMASK(14, 12)
#define UNICAM_ICM_MASK GENMASK(16, 15)
#define UNICAM_IDM_MASK GENMASK(17, 17)

/* UNICAM_ICC Register */
#define UNICAM_ICFL_MASK GENMASK(4, 0)
#define UNICAM_ICFH_MASK GENMASK(9, 5)
#define UNICAM_ICST_MASK GENMASK(12, 10)
#define UNICAM_ICLT_MASK GENMASK(15, 13)
#define UNICAM_ICLL_MASK GENMASK(31, 16)

/* UNICAM_DCS Register */
#define UNICAM_DIE BIT(0)
#define UNICAM_DIM BIT(1)
#define UNICAM_DBOB BIT(3)
#define UNICAM_FDE BIT(4)
#define UNICAM_LDP BIT(5)
#define UNICAM_EDL_MASK GENMASK(15, 8)

/* UNICAM_DBCTL Register */
#define UNICAM_DBEN BIT(0)
#define UNICAM_BUF0_IE BIT(1)
#define UNICAM_BUF1_IE BIT(2)

/* UNICAM_CMP[0,1] register */
#define UNICAM_PCE BIT(31)
#define UNICAM_GI BIT(9)
#define UNICAM_CPH BIT(8)
#define UNICAM_PCVC_MASK GENMASK(7, 6)
#define UNICAM_PCDT_MASK GENMASK(5, 0)

/* UNICAM_MISC register */
#define UNICAM_FL0 BIT(6)
#define UNICAM_FL1 BIT(9)


#define BIT(n) (UINT32_C(1) << (n))
#define u32 uint32_t
#define AARCH 32
#define GENMASK(h, l) ((~0 - (1 << (l)) + 1) & (~0 >> (AARCH - 1 - (h))))

#define ARM_IO_BASE (uintptr_t)0xf2000000
#define ARM_CSI0_BASE (ARM_IO_BASE + 0x800000)
#define ARM_CSI0_END (ARM_CSI0_BASE + 0x7FF)
#define ARM_CSI0_CLKGATE (ARM_IO_BASE + 0x802000) // 4 bytes
#define ARM_CSI1_BASE (ARM_IO_BASE + 0x801000)
#define ARM_CSI1_END (ARM_CSI1_BASE + 0x7FF)
#define ARM_CSI1_CLKGATE (ARM_IO_BASE + 0x802004) // 4 bytes
#define ARM_CM_BASE (ARM_IO_BASE + 0x101000)
#define ARM_CM_CAM0CTL (ARM_CM_BASE + 0x40)
#define ARM_CM_CAM0DIV (ARM_CM_BASE + 0x44)
#define ARM_CM_CAM1CTL (ARM_CM_BASE + 0x48)
#define ARM_CM_CAM1DIV (ARM_CM_BASE + 0x4C)
#define ARM_CM_PASSWD (0x5A << 24)

static void usleep(uint64_t delta)
{
    volatile struct
    {
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

    do
    {
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

void setup_csiclk()
{
    *(volatile uint32_t *)(ARM_CM_CAM1CTL) = LE32(ARM_CM_PASSWD | (1 << 5));
    usleep(100);
    while ((*(volatile uint32_t *)(ARM_CM_CAM1CTL)) & LE32(1 << 7)) {}
    usleep(100);
    *(volatile uint32_t *)(ARM_CM_CAM1DIV) =
        LE32(ARM_CM_PASSWD | (4 << 12)); // divider , 12=100MHz on pi3 ??
    usleep(100);
    *(volatile uint32_t *)(ARM_CM_CAM1CTL) =
        LE32(ARM_CM_PASSWD | 6 | (1 << 4)); // pll? 6=plld, 5=pllc
    usleep(100);
    while (((*(volatile uint32_t *)(ARM_CM_CAM1CTL)) & LE32(1 << 7)) == 0) {}
    usleep(100);
}

void ClockWrite(uint32_t nValue)
{
    *(volatile uint32_t *)(ARM_CSI1_CLKGATE) = LE32(ARM_CM_PASSWD | nValue);
}


void SetField(uint32_t *pValue, uint32_t nValue, uint32_t nMask)
{
    uint32_t nTempMask = nMask;
    while (!(nTempMask & 1)) {
        nValue <<= 1;
        nTempMask >>= 1;
    }

    *pValue = (*pValue & ~nMask) | nValue;
}

uint32_t ReadReg(uint32_t nOffset)
{
    uint32_t temp;
    temp = LE32(*(volatile uint32_t *)(ARM_CSI1_BASE + nOffset));

    return temp;
}

void WriteReg(uint32_t nOffset, uint32_t nValue)
{
    *(volatile uint32_t *)(ARM_CSI1_BASE + nOffset) = LE32(nValue);
}

void WriteRegField(uint32_t nOffset, uint32_t nValue, uint32_t nMask)
{
    uint32_t nBuffer = ReadReg(nOffset);
    SetField(&nBuffer, nValue, nMask);
    WriteReg(nOffset, nBuffer);
}


void unicam_run(uintptr_t address , uint8_t lanes, uint8_t datatype, uint32_t width , uint32_t height , uint8_t bbp)
{
    //enable power domain
    enable_unicam_domain();

    //enable to clock to unicam
    setup_csiclk();

    // Enable lane clocks (2 lanes)
    ClockWrite(0b010101);

    // Basic init
    WriteReg(UNICAM_CTRL, UNICAM_MEM);

    // Enable analogue control, and leave in reset.
    uint32_t nValue = UNICAM_AR;
    SetField(&nValue, 7, UNICAM_CTATADJ_MASK);
    SetField(&nValue, 7, UNICAM_PTATADJ_MASK);
    WriteReg(UNICAM_ANA, nValue);

    usleep(1000);

    // Come out of reset
    WriteRegField(UNICAM_ANA, 0, UNICAM_AR);

    // Peripheral reset
    WriteRegField(UNICAM_CTRL, 1, UNICAM_CPR);
    WriteRegField(UNICAM_CTRL, 0, UNICAM_CPR);

    WriteRegField(UNICAM_CTRL, 0, UNICAM_CPE);

    // Enable Rx control (CSI2 DPHY)
    nValue = ReadReg(UNICAM_CTRL);
    SetField(&nValue, UNICAM_CPM_CSI2, UNICAM_CPM_MASK);
    SetField(&nValue, UNICAM_DCM_STROBE, UNICAM_DCM_MASK);

    // Packet framer timeout
    SetField(&nValue, 0xf, UNICAM_PFT_MASK);
    SetField(&nValue, 128, UNICAM_OET_MASK);
    WriteReg(UNICAM_CTRL, nValue);

    WriteReg(UNICAM_IHWIN, 0);
    WriteReg(UNICAM_IVWIN, 0);

    // AXI bus access QoS setup
    nValue = ReadReg(UNICAM_PRI);
    SetField(&nValue, 0, UNICAM_BL_MASK);
    SetField(&nValue, 0, UNICAM_BS_MASK);
    SetField(&nValue, 0xe, UNICAM_PP_MASK);
    SetField(&nValue, 8, UNICAM_NP_MASK);
    SetField(&nValue, 2, UNICAM_PT_MASK);
    SetField(&nValue, 1, UNICAM_PE);
    WriteReg(UNICAM_PRI, nValue);

    WriteRegField(UNICAM_ANA, 0, UNICAM_DDL);

    uint32_t nLineIntFreq = height >> 2;
    nValue = UNICAM_FSIE | UNICAM_FEIE | UNICAM_IBOB;
    SetField(&nValue, nLineIntFreq >= 128 ? nLineIntFreq : 128, UNICAM_LCIE_MASK);
    WriteReg(UNICAM_ICTL, nValue);
    WriteReg(UNICAM_STA, UNICAM_STA_MASK_ALL);
    WriteReg(UNICAM_ISTA, UNICAM_ISTA_MASK_ALL);

    WriteRegField(UNICAM_CLT, 2, UNICAM_CLT1_MASK); // tclk_term_en
    WriteRegField(UNICAM_CLT, 6, UNICAM_CLT2_MASK); // tclk_settle
    WriteRegField(UNICAM_DLT, 2, UNICAM_DLT1_MASK); // td_term_en
    WriteRegField(UNICAM_DLT, 6, UNICAM_DLT2_MASK); // ths_settle
    WriteRegField(UNICAM_DLT, 0, UNICAM_DLT3_MASK); // trx_enable

    WriteRegField(UNICAM_CTRL, 0, UNICAM_SOE);

    // Packet compare setup - required to avoid missing frame ends
    nValue = 0;
    SetField(&nValue, 1, UNICAM_PCE);
    SetField(&nValue, 1, UNICAM_GI);
    SetField(&nValue, 1, UNICAM_CPH);
    SetField(&nValue, 0, UNICAM_PCVC_MASK);
    SetField(&nValue, 1, UNICAM_PCDT_MASK);
    WriteReg(UNICAM_CMP0, nValue);

    // Enable clock lane and set up terminations (CSI2 DPHY, non-continous clock)
    nValue = 0;
    SetField(&nValue, 1, UNICAM_CLE);
    SetField(&nValue, 1, UNICAM_CLLPE);
    WriteReg(UNICAM_CLK, nValue);

    // Enable required data lanes with appropriate terminations.
    // The same value needs to be written to UNICAM_DATn registers for
    // the active lanes, and 0 for inactive ones.
    // (CSI2 DPHY, non-continous clock, 2 data lanes)
    nValue = 0;
    SetField(&nValue, 1, UNICAM_DLE);
    SetField(&nValue, 1, UNICAM_DLLPE);
    WriteReg(UNICAM_DAT0, nValue);
    if (lanes == 1)
        WriteReg(UNICAM_DAT1, 0);
    if (lanes == 2)
        WriteReg(UNICAM_DAT1, nValue);

    WriteReg(UNICAM_IBLS, width*(bbp/8));

    // Write DMA buffer address

    WriteReg(UNICAM_IBSA0, ((u32)(address) & ~0xC0000000) | 0xC0000000);
    WriteReg(UNICAM_IBEA0,
            ((uint32_t)(address + (width * height * (bbp/8))) & ~0xC0000000) | 0xC0000000);

    // Set packing configuration
    uint32_t nUnPack = UNICAM_PUM_NONE;
    uint32_t nPack = UNICAM_PPM_NONE;

    nValue = 0;
    SetField(&nValue, nUnPack, UNICAM_PUM_MASK);
    SetField(&nValue, nPack, UNICAM_PPM_MASK);
    WriteReg(UNICAM_IPIPE, nValue);

    // CSI2 mode, hardcode VC 0 for now.
    WriteReg(UNICAM_IDI0, (0 << 6) | datatype);

    nValue = ReadReg(UNICAM_MISC);
    SetField(&nValue, 1, UNICAM_FL0);
    SetField(&nValue, 1, UNICAM_FL1);
    WriteReg(UNICAM_MISC, nValue);

    // Clear ED setup
    WriteReg(UNICAM_DCS, 0);

    // Enable peripheral
    WriteRegField(UNICAM_CTRL, 1, UNICAM_CPE);

    // Load image pointers
    WriteRegField(UNICAM_ICTL, 1, UNICAM_LIP_MASK);
}

#define UNICAM_MODE     0x22
#define UNICAM_WIDTH    720
#define UNICAM_HEIGHT   576
#define UNICAM_BPP      16

/* 
    Report stealth mode - in normal cases add a text on the screen, unless user requested 
    framethrower support during boot. In that case, set the framethrower output.
*/
void platform_report_stealth()
{
    int unicam_initialized = 0;

    of_node_t *e = dt_find_node("/chosen");
    if (e)
    {
        of_property_t *prop = dt_find_property(e, "bootargs");
        if (prop)
        {
            const char *cmdline = prop->op_value;
            
            if (!!find_token(cmdline, "unicam.boot"))
            {
                struct Size sz = { 720, 576 };
                void *framebuffer = NULL;
                uint32_t pitch = 0;

                init_display(sz, &framebuffer, &pitch);

                setup_csiclk();
                unicam_run((uintptr_t)framebuffer, 1, UNICAM_MODE, 720, 576, UNICAM_BPP);
                
                unicam_initialized = 1;
            }
        }
    }

    if (!unicam_initialized)
    {
        struct Size sz = get_display_size();
        uint32_t last_x = text_x;
        uint32_t last_y = text_y;

        uint32_t start_y = (sz.height + EmuLogo.el_Height) / 2;
        
        text_y = start_y / 16;
        text_x = (sz.width - 20 * 8) / 16;

        kprintf_pc(__putc, NULL, "!!! STEALTH MODE !!!");

        text_x = last_x;
        text_y = last_y;
    }
}
