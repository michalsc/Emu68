/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <support.h>

const char * find_token(const char * string, const char * token)
{
    const char * ret = NULL;

    if (string)
    {
        do {
            while (*string == ' ' || *string == '\t') {
                string++;
            }

            if (*string == 0)
                break;

            for (int i=0; token[i] != 0; i++)
            {
                if (string[i] != token[i])
                {
                    break;
                }

                if (token[i] == '=') {
                    ret = string;
                    break;
                }

                if (string[i+1] == 0 || string[i+1] == ' ' || string[i+1] == '\t') {
                    ret = string;
                    break;
                }
            }

            if (ret)
                break;

            while(*string != 0) {
                if (*string != ' ' && *string != '\t')
                    string++;
                else break;
            }

        } while(!ret && *string != 0);
    }
    return ret;
}
