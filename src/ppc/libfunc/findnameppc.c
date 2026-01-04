
#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

#include <stddef.h>
#include "../support.h"

#define DFUNC(x) x

struct Node * L_FindNamePPC(struct PPCBase *, struct List *list, STRPTR name)
{
    DFUNC(kprintf("[PPC] powerpc.library/FindNamePPC(%08x, '%s')\n", list, name));

    struct Node *next;
    struct Node *node = list->lh_Head;

    while((next = node->ln_Succ))
    {
        if (strcmp(node->ln_Name, name) == 0) {
            return node;
        }
    }

    return NULL;
}
