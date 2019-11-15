#ifndef __FEATURES_H
#define __FEATURES_H

#include "config.h"

typedef struct {
    char    ARM_SUPPORTS_DIV;
    char    ARM_SUPPORTS_BITFLD;
    char    ARM_SUPPORTS_BITCNT;
    char    ARM_SUPPORTS_SWP;
    char    ARM_SUPPORTS_VDIV;
    char    ARM_SUPPORTS_SQRT;
} features_t;

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

#endif /* __FEATURES_H */
