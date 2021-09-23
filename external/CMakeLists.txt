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

add_subdirectory(capstone)
add_subdirectory(tiny-stl)
