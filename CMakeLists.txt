cmake_minimum_required(VERSION 3.14.0)
project(Emu68 VERSION 0.1.1)

include(cmake/verstring.cmake)
include(cmake/firmware_download.cmake)
get_verstring(VERSTRING)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(SUPPORTED_TARGETS "raspi" "raspi64" "pbpro" "rockpro64")
set(TARGET "raspi64" CACHE STRING "One of target machines: ${SUPPORTED_TARGETS}")
set_property(CACHE TARGET PROPERTY STRINGS ${SUPPORTED_TARGETS})

set(ARCH_FILES "")
set(TARGET_FILES "")
set(BASE_FILES
    src/support.c
    src/tlsf.c
    src/devicetree.c
    src/cpp_support.cpp
    src/EmuLogo.c
    src/HunkLoader.c
    src/ElfLoader.c
)
set(EMU68_FILES
    src/M68k_Translator.c
    src/M68k_SR.c
    src/M68k_MULDIV.c
    src/M68k_MOVE.c
    src/M68k_EA.c
    src/M68k_LINE0.c
    src/M68k_LINE4.c
    src/M68k_LINE5.c
    src/M68k_LINE6.c
    src/M68k_LINE8.c
    src/M68k_LINE9.c
    src/M68k_LINEB.c
    src/M68k_LINEC.c
    src/M68k_LINED.c
    src/M68k_LINEE.c
    src/M68k_LINEF.c
)

if (${TARGET} IN_LIST SUPPORTED_TARGETS)
    message("-- Selected target machine: ${TARGET}")
    if(${TARGET} STREQUAL "raspi64" OR ${TARGET} STREQUAL "raspi")
        if (${TARGET} STREQUAL "raspi64")
            list(APPEND TARGET_FILES src/raspi/start_rpi64.c)
        else()
            list(APPEND TARGET_FILES src/raspi/start_rpi.c)
        endif()
        list(APPEND TARGET_FILES src/raspi/support_rpi.c)
        download_raspi_firmware()
        install(DIRECTORY ${CMAKE_BINARY_DIR}/firmware/ DESTINATION .)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
        install(FILES ${CMAKE_SOURCE_DIR}/scripts/config.txt DESTINATION .)
        add_compile_definitions(RASPI)
    elseif(${TARGET} STREQUAL "pbpro")
        list(APPEND TARGET_FILES
            src/pine64/start_pbpro.c
            src/pine64/support_pbpro.c
        )
        install(FILES ${CMAKE_SOURCE_DIR}/blobs/rk3399-pinebookpro.dtb DESTINATION .)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
        install(FILES ${CMAKE_SOURCE_DIR}/scripts/extlinux.conf DESTINATION extlinux/)
    endif()

    if (${TARGET} STREQUAL "raspi")
        set(TARGET_ARCH "armhf")
    else()
        set(TARGET_ARCH "aarch64")
    endif()
else()
    message(FATAL_ERROR "Wrong target machine specified: ${TARGET}")
endif()

add_link_options(-Wl,--build-id -nostdlib -nostartfiles -static)
add_compile_options(-mbig-endian -fno-exceptions -ffreestanding -pedantic -pedantic-errors -Wall -Wextra -Werror)

if(${TARGET_ARCH} STREQUAL "armhf")
    list(APPEND ARCH_FILES
        src/armhf/start.c
        src/armhf/RegisterAllocator.c
    )
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/ldscript-be.lds)
    add_link_options(-Wl,--be8 -Wl,--format=elf32-bigarm -T ${LINKER_SCRIPT})
    add_compile_options(-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -O3 -ffixed-r11 -fomit-frame-pointer)
else()
    list(APPEND ARCH_FILES
        src/aarch64/start.c
        src/aarch64/mmu.c
        src/aarch64/RegisterAllocator64.c
    )
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/ldscript-be64.lds)
    add_link_options(-Wl,-EB -Wl,--format=elf64-bigaarch64 -T ${LINKER_SCRIPT})
    add_compile_options(-fomit-frame-pointer -O3)
    #-ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26 -ffixed-x27 -ffixed-x28 -ffixed-x29 -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x18)
endif()

add_executable(Emu68.elf
    ${ARCH_FILES}
    ${TARGET_FILES}
    ${BASE_FILES}
    ${EMU68_FILES}
)

add_custom_command(
    TARGET Emu68.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_BINARY_DIR}/Emu68.elf" "${CMAKE_BINARY_DIR}/Emu68.img"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(external/tiny-stl)

target_link_libraries(Emu68.elf tinystl)
target_include_directories(Emu68.elf PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(Emu68.elf PRIVATE VERSION_STRING="${VERSTRING}")
