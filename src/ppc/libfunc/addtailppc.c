
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_AddTailPPC(struct PPCBase *, struct List *list, struct Node *node)
{
    DFUNC(kprintf("[PPC] powerpc.library/AddTailPPC(%08x, %08x)\n", list, node));

    node->ln_Succ = (struct Node *)&list->lh_Tail;
    node->ln_Pred = list->lh_TailPred;
    list->lh_TailPred->ln_Succ = node;
    list->lh_TailPred = node;
}
