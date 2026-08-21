# mingw-w64 (x86_64) クロスコンパイル用ツールチェイン
#
# Phase 0 のローカル検証用。本番ターゲットは clang-cl + Windows SDK であり、
# mingw-w64 はロジック層のコンパイル検証とテスト実行を Windows 実機なしで
# 回すための足場として使う (WSL interop で .exe をそのまま実行できる)。

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

# 生成した .exe は WSL interop 経由で実行するため、ランタイム DLL に依存させない
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
