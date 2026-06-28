
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_NewListdPPC(struct PPCBase *, struct List *list)
{
    DFUNC(kprintf("[PPC] powerpc.library/NewListPPC(%08x)\n", list));

    list->lh_TailPred_ = list;
    list->lh_Tail = 0;
    list->lh_Head = (struct Node *)&list->lh_Tail;
}
