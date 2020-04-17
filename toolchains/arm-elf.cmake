set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CROSS_COMPILE arm-none-eabi)

set(CMAKE_C_COMPILER ${CROSS_COMPILE}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}-g++)
set(CMAKE_AR ${CROSS_COMPILE}-ar)
set(CMAKE_RANLIB ${CROSS_COMPILE}-ranlib)
set(CMAKE_OBJCOPY ${CROSS_COMPILE}-objcopy)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)