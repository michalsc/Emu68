#ifndef EXEC_LISTS_H
#define EXEC_LISTS_H

/*
    Copyright � 1995-2017, The AROS Development Team. All rights reserved.
    $Id$

    Structures and macros for exec lists.
*/

#ifndef NULL
#define NULL ((void*)0)
#endif

/**************************************
		Includes
**************************************/
#ifndef EXEC_NODES_H
#   include "nodes.h"
#endif


/**************************************
	       Structures
**************************************/
/* Normal list */
struct List
{
    struct Node lh_Head;
    struct Node lh_Tail;
};

static inline struct Node * REMOVE(struct Node *n)
{
    n->ln_Prev->ln_Next = n->ln_Next;
    n->ln_Next->ln_Prev = n->ln_Prev;

    return n;
}

static inline struct Node * ADDHEAD(struct List *l, struct Node *n)
{
    if (n == NULL) return NULL;
    if (l == NULL) return NULL;
    
    n->ln_Prev = &l->lh_Head;
    l->lh_Head.ln_Next->ln_Prev = n;

    n->ln_Next = l->lh_Head.ln_Next;
    l->lh_Head.ln_Next = n;

    return n;
}

static inline struct Node * ADDTAIL(struct List *l, struct Node *n)
{
    if (n == NULL) return NULL;
    if (l == NULL) return NULL;

    n->ln_Next = &l->lh_Tail;
    n->ln_Prev = l->lh_Tail.ln_Prev;

    l->lh_Tail.ln_Prev->ln_Next = n;
    l->lh_Tail.ln_Prev = n;

    return n;
}

static inline struct Node * GETHEAD(struct List *l)
{
    if (l->lh_Head.ln_Next == &l->lh_Tail)
        return NULL;
    else
        return l->lh_Head.ln_Next;
}

static inline struct Node * GETTAIL(struct List *l)
{
    if (l->lh_Tail.ln_Prev == &l->lh_Head)
        return NULL;
    else
        return l->lh_Tail.ln_Prev;
}

static inline struct Node * REMHEAD(struct List *l)
{
    struct Node *n = GETHEAD(l);
    
    if (n != NULL) REMOVE(n);

    return n;
}

static inline struct Node * REMTAIL(struct List *l)
{
    struct Node *n = GETTAIL(l);
    
    if (n != NULL) REMOVE(n);

    return n;
}

static inline void NEWLIST(struct List *l)
{
    if (l == NULL) return;

    l->lh_Head.ln_Next = &l->lh_Tail;
    l->lh_Head.ln_Prev = NULL;
        
    l->lh_Tail.ln_Prev = &l->lh_Head;
    l->lh_Tail.ln_Next = NULL;
}

#define ForeachNode(list, node)                                 \
for                                                             \
(                                                               \
    node = (&((struct List *)(list))->lh_Head);   \
    ((struct Node *)(node))->ln_Next;                           \
    node = (((struct Node *)(node))->ln_Next)     \
)

#define ForeachNodeSafe(list, current, next)                    \
for                                                             \
(                                                               \
    node = (typeof(node))(&((struct List *)(list))->lh_Head);   \
    (next = (typeof(next))((struct Node *)(current))->ln_Next); \
    current = (typeof(current))next                             \
)

#if 0
/**************************************
	       Macros
**************************************/
#define IsListEmpty(l) \
	(((struct Node *)((struct List *)(l))->lh_TailPred) == (struct Node *)(l))

#define IsMinListEmpty(l) \
	( (((struct MinList *)l)->mlh_TailPred) == (struct MinNode *)(l) )

#define IsMsgPortEmpty(mp) \
      ( (((struct MsgPort *)(mp))->mp_MsgList.lh_TailPred) \
	    == (struct Node *)(&(((struct MsgPort *)(mp))->mp_MsgList)) )

#ifndef __GNUC__
#define NEWLIST(_l)                                     \
do                                                      \
{                                                       \
    struct List *__aros_list_tmp = (struct List *)(_l), \
                *l = __aros_list_tmp;                   \
                                                        \
    l->lh_TailPred = (struct Node *)l;                  \
    l->lh_Tail     = 0;                                 \
    l->lh_Head     = (struct Node *)&l->lh_Tail;        \
} while (0)
#else /* __GNUC__ */
#define NEWLIST(_l)                                     \
do                                                      \
{                                                       \
    struct List *__aros_list_tmp = (struct List *)(_l), \
                *l = __aros_list_tmp;                   \
                                                        \
    l->lh_TailPred_= l;                                 \
    l->lh_Tail     = 0;                                 \
    l->lh_Head     = (struct Node *)&l->lh_Tail;        \
} while (0)
#endif /* __GNUC__ */

#define ADDHEAD(_l,_n)                                  \
do                                                      \
{                                                       \
    struct Node *__aros_node_tmp = (struct Node *)(_n), \
                *n = __aros_node_tmp;                   \
    struct List *__aros_list_tmp = (struct List *)(_l), \
                *l = __aros_list_tmp;                   \
                                                        \
    n->ln_Succ          = l->lh_Head;                   \
    n->ln_Pred          = (struct Node *)&l->lh_Head;   \
    l->lh_Head->ln_Pred = n;                            \
    l->lh_Head          = n;                            \
} while (0)

#define ADDTAIL(_l,_n)                                    \
do                                                        \
{                                                         \
    struct Node *__aros_node_tmp = (struct Node *)(_n),   \
                *n = __aros_node_tmp;                     \
    struct List *__aros_list_tmp = (struct List *)(_l),   \
                *l = __aros_list_tmp;                     \
                                                          \
    n->ln_Succ              = (struct Node *)&l->lh_Tail; \
    n->ln_Pred              = l->lh_TailPred;             \
    l->lh_TailPred->ln_Succ = n;                          \
    l->lh_TailPred          = n;                          \
} while (0)



static inline struct Node * REMTAIL(struct List *l)
{
    return (l->lh_TailPred->ln_Pred) ? REMOVE(l->lh_TailPred) : (struct Node *)0;
}
#define GetHead(_l)                                     \
({                                                      \
    struct List *__aros_list_tmp = (struct List *)(_l), \
                *l = __aros_list_tmp;                   \
                                                        \
   l->lh_Head->ln_Succ ? l->lh_Head : (struct Node *)0; \
})

#define GetTail(_l)                                              \
({                                                               \
    struct List *__aros_list_tmp = (struct List *)(_l),          \
                *l = __aros_list_tmp;                            \
                                                                 \
    l->lh_TailPred->ln_Pred ? l->lh_TailPred : (struct Node *)0; \
})

#define GetSucc(_n)                                      \
({                                                       \
    struct Node *__aros_node_tmp = (struct Node *)(_n),  \
                *n = __aros_node_tmp;                    \
                                                         \
    (n && n->ln_Succ && n->ln_Succ->ln_Succ) ? n->ln_Succ : (struct Node *)0; \
})

#define GetPred(_n)                                      \
({                                                       \
    struct Node *__aros_node_tmp = (struct Node *)(_n),  \
                *n = __aros_node_tmp;                    \
                                                         \
    (n && n->ln_Pred && n->ln_Pred->ln_Pred) ? n->ln_Pred : (struct Node *)0; \
})

static inline struct Node * REMHEAD(struct List *l)
{
    return l->lh_Head->ln_Succ ? REMOVE(l->lh_Head) : (struct Node *)0;
}

#define ForeachNode(list, node)                        \
for                                                    \
(                                                      \
    node = (typeof(node))(((struct List *)(list))->lh_Head); \
    ((struct Node *)(node))->ln_Succ;                  \
    node = (typeof(node))(((struct Node *)(node))->ln_Succ)  \
)

#define ForeachNodeSafe(list, current, next)              \
for                                                       \
(                                                         \
    current = (typeof(current))(((struct List *)(list))->lh_Head); \
    (next = (typeof(next))((struct Node *)(current))->ln_Succ); \
    current = (typeof(current))next                                \
)

#define ListLength(list,count)     \
do {		                   \
    struct Node * __n;	           \
    count = 0;		           \
    ForeachNode (list,__n) count ++; \
} while (0)
#endif

#endif /* EXEC_LISTS_H */
