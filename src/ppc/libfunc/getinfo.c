#pragma pack(push,2)
#include <exec/types.h>
#include <utility/tagitem.h>
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#pragma pack(pop)

#include "../support.h"
#include "../powerpc.h"

#define DFUNC(x) x

void L_GetInfo(struct PPCBase *PowerPCBase, struct TagItem *tagList)
{
    DFUNC(kprintf("[PPC] powerpc.library/GetInfo(%08x)\n", tagList));

    struct PrivatePPCBase *base = (struct PrivatePPCBase *)PowerPCBase;
    struct TagItem *listPtr = tagList;
    struct TagItem *tagItem;

    while((tagItem = NextTagItemPPC(&listPtr)))
    {
        switch(tagItem->ti_Tag) {
            case GETINFO_CPU:
                tagItem->ti_Data = CPUF_EMU68;
                break;
            case GETINFO_PVR:
                tagItem->ti_Data = base->pp_pvr;
                break;
            case GETINFO_ICACHE:
                tagItem->ti_Data = CACHEF_ON_UNLOCKED;
                break;
            case GETINFO_DCACHE:
                tagItem->ti_Data = CACHEF_ON_UNLOCKED;
                break;
            case GETINFO_PAGETABLE:
                tagItem->ti_Data = -1;  /* No user available MMU */
                break;
            case GETINFO_TABLESIZE:
                tagItem->ti_Data = 0;   /* No user available MMU */
                break;
            case GETINFO_BUSCLOCK:
                asm volatile("mfspr %0, 904":"=r"(tagItem->ti_Data));
                break;
            case GETINFO_CPUCLOCK:
                /* TODO */
                break;
            case GETINFO_CPULOAD:
                /* TODO */
                break;
            case GETINFO_SYSTEMLOAD:
                /* TODO */
                break;
            default:
                DFUNC(kprintf("[PPC]  Don't know what to do with tag %08x\n", tagItem->ti_Tag));
                break;
        }
    }
}
