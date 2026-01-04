
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_AddHeadPPC(struct PPCBase *, struct List *list, struct Node *node)
{
    DFUNC(kprintf("[PPC] powerpc.library/AddHeadPPC(%08x, %08x)\n", list, node));

    node->ln_Succ = list->lh_Head;
    node->ln_Pred = (struct Node *)&list->lh_Head;
    list->lh_Head->ln_Pred = node;
    list->lh_Head = node;
}
