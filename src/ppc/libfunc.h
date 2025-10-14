#ifndef __LIBFUNC_H
#define __LIBFUNC_H

#pragma pack(push,2)
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <devices/timer.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#pragma pack(pop)

void L_AddHeadPPC(struct PowerPCBase *, struct List *list, struct Node *node);
void L_AddTailPPC(struct PowerPCBase *, struct List *list, struct Node *node);
void L_AddTimePPC(struct PowerPCBase *, struct timeval *dest, struct timeval *source);
LONG L_CmpTimePPC(struct PowerPCBase *, struct timeval *dest, struct timeval *source);
void L_CopyMemPPC(struct PowerPCBase *, void * source, void * dest, ULONG size);
void L_EnqueuePPC(struct PowerPCBase *, struct List *list, struct Node *node);
struct Node * L_FindNamePPC(struct PowerPCBase *, struct List *list, STRPTR name);
void L_NewListdPPC(struct PowerPCBase *, struct List *list);
struct Node * L_RemHeadPPC(struct PowerPCBase *, struct List *list);
struct Node * L_RemTailPPC(struct PowerPCBase *, struct List *list);
void L_RemovePPC(struct PowerPCBase *, struct Node *node);
void L_SubTimePPC(struct PowerPCBase *, struct timeval *dest, struct timeval *source);

#endif /* __LIBFUNC_H */
