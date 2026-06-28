#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#pragma pack(pop)

#include "../support.h"
#include "../powerpc.h"

#define DFUNC(x) x

void L_GetHALInfo(struct PPCBase *PowerPCBase, struct TagItem *tagList)
{
    DFUNC(kprintf("[PPC] powerpc.library/GetHALInfo(%08x)\n", tagList));

    struct TagItem *listPtr = tagList;
    struct TagItem *tagItem;

    while((tagItem = NextTagItemPPC(&listPtr)))
    {
        switch(tagItem->ti_Tag) {
            case HINFO_ALEXC_LOW: // fallthrough
            case HINFO_ALEXC_HIGH:
                tagItem->ti_Data = 0;
                break;
            default:
                DFUNC(kprintf("[PPC]  Don't know what to do with tag %08x\n", tagItem->ti_Tag));
                break;
        }
    }
}
