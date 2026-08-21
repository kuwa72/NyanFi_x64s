#!/usr/bin/env bash
# Phase 0 のビルドとテストを 1 コマンドで回す
#
#   scripts/build.sh            configure + build + test
#   scripts/build.sh --no-test  configure + build のみ
#   scripts/build.sh --clean    build ディレクトリを作り直す
#
# 本番ターゲットは clang-cl だが、ローカル検証は mingw-w64 で行う。
# 生成した .exe は WSL interop でそのまま実行できる。
set -euo pipefail

if [[ -d /home/linuxbrew/.linuxbrew/bin ]]; then
	export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
RUN_TESTS=1

for arg in "$@"; do
	case "$arg" in
	--no-test) RUN_TESTS=0 ;;
	--clean) rm -rf "$BUILD" ;;
	*)
		echo "不明な引数: $arg" >&2
		exit 2
		;;
	esac
done

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
	--toolchain "$ROOT/cmake/toolchain-mingw-w64.cmake" \
	-DCMAKE_BUILD_TYPE=Debug

cmake --build "$BUILD"

if [[ "$RUN_TESTS" == "1" ]]; then
	ctest --test-dir "$BUILD" --output-on-failure
fi
