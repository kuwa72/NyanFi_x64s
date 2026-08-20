# Phase 0 でシムのみでビルドを通す対象ファイル
#
# 選定基準: Vcl.* を include せず、TForm / __published にも触れず、
# Global.h / MainFrm.h などの GUI グローバルにも依存しないファイル。
#
# ビルドが通ったものだけをここに載せる。通らなかったものは
# docs/port/phase0-report.md に理由付きで記録する。
# 候補全体の通過状況は scripts/probe.sh が出力する。

set(NYANFI_PHASE0_SOURCES
	src/file_filter.cpp
	src/htmconv.cpp
	src/usr_arc.cpp
	src/usr_cmdlist.cpp
	src/usr_color.cpp
	src/usr_exif.cpp
	src/usr_file_ex.cpp
	src/usr_file_inf.cpp
	src/usr_id3.cpp
	src/usr_key.cpp
	src/usr_migemo.cpp
	src/usr_mmfile.cpp
	src/usr_str.cpp
	src/usr_wic.cpp
	src/usr_xd2tx.cpp
)

# 候補15ファイルすべてが通過している (2026-08-20)。
