/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>

#include <emu68/TranslatorContext>
#include <emu68/List>
#include <emu68/GPR>
#include <emu68/FPR>
#include <emu68/ppc/RegisterNode>
#include <emu68/ppc/PPCLocalState.hpp>

#include "support.h"

namespace Emu68::PPC {

struct RegisterSnapshot : public Node {
    RegisterNode rn[64];
    List<RegisterNode> free_pool;
    List<RegisterNode> gpr_lru;
    List<RegisterNode> fpr_lru;
    int32_t tc_pc_rel;
    uint8_t reg_ctx;
    uint32_t gpr_tmp_pool;
    uint32_t fpr_tmp_pool;
    uint32_t* code_ptr;
};

class PPCTranslatorContext : public TranslatorContext {
    uint32_t* tc_PPCCodeStart;
    uint32_t* tc_PPCCodePtr;

    RegisterNode rn[64];

    int32_t tc_pc_rel;
    uint8_t reg_ctx;
    uint32_t gpr_tmp_pool;
    uint32_t fpr_tmp_pool;

    List<RegisterNode> free_pool;
    List<RegisterNode> gpr_lru;
    List<RegisterNode> fpr_lru;
    List<RegisterSnapshot> snapshots;

    struct FlushItem {
        uint8_t Vn, Size, Pos, ARM;
    } flush_store[20];

    uint8_t intMapFPR(uint8_t reg, int load, int set_dirty);
    uint8_t intMapGPR(uint8_t reg, int load, int set_dirty);

    void markForFlush(uint8_t vn, uint8_t size, uint8_t lane, uint8_t arm) {
        flush_store[lane + (vn - 22) * 4].Vn = vn;
        flush_store[lane + (vn - 22) * 4].Size = size;
        flush_store[lane + (vn - 22) * 4].Pos = lane;
        flush_store[lane + (vn - 22) * 4].ARM = arm;
    }

    void purgeFlushStore();

public:
    PPCTranslatorContext() : TranslatorContext(), tc_pc_rel(0), reg_ctx(0xff) {
        for (int i=0; i < 64; i++) free_pool.addHead(&rn[i]);
    }

    virtual uint8_t allocARMRegister() override;
    virtual uint8_t allocFPRegister() override;
    virtual void freeARMRegister(uint8_t arm_reg) override;
    virtual void freeFPRegister(uint8_t fp_reg) override;

    void freeARMRegister(GPR) = delete;
    void freeARMRegister(FPR) = delete;
    void freeFPRegister(FPR) = delete;
    void freeFPRegister(GPR) = delete;

    int32_t getPCRel() const { return tc_pc_rel; }
    void getOffsetPC(int8_t *offset);
    void advancePC(uint8_t offset);
    uint32_t* getPC() const { return tc_PPCCodePtr; }
    void setPC(uint32_t* pc) { tc_PPCCodePtr = pc; }
    void setPCStart(uint32_t* pc) { tc_PPCCodeStart = pc; }
    void flushPC();
    void resetOffsetPC() { tc_pc_rel = 0; }
    void emitException(uint16_t type);
    void emitLocalExit(uint32_t insn_fixup);
    void storeDirtyGPRs();
    void storeDirtyFPRs();
    GPR tryCTX() const { return GPR(reg_ctx); }
    GPR getCTX();
    void flushCTX();
    uint32_t getTempAllocMask() const { return gpr_tmp_pool; }
    void emitAddImmediate(uint8_t rd, int32_t delta);
    GPR mapGPRForRead(uint8_t reg) { return GPR(intMapGPR(reg, 1, 0)); }
    GPR mapGPRForReadAndWrite(uint8_t reg) { return GPR(intMapGPR(reg, 1, 1)); }
    GPR mapGPRForWrite(uint8_t reg) { return GPR(intMapGPR(reg, 0, 1)); }
    FPR mapFPRForRead(uint8_t reg) { return FPR(intMapFPR(reg, 1, 0)); }
    FPR mapFPRForReadAndWrite(uint8_t reg) { return FPR(intMapFPR(reg, 1, 1)); }
    FPR mapFPRForWrite(uint8_t reg) { return FPR(intMapFPR(reg, 0, 1)); }
    FPR tryGetFPR(uint8_t reg);
    GPR tryGetGPR(uint8_t reg);
    void setDirtyGPR(uint8_t reg);
    void setDirtyFPR(uint8_t reg);
    void flushAllFPRs();
    void flushAllGPRs();
    void putToLocalState(PPCLocalState *ls);

    uint32_t* save();
    uint32_t* restore();
};

inline void PPCTranslatorContext::getOffsetPC(int8_t *offset) {
    // Calculate new PC relative offset
    int new_offset = tc_pc_rel + *offset;

    // If overflow would occur then compute PC and get new offset
    if (new_offset > 127 || new_offset < -127)
    {
        if (tc_pc_rel > 0)
            emit(add_immed(REG_PC, REG_PC, tc_pc_rel));
        else
            emit(sub_immed(REG_PC, REG_PC, -tc_pc_rel));

        tc_pc_rel = 0;
        new_offset = *offset;
    }

    *offset = new_offset;
}

inline void PPCTranslatorContext::advancePC(uint8_t offset)
{
    // Update code pointer
    tc_PPCCodePtr += offset / 4;

    // Calculate new PC relative offset
    tc_pc_rel += (int)offset;

    // If overflow would occur then compute PC and get new offset
    if (_abs(tc_pc_rel) > 120)
    {
        if (tc_pc_rel > 0)
            emit(add_immed(REG_PC, REG_PC, _abs(tc_pc_rel)));
        else
            emit(sub_immed(REG_PC, REG_PC, _abs(tc_pc_rel)));

        tc_pc_rel = 0;
    }
}

inline void PPCTranslatorContext::flushPC()
{
    if (tc_pc_rel > 0)
        emit(add_immed(REG_PC, REG_PC, tc_pc_rel));
    else if (tc_pc_rel < 0)
        emit(sub_immed(REG_PC, REG_PC, -tc_pc_rel));

    tc_pc_rel = 0;
}

} // namespace Emu68::PPC
