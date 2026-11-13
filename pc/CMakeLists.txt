# PC port build.
#
#   recvx_game     : the original game + platform code from the decomp, compiled
#                    unmodified except for '#ifdef TARGET_PC' asm replacements.
#   recvx_platform : the new PC abstraction layer (PS2 hardware/IOP emulation at
#                    library boundaries, renderer, audio, input, overlay).
#   recvx          : the executable.

set(DECOMP ${RECVX_DECOMP_DIR})
add_compile_definitions(_FILE_OFFSET_BITS=64)   # disc images over 2 GB (cdvd.c fseeko); mingw-w64 honours it too
if(WIN32)
    # the crash log walks the EBP frame chain (host/log.c); measured on Linux: +1..5% game logic
    add_compile_options(-fno-omit-frame-pointer)
endif()
set(PROG ${DECOMP}/src/ps2/veronica/prog)
# x86: SSE for single-precision floats (x87 keeps extended intermediates); AArch64 has them natively
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|i[3-6]86|AMD64|amd64)" OR WIN32)
    set(RECVX_X86_FP -msse2 -mfpmath=sse)
else()
    set(RECVX_X86_FP)
endif()
set(KATANA_INC ${DECOMP}/include/recvx-decomp-katana/KATANA/Include)

# SDL3 is built from source and linked statically by default, so the binary does not
# depend on a (32-bit) SDL3 package being installed. SDL still dlopen()s the system's
# X11/Wayland/audio libraries at runtime.
option(RECVX_VENDORED_SDL "Build SDL3 from source and link it statically" ON)
if(NINTENDO_SWITCH)
    # devkitPro ships SDL2 only: the SDL3 calls go through a compatibility layer over it (pc/switch)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NXSDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(NXSDL2IMG REQUIRED IMPORTED_TARGET SDL2_image)
    add_library(sdl3compat STATIC switch/sdl3compat.c switch/nx_fatal.c switch/fcast_log.c switch/nx_gyro.c)
    target_include_directories(sdl3compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/switch/include)
    target_link_libraries(sdl3compat PUBLIC PkgConfig::NXSDL2IMG PkgConfig::NXSDL2 EGL glapi drm_nouveau)
    add_library(SDL3::SDL3 ALIAS sdl3compat)
elseif(RECVX_VENDORED_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.16 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 REQUIRED CONFIG)
endif()

# Dear ImGui (MIT) for the F1 overlay, with its SDL3 + OpenGL3 backends.
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
if(NINTENDO_SWITCH)   # the SDL2 backend, behind switch/imgui/imgui_impl_sdl3.h
    set(IMGUI_PLATFORM ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp switch/imgui_sdl2_shim.cpp)
else()
    set(IMGUI_PLATFORM ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)
