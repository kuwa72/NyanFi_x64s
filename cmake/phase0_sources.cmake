# Phase 0 でシムのみでビルドを通す対象ファイル
#
# 選定基準: Vcl.* を include せず、TForm / __published にも触れず、
# Global.h / MainFrm.h などの GUI グローバルにも依存しないファイル。
#
# ビルドが通ったものだけをここに載せる。通らなかったものは
# docs/port/phase0-report.md に理由付きで記録する。
# 候補全体の通過状況は scripts/probe.sh が出力する。

set(NYANFI_PHASE0_SOURCES
	# ここに検証済みのファイルを追加していく
)
