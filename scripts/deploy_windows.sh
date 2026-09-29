#!/usr/bin/env bash
# 現在のコードを Release でビルドして Windows 側にデプロイする
#
#   scripts/deploy_windows.sh                              # ビルド + C:\Users\<user>\NyanFi-test へ配置
#   DEPLOY_DIR=/mnt/c/Users/ykuwa/Desktop/NyanFi scripts/deploy_windows.sh
#   scripts/deploy_windows.sh --deploy-dir /mnt/c/... --build-dir /tmp/nyanfi-deploy --no-build
#
# 内容: GUI (nyanfi.exe) を Release + 静的 wx でクロスビルドし、
# Windows 側ディレクトリへコピーする。既存の build/ (Debug core 用) は触らない。
set -euo pipefail

if [[ -d /home/linuxbrew/.linuxbrew/bin ]]; then
	export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-deploy}"
DEPLOY="${DEPLOY_DIR:-/mnt/c/Users/ykuwa/NyanFi-test}"
WX_CONFIG_DEFAULT="$HOME/opt/wx-3.3.3-mingw64/bin/wx-config"
WX_CONFIG="${WX_CONFIG:-$WX_CONFIG_DEFAULT}"
DO_BUILD=1

for arg in "$@"; do
	case "$arg" in
	--no-build) DO_BUILD=0 ;;
	--deploy-dir=*) DEPLOY="${arg#--deploy-dir=}" ;;
	--build-dir=*) BUILD="${arg#--build-dir=}" ;;
	--deploy-dir|--build-dir)
		echo "使い方: $0 [--deploy-dir=PATH] [--build-dir=PATH] [--no-build]" >&2
		exit 2
		;;
	--deploy-dir*|--build-dir*)
		# "--deploy-dir PATH" 形式 (スペース区切り)
		echo "スペース区切りではなく = 形式で指定してください: $arg" >&2
		exit 2
		;;
	*)
		# 位置引数は DEPLOY_DIR の省略形として受け付ける
		DEPLOY="$arg"
		;;
	esac
done

# 明示オプションの後に残る "--deploy-dir PATH" 形式の取り出し (互換のため)
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
	case "${args[$i]}" in
	--deploy-dir) DEPLOY="${args[$((i + 1))]}" ;;
	--build-dir) BUILD="${args[$((i + 1))]}" ;;
	esac
done

if [[ ! -x "$WX_CONFIG" ]]; then
	echo "wx-config が見つかりません: $WX_CONFIG" >&2
	echo "scripts/build_wx.sh で用意し、WX_CONFIG=... で指定してください" >&2
	exit 1
fi

if [[ "$DO_BUILD" == "1" ]]; then
	echo "==> configure: $BUILD (Release, GUI=ON)"
	cmake -S "$ROOT" -B "$BUILD" -G Ninja \
		--toolchain "$ROOT/cmake/toolchain-mingw-w64.cmake" \
		-DCMAKE_BUILD_TYPE=Release \
		-DNYANFI_BUILD_GUI=ON \
		-DNYANFI_BUILD_TESTS=OFF \
		-DWX_CONFIG="$WX_CONFIG"

	echo "==> build nyanfi"
	cmake --build "$BUILD" --target nyanfi
else
	echo "==> ビルドをスキップ (--no-build)"
fi

EXE="$BUILD/gui/nyanfi.exe"
if [[ ! -f "$EXE" ]]; then
	echo "nyanfi.exe がありません: $EXE" >&2
	exit 1
fi

echo "==> deploy: $EXE -> $DEPLOY/"
mkdir -p "$DEPLOY"
cp -v "$EXE" "$DEPLOY/"

echo "==> 依存 DLL の確認 (システム DLL のみであること)"
x86_64-w64-mingw32-objdump -p "$DEPLOY/nyanfi.exe" | grep -i "DLL Name" | sort -u
ls -lh "$DEPLOY/nyanfi.exe"

if command -v wslpath >/dev/null 2>&1; then
	echo "==> Windows パス: $(wslpath -w "$DEPLOY")"
fi
echo "完了: エクスプローラで開いて nyanfi.exe を実行してください"
