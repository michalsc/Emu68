
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include <stddef.h>

#include "../support.h"

#define DFUNC(x) x

struct Node * L_RemTailPPC(struct PPCBase *, struct List *list)
{
    DFUNC(kprintf("[PPC] powerpc.library/RemTailPPC(%08x)\n", list));

    if (list->lh_TailPred->ln_Pred == NULL) return NULL;

    struct Node *node = list->lh_TailPred;

    node->ln_Succ->ln_Pred = node->ln_Pred;
    node->ln_Pred->ln_Succ = node->ln_Succ;

    return node;
}
