/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _ELFLOADER_H
#define _ELFLOADER_H

void * LoadELFFile(void *mem, void *load_address);
int GetElfSize(void *file, uint32_t *size_rw, uint32_t *size_ro);
uint32_t GetELFMachine();

#endif /* _ELFLOADER_H */
