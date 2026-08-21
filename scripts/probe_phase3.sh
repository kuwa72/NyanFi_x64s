#!/usr/bin/env bash
# 未移植の src ファイルを 1 本ずつ構文チェックし、エラー数とエラーの種類を出す
#
#   scripts/probe_phase3.sh            未移植ファイル全部の表
#   scripts/probe_phase3.sh Global     指定ファイルのエラーを分類して表示
#
# Phase 3 (Global.cpp / MainFrm.cpp / 各フォーム) の着手順を、行数ではなく
# 「シムに何が足りないか」で決めるための計測。ログは build-probe/ に残る。
set -uo pipefail

if [[ -d /home/linuxbrew/.linuxbrew/bin ]]; then
	export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGDIR="${ROOT}/build-probe"
CXX="${CXX:-x86_64-w64-mingw32-g++}"

FLAGS=(
	-std=c++20
	-fsyntax-only
	-fpermissive
	-finput-charset=UTF-8
	-fexec-charset=CP932
	-DUNICODE -D_UNICODE -DNOMINMAX -DWINVER=0x0601 -D_WIN32_WINNT=0x0601
	-I "${ROOT}/compat/include"
	-I "${ROOT}/src"
	-include vcl_shim.h
)

mkdir -p "$LOGDIR"

# 未移植 = cmake/phase0_sources.cmake に名前が無いもの
unported() {
	for f in "${ROOT}"/src/*.cpp; do
		b="$(basename "$f")"
		grep -q "$b" "${ROOT}/cmake/phase0_sources.cmake" || echo "${b%.cpp}"
	done
}

if [[ $# -gt 0 ]]; then
	name="${1%.cpp}"
	"$CXX" "${FLAGS[@]}" "${ROOT}/src/${name}.cpp" >"${LOGDIR}/${name}.log" 2>&1
	echo "=== ${name}.cpp のエラー分類 (多い順) ==="
	grep ' error: ' "${LOGDIR}/${name}.log" \
		| sed -E "s/.* error: //; s/'[^']*'/'X'/g; s/[0-9]+/N/g" \
		| sort | uniq -c | sort -rn | head -25
	echo
	echo "=== 不足しているヘッダ ==="
	grep -oE "fatal error: [^:]+: No such file" "${LOGDIR}/${name}.log" | sort -u
	exit 0
fi

printf '%-18s %8s %8s\n' FILE LINES ERRORS
printf '%s\n' "----------------------------------------------"
for name in $(unported); do
	src="${ROOT}/src/${name}.cpp"
	"$CXX" "${FLAGS[@]}" "$src" >"${LOGDIR}/${name}.log" 2>&1
	printf '%-18s %8s %8s\n' "$name" "$(wc -l <"$src")" "$(grep -c ' error: ' "${LOGDIR}/${name}.log")"
done
