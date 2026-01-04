
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_EnqueuePPC(struct PPCBase *, struct List *list, struct Node *node)
{
    DFUNC(kprintf("[PPC] powerpc.library/EnqueuePPC(%08x, %08x)\n", list, node));

    LONG pri = node->ln_Pri;
    struct Node *next = list->lh_Head;

    while(next->ln_Succ)
    {
        LONG nodePri = next->ln_Pri;
        if (pri > nodePri) {
            break;
        }
        next = next->ln_Succ;
    }

    struct Node *pred = next->ln_Pred;
    next->ln_Pred = node;
    node->ln_Succ = next;
    node->ln_Pred = pred;
    pred->ln_Succ = node;
}
