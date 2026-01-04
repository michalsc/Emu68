/*
 * © Copyright 2026 Claude Schwarz
 * SPDX-License-Identifier: MPL
 *
 * This file is a host adapter for the Lib(X)SVF library.
 * The Lib(X)SVF library itself is distributed under a different license.
 *
 * Lib(X)SVF Copyrights:
 * Copyright (C) 2009 RIEGL Research ForschungsGmbH
 * Copyright (C) 2009 Clifford Wolf <clifford@clifford.at>
 *
 * The Lib(X)SVF license is a permissive ISC-style license, granting broad
 * permissions for use, modification, and distribution. For the full text,
 * see libxsvf/COPYING.
 */

#include "libxsvf/libxsvf.h"
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __GNUC__
#include <sys/types.h>
#endif

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+ External Function Declarations +-+-+-+-+-+-+-+-+-+-+-+-+
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

extern volatile unsigned int* gpio;

extern void put_char(uint8_t c);
extern int vkprintf_pc(void (*putc_func)(void*, const char), void* userdata, const char* fmt, va_list ap);
extern void* tlsf;
extern void* my_malloc(size_t size);
extern void* my_calloc(size_t nmemb, size_t size);
extern void* my_realloc(void* ptr, size_t size);
extern void  my_free(void* ptr);
extern int my_vsnprintf(char* str, size_t size, const char* format, va_list ap);

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+ GPIO and Pin Definitions +-+-+-+-+-+-+-+-+-+-+-+-+-+
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

#define PINTCK 26
#define PINTMS 24
#define PINTDI 27
#define PINTDO 25

static inline uint32_t le32(uint32_t x)
{
    return __builtin_bswap32(x);
}

#define SET_GPIO(g)     *(gpio + 7) = le32((1 << g))
#define RESET_GPIO(g)   *(gpio + 10) = le32((1 << g))
#define GET_GPIO(g)     le32(*(gpio + 13) & (1 << g))
#define PF(f, i)        ((f) << (((i) % 10) * 3)) // Pin function
#define GO(i)           PF(1, (i)) // GPIO output
#define GI(i)           PF(0, (i)) // GPIO input


//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+ Private Screen IO Functions +-+-+-+-+-+-+-+-+-+-+-+-+-+
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Wrapper to provide a character output function for vkprintf_pc.
 */
static void putcCallback(void* data, char c)
{
    (void)data;
    put_char(c);
}

/**
 * @brief Writes a buffer of a given length to the screen.
 */
static void screenWriteBuf(const char* buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        putcCallback(NULL, (unsigned char)buf[i]);
    }
}

/**
 * @brief A vsnprintf implementation using the external my_vsnprintf.
 */
static int screenVsnprintf(char* buf, size_t bufsize, const char* fmt, va_list ap)
{
    return my_vsnprintf(buf, bufsize, fmt, ap);
}

/**
 * @brief A printf-like function that writes formatted output to the screen.
 */
static void screenPrintf(const char* fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = screenVsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n < sizeof(tmp)) {
            screenWriteBuf(tmp, (size_t)n);
        } else {
            screenWriteBuf(tmp, sizeof(tmp) - 1);
        }
    }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+ Host and State Management +-+-+-+-+-+-+-+-+-+-+-+-+-+
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Holds user data for the libxsvf host, tracking state and buffer info.
 */
struct UhostData {
    const uint8_t*  data;
    int             data_length;
    int             pos;
    int             verbose;
    int             clock_count;
    int             bit_count_tdi;
    int             bit_count_tdo;
    int             return_values[256];
    int             return_val_i;
    unsigned long   jtag_usercode;
    unsigned long   svf_usercode;
    unsigned long   detected_idcode;
    int             silent;
};
static struct UhostData g_uhost_data;

// Tracks max memory allocated by libxsvf for debugging.
static int g_realloc_max_size[LIBXSVF_MEM_NUM];

