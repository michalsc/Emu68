#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#pragma pack(pop)

#include "../libfunc.h"

ULONG L_GetTagDataPPC(struct PPCBase *PowerPCBase, ULONG tag, ULONG defaultValue, struct TagItem *tagList)
{
    struct TagItem *tagItem = FindTagItemPPC(tag, tagList);

    if (tagItem != NULL) return tagItem->ti_Data;
    else return defaultValue;
}
