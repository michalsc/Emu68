# In external repositories do not enfore the pedantic/restrictive compile flags
# Get the COMPILE_OPTIONS property for local scope, remove unnecessary items and
# set the property again
get_property(opt DIRECTORY PROPERTY COMPILE_OPTIONS)
list(REMOVE_ITEM opt "-Wall" "-Wextra" "-Werror" "-Wpedantic" "-pedantic")
set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${opt}")

# Some capstone specific settings
set(CAPSTONE_BUILD_STATIC_RUNTIME OFF CACHE BOOL "Embed static runtime")
set(CAPSTONE_BUILD_STATIC ON CACHE BOOL "Build static library")
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(CAPSTONE_BUILD_DIET OFF CACHE BOOL "Build diet library")
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "Build cstool")
set(CAPSTONE_USE_DEFAULT_ALLOC OFF CACHE BOOL "Use default memory allocation functions")
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "Whether architectures are enabled by default")
set(CAPSTONE_INSTALL OFF CACHE BOOL "Generate install target")
set(CAPSTONE_M68K_SUPPORT ON CACHE BOOL "CAPSTONE_M68K_SUPPORT")

set(LIBDEFLATE_BUILD_STATIC_LIB ON CACHE BOOL "Build the static library")
set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "Build the shared library")
set(LIBDEFLATE_COMPRESSION_SUPPORT OFF CACHE BOOL "Support compression")
set(LIBDEFLATE_DECOMPRESSION_SUPPORT ON CACHE BOOL "Support decompression")
set(LIBDEFLATE_ZLIB_SUPPORT ON CACHE BOOL "Support the zlib format")
set(LIBDEFLATE_GZIP_SUPPORT ON CACHE BOOL "Support the gzip format")
set(LIBDEFLATE_FREESTANDING ON CACHE BOOL
       "Build a freestanding library, i.e. a library that doesn't link to any
       libc functions like malloc(), free(), and memcpy().  Library users will
       need to call libdeflate_set_memory_allocator().")
set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "Build the libdeflate-gzip program")
set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "Build the test programs")
set(LIBDEFLATE_USE_SHARED_LIB OFF CACHE BOOL 
       "Link the libdeflate-gzip and test programs to the shared library instead
       of the static library")

add_subdirectory(capstone EXCLUDE_FROM_ALL)
add_subdirectory(tiny-stl EXCLUDE_FROM_ALL)
add_subdirectory(libdeflate EXCLUDE_FROM_ALL)


