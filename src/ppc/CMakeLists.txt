cmake_minimum_required(VERSION 3.14.0)
project(Emu68-PowerPC-ROM VERSION 0.1.0)
set(PROJECT_VERSION 0.1.0)

find_program(GZIP gzip REQUIRED)
find_program(XXD xxd REQUIRED)

include(../../cmake/verstring.cmake)

set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)

add_executable(ppc_rom
  init.c
  kernel.c
  support.c
)

get_verstring(VERSTRING)

target_link_options(ppc_rom PRIVATE -Wl,--build-id=none -nostdlib -nostartfiles -static -Wl,-Map -Wl,${CMAKE_BINARY_DIR}/ppc_rom.map -T ${LINKER_SCRIPT} ${CMAKE_SOURCE_DIR}/../../scripts/ldscript-ppc.lds)
target_compile_options(ppc_rom PRIVATE -fpack-struct=2 -falign-functions=32 -O2 -mcpu=G3 -msoft-float -fno-asynchronous-unwind-tables -fno-dwarf2-cfi-asm -fno-exceptions -ffreestanding -Wall -Wextra -Werror)
target_include_directories(ppc_rom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR})

add_custom_command(
    TARGET ppc_rom POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary ppc_rom ppc_rom.img
    COMMAND ${XXD} -i ppc_rom.img >ppc_rom.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

configure_file(version.h.in version.h @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/ppc_rom.h DESTINATION .)