static void* hRealloc(struct libxsvf_host* h, void* ptr, int size, enum libxsvf_mem which);
static int hSetup(struct libxsvf_host* h);
static int hShutdown(struct libxsvf_host* h);
static void hUdelay(struct libxsvf_host* h, long usecs, int tms, long num_tck);
static int hGetbyte(struct libxsvf_host* h);
static int hPulseTck(struct libxsvf_host* h, int tms, int tdi, int tdo, int rmask, int sync);
static int hSetFrequency(struct libxsvf_host* h, int v);
static void hReportTapstate(struct libxsvf_host* h);
static void hReportDevice(struct libxsvf_host* h, unsigned long idcode);
static void hReportStatus(struct libxsvf_host* h, const char* message);
static void hReportError(struct libxsvf_host* h, const char* file, int line, const char* message);
static void hReportUsercode(struct libxsvf_host* h, unsigned long usercode);

/**
 * @brief Global libxsvf host interface structure, configured with our callbacks.
 */
static struct libxsvf_host g_libxsvf_host = {
    .setup = hSetup,
    .shutdown = hShutdown,
    .udelay = hUdelay,
    .getbyte = hGetbyte,
    .sync = NULL,
    .pulse_tck = hPulseTck,
    .pulse_sck = NULL,
    .set_trst = NULL,
    .set_frequency = hSetFrequency,
    .report_tapstate = hReportTapstate,
    .report_device = hReportDevice,
    .report_status = hReportStatus,
    .report_error = hReportError,
    .report_usercode = hReportUsercode,
    .realloc = hRealloc,
    .tap_state = LIBXSVF_TAP_INIT,
    .user_data = &g_uhost_data
};

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+- Private Helper Functions +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Custom strlen implementation to avoid stdlib dependency.
 */
static size_t prvStrlen(const char* s)
{
    size_t i = 0;
    while (s[i]) {
        i++;
    }
    return i;
}

/**
 * @brief Custom memcmp implementation to avoid stdlib dependency.
 */
