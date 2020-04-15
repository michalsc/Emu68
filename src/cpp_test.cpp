extern "C" {
    #include "support.h"
}
#include <tinystl/vector>
#include <emu68/CodeGenerator.h>
#include "aarch64/CodeGenerator_AArch64.h"

using namespace emu68;

void foo() {
    CodeGenerator<AArch64> cgen((uint16_t *)0x1000);
    cgen.Compile();
}
