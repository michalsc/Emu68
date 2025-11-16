/*
    Copyright � 1995-2011, The AROS Development Team. All rights reserved.
    $Id$
*/

#ifndef EXEC_NODES_H
#define EXEC_NODES_H

struct Node
{
    struct Node * ln_Next;
    struct Node * ln_Prev;
};

#endif /* EXEC_NODES_H */
