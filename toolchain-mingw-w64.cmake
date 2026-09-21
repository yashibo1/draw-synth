# Cross-compilation toolchain for building DrawSynth for Windows x64 from
# Linux (or macOS) using MinGW-w64. This is what makes it possible to produce
# a Windows installer without a Windows machine.
#
# Usage:
#   cmake -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-windows --target DrawSynth_VST3 -j
#
# Requires (Debian/Ubuntu): sudo apt install g++-mingw-w64-x86-64
# The POSIX-threading variant is required - JUCE relies on full C++11
# <thread>/<mutex>/<condition_variable>, which the Win32-threading MinGW
# variant does not fully implement.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(DRAWSYNTH_MINGW_PREFIX x86_64-w64-mingw32)

find_program(DRAWSYNTH_MINGW_CXX NAMES ${DRAWSYNTH_MINGW_PREFIX}-g++-posix ${DRAWSYNTH_MINGW_PREFIX}-g++)
find_program(DRAWSYNTH_MINGW_CC  NAMES ${DRAWSYNTH_MINGW_PREFIX}-gcc-posix ${DRAWSYNTH_MINGW_PREFIX}-gcc)
find_program(DRAWSYNTH_MINGW_RC  NAMES ${DRAWSYNTH_MINGW_PREFIX}-windres)

if (NOT DRAWSYNTH_MINGW_CXX OR NOT DRAWSYNTH_MINGW_CC)
    message(FATAL_ERROR
        "MinGW-w64 cross compiler not found. Install it with:\n"
        "  sudo apt install g++-mingw-w64-x86-64\n"
        "and make sure the POSIX-threading variant is selected "
        "(update-alternatives --config ${DRAWSYNTH_MINGW_PREFIX}-g++ if needed).")
endif()

set(CMAKE_CXX_COMPILER ${DRAWSYNTH_MINGW_CXX})
set(CMAKE_C_COMPILER   ${DRAWSYNTH_MINGW_CC})
set(CMAKE_RC_COMPILER  ${DRAWSYNTH_MINGW_RC})

set(CMAKE_FIND_ROOT_PATH /usr/${DRAWSYNTH_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Statically link the MinGW runtime/threading libraries into the plugin so
# the installed .vst3 doesn't depend on the target machine having a matching
# libstdc++-6.dll / libwinpthread-1.dll / libgcc_s_seh-1.dll on its PATH.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
