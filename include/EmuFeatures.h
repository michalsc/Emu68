#ifndef _EMU_FEATURES_H
#define _EMU_FEATURES_H

#include <stdint.h>
#include "config.h"

typedef struct {
    uint8_t ARM_SUPPORTS_DIV;
    uint8_t ARM_SUPPORTS_BITFLD;
    uint8_t ARM_SUPPORTS_BITCNT;
    uint8_t ARM_SUPPORTS_SWP;
    uint8_t ARM_SUPPORTS_VDIV;
    uint8_t ARM_SUPPORTS_SQRT;
} features_t;

typedef struct {
    uint16_t M68K_TRANSLATION_DEPTH;
    uint8_t M68K_ALLOW_UNALIGNED_FPU;
} options_t;

#if SET_FEATURES_AT_RUNTIME

extern features_t Features;

#else

static const features_t Features = {
    ARM_FEATURE_HAS_DIV,
    ARM_FEATURE_HAS_BITFLD,
    ARM_FEATURE_HAS_BITCNT,
    ARM_FEATURE_HAS_SWP,
    ARM_FEATURE_HAS_VDIV,
    ARM_FEATURE_HAS_SQRT,
};

#endif

#if SET_OPTIONS_AT_RUNTIME

extern options_t Options;

#else

static const options_t Options = {
    EMU68_M68K_INSN_DEPTH,
    1,
};

#endif


#endif /* _EMU_FEATURES_H */
