#include <stdio.h>

#include "RegisterAllocator.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Test\n");

    for (int i=0; i < 16; i++)
    {
        if (i == 8)
            RA_UpdateSlot(0);
        
        if (i == 12)
            RA_UpdateSlot(0);
            
        printf("%02d: %d\n", i, RA_SelectNewSlot());
    }

    return 0;
}
