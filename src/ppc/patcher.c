#include <exec/types.h>
#include <powerpc/warpup_macros.h>

#include "libfunc.h"
#include "support.h"

#define D(x) /* x */

const struct {
    WORD lvo;
    APTR function;
} patch[] = {
    { -408, L_AddHeadPPC },
    { -414, L_AddTailPPC },
    { -420, L_RemovePPC },
    { -426, L_RemHeadPPC },
    { -432, L_RemTailPPC },
    { -432, L_EnqueuePPC },
    { -444, L_FindNamePPC },
    { -450, L_FindTagItemPPC },
    { -456, L_GetTagDataPPC },
    { -462, L_NextTagItemPPC },
    { -588, L_ChangeMMU },
    { -660, L_CopyMemPPC },
    { -690, L_AddTimePPC },
    { -696, L_SubTimePPC },
    { -702, L_CmpTimePPC },
    { -774, L_NewListdPPC },

    { 0, NULL }
};

void PatchLVOTable(struct PPCBase *ppcbase)
{
    ULONG lvo = (ULONG)ppcbase;
    ULONG pos;

    D(kprintf("[PPC] Patching PowerPC Library base\n"));

    for (pos = 0; patch[pos].lvo; pos++) {
        *(APTR *)(lvo + 6 * patch[pos].lvo + 2) = patch[pos].function;
    }

    D(kprintf("[PPC] Patched %d functions\n", pos));
}
