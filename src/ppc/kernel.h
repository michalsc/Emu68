#ifndef _KERNEL_H
#define _KERNEL_H

#include "powerpc.h"

void PatchLVOTable(struct PPCBase *ppcbase);
void SendPacketMessage(struct PrivatePPCBase * PPCBase, APTR message);
APTR StartRecievingMessage(struct PrivatePPCBase * PPCBase);
void EndReceivingMessage(struct PrivatePPCBase * PPCBase);

#endif /* _KERNEL_H */
