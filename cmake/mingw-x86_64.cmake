# 64-bit Windows executable with mingw-w64 GCC (docs/superpowers/specs/2026-11-11-build-64bit-pc-design.md).
#   cmake -B build64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake -DRECVX_64=ON
# GCC, not clang (see mingw-i686.cmake).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(MINGW_PREFIX "$ENV{MINGW_PREFIX}" CACHE PATH "directory holding bin/x86_64-w64-mingw32-gcc (empty: PATH)")
if(MINGW_PREFIX)
    set(_p "${MINGW_PREFIX}/bin/")
endif()
set(CMAKE_C_COMPILER   ${_p}x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER ${_p}x86_64-w64-mingw32-g++)
find_program(RECVX_WINDRES NAMES ${_p}x86_64-w64-mingw32-windres x86_64-w64-mingw32-windres windres)
set(CMAKE_RC_COMPILER  ${RECVX_WINDRES})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
