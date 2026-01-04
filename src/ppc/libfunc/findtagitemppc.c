#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

struct TagItem * L_FindTagItemPPC(struct PPCBase *PowerPCBase, ULONG tag, struct TagItem *tagList)
{
    DFUNC(kprintf("[PPC] powerpc.library/FindTagItemPPC(%08x, %08x)\n", tag, tagList));

    struct TagItem *listPtr = tagList;
    struct TagItem *tagItem;

    while((tagItem = NextTagItemPPC(&listPtr)))
    {
        if (tagItem->ti_Data == tag)
            return tagItem;
    }
    
    return NULL;
}
