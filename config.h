/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_H
#define _CONFIG_H

#define EMU68_ARM_CACHE_SIZE    (4*1024*1024)
#define EMU68_M68K_INSN_DEPTH   255
#define EMU68_HOST_BIG_ENDIAN   1
#define EMU68_HAS_SETEND        1

#ifndef VERSION_STRING_DATE
#define VERSION_STRING_DATE ""
#endif

#endif /* _CONFIG_H */