static int prvMemcmp(const void* s1, const void* s2, size_t n)
{
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while (n-- > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

/**
 * @brief Custom memcpy implementation to avoid stdlib dependency.
 */
static void* prvMemcpy(void* dest, const void* src, size_t n)
{
    char* d = dest;
    const char* s = src;
    while (n-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

/**
 * @brief Custom hex string to unsigned long conversion.
 */
static unsigned long prvStrtoulHex(const char* s)
{
    unsigned long val = 0;
    char c;
    while ((c = *s++)) {
        if (c >= '0' && c <= '9') {
            val = (val << 4) + (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            val = (val << 4) + (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            val = (val << 4) + (c - 'A' + 10);
        } else {
            break;
        }
    }
    return val;
}

/**
 * @brief Extracts the USERCODE from a buffer containing SVF data.
 * @return The usercode, or 0 if not found.
 */
unsigned long getUsercodeFromSvf(const uint8_t* data, int data_length)
{
    const char* svf_text = (const char*)data;
    const char* end = svf_text + data_length;
    const char* pattern = "!NOTE \"USERCODE\"";
    size_t pattern_length = prvStrlen(pattern);

    const char* p = svf_text;
    while (p + pattern_length <= end) {
        if (prvMemcmp(p, pattern, pattern_length) == 0) {
            p += pattern_length;
            while (p < end && (*p == ' ' || *p == '\t')) { // Skip whitespace
                p++;
            }

            if (p < end && *p == '"') {
                p++; // Skip opening quote
                const char* code_start = p;
                while (p < end && *p != '"') {
                    p++;
                }
                if (p < end) { // Found closing quote
                    char num_buffer[12]; // For hex code + null
                    size_t code_length = p - code_start;
                    if (code_length > 0 && code_length < sizeof(num_buffer)) {
                        prvMemcpy(num_buffer, code_start, code_length);
                        num_buffer[code_length] = '\0';
                        return prvStrtoulHex(num_buffer);
                    }
                }
            }
        }
        p++;
    }
    return 0; // Not found
}

/**
 * @brief Prints a standardized failure message to the screen.
 */
static void printFailureMessage()
{
    screenPrintf("\n--- CPLD Action Failed ---\n");
    screenPrintf("CPLD check or programming was not successful.\n");
    screenPrintf("The system will attempt to start PiStorm, but it is not expected to work correctly.\n");
    screenPrintf("Please check the PiStorm hardware, Raspberry Pi connections, and the programming files.\n");
    screenPrintf("--------------------------\n");
}


//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+ Platform-Specific JTAG/Delay Functions +-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Delays execution for a specified number of microseconds.
 */
void platformUdelayUs(long usecs)
{
    volatile long i;
    for (i = 0; i < usecs * 100; i++) {
        __asm__ volatile("nop");
    }
}

/**
 * @brief Sets the JTAG TMS (Test Mode Select) line state.
 */
void jtagSetTms(int val)
{
    if (val) {
        SET_GPIO(PINTMS);
    } else {
        RESET_GPIO(PINTMS);
    }
}

/**
 * @brief Sets the JTAG TDI (Test Data In) line state.
 */
void jtagSetTdi(int val)
{
    if (val) {
        SET_GPIO(PINTDI);
    } else {
        RESET_GPIO(PINTDI);
    }
}

/**
 * @brief Pulses the JTAG TCK (Test Clock) line once.
 */
void jtagPulseTck(void)
{
    RESET_GPIO(PINTCK);
    platformUdelayUs(8);
    SET_GPIO(PINTCK);
    platformUdelayUs(8);
}

/**
 * @brief Reads the JTAG TDO (Test Data Out) line state.
 */
int jtagReadTdo(void)
{
    volatile uint32_t pin = le32(*(gpio + 13));
    pin = le32(*(gpio + 13));
    return (pin & (1 << PINTDO)) ? 1 : 0;
}


//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+ libxsvf Host Callback Functions +-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Implements realloc for libxsvf using a TLSF memory pool.
 */
static void* hReallocTlsf(struct libxsvf_host* h, void* ptr, int size, enum libxsvf_mem which)
{
    (void)h;
    if (which >= 0 && which < LIBXSVF_MEM_NUM) {
        if (size > g_realloc_max_size[which]) {
            g_realloc_max_size[which] = size;
        }
    }

    if (size <= 0) {
        if (ptr) {
            my_free(ptr);
        }
        return NULL;
    }

    if (ptr == NULL) {
        void* p = my_malloc((size_t)size);
        if (p) {
            memset(p, 0, size);
        }
        return p;
    } else {
        return my_realloc(ptr, (size_t)size);
    }
}

static void* hRealloc(struct libxsvf_host* h, void* ptr, int size, enum libxsvf_mem which)
{
    return hReallocTlsf(h, ptr, size, which);
}

/**
 * @brief Sets up the JTAG hardware and initializes host state.
 */
static int hSetup(struct libxsvf_host* h)
{
    (void)h;
    g_uhost_data.pos = 0;
    g_uhost_data.clock_count = 0;
    g_uhost_data.bit_count_tdi = 0;
    g_uhost_data.bit_count_tdo = 0;
    g_uhost_data.return_val_i = 0;
    memset(g_realloc_max_size, 0, sizeof(g_realloc_max_size));
    if (g_uhost_data.verbose) {
        screenPrintf("[libxsvf] setup\n");
    }

    *(gpio + 2) = le32(GI(PINTDO) | GO(PINTMS) | GO(PINTCK) | GO(PINTDI)); // GPIO 20-29
    return 0;
}

/**
 * @brief Shuts down the JTAG hardware, setting pins to input.
 */
static int hShutdown(struct libxsvf_host* h)
{
    (void)h;
    if (g_uhost_data.verbose) {
        screenPrintf("[libxsvf] shutdown\n");
    }
    *(gpio + 2) = le32(GI(PINTDO) | GI(PINTMS) | GI(PINTCK) | GI(PINTDI)); // GPIO 20-29
    //platformUdelayUs(500000);
    //platformUdelayUs(500000);
    //platformUdelayUs(500000);
    return 0;
}

/**
 * @brief Provides a microsecond delay or pulses TCK.
 */
static void hUdelay(struct libxsvf_host* h, long usecs, int tms, long num_tck)
{
    (void)h;
    if (num_tck > 0) {
        jtagSetTms(tms);
        while (num_tck-- > 0) {
            jtagPulseTck();
            g_uhost_data.clock_count++;
        }
    }
    if (usecs > 0) {
        platformUdelayUs(usecs);
    }
}

/**
 * @brief Gets the next byte from the SVF/XSVF data buffer.
 */
static int hGetbyte(struct libxsvf_host* h)
{
    (void)h;
    if (g_uhost_data.pos >= g_uhost_data.data_length) {
        return -1;
    }
    return g_uhost_data.data[g_uhost_data.pos++];
}

/**
 * @brief Pulses TCK while managing TDI, TDO, and TMS. The core of JTAG communication.
 */
static int hPulseTck(struct libxsvf_host* h, int tms, int tdi, int tdo, int rmask, int sync)
{
    (void)h;
    (void)sync;

    jtagSetTms(tms);

    if (tdi >= 0) {
        g_uhost_data.bit_count_tdi++;
        jtagSetTdi(tdi);
    }

    jtagPulseTck();

    int line_tdo = jtagReadTdo();
    int rc = line_tdo >= 0 ? line_tdo : 0;

    if (rmask && g_uhost_data.return_val_i < 256) {
        g_uhost_data.return_values[g_uhost_data.return_val_i++] = line_tdo ? 1 : 0;
    }

    if (tdo >= 0 && line_tdo >= 0) {
        g_uhost_data.bit_count_tdo++;
        if (tdo != line_tdo) {
            rc = -1;
        }
    }

    g_uhost_data.clock_count++;
    return rc;
}

/**
 * @brief Sets the JTAG frequency (ignored in this implementation).
 */
static int hSetFrequency(struct libxsvf_host* h, int v)
{
    (void)h;
    (void)v;
    if (g_uhost_data.verbose) {
        screenPrintf("[libxsvf] set_frequency ignored: %d\n", v);
    }
    return 0;
}

/**
 * @brief Reports the current TAP state (disabled).
 */
static void hReportTapstate(struct libxsvf_host* h)
{
    (void)h;
    //if (g_uhost_data.verbose)
    //    screenPrintf("[libxsvf] TAP state: %s\n", libxsvf_state2str(h->tap_state));
}

/**
 * @brief Reports the detected device IDCODE and identifies supported CPLDs.
 */
static void hReportDevice(struct libxsvf_host* h, unsigned long idcode)
{
    (void)h;
    unsigned long part;
    unsigned long manufacturer;
    const char* part_name = "Unknown";
    int supported = 0;

    part = (idcode >> 12) & 0xffff;
    manufacturer = (idcode >> 1) & 0x7ff;

    if (manufacturer == 0x6E) { // Altera
        switch (part) {
            case 0x20a1:
                part_name = "MaxII EPM240";
                supported = 1;
                break;
            case 0x20a2:
                part_name = "MaxII EPM570";
                supported = 1;
                break;
            case 0x20a5:
                part_name = "MaxV 5M240Z";
                supported = 1;
                break;
            case 0x20a6:
                part_name = "MaxV 5M570Z";
                supported = 1;
                break;
            default:
                part_name = "Altera (Unsupported)";
                break;
        }
    }

    if (supported) {
        g_uhost_data.detected_idcode = idcode;
        if (!g_uhost_data.silent) {
            screenPrintf("Supported CPLD detected: %s\n", part_name);
        }
    } else {
        if (!g_uhost_data.silent) {
            screenPrintf("Unsupported device: idcode=0x%08lx (manufacturer=0x%03lx, part=0x%04lx)\n",
                      idcode, manufacturer, part);
        }
    }
}

/**
 * @brief Reports a status message from the library (disabled).
 */
static void hReportStatus(struct libxsvf_host* h, const char* message)
{
    (void)h;
    (void)message;
    //if (message)
    //    screenPrintf("[STATUS] %s\n", message);
}

/**
 * @brief Reports the device usercode and looks up a friendly name.
 *
 * This function first checks for a new usercode format indicated by the top nibble.
 * If the format is new (nibble 1-3), it decodes chip type, version, and a custom
 * field. If the format is old (nibble 0) or unrecognized, it falls back to a
 * legacy lookup table.
 */
static void hReportUsercode(struct libxsvf_host* h, unsigned long usercode)
{
    (void)h;
    g_uhost_data.jtag_usercode = usercode;

    unsigned int type_nibble = (usercode >> 28) & 0xF;

    // New format: 0xTMMMAAAA where T is type, MMM is version, AAAA is custom
    if (type_nibble > 0 && type_nibble <= 3) {
        const char* chip_type_str = "Unknown";
        switch (type_nibble) {
            case 0x1: chip_type_str = "MaxII EPM240"; break;
            case 0x2: chip_type_str = "MaxII EPM570"; break;
            case 0x3: chip_type_str = "MaxV 5M240Z"; break;
            case 0x4: chip_type_str = "MaxV 5M570Z"; break;
        }

        unsigned int major = (usercode >> 24) & 0xF;
        unsigned int minor = (usercode >> 20) & 0xF;
        unsigned int patch = (usercode >> 16) & 0xF;

        char custom_field[5];
        unsigned char byte1 = (usercode >> 8) & 0xFF;
        unsigned char byte2 = usercode & 0xFF;

        // Use ASCII if printable, otherwise format as hex
        if (byte1 >= 32 && byte1 <= 126 && byte2 >= 32 && byte2 <= 126) {
            custom_field[0] = (char)byte1;
            custom_field[1] = (char)byte2;
            custom_field[2] = '\0';
        } else {
            const char* hex_digits = "0123456789ABCDEF";
            custom_field[0] = hex_digits[byte1 >> 4];
            custom_field[1] = hex_digits[byte1 & 0x0F];
            custom_field[2] = hex_digits[byte2 >> 4];
            custom_field[3] = hex_digits[byte2 & 0x0F];
            custom_field[4] = '\0';
        }

        if (!g_uhost_data.silent) {
            screenPrintf("usercode=0x%08lx (Type: %s, Version: %u.%u.%u, Custom: %s)\n",
                       usercode, chip_type_str, major, minor, patch, custom_field);
        }
        return;
    }

    // --- Legacy Format Fallback ---
    struct UsercodeLookup {
        unsigned long usercode;
        const char* chip;
        const char* name;
    };
    static const struct UsercodeLookup USERCODE_LOOKUP_TABLE[] = {
        // MaxII EPM240
        { 0x00189844, "MaxII EPM240", "EPM240_PiStormX_basic" },
        { 0x0018A0B9, "MaxII EPM240", "EPM240_PiStormX_skip_s5s6" },
        { 0x0018569E, "MaxII EPM240", "EPM240_bitstream" },
        { 0x00185866, "MaxII EPM240", "EPM240_experimental" },
        { 0x00187C10, "MaxII EPM240", "EPM240_long_data" },
        { 0x00186C9D, "MaxII EPM240", "EPM240_long_hold" },
        { 0x00187FC6, "MaxII EPM240", "EPM240_longer_hold" },
        { 0x0018736F, "MaxII EPM240", "EPM240_original" },
        // MaxII EPM570
        { 0x00336118, "MaxII EPM570", "bitstream" },
        { 0x003354F0, "MaxII EPM570", "bitstream_experimental" },
        { 0x00337130, "MaxII EPM570", "bitstream_long_data" },
        { 0x00336000, "MaxII EPM570", "bitstream_long_hold" },
        { 0x00338451, "MaxII EPM570", "bitstream_longer_hold" },
        { 0x0033700A, "MaxII EPM570", "bitstream_original" },
        // MaxV
        { 0x00187AFD, "MaxV 5M240Z", "maxv_bitstream" },
        // End of list
        { 0, NULL, NULL }
    };

    const char* found_name = NULL;
    const char* found_chip = NULL;

    for (int i = 0; USERCODE_LOOKUP_TABLE[i].name != NULL; i++) {
        if (USERCODE_LOOKUP_TABLE[i].usercode == usercode) {
            found_chip = USERCODE_LOOKUP_TABLE[i].chip;
            found_name = USERCODE_LOOKUP_TABLE[i].name;
            break;
        }
    }

    if (!g_uhost_data.silent) {
        if (found_name) {
            screenPrintf("usercode=0x%08lx (%s: %s)\n", usercode, found_chip, found_name);
        } else {
            screenPrintf("usercode=0x%08lx (Unknown usercode)\n", usercode);
        }
    }
}

/**
 * @brief Reports an error from the library.
 */
static void hReportError(struct libxsvf_host* h, const char* file, int line, const char* message)
{
    (void)h;
    (void)file;
    (void)line;
    screenPrintf("[ERROR] %s\n", message ? message : "unknown");
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+- Public CPLD Control Functions +-+-+-+-+-+-+-+-+-+-+-+-
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

/**
 * @brief Resets the CPLD's JTAG interface and disables ISC mode via libxsvf.
 *
 * This function sends a specific sequence of SVF commands to ensure the CPLD
 * is not in In-System-Configuration (ISC) mode and brings the JTAG TAP to
 * a clean IDLE state. This can be useful to recover the CPLD after a failed
 * programming attempt.
 *
 * This function is self-contained and performs its own JTAG setup.
 *
 * @param verbose If non-zero, enables detailed debug output.
 * @return 0 on success, -1 on failure.
 */
int ps_cpld_reset_isc(int verbose)
{
    const char *isc_disable_svf =
        "STATE IDLE;"
        "SIR 10 TDI (201);"  /* ISC_DISABLE */
        "RUNTEST 103 TCK;"
        "SIR 10 TDI (3FF);"  /* BYPASS */
        "RUNTEST 100 TCK;"
        "STATE IDLE;";
    int return_code = 0;

    memset(&g_uhost_data, 0, sizeof(g_uhost_data));
    g_uhost_data.data = (const uint8_t*)isc_disable_svf;
    g_uhost_data.data_length = prvStrlen(isc_disable_svf);
    g_uhost_data.verbose = verbose;
    g_uhost_data.silent = verbose ? 0 : 1;

    if (g_libxsvf_host.setup) {
        g_libxsvf_host.setup(&g_libxsvf_host);
    }

    g_libxsvf_host.tap_state = LIBXSVF_TAP_INIT;

    return return_code;
}

/**
 * @brief Manages CPLD programming and querying.
 *
 * This function handles JTAG communication with a CPLD. It supports silent
 * usercode reading, programming if usercodes differ, and forced programming.
 *
 * @param data Pointer to the SVF data buffer.
 * @param data_length Length of the data buffer.
 * @param play_mode
 *        - 0: Silently read usercode. No output, no programming.
 *        - 1: Program only if SVF usercode differs from device usercode.
 *        - 2: Force programming regardless of current usercode.
 * @param verbose If non-zero, enables detailed debug output from the libxsvf host.
 *
 * @return The detected CPLD usercode on success, or -1 on failure.
 */
int ps_cpld_load(const uint8_t* data, int data_length, int play_mode, int verbose)
{
    int return_code = 0;
    memset(&g_uhost_data, 0, sizeof(g_uhost_data));

    g_uhost_data.data = data;
    g_uhost_data.data_length = data_length;
    g_uhost_data.verbose = verbose;

    if (play_mode == 0) {
        g_uhost_data.silent = 1;
        g_uhost_data.verbose = 0; // Force off for silent mode
    }

    if (g_libxsvf_host.setup) {
        g_libxsvf_host.setup(&g_libxsvf_host);
    }

    g_libxsvf_host.tap_state = LIBXSVF_TAP_INIT;

    // Step 1: Detect CPLD
    if (return_code == 0) {
        if (!g_uhost_data.silent) {
            screenPrintf("Detecting JTAG devices...\n");
        }
        if (libxsvf_scan(&g_libxsvf_host) < 0) {
            if (!g_uhost_data.silent) {
                screenPrintf("Error during JTAG scan.\n");
            }
            return_code = -1;
        } else if (g_uhost_data.detected_idcode == 0) {
            if (!g_uhost_data.silent) {
                screenPrintf("No supported CPLD found.\n");
            }
            return_code = -1;
        }
    }

    // Step 2: Check usercode and program if necessary
    if (return_code == 0) {
        g_libxsvf_host.tap_state = LIBXSVF_TAP_INIT;
        g_uhost_data.pos = 0;

        if (play_mode == 0) { // Mode 0: Just read usercode
            if (libxsvf_read_usercode(&g_libxsvf_host) < 0) {
                return_code = -1;
            }
        } else { // Mode 1 or 2: Programming logic
            g_uhost_data.svf_usercode = getUsercodeFromSvf(data, data_length);
            int program_device = 0;

            if (play_mode == 2) {
                screenPrintf("Forcing device programming.\n");
                program_device = 1;
            } else { // play_mode == 1
                if (libxsvf_read_usercode(&g_libxsvf_host) < 0) {
                    screenPrintf("Failed to read usercode from JTAG.\n");
                    return_code = -1;
                } else {
                    if (g_uhost_data.svf_usercode != 0 && g_uhost_data.jtag_usercode == g_uhost_data.svf_usercode) {
                        screenPrintf("JTAG usercode matches SVF usercode. Skipping programming.\n");
                    } else {
                        if (g_uhost_data.svf_usercode != 0) {
                            screenPrintf("JTAG usercode (0x%08lx) does not match SVF usercode (0x%08lx). Programming device...\n", g_uhost_data.jtag_usercode, g_uhost_data.svf_usercode);
                        }
                        program_device = 1;
                    }
                }
            }

            if (return_code == 0 && program_device) {
                g_libxsvf_host.tap_state = LIBXSVF_TAP_INIT;
                g_uhost_data.pos = 0;

                if (libxsvf_svf(&g_libxsvf_host) != 0) {
                    screenPrintf("Programming failed.\n");
                    return_code = -1;
                } else {
                    // Step 3: Verify usercode after programming
                    screenPrintf("Programming finished. Verifying usercode...\n");
                    g_libxsvf_host.tap_state = LIBXSVF_TAP_INIT;
                    g_uhost_data.pos = 0;
                    if (libxsvf_read_usercode(&g_libxsvf_host) != 0 || (g_uhost_data.svf_usercode != 0 && g_uhost_data.jtag_usercode != g_uhost_data.svf_usercode)) {
                        screenPrintf("Usercode verification failed. JTAG usercode: 0x%08lx, expected: 0x%08lx\n", g_uhost_data.jtag_usercode, g_uhost_data.svf_usercode);
                        return_code = -1;
                    } else {
                        screenPrintf("Usercode verified successfully.\n");
                    }
                }
            }
        }
    }

    if (return_code != 0 && !g_uhost_data.silent) {
        printFailureMessage();
        ps_cpld_reset_isc(0);
    }

    if (g_uhost_data.verbose) {
        screenPrintf("[libxsvf] finished rc=%d\n", return_code);
        screenPrintf(" Total clock cycles: %d\n", g_uhost_data.clock_count);
        screenPrintf(" Significant TDI bits: %d\n", g_uhost_data.bit_count_tdi);
        screenPrintf(" Significant TDO bits: %d\n", g_uhost_data.bit_count_tdo);
        if (g_uhost_data.return_val_i > 0) {
            screenPrintf(" RMASK bits (%d):", g_uhost_data.return_val_i);
            for (int i = 0; i < g_uhost_data.return_val_i; i++) {
                screenPrintf(" %d", g_uhost_data.return_values[i] ? 1 : 0);
            }
            screenPrintf("\n");
        }
    }

    if (g_libxsvf_host.shutdown) {
        g_libxsvf_host.shutdown(&g_libxsvf_host);
    }

    return (return_code == 0) ? (int)g_uhost_data.jtag_usercode : -1;
}