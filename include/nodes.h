/*
    Copyright � 1995-2011, The AROS Development Team. All rights reserved.
    $Id$
*/

#ifndef EXEC_NODES_H
#define EXEC_NODES_H

#define __mayalias __attribute__((__may_alias__))

struct __mayalias Node;
struct Node
{
    struct Node * ln_Succ,
		* ln_Pred;
};

#endif /* EXEC_NODES_H */
