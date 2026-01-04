#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

struct TagItem * L_NextTagItemPPC(struct PPCBase *, struct TagItem **tagList)
{
    DFUNC(kprintf("[PPC] powerpc.library/NextTagItemPPC(%08x)\n", tagList));
    
    if (*tagList == NULL) return NULL;

    while(1) {
        /* Handle special Tags or return current TagItem and advance pointer to the TagList */
        switch ((*tagList)->ti_Tag)
        {
            case TAG_MORE:
                (*tagList) = (struct TagItem *)((*tagList)->ti_Data);
                if ((*tagList) == NULL) return NULL;
                continue;
            
            case TAG_SKIP:
                (*tagList) += (*tagList)->ti_Data + 1;
                continue;

            case TAG_IGNORE:
                break;

            case TAG_END:
                (*tagList) = NULL;
                return NULL;

            default:
                return (*tagList)++;
        }

        (*tagList)++;
    }
}
