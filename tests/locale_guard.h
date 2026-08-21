/**
 * @file tests/locale_guard.h
 * @brief ANSI コードページに依存する検証を切り分けるためのガード
 *
 * NyanFi は日本語 Windows (ACP=932) を前提に作られており、以下は実行環境の
 * ANSI コードページに依存する。これは C++Builder 版から引き継いだ挙動で、
 * 移植で持ち込んだものではない。
 *
 *   - `AnsiString` との相互変換 (CP_ACP)。ACP≠932 では日本語が往復しない
 *   - `str_len_half` / `max_len_half` の半角換算幅。`AnsiString` のバイト長で
 *     全角判定をしているため、ACP≠932 では全角文字が 1 と数えられる
 *
 * GitHub Actions のランナーは英語版 Windows (ACP=1252) なので、これらは
 * 検証できない。黙って落ちる/消えるのを避けるため、スキップした事実を
 * doctest の出力に残す。
 *
 * 恒久対策の候補 (未実施):
 *   - 幅の判定を East Asian Width ベースに置き換える (ロケール非依存になる)
 *   - 実行ファイルのマニフェストに activeCodePage を指定してプロセスの ACP を
 *     固定する (Windows 11 / Server 2025 以降)
 */
#ifndef NYANFI_TESTS_LOCALE_GUARD_H
#define NYANFI_TESTS_LOCALE_GUARD_H

#include "compat/config.h"  //GetACP のために windows.h を取り込む
#include "doctest/doctest.h"

/// 実行環境の ANSI コードページが CP932 か
inline bool nyanfi_acp_is_932()
{
	return ::GetACP() == 932;
}

/// ACP が 932 でなければ、理由を出力してテストケースを打ち切る
#define NYANFI_REQUIRE_ACP_932()                                                          \
	do {                                                                                  \
		if (!nyanfi_acp_is_932()) {                                                       \
			MESSAGE("skipped: ANSI code page is not 932 (CP_ACP dependent check)");        \
			return;                                                                       \
		}                                                                                 \
	} while (0)

#endif  // NYANFI_TESTS_LOCALE_GUARD_H
