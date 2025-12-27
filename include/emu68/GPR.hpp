#pragma once

#include <cstdint>

#include <emu68/TranslatorContext>

namespace Emu68 {

struct M68KTag {};
struct PPCTag {};

template<typename ContextTag>
class GPRImpl {
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

    explicit GPRImpl(uint8_t r) : reg(r), auto_managed(false) {}
    
    // Auto-allocate from current context
    static GPRImpl allocate() {
        return GPRImpl(context->allocARMRegister(), true);
    }

    GPRImpl() : reg(0xff), auto_managed(false) {}
    
    ~GPRImpl() { 
        if (auto_managed && reg != 0xff && context != nullptr) {
            context->freeARMRegister(reg); 
        }
    }

    GPRImpl(const GPRImpl&) = delete;
    GPRImpl& operator=(const GPRImpl&) = delete;
    
    // Move constructor
    GPRImpl(GPRImpl&& other) noexcept : reg(other.reg), auto_managed(other.auto_managed) {
        other.reg = 0xff;
        other.auto_managed = false;
    }

    GPRImpl& operator=(GPRImpl&& other) noexcept {
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
    GPRImpl(uint8_t r, bool managed) : reg(r), auto_managed(managed) {}
};

template<typename ContextTag>
TranslatorContext* GPRImpl<ContextTag>::context = nullptr;

namespace M68K {
    using GPR = GPRImpl<M68KTag>;
}

namespace PPC {
    using GPR = GPRImpl<PPCTag>;
}

} // namespace Emu68
