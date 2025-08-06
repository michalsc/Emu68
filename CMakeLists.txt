cmake_minimum_required(VERSION 3.14.0)
project(Emu68 VERSION 1.0.6)
set(PROJECT_VERSION 1.0.6)

include(cmake/verstring.cmake)
include(cmake/firmware_download.cmake)
get_verstring(VERSTRING)
get_git_sha(GIT_SHA)

set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(SUPPORTED_TARGETS "raspi64" "pbpro" "rockpro64" "virt")
set(TARGET "raspi64" CACHE STRING "One of target machines: ${SUPPORTED_TARGETS}")
set_property(CACHE TARGET PROPERTY STRINGS ${SUPPORTED_TARGETS})

set(SUPPORTED_VARIANTS "none" "pistorm" "pistorm32lite")
set(VARIANT "none" CACHE STRING "Compilation variant: ${SUPPORTED_VARIANTS}")
set_property(CACHE VARIANT PROPERTY STRINGS ${SUPPORTED_VARIANTS})

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
    src/md5.c
    src/disasm.c
    src/findtoken.c
    src/cache.c
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
    src/M68k_Exception.c
    src/M68k_CC.c
    src/ExecutionLoop.c
    
    src/math/__rem_pio2.c
    src/math/__rem_pio2_large.c
    src/math/__tan.c
    src/math/__sin.c
    src/math/__cos.c
    src/math/__expo2.c
    src/math/atan.c
    src/math/atanh.c
    src/math/acos.c
    src/math/asin.c
    src/math/tan.c
    src/math/tanh.c
    src/math/scalbn.c
    src/math/floor.c
    src/math/cosh.c
    src/math/exp.c
    src/math/exp10.c
    src/math/exp2.c
    src/math/expm1.c
    src/math/log.c
    src/math/log1p.c
    src/math/log10.c
    src/math/log2.c
    src/math/sinh.c
    src/math/pow.c
    src/math/modf.c
    src/math/sincos.c
    src/math/sin.c
    src/math/cos.c
    src/math/remquo.c
    src/math/96bit.c
)

set_source_files_properties(src/math/96bit.c PROPERTIES COMPILE_FLAGS 
    "-fcall-saved-x4 -fcall-saved-x5 -fcall-saved-x6 -fcall-saved-x7 -fcall-saved-x8 \
    -fcall-saved-x9 -fcall-saved-x10 -fcall-saved-x11 -fcall-saved-x12 -fcall-saved-x13 \
    -fcall-saved-x14 -fcall-saved-x15 -fcall-saved-x16 -fcall-saved-x17 -fcall-saved-x18")

# ExecutionLoop breaks the C ABI for aarch64. All m68k registers are fixed (x13-x29)
# The temporary arm code and PC are fixed too (x12, x18)
set_source_files_properties(src/ExecutionLoop.c PROPERTIES COMPILE_FLAGS 
    "-fcall-used-x4 -fcall-used-x5 -fcall-used-x6 -fcall-used-x7 -fcall-used-x8 \
     -fcall-used-x9 -fcall-used-x10 -fcall-used-x11 -ffixed-x12 -ffixed-x18  \
     -ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26  \
     -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x27 -ffixed-x28 -ffixed-x29")

if (${TARGET} IN_LIST SUPPORTED_TARGETS)
    message("-- Selected target machine: ${TARGET}")
    if(${TARGET} STREQUAL "raspi64")
        list(APPEND TARGET_FILES src/raspi/start_rpi64.c)
        list(APPEND TARGET_FILES src/raspi/support_rpi.c)
        list(APPEND TARGET_FILES src/raspi/topaz.c)
        download_raspi_firmware()
        if(${VARIANT} STREQUAL "pistorm" OR ${VARIANT} STREQUAL "pistorm32lite")
            configure_file(scripts/config_pistorm.txt scripts/config.txt @ONLY)
            install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION . RENAME Emu68-${VARIANT})
            install(FILES ${CMAKE_BINARY_DIR}/scripts/config.txt DESTINATION .)
        else()
            install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
            install(FILES ${CMAKE_SOURCE_DIR}/scripts/config.txt DESTINATION .)
        endif()
        add_compile_definitions(RASPI)
    elseif(${TARGET} STREQUAL "pbpro")
        list(APPEND TARGET_FILES
            src/pine64/start_pbpro.c
            src/pine64/support_pbpro.c
        )
        install(FILES ${CMAKE_SOURCE_DIR}/blobs/rk3399-pinebookpro.dtb DESTINATION .)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
        install(FILES ${CMAKE_SOURCE_DIR}/scripts/extlinux.conf DESTINATION extlinux/)
    elseif(${TARGET} STREQUAL "virt")
        list(APPEND TARGET_FILES
            src/virt/start_virt.c
            src/virt/support_virt.c
        )
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
    endif()

    set(TARGET_ARCH "aarch64")
else()
    message(FATAL_ERROR "Wrong target machine specified: ${TARGET}")
endif()

if (${VARIANT} IN_LIST SUPPORTED_VARIANTS)
    message("-- Selected variant: ${VARIANT}")
    if(${VARIANT} STREQUAL "pistorm" OR ${VARIANT} STREQUAL "pistorm32lite")
        if(${TARGET} STREQUAL "raspi64")
            add_compile_definitions(PISTORM)
            if(${VARIANT} STREQUAL "pistorm32lite")
                add_compile_definitions(PISTORM32LITE)
                add_compile_definitions(PISTORM32)
                list(APPEND BASE_FILES
                    src/pistorm/ps32_protocol.c
                )
            else()
                list(APPEND BASE_FILES
                    src/pistorm/ps_protocol.c
                )
            endif()
            include_directories(src/pistorm)
            list(APPEND BASE_FILES
                src/boards/devicetree.c
                src/boards/z2ram.c                
                src/boards/sdcard.c
                src/boards/68040.c
                src/boards/emmc.c
                src/boards/unicam.c
            )
        else()
            message(FATAL_ERROR "PiStorm variants are supported on raspi targets, only.")
        endif()
    endif()
    
else()
    message(FATAL_ERROR "Wrong variant selected: ${VARIANT}")
endif()

add_link_options(-Wl,--build-id -nostdlib -nostartfiles -static)
add_compile_options(-mbig-endian -fno-exceptions -ffreestanding -Wall -Wextra -Werror)

list(APPEND ARCH_FILES
    src/aarch64/start.c
    src/aarch64/mmu.c
    src/aarch64/RegisterAllocator64.c
    src/aarch64/vectors.c
)
set(CAPSTONE_ARM64_SUPPORT ON CACHE BOOL "CAPSTONE_ARM64_SUPPORT")
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/ldscript-be64.lds)
add_link_options(-Wl,-EB -Wl,-Map -Wl,${CMAKE_BINARY_DIR}/Emu68.map -Wl,--format=elf64-bigaarch64 -T ${LINKER_SCRIPT})
add_compile_options(-march=armv8-a+crc -mtune=cortex-a72 -fomit-frame-pointer -O3 -ffixed-x12 -ffixed-q31 -ffixed-q30 -ffixed-q29 -ffixed-q28)

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
    add_compile_options(-mno-outline-atomics)
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

add_subdirectory(external)

target_link_libraries(Emu68.elf tinystl capstone-static libdeflate_static)
target_include_directories(Emu68.elf PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/external/capstone/include)
target_include_directories(Emu68.elf PRIVATE ${CMAKE_BINARY_DIR}/include)
target_compile_definitions(Emu68.elf PRIVATE)

configure_file(include/version.h.in include/version.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/.git/index)
