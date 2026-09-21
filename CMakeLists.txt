cmake_minimum_required(VERSION 3.20)

project(fps-limiter VERSION 0.1.0 LANGUAGES C CXX)

# Este projeto gera binarios Windows (PE). No Linux usamos Zig
# via toolchain file (padrao, sem sudo: `mise install` + tasks build).
# Alternativa: MinGW-w64 via apt com cmake/toolchain-mingw64.cmake.
# No Windows usamos MSVC/VS2022 nativo.
if(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
  message(WARNING "Voce esta configurando para o host (${CMAKE_SYSTEM_NAME}). "
    "O alvo dx11-test so e util como binario Windows. "
    "No Linux use: cmake -B build --toolchain cmake/toolchain-zig.cmake")
endif()

add_subdirectory(dx11-test)
