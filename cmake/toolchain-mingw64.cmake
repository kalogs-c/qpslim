# Toolchain para cross-compilar Windows x64 no Linux com MinGW-w64.
# Uso:
#   cmake -B build --toolchain cmake/toolchain-mingw64.cmake
#   cmake --build build
#
# Requer (no Linux): mingw-w64, g++-mingw-w64-x86-64
# No Windows com VS2022: nao use este arquivo, configure direto.
#
# Se o MinGW estiver extraido em local custom (ex. sem sudo),
# exporte MINGW_ROOT=/caminho/para/root/usr antes de configurar.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(DEFINED ENV{MINGW_ROOT})
  set(_MINGW_BIN "$ENV{MINGW_ROOT}/bin")
  set(CMAKE_C_COMPILER "${_MINGW_BIN}/x86_64-w64-mingw32-gcc")
  set(CMAKE_CXX_COMPILER "${_MINGW_BIN}/x86_64-w64-mingw32-g++")
  set(CMAKE_RC_COMPILER "${_MINGW_BIN}/x86_64-w64-mingw32-windres")
else()
  set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
  set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
endif()

# Nao procurar programas do host no target
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
