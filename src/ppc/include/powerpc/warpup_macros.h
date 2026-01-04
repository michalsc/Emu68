#ifndef _POWERPC_WARPUP_MACROS_H
#define _POWERPC_WARPUP_MACROS_H

#define __ARGUMENT_ERROR(caller,arg,is) do { extern int caller##_argument_##arg##_uses_R##is; caller##_argument_##arg##_uses_R##is = 1; } while(0)

/*
    This file is for internal use only. First argument is forced as APTR and not checked
    for correct type (this is the library base) 
*/

#define PPCLP1(baseName,lvo,returnType,arg1Type,arg1Reg,arg1)                           \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP1,1,arg1Reg);                            \
        returnType (*__lvo_func)(APTR) = (returnType (*)(APTR))(*__funcPtr);    \
        __lvo_func(__a1);                                                               \
    })

#define PPCLP1NR(baseName,lvo,arg1Type,arg1Reg,arg1)                                    \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP1NR,1,arg1Reg);                          \
        void (*__lvo_func)(APTR) = (void (*)(APTR))(*__funcPtr);                \
        __lvo_func(__a1);                                                               \
    })

#define PPCLP2(baseName,lvo,returnType,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2)     \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP2,1,arg1Reg);                            \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP2,2,arg2Reg);                            \
        returnType (*__lvo_func)(APTR,arg2Type) = (returnType (*)(APTR,arg2Type))*__funcPtr; \
        __lvo_func(__a1,__a2);                                                          \
    })

#define PPCLP2NR(baseName,lvo,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2)              \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP2NR,1,arg1Reg);                          \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP2NR,2,arg2Reg);                          \
        void (*__lvo_func)(APTR,arg2Type) = (void (*)(APTR,arg2Type))*__funcPtr; \
        __lvo_func(__a1,__a2);                                                          \
    })

#define PPCLP3(baseName,lvo,returnType,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2,arg3Type,arg3Reg,arg3) \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        arg3Type __a3 = (arg3);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP3,1,arg1Reg);                            \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP3,2,arg2Reg);                            \
        if((arg3Reg)!=5) __ARGUMENT_ERROR(PPCLP3,3,arg3Reg);                            \
        returnType (*__lvo_func)(APTR,arg2Type,arg3Type) =                          \
            (returnType (*)(APTR,arg2Type,arg3Type))*__funcPtr;                     \
        __lvo_func(__a1,__a2,__a3);                                                     \
    })

#define PPCLP3NR(baseName,lvo,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2,arg3Type,arg3Reg,arg3) \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        arg3Type __a3 = (arg3);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP3NR,1,arg1Reg);                          \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP3NR,2,arg2Reg);                          \
        if((arg3Reg)!=5) __ARGUMENT_ERROR(PPCLP3NR,3,arg3Reg);                          \
        void (*__lvo_func)(APTR,arg2Type,arg3Type) =                                \
            (void (*)(APTR,arg2Type,arg3Type))*__funcPtr;                           \
        __lvo_func(__a1,__a2,__a3);                                                     \
    })

#define PPCLP4(baseName,lvo,returnType,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2,arg3Type,arg3Reg,arg3,arg4Type,arg4Reg,arg4) \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        arg3Type __a3 = (arg3);                                                         \
        arg4Type __a4 = (arg4);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP4,1,arg1Reg);                            \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP4,2,arg2Reg);                            \
        if((arg3Reg)!=5) __ARGUMENT_ERROR(PPCLP4,3,arg3Reg);                            \
        if((arg4Reg)!=6) __ARGUMENT_ERROR(PPCLP4,4,arg4Reg);                            \
        returnType (*__lvo_func)(APTR,arg2Type,arg3Type,arg4Type) =                 \
                (returnType (*)(APTR,arg2Type,arg3Type,arg4Type))*__funcPtr;        \
        __lvo_func(__a1,__a2,__a3,__a4);                                                \
    })

#define PPCLP4NR(baseName,lvo,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2,arg3Type,arg3Reg,arg3,arg4Type,arg4Reg,arg4) \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        arg3Type __a3 = (arg3);                                                         \
        arg4Type __a4 = (arg4);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP4NR,1,arg1Reg);                          \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP4NR,2,arg2Reg);                          \
        if((arg3Reg)!=5) __ARGUMENT_ERROR(PPCLP4NR,3,arg3Reg);                          \
        if((arg4Reg)!=6) __ARGUMENT_ERROR(PPCLP4NR,4,arg4Reg);                          \
        void (*__lvo_func)(APTR,arg2Type,arg3Type,arg4Type) =                       \
            (void (*)(APTR,arg2Type,arg3Type,arg4Type))*__funcPtr;                  \
        __lvo_func(__a1,__a2,__a3,__a4);                                                \
    })


#define PPCLP5(baseName,lvo,returnType,arg1Type,arg1Reg,arg1,arg2Type,arg2Reg,arg2,arg3Type,arg3Reg,arg3,arg4Type,arg4Reg,arg4,arg5Type,arg5Reg,arg5) \
    ({                                                                                  \
        APTR __a1 = (arg1);                                                         \
        arg2Type __a2 = (arg2);                                                         \
        arg3Type __a3 = (arg3);                                                         \
        arg4Type __a4 = (arg4);                                                         \
        arg5Type __a5 = (arg5);                                                         \
        APTR * __funcPtr = (APTR*)(2+((ULONG)baseName)+lvo);                            \
        if((arg1Reg)!=3) __ARGUMENT_ERROR(PPCLP5,1,arg1Reg);                            \
        if((arg2Reg)!=4) __ARGUMENT_ERROR(PPCLP5,2,arg2Reg);                            \
        if((arg3Reg)!=5) __ARGUMENT_ERROR(PPCLP5,3,arg3Reg);                            \
        if((arg4Reg)!=6) __ARGUMENT_ERROR(PPCLP5,4,arg4Reg);                            \
        if((arg5Reg)!=7) __ARGUMENT_ERROR(PPCLP5,5,arg5Reg);                            \
        returnType (*__lvo_func)(APTR,arg2Type,arg3Type,arg4Type,arg5Type) =        \
                (returnType (*)(APTR,arg2Type,arg3Type,arg4Type,arg5Type))*__funcPtr; \
        __lvo_func(__a1,__a2,__a3,__a4,__a5);                                           \
    })

#endif /* _POWERPC_WARPUP_MACROS_H */