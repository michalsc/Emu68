#pragma once

#include <cstdint>

#include <emu68/TranslatorContext>
#include <emu68/GPR>

namespace Emu68 {

template<typename ContextTag>
class FPRImpl {
    uint8_t reg;
    bool auto_managed;
    
    static TranslatorContext* context;
    
public:
    // Set context for this specific tag type
    static void setContext(TranslatorContext* tc) {
        context = tc;
    }
    
    static TranslatorContext* getContext() {
        return context;
    }

    explicit FPRImpl(uint8_t r) : reg(r), auto_managed(false) {}
    
    // Auto-allocate from current context
    static FPRImpl allocate() {
        return FPRImpl(context->allocARMRegister(), true);
    }

    FPRImpl() : reg(0xff), auto_managed(false) {}
    
    ~FPRImpl() { 
        if (auto_managed && reg != 0xff && context != nullptr) {
            context->freeARMRegister(reg); 
        }
    }

    FPRImpl(const FPRImpl&) = delete;
    FPRImpl& operator=(const FPRImpl&) = delete;
    
    // Move constructor
    FPRImpl(FPRImpl&& other) noexcept : reg(other.reg), auto_managed(other.auto_managed) {
        other.reg = 0xff;
        other.auto_managed = false;
    }

    FPRImpl& operator=(FPRImpl&& other) noexcept {
        if (this != &other) {
            if (auto_managed && reg != 0xff && context != nullptr) {
                context->freeARMRegister(reg);
            }
            
            reg = other.reg;
            auto_managed = other.auto_managed;
            
            other.reg = 0xff;
            other.auto_managed = false;
        }
        return *this;
    }

    bool isValid() const { return reg != 0xff; }
    bool isAutoManaged() const { return auto_managed; }
    operator uint8_t() const { return reg; }
    uint8_t get() const { return reg; }

private:
    FPRImpl(uint8_t r, bool managed) : reg(r), auto_managed(managed) {}
};

template<typename ContextTag>
TranslatorContext* FPRImpl<ContextTag>::context = nullptr;

namespace M68K {
    using FPR = FPRImpl<M68KTag>;
}

namespace PPC {
    using FPR = FPRImpl<PPCTag>;
}

} // namespace Emu68
