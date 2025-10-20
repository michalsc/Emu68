#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../libfunc.h"

ULONG L_GetTagDataPPC(struct PPCBase *base, ULONG tag, ULONG defaultValue, struct TagItem *tagList)
{
    struct TagItem *tagItem = L_FindTagItemPPC(base, tag, tagList);

    if (tagItem != NULL) return tagItem->ti_Data;
    else return defaultValue;
}
