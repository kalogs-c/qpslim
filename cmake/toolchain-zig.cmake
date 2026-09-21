# Toolchain Zig -> Windows x64 (windows-gnu).
# Uso:
#   cmake -B build --toolchain cmake/toolchain-zig.cmake -G Ninja
#   cmake --build build
#
# Requer: zig no PATH (via mise: `mise install`).
# O Zig traz headers/libs windows-gnu embutidos — sem apt/sudo.
# Fallback: cmake/toolchain-mingw64.cmake (GCC via apt).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(ZIG_EXE zig REQUIRED)

# CMAKE_<LANG>_COMPILER como lista: executavel + subcomando.
set(CMAKE_C_COMPILER "${ZIG_EXE}" cc)
set(CMAKE_CXX_COMPILER "${ZIG_EXE}" c++)

set(_ZIG_TARGET "x86_64-windows-gnu")
set(CMAKE_C_FLAGS_INIT "-target ${_ZIG_TARGET}")
set(CMAKE_CXX_FLAGS_INIT "-target ${_ZIG_TARGET}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target ${_ZIG_TARGET}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-target ${_ZIG_TARGET}")

# Evita linkar executavel de teste durante a deteccao do compilador.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Sem .rc por enquanto; dispensa windres neste toolchain.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
