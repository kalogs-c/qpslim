add_executable(dx11-test WIN32 main.cpp)

# dx11-test e propositalmente simples: sem vcpkg, sem conan, sem libs externas.
# So Windows SDK (d3d11 + dxgi) que ja vem com VS2022, MinGW-w64 e Zig.
# (Sem d3dcompiler: nao compilamos shaders neste demo. O toolchain do Zig
# nao empacota esse import lib, e ele e desnecessario aqui.)
target_link_libraries(dx11-test PRIVATE d3d11 dxgi)

target_compile_features(dx11-test PRIVATE cxx_std_17)

if(MSVC)
  # /W4 sem /WX para nao travar PoC com warnings do SDK
  target_compile_options(dx11-test PRIVATE /W4)
  # Subsystem Windows, entry wWinMain
  set_target_properties(dx11-test PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Zig cc (clang) mirando x86_64-windows-gnu: sem flags GNU
  # (-municode/-static-* sao especificas do GCC; o Zig resolve
  # entry point e runtime sozinho).
  target_compile_options(dx11-test PRIVATE -Wall -Wextra)
  # -mwindows do WIN32 e ignorado pelo driver do Zig; forca GUI aqui.
  target_link_options(dx11-test PRIVATE "LINKER:--subsystem,windows")
else()
  target_compile_options(dx11-test PRIVATE -Wall -Wextra)
  # Link estatico de libgcc/libstdc++ para rodar no Windows sem DLLs extras.
  # (Sem -municode: entry e WinMain ANSI, igual no MSVC e no Zig.)
  target_link_options(dx11-test PRIVATE -static-libgcc -static-libstdc++)
endif()
