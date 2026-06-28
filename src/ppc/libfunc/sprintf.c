#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

void L_SPrintF(struct PPCBase *, STRPTR format_string, APTR args)
{
    (void)format_string;
    (void)args;

}
