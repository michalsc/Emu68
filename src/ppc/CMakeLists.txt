cmake_minimum_required(VERSION 3.14.0)
project(ppc_rom VERSION 0.1.0)
set(PROJECT_VERSION 0.1.0)

find_program(GZIP gzip REQUIRED)
find_program(XXD xxd REQUIRED)

set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)

add_executable(ppc_rom
  init.c
)

target_link_options(ppc_rom PRIVATE -Wl,--build-id=none -nostdlib -nostartfiles -static -Wl,-Map -Wl,${CMAKE_BINARY_DIR}/ppc_rom.map -T ${LINKER_SCRIPT} ${CMAKE_SOURCE_DIR}/../../scripts/ldscript-ppc.lds)
target_compile_options(ppc_rom PRIVATE -falign-functions=32 -O2 -mcpu=G3 -msoft-float -fno-asynchronous-unwind-tables -fno-dwarf2-cfi-asm -fno-exceptions -ffreestanding -Wall -Wextra -Werror)

add_custom_command(
    TARGET ppc_rom POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary ppc_rom ppc_rom.img
    COMMAND ${XXD} -i ppc_rom.img >ppc_rom.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_BINARY_DIR}/ppc_rom.h DESTINATION .)
