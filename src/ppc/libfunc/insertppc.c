
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_InsertPPC(struct PPCBase *, struct List *list, struct Node *node, struct Node *pred)
{
    DFUNC(kprintf("[PPC] powerpc.library/InsertPPC(%08x, %08x, %08x)\n", list, node, pred));

    /* If pred != NULL, insert right after pred, otherwise equals to AddHeadPPC() */
    if (pred != NULL) {
        node->ln_Pred = pred;
        node->ln_Succ = pred->ln_Succ;
        pred->ln_Succ->ln_Pred = node; 
        pred->ln_Succ = node;
    }
    else {
        node->ln_Succ = list->lh_Head;
        node->ln_Pred = (struct Node *)&list->lh_Head;
        list->lh_Head->ln_Pred = node;
        list->lh_Head = node;
    }
}
