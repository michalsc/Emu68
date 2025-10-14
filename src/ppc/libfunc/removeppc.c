#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

void L_RemovePPC(struct PowerPCBase *, struct Node *node)
{
    node->ln_Succ->ln_Pred = node->ln_Pred;
    node->ln_Pred->ln_Succ = node->ln_Succ;
}
