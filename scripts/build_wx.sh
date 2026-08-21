#!/usr/bin/env bash
# wxWidgets を mingw-w64 ターゲットでビルドして $WX_PREFIX に入れる
#
# issue #1 の Phase 2 (GUI を VCL から wxWidgets へ) 用。
# プリビルドの wxMSW バイナリもあるが、CRT (UCRT/msvcrt) と例外モデルが手元の
# ツールチェインと一致している保証がないため、同じコンパイラでソースから作る。
#
#   scripts/build_wx.sh            ビルドして導入
#   WX_PREFIX=... scripts/build_wx.sh
#
# 生成物はリポジトリ外 (既定 ~/opt/wx-<version>-mingw64) に置く。
set -euo pipefail

if [[ -d /home/linuxbrew/.linuxbrew/bin ]]; then
	export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
fi

WX_VERSION="${WX_VERSION:-3.3.3}"
WX_PREFIX="${WX_PREFIX:-$HOME/opt/wx-${WX_VERSION}-mingw64}"
WORK="${WX_WORK:-$HOME/.cache/nyanfi-wx}"
HOST=x86_64-w64-mingw32
JOBS="$(nproc)"

mkdir -p "$WORK"
cd "$WORK"

TARBALL="wxWidgets-${WX_VERSION}.tar.bz2"
if [[ ! -f "$TARBALL" ]]; then
	echo "==> ダウンロード: $TARBALL"
	curl -fsSL -o "$TARBALL" \
		"https://github.com/wxWidgets/wxWidgets/releases/download/v${WX_VERSION}/${TARBALL}"
fi

SRC="$WORK/wxWidgets-${WX_VERSION}"
if [[ ! -d "$SRC" ]]; then
	echo "==> 展開"
	tar xf "$TARBALL"
fi

BUILD="$SRC/build-mingw64"
mkdir -p "$BUILD"
cd "$BUILD"

if [[ ! -f config.status ]]; then
	echo "==> configure"
	# NyanFi が必要とするのはウィンドウ・コントロール・自前描画。
	# webview / OpenGL / メディア再生は使わないので落として時間を稼ぐ。
	../configure \
		--host="$HOST" \
		--build="$(../config.guess)" \
		--prefix="$WX_PREFIX" \
		--disable-shared \
		--disable-webview \
		--disable-mediactrl \
		--without-opengl \
		--without-subdirs \
		--disable-sysoptions \
		--with-msw \
		CXXFLAGS="-O2" \
		CFLAGS="-O2"
fi

echo "==> make -j${JOBS}"
make -j"$JOBS"

echo "==> install"
make install

echo
echo "wxWidgets ${WX_VERSION} を導入しました: $WX_PREFIX"
echo "  wx-config: $WX_PREFIX/bin/wx-config"
