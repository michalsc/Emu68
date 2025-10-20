#ifndef __TYPES_H
#define __TYPES_H

typedef unsigned char       UBYTE;
typedef signed char         BYTE;
typedef unsigned short      UWORD;
typedef signed short        WORD;
typedef unsigned int        ULONG;
typedef signed int          LONG;
typedef void *              APTR;
typedef unsigned long       IPTR;
typedef signed long         SIPTR;
typedef char *              STRPTR;
typedef int                 BPTR;

typedef void                VOID;

typedef float               FLOAT;
typedef double              DOUBLE;

#ifndef NULL
#define NULL ((APTR)0)
#endif

#endif /* __TYPES_H */
