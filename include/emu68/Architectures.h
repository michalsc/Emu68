/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU64_ARCHITECTURES_H
#define _EMU64_ARCHITECTURES_H

namespace emu68 {

struct AArch64 { static const int RegEnd = 11; static const int RegStart = 0; static const int FPURegEnd = 7; static const int FPURegStart = 1; };
struct ARM { static const int RegEnd = 7; static const int RegStart = 0; static const int FPURegEnd = 7; static const int FPURegStart = 1; };
struct Thumb2 { static const int RegEnd = 7; static const int RegStart = 0; static const int FPURegEnd = 7; static const int FPURegStart = 1; };

}

#endif /* _EMU64_ARCHITECTURES_H */
