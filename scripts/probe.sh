#!/usr/bin/env bash
# Phase 0 候補ファイルを 1 本ずつ構文チェックし、通過状況の表を出す
#
#   scripts/probe.sh            全候補をチェックして表を表示
#   scripts/probe.sh usr_str    指定ファイルだけをチェックし、エラーを全部出す
#
# ログは build/probe/<name>.log に残る。エラーの分類は
# docs/port/phase0-report.md にまとめる。
set -uo pipefail

if [[ -d /home/linuxbrew/.linuxbrew/bin ]]; then
	export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGDIR="${ROOT}/build/probe"
CXX="${CXX:-x86_64-w64-mingw32-g++}"

# Vcl.* を include せず GUI グローバルにも依存しない候補 (行数の少ない順)
CANDIDATES=(
	usr_migemo
	usr_xd2tx
	usr_mmfile
	usr_key
	usr_id3
	usr_color
	file_filter
	usr_wic
	htmconv
	usr_exif
	usr_arc
	usr_cmdlist
	usr_file_ex
	usr_file_inf
	usr_str
)

FLAGS=(
	-std=c++20
	-fsyntax-only
	-municode
	-finput-charset=UTF-8
	-fexec-charset=CP932
	-DUNICODE -D_UNICODE -DNOMINMAX -DWINVER=0x0601 -D_WIN32_WINNT=0x0601
	-I "${ROOT}/compat/include"
	-I "${ROOT}/src"
	-include vcl_shim.h
)

mkdir -p "$LOGDIR"

if [[ $# -gt 0 ]]; then
	name="${1%.cpp}"
	"$CXX" "${FLAGS[@]}" "${ROOT}/src/${name}.cpp" 2>&1 | tee "${LOGDIR}/${name}.log"
	exit "${PIPESTATUS[0]}"
fi

printf '%-16s %8s %8s %s\n' FILE LINES ERRORS STATUS
printf '%s\n' "----------------------------------------------------"
total_pass=0
total_lines=0
pass_lines=0
for name in "${CANDIDATES[@]}"; do
	src="${ROOT}/src/${name}.cpp"
	lines=$(wc -l <"$src")
	total_lines=$((total_lines + lines))
	"$CXX" "${FLAGS[@]}" "$src" >"${LOGDIR}/${name}.log" 2>&1
	rc=$?
	errors=$(grep -c ' error: ' "${LOGDIR}/${name}.log")
	if [[ "$rc" == "0" ]]; then
		status=PASS
		total_pass=$((total_pass + 1))
		pass_lines=$((pass_lines + lines))
	else
		status=FAIL
	fi
	printf '%-16s %8s %8s %s\n' "$name" "$lines" "$errors" "$status"
done
printf '%s\n' "----------------------------------------------------"
printf '通過 %d/%d ファイル, %d/%d 行\n' \
	"$total_pass" "${#CANDIDATES[@]}" "$pass_lines" "$total_lines"
