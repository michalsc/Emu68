#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include "../support.h"

#define DFUNC(x) x

void L_RemovePPC(struct PPCBase *, struct Node *node)
{
    DFUNC(kprintf("[PPC] powerpc.library/RemHeadPPC(%08x)\n", node));

    node->ln_Succ->ln_Pred = node->ln_Pred;
    node->ln_Pred->ln_Succ = node->ln_Succ;
}
