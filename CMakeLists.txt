cmake_minimum_required(VERSION 3.14.0)
project(Emu68 VERSION 0.1.0)

include(cmake/verstring.cmake)
include(cmake/firmware_download.cmake)
get_verstring(VERSTRING)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(CMAKE_INSTALL_PREFIX "")
set(SUPPORTED_TARGETS "raspi" "raspi64" "pbpro" "rockpro64")
set(TARGET "raspi" CACHE STRING "One of target machines: ${SUPPORTED_TARGETS}")

set(ARCH_FILES "")
set(TARGET_FILES "")

add_subdirectory(external/tiny-stl)

if (${TARGET} IN_LIST SUPPORTED_TARGETS)
    message("-- Selected target machine: ${TARGET}")
    if(${TARGET} STREQUAL "raspi64" OR ${TARGET} STREQUAL "raspi")
        download_raspi_firmware()
        install(DIRECTORY ${CMAKE_BINARY_DIR}/firmware/ DESTINATION .)
    elseif(${TARGET} STREQUAL "pbpro")
        install(FILES ${CMAKE_SOURCE_DIR}/blobs/rk3399-pinebookpro.dtb DESTINATION .)
    endif()

    if (${TARGET} STREQUAL "raspi")
        set(TARGET_ARCH "armhf")
    else()
        set(TARGET_ARCH "aarch64")
    endif()
else()
    message(FATAL_ERROR "Wrong target machine specified: ${TARGET}")
endif()

if(${TARGET_ARCH} STREQUAL "armhf")
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/armhf/ldscript-be.lds)
    add_compile_options(-mbig-endian -mcpu=cortex-a7 -mfpu=neon-vfpv4 -O3 -pedantic -pedantic-errors -ffixed-r11 -fomit-frame-pointer -Wall -Wextra -Werror)
else()
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/aarch64/ldscript-be64.lds)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--build-id -Wl,-EB -Wl,--format=elf64-bigaarch64 -nostdlib -nostartfiles -static -T ${LINKER_SCRIPT}")
    list(APPEND ARCH_FILES src/aarch64/start.c)
    add_compile_options(-mbig-endian -O3 -pedantic -pedantic-errors -ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26 -ffixed-x27 -ffixed-x28 -ffixed-x29 -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x18 -fomit-frame-pointer -Wall -Wextra -Werror)
endif()



message("${CMAKE_EXE_LINKER_FLAGS}")

add_executable(Emu68
    ${ARCH_FILES}
)

target_link_libraries(Emu68 tinystl)
target_include_directories(Emu68 PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(Emu68 PRIVATE VERSION_STRING="${VERSTRING}")

