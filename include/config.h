/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_H
#define _CONFIG_H

#define ARM_FEATURE_HAS_DIV     1
#define ARM_FEATURE_HAS_BITFLD  1
#define ARM_FEATURE_HAS_BITCNT  1
#define ARM_FEATURE_HAS_SWP     1
#define ARM_FEATURE_HAS_VDIV    1
#define ARM_FEATURE_HAS_SQRT    1

#ifndef SET_FEATURES_AT_RUNTIME
#define SET_FEATURES_AT_RUNTIME 1
#endif

#ifndef SET_OPTIONS_AT_RUNTIME
#define SET_OPTIONS_AT_RUNTIME 0
#endif

#define EMU68_ARM_CACHE_SIZE    (4*1024*1024)
#define EMU68_M68K_INSN_DEPTH   255
#define EMU68_HOST_BIG_ENDIAN   1
#define EMU68_HAS_SETEND        1

#ifndef VERSION_STRING_DATE
#define VERSION_STRING_DATE ""
#endif

#define KERNEL_SYS_PAGES        2
#define KERNEL_JIT_PAGES        16
#define KERNEL_RSRVD_PAGES      ((KERNEL_JIT_PAGES) + (KERNEL_SYS_PAGES))

#endif /* _CONFIG_H */
