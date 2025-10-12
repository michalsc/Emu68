#include <powerpc/powerpc.h>
#include "libstructs.h"
#include "support.h"

void Exception_Entry(struct PPCBase * PowerPCBase, struct iframe *iframe)
{
    (void)PowerPCBase;

    /* Get the vector we are in, recaltulate the fields to match what's expected */
    ULONG ExceptionVector = iframe->if_Context.ec_ExcID & 0xfff0;
    iframe->if_ExcNum = ExceptionVector >> 8;
    iframe->if_Context.ec_ExcID = 1 << iframe->if_ExcNum;

    kprintf("e\n");
}
