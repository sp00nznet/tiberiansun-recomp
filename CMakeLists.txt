# =============================================================================
# Tiberian Sun and Firestorm - static recompilation host (32-bit, MSVC x86)
#
#   py -3 run_lift.py        # -> src/recomp/gen/  (needs work/functions.json)
#   build.cmd                # vcvarsall x86 + CMake + Ninja -> build/ts.exe
#   build\ts.exe --run --watchdog 60
# =============================================================================
cmake_minimum_required(VERSION 3.20)
# The C runtime linked in (/MT): ts.exe runs on a PC without the x86 Visual
# C++ redistributable (the lab's test VM had none, and neither may a player).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(ts C)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "the host must be 32-bit: use build.cmd (vcvarsall x86), or on Linux build-linux.sh")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(PCRECOMP "${CMAKE_SOURCE_DIR}/../tools" CACHE PATH "pcrecomp checkout")
file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/recomp/gen/*.c")
if(NOT GEN_SOURCES)
  message(FATAL_ERROR "src/recomp/gen is empty -- run ./setup.sh (Setup.cmd on Windows) first: it lifts the game")
endif()

# ---- native Linux: the lifted C on runtime/win32hle (cmake/linux-i386.cmake)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
  set(HLE ${PCRECOMP}/runtime/win32hle)
  file(GLOB HLE_SOURCES CONFIGURE_DEPENDS "${HLE}/*.c")
  list(FILTER HLE_SOURCES EXCLUDE REGEX "_selftest\.c$")
  add_executable(ts src/linux/main.c src/linux/blowfish.c src/linux/script.c src/linux/mods.c src/runtime/hdvox.c ${HLE_SOURCES} ${GEN_SOURCES})
  target_include_directories(ts PRIVATE src/linux src/runtime src/recomp/gen ${HLE} ${PCRECOMP}/runtime/recomp32)
  target_compile_definitions(ts PRIVATE RECOMP_RETADDR=0u RECOMP_FLAT_MEMORY RECOMP_LOCAL_REGS)
  set(GEN_OPT "-O2" CACHE STRING "optimisation for the generated C")
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;${GEN_OPT};-fno-strict-aliasing;-fwrapv")
  target_compile_options(ts PRIVATE -fno-strict-aliasing -fwrapv)
  target_link_libraries(ts PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_TTF pthread m)
  return()
endif()

if(NOT MSVC)
  message(FATAL_ERROR "runtime/native32 is MSVC x86 inline assembly")
endif()

add_executable(ts
  src/runtime/host.c
  src/runtime/input.c
  src/runtime/mods.c
  src/runtime/oracle.c
  src/runtime/present.c
  src/runtime/hdvox.c
  ${PCRECOMP}/runtime/native32/native32.c
  ${PCRECOMP}/runtime/recomp32/image_loader.c
  ${PCRECOMP}/runtime/recomp32/recomp_trace.c
  ${GEN_SOURCES}
)
target_include_directories(ts PRIVATE
  src/runtime src/recomp/gen ${PCRECOMP}/runtime/native32 ${PCRECOMP}/runtime/recomp32)
# RECOMP_RETADDR=0u: a stack imbalance during bring-up leaks a harmless 0, not
# 0xDEAD0000 into a size argument (forcecommander).
target_compile_definitions(ts PRIVATE RECOMP_RETADDR=0u _CRT_SECURE_NO_WARNINGS
                                    RECOMP_FLAT_MEMORY RECOMP_LOCAL_REGS)
# Linked high and not relocatable, so the guest's 0x00400000..0x00B7A000 is
# free.
target_link_options(ts PRIVATE /BASE:0x60000000 /FIXED:NO /DYNAMICBASE:NO
                               /NXCOMPAT /LARGEADDRESSAWARE /STACK:0x1000000
                               /MANIFESTUAC:NO)
set(GEN_OPT "/O2" CACHE STRING "optimisation for the generated C")
# Visual Studio 2026's compiler (19.50 and later) drops the sign test of a
# 16-bit value shifted to the top of a register (test ax,ax; jl), which lets
# a -1 index past a bounds check; Red Alert 2 crashed on it. Its older
# optimiser compiles it correctly.
if(MSVC_VERSION GREATER_EQUAL 1950)
  string(APPEND GEN_OPT ";/d2SSAOptimizer-")
endif()
set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/w;${GEN_OPT}")
set_source_files_properties(src/runtime/host.c src/runtime/input.c src/runtime/mods.c src/runtime/oracle.c src/runtime/present.c src/runtime/hdvox.c PROPERTIES COMPILE_OPTIONS "/W3")
target_link_libraries(ts PRIVATE user32 gdi32 kernel32 ole32 uuid ddraw dxguid winmm d3d11 dxgi shell32)

option(TS_TRACE "record a ring buffer of lifted function entries" OFF)
if(TS_TRACE)
  target_compile_definitions(ts PRIVATE RECOMP_TRACE=1)
endif()
