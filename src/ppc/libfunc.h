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

void L_SPrintF(struct PPCBase *, STRPTR format_string, APTR args);
void L_AddHeadPPC(struct PPCBase *, struct List *list, struct Node *node);
void L_AddTailPPC(struct PPCBase *, struct List *list, struct Node *node);
void L_AddTimePPC(struct PPCBase *, struct timeval *dest, struct timeval *source);
void L_ChangeMMU(struct PPCBase *, ULONG);
LONG L_CmpTimePPC(struct PPCBase *, struct timeval *dest, struct timeval *source);
void L_CopyMemPPC(struct PPCBase *, void * source, void * dest, ULONG size);
void L_EnqueuePPC(struct PPCBase *, struct List *list, struct Node *node);
struct Node * L_FindNamePPC(struct PPCBase *, struct List *list, STRPTR name);
struct TagItem * L_FindTagItemPPC(struct PPCBase *base, ULONG tag, struct TagItem *tagList);
ULONG L_GetTagDataPPC(struct PPCBase *base, ULONG tag, ULONG defaultValue, struct TagItem *tagList);
void L_NewListdPPC(struct PPCBase *, struct List *list);
struct TagItem * L_NextTagItemPPC(struct PPCBase *, struct TagItem **tagList);
struct Node * L_RemHeadPPC(struct PPCBase *, struct List *list);
struct Node * L_RemTailPPC(struct PPCBase *, struct List *list);
void L_RemovePPC(struct PPCBase *, struct Node *node);
void L_SubTimePPC(struct PPCBase *, struct timeval *dest, struct timeval *source);
void L_InsertPPC(struct PPCBase *, struct List *list, struct Node *node, struct Node *pred);
void L_GetInfo(struct PPCBase *PowerPCBase, struct TagItem *tagList);
void L_GetHALInfo(struct PPCBase *PowerPCBase, struct TagItem *tagList);
ULONG L_Super(struct PPCBase *);
void L_User(struct PPCBase *, ULONG key);
void L_CauseInterrupt(struct PPCBase *PowerPCBase);

#endif /* __LIBFUNC_H */