endif()
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_PLATFORM} ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
if(NINTENDO_SWITCH)
    target_include_directories(imgui BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/switch/imgui)
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)
    set_source_files_properties(${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp PROPERTIES COMPILE_OPTIONS "-include;glad/glad.h")
    target_link_libraries(imgui PUBLIC glad)   # libglad.pc is broken ($(DEVKITPRO) prefix)
endif()
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ---------------------------------------------------------------- headers
# The Katana SDK headers are included with inconsistent case (SG_MWPLY.H vs
# sg_mwply.h). Mirror them under both spellings for case-sensitive filesystems.
set(CASEFOLD_DIR ${CMAKE_BINARY_DIR}/katana_casefold)
file(GLOB KATANA_HEADERS RELATIVE ${KATANA_INC} ${KATANA_INC}/*)
foreach(h ${KATANA_HEADERS})
    string(TOLOWER ${h} lo)
    string(TOUPPER ${h} up)
    foreach(n ${lo} ${up})
        if(NOT n STREQUAL h AND NOT EXISTS ${KATANA_INC}/${n} AND NOT EXISTS ${CASEFOLD_DIR}/${n})
            file(MAKE_DIRECTORY ${CASEFOLD_DIR})
            file(CREATE_LINK ${KATANA_INC}/${h} ${CASEFOLD_DIR}/${n} COPY_ON_ERROR SYMBOLIC)
        endif()
    endforeach()
endforeach()

# ---------------------------------------------------------------- game (decomp)
file(GLOB GAME_SOURCES ${PROG}/*.c)
set(GAME_EXCLUDE
    gcc_wrapper.c        # ee-gcc soft-float/int64 runtime helpers
    ps2_MovieFunc.c      # libmpeg/IPU movie player -> pc/src/movie
    ps2_MovieWork.c
    ps2_Vu1Scissor2.c    # #included by ps2_Vu1Strip.c
)
foreach(x ${GAME_EXCLUDE})
    list(REMOVE_ITEM GAME_SOURCES ${PROG}/${x})
endforeach()

# PC implementations of decomp-declared APIs that need the decomp's types
file(GLOB GLUE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/glue/*.c)

add_library(recvx_game OBJECT ${GAME_SOURCES} ${GLUE_SOURCES})
target_compile_definitions(recvx_game PRIVATE main=recvx_game_main)
target_include_directories(recvx_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
    ${DECOMP}/src/cri/mwlib/ee/lib/libadxe
    ${PROG}
)
# Katana/CRI dirs ship libc-named headers (math.h, stddef.h, ...); search them
# after the host's system headers.
target_compile_options(recvx_game PRIVATE
    "SHELL:-idirafter ${KATANA_INC}"
    "SHELL:-idirafter ${CASEFOLD_DIR}"
    "SHELL:-idirafter ${DECOMP}/include/recvx-decomp-cri/cri/mwlib/include"
    "SHELL:-idirafter ${DECOMP}/include/recvx-decomp-cri/cri/mwlib/ee/include"
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/pc_prelude.h"
    -std=gnu11
    -fno-strict-aliasing
    -fwrapv
    -fsigned-char             # EE char is signed
    -ffp-contract=off         # no FMA fusion: keep the PS2's operation order
    ${RECVX_X86_FP}           # IEEE single precision, no x87 extended intermediates
    -fno-tree-vectorize
    -ftrivial-auto-var-init=zero  # deterministic stand-in for MWCC's stale registers (docs/dev/uninitialized-locals.txt)
)
# the decomp is compiled as is, without warnings (-w), but a call to an undeclared function stays an
# error: without a prototype a float argument goes as a double and a char/float return is read as an
# int. -w also hides -Werror=..., so this relies on GCC 14's permerror (no -fpermissive here; the other
# permerrors the decomp needs are switched off one by one). GCC 13 (CI) only warns: the ctest
# decomp_declarations checks it there. Missing returns and uninitialized locals are listed by
# pc/tools/decomp_warnings.py. The port's own glue gets warnings.
if(RECVX_64)
    # 64-bit: a pointer no longer fits an int. Every pointer<->integer conversion is an error, so
    # each goes through pc_ptr.h (P32/S32/PS2_ADDR/PS2_HOST) or intptr_t. -w would hide -Werror=...,
    # so the other warnings are switched off by group instead.
    set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-Wno-incompatible-pointer-types;-Wno-return-mismatch;-Wno-multichar;-Wno-overflow;-Wno-div-by-zero;-Wno-attributes;-Wno-pointer-sign;-Wno-discarded-qualifiers;-Wno-builtin-declaration-mismatch;-Wno-shift-count-overflow;-Wno-implicit-int;-Wno-deprecated-declarations;-Wno-pointer-compare;-Wno-format;-Wno-unused-value;-Werror=pointer-to-int-cast;-Werror=int-to-pointer-cast;-Werror=int-conversion")
else()
set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-w;-Wno-incompatible-pointer-types;-Wno-int-conversion;-Wno-return-mismatch")
endif()
set_source_files_properties(${GLUE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wno-unused-function")
if(WIN32)
    # GCC (not MSVC) bitfield layout, as on Linux and the EE
    target_compile_options(recvx_game PRIVATE -mno-ms-bitfields)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # the executable is non-PIE (fixed address window); position-independent code only costs
    # get_pc_thunk calls on x86-32
    target_compile_options(recvx_game PRIVATE -fno-pie)
endif()

# ---------------------------------------------------------------- platform layer
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(recvx_platform OBJECT ${PLATFORM_SOURCES})
# xxHash (BSD-2-Clause, header only): XXH3, the hash PCSX2 names its texture replacements by (src/gs/hdpack.c)
include(FetchContent)
FetchContent_Declare(xxhash GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git GIT_TAG v0.8.3 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
# bcdec (MIT / Unlicense, header only): BC1-BC7 decoding for the replacements' content check
FetchContent_Declare(bcdec GIT_REPOSITORY https://github.com/iOrange/bcdec.git GIT_TAG 80859ed3b7afb1c527a2a99d70c61457bea72d0c SOURCE_SUBDIR none)
# SMAA (MIT): the anti-aliasing shader (SMAA.hlsl, built as GLSL 3.30) and its area and search tables
# (src/gs/gs_gl.c smaa_*); the shader is embedded as bytes in a generated smaa_hlsl.h
FetchContent_Declare(smaa GIT_REPOSITORY https://github.com/iryoku/smaa.git GIT_TAG 71c806a838bdd7d517df19192a20f0c61b3ca29d SOURCE_SUBDIR none)
FetchContent_MakeAvailable(xxhash bcdec smaa)
file(READ ${smaa_SOURCE_DIR}/SMAA.hlsl SMAA_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," SMAA_BYTES "${SMAA_HEX}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/smaa_hlsl.h "static const char smaa_hlsl[] = { ${SMAA_BYTES} 0 };\n")
target_include_directories(recvx_platform PRIVATE ${xxhash_SOURCE_DIR} ${bcdec_SOURCE_DIR}
    ${smaa_SOURCE_DIR}/Textures ${CMAKE_CURRENT_BINARY_DIR}/generated)
set_property(SOURCE src/host/host.c src/host/overlay.cpp APPEND PROPERTY COMPILE_DEFINITIONS "RECVX_VERSION=\"${RECVX_VERSION}\"")
set_property(SOURCE src/host/update.c APPEND PROPERTY COMPILE_DEFINITIONS   # the release this is (update check)
    RECVX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} RECVX_VERSION_MINOR=${PROJECT_VERSION_MINOR} RECVX_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_include_directories(recvx_platform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
)
target_compile_options(recvx_platform PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    -fno-strict-aliasing -fwrapv -ffp-contract=off ${RECVX_X86_FP}
    -Wall -Wno-unused-function
)
if(RECVX_64)
    target_compile_definitions(recvx_game PRIVATE RECVX_64=1)
    target_compile_definitions(recvx_platform PRIVATE RECVX_64=1)
    if(RECVX_PIE)
        target_compile_definitions(recvx_game PRIVATE RECVX_PIE=1)
        target_compile_definitions(recvx_platform PRIVATE RECVX_PIE=1)
    endif()
    target_compile_options(recvx_platform PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=implicit-function-declaration>)
    set_source_files_properties(${GLUE_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-Wall;-Wno-unused-function;-Werror=pointer-to-int-cast;-Werror=int-to-pointer-cast;-Werror=int-conversion;-Werror=incompatible-pointer-types")
endif()
target_link_libraries(recvx_platform PRIVATE SDL3::SDL3 imgui)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(recvx_platform PRIVATE -fno-pie)
    if(RECVX_VENDORED_SDL)
        # Vulkan headers for HDR output (src/render/hdr_vk.c; the loader is SDL's, at runtime); else the system's
        target_include_directories(recvx_platform PRIVATE ${sdl3_SOURCE_DIR}/src/video/khronos)
    endif()
endif()
if(WIN32)
    target_compile_options(recvx_platform PRIVATE $<$<COMPILE_LANGUAGE:C>:-mno-ms-bitfields>)
endif()

# ---------------------------------------------------------------- executable
set(RECVX_LAYOUT_PAD 0 CACHE STRING "test aid: bytes of padding before all code, data and bss (pc/tools/layout_check.sh)")
if(RECVX_LAYOUT_PAD)
    set(LAYOUT_PAD_SRC tests/layout_pad.c)
    set_source_files_properties(tests/layout_pad.c PROPERTIES COMPILE_DEFINITIONS RECVX_LAYOUT_PAD=${RECVX_LAYOUT_PAD})
endif()
if(WIN32)
    enable_language(RC)
    set(RECVX_RES res/recvx.rc)   # the program icon
    set_source_files_properties(res/recvx.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/recvx-enhanced.ico)
endif()
add_executable(recvx ${LAYOUT_PAD_SRC} ${RECVX_RES} $<TARGET_OBJECTS:recvx_game> $<TARGET_OBJECTS:recvx_platform>
               $<$<TARGET_EXISTS:glibc_compat>:$<TARGET_OBJECTS:glibc_compat>>)
target_link_libraries(recvx PRIVATE imgui SDL3::SDL3 m)
if(WIN32)
    target_link_libraries(recvx PRIVATE pthread shell32 dwmapi winhttp)   # clock_gettime (winpthreads); borderless corners (settings.c); update check (update.c)
    # Same address window as Linux: fixed image base, no ASLR relocation; large-address-aware
    # gives the 32-bit exe 4 GB on 64-bit Windows (Linux already uses addresses above 2 GB). -static: libstdc++,
    # libgcc and winpthread go into the exe, only system DLLs (UCRT, opengl32, ...) remain.
    if(RECVX_PIE)
        # the image loads anywhere (ASLR, above 4 GB): PS2 addresses are relative to it (pc/include/pc_ptr.h)
        target_link_options(recvx PRIVATE -static -Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--stack,8388608)
    elseif(RECVX_64)
        # same window as the 32-bit build, so every game object fits a 32-bit slot as it is
        target_link_options(recvx PRIVATE -static -Wl,--image-base,0x30000000 -Wl,--disable-dynamicbase
                            -Wl,--disable-high-entropy-va -Wl,--stack,8388608)
    else()
    target_link_options(recvx PRIVATE -static -Wl,--image-base,0x30000000 -Wl,--disable-dynamicbase -Wl,--large-address-aware
                        -Wl,--stack,8388608)
    endif()
endif()
if(NINTENDO_SWITCH)
    # libnx's linker script with the read-only data moved into the writable data segment (after the
    # RELRO part libnx protects at start): the decomp writes into string literals, as on the PS2.
    # __executable_start: the image's first byte, PS2 address 0x30000000 (pc_ptr.h).
    file(READ ${NX_ROOT}/switch.ld NX_LD)
    set(nx_ro "*(.rodata .rodata.* .gnu.linkonce.r.*)")
    set(nx_rw "*(.data .data.* .gnu.linkonce.d.*)")
    string(FIND "${NX_LD}" "${nx_ro}" nx_ro_at)
    string(FIND "${NX_LD}" "${nx_rw}" nx_rw_at)
    string(FIND "${NX_LD}" "KEEP (*(.crt0))" nx_crt0_at)
    if(nx_ro_at LESS 0 OR nx_rw_at LESS 0 OR nx_crt0_at LESS 0)
        message(FATAL_ERROR "libnx's switch.ld changed: update the edits in pc/CMakeLists.txt")
    endif()
    # inside .text, not at the top level: there it would be an absolute 0, not moved with the image
    string(REPLACE "KEEP (*(.crt0))" "__executable_start = .; KEEP (*(.crt0))" NX_LD "${NX_LD}")
    string(REPLACE "${nx_ro}" "" NX_LD "${NX_LD}")
    string(REPLACE "${nx_rw}" "${nx_rw} ${nx_ro}" NX_LD "${NX_LD}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/recvx.ld "${NX_LD}")
    # switch.specs names its own script (-T): the same specs without it, and ours on the command line
    file(READ ${NX_ROOT}/switch.specs NX_SPECS)
    string(REPLACE "-T %:getenv(DEVKITPRO /libnx/switch.ld) " "" NX_SPECS "${NX_SPECS}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/recvx.specs "${NX_SPECS}")
    set(nx_ld ${CMAKE_CURRENT_BINARY_DIR}/recvx.ld)
    set(nx_specs ${CMAKE_CURRENT_BINARY_DIR}/recvx.specs)
    find_program(CYGPATH cygpath)
    if(CYGPATH)   # the compiler is a native Windows program: it reads C:/ paths
        execute_process(COMMAND ${CYGPATH} -m ${nx_ld} OUTPUT_VARIABLE nx_ld OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${CYGPATH} -m ${nx_specs} OUTPUT_VARIABLE nx_specs OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    string(REGEX REPLACE "-specs=[^ ]*switch.specs" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    target_link_options(recvx PRIVATE -specs=${nx_specs} -T${nx_ld} -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/recvx.map)
    target_link_libraries(recvx PRIVATE stdc++)
    set_target_properties(recvx PROPERTIES OUTPUT_NAME recvx SUFFIX .elf)
    nx_generate_nacp(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/recvx.nacp NAME "Code: Veronica X" AUTHOR "RECOMP-GUTO" VERSION "1.0")
    nx_create_nro(recvx NACP ${CMAKE_CURRENT_BINARY_DIR}/recvx.nacp ICON ${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Place the image inside the PS2 address window (see docs/ARCHITECTURE.md, "Data model and memory").
    target_link_options(recvx PRIVATE -no-pie -Wl,-Ttext-segment=0x30000000 -static-libgcc -static-libstdc++)
    set_target_properties(recvx PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

# ---------------------------------------------------------------- tests
if(CMAKE_TESTING_ENABLED)
add_executable(test_vu tests/test_vu.c src/ps2/vu.c)
target_compile_options(test_vu PRIVATE -std=gnu11 -msse2 -mfpmath=sse -ffp-contract=off)
target_link_libraries(test_vu PRIVATE m)
add_test(NAME vu COMMAND test_vu)
# the same against the plain C (non-SSE) native paths, as an arm64 build would compile them
add_executable(test_vu_portable tests/test_vu.c src/ps2/vu.c)
target_compile_definitions(test_vu_portable PRIVATE RECVX_VU_PORTABLE)
target_compile_options(test_vu_portable PRIVATE -std=gnu11 -msse2 -mfpmath=sse -ffp-contract=off)
target_link_libraries(test_vu_portable PRIVATE m)
add_test(NAME vu_portable COMMAND test_vu_portable)

# decodes every movie on the disc (needs RECVX_ISO; skipped otherwise)
add_executable(test_movies tests/test_movies.c src/movie/mpeg2.c src/movie/pss.c)
target_compile_options(test_movies PRIVATE -std=gnu11 -O2)
target_link_libraries(test_movies PRIVATE m)
add_test(NAME movies COMMAND test_movies)
set_tests_properties(movies PROPERTIES SKIP_RETURN_CODE 77)

# calls to undeclared functions in the decomp: GCC 14+ stops the build on them, GCC 13 only warns (and -w hides it)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_test(NAME decomp_declarations COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/decomp_warnings.py --declarations ${CMAKE_BINARY_DIR})
endif()

# control bindings (pc/src/input/bindings.c): defaults, rebinding, ini, a virtual SDL gamepad
add_executable(test_bindings tests/test_bindings.c src/input/bindings.c)
target_compile_options(test_bindings PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=gnu11>)
target_link_libraries(test_bindings PRIVATE SDL3::SDL3)
add_test(NAME bindings COMMAND test_bindings)

# modern controls' direction math (pc/src/input/modern.c): camera + stick -> angle, the cut hold
add_executable(test_modern tests/test_modern.c src/input/modern.c)
target_compile_options(test_modern PRIVATE -std=gnu11)
target_link_libraries(test_modern PRIVATE m)
add_test(NAME modern COMMAND test_modern)

# pure rules of the PC hooks (pc/include/pc_rules.h, src/gs/native.h): added enemies' helper code and holders,
# dual weapons' second target, hor+ margin meshes and clipping, warp music, "Unlock all";
# with the game's flags and types
add_executable(test_rules tests/test_rules.c)
target_include_directories(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,INCLUDE_DIRECTORIES>)
target_compile_options(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,COMPILE_OPTIONS>)
target_link_libraries(test_rules PRIVATE m)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(test_rules PRIVATE -no-pie)   # (the game's flags include -fno-pie)
endif()
if(RECVX_PIE)
    target_compile_definitions(test_rules PRIVATE RECVX_PIE=1)
elseif(RECVX_64 AND WIN32)
    # its game structures hold slots: the image must lie in the PS2 window, as the game's
    target_link_options(test_rules PRIVATE -Wl,--image-base,0x30000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va)
endif()
add_test(NAME rules COMMAND test_rules)

# every recvx.ini key has a row in the settings table (F1 and OPTION > ENHANCED SETTINGS) or is listed
add_test(NAME settings COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_settings.cmake)

# native texture queue (pc/src/gs/textures.c): superseded uploads never written, the rest only when read
add_executable(test_ntex tests/test_ntex.c src/gs/textures.c)
target_compile_options(test_ntex PRIVATE -std=gnu11)
add_test(NAME ntex COMMAND test_ntex)

# "You appear as" list (pc/src/host/actors.c): event doubles hidden, indices kept
add_executable(test_appear tests/test_appear.c src/host/actors.c)
add_test(NAME appear COMMAND test_appear)

# enemy variants (pc/src/host/variants.c): the built-in table, the mods/enemies.ini reader
add_executable(test_variants tests/test_variants.c src/host/variants.c src/host/actors.c src/host/fileio.c)
add_test(NAME variants COMMAND test_variants)

# UTF-8 file paths and replace-on-rename saving (pc/src/host/fileio.c)
add_executable(test_fileio tests/test_fileio.c src/host/fileio.c)
target_compile_options(test_fileio PRIVATE -std=gnu11)
add_test(NAME fileio COMMAND test_fileio)

# native save files (pc/src/host/savestore.c)
add_executable(test_savestore tests/test_savestore.c src/host/savestore.c src/host/fileio.c)
target_compile_options(test_savestore PRIVATE -std=gnu11)
target_link_libraries(test_savestore PRIVATE SDL3::SDL3)
add_test(NAME savestore COMMAND test_savestore)
endif()
