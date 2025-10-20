#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../libfunc.h"

struct TagItem * L_FindTagItemPPC(struct PPCBase *base, ULONG tag, struct TagItem *tagList)
{
    struct TagItem *listPtr = tagList;
    struct TagItem *tagItem;

    while((tagItem = L_NextTagItemPPC(base, &listPtr)))
    {
        if (tagItem->ti_Data == tag)
            return tagItem;
    }
    
    return NULL;
}
