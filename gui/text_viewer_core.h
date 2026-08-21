/**
 * @file gui/text_viewer_core.h
 * @brief テキストビューアのロジック層 (wx 非依存)
 *
 * gui/text_viewer.h (wxWindow 版) から使う「ファイル読み込み・文字コード判定・
 * 行分割・折り返し計算」だけを切り出したもの。gui/file_pane.h/.cpp に対する
 * gui/file_item.h/.cpp と同じ関係で、wx に依存しないため
 * tests/core/test_gui_text_viewer.cpp から直接テストできる
 * (nyanfi_gui_core、ルート CMakeLists.txt に追加)。
 *
 * 文字コード判定は自前で書かず、移植済みの get_MemoryCodePage
 * (src/usr_str.cpp) をそのまま使う。BOM のスキップ幅の決め方
 * (UTF-16 は2バイト、UTF-8 は3バイト) は src/usr_file_inf.cpp の
 * get_top_line() と同じ判定を踏襲した。
 */
#ifndef NYANFI_GUI_TEXT_VIEWER_CORE_H
#define NYANFI_GUI_TEXT_VIEWER_CORE_H

#include <vector>

#include "usr_str.h"

namespace text_viewer_core {

/**
 * @brief 一度に読み込む最大バイト数 (8MB)
 * @details 大きなファイル (ログ等) を開いても固まらないための上限。
 *          VCL 版 (TxtViewer.cpp) に相当する明確な既定値は見当たらなかった
 *          ため、テキストビューアとして実用的な範囲で新規に決めた値
 *          (推測・要検証)。超えた場合は先頭 kMaxViewBytes だけを対象にし、
 *          LoadResult::truncated を true にする。
 */
constexpr Int64 kMaxViewBytes = 8LL * 1024 * 1024;

/** @brief LoadForView() の結果 */
struct LoadResult {
	bool ok = false;                   //!< 読み込みに成功したか (バイナリ判定も ok=true)
	UnicodeString error;                //!< 失敗時のメッセージ (ok=false のときのみ)

	int  code_page = 0;                 //!< 判定したコードページ (get_MemoryCodePage 準拠。0はここでは出さない)
	bool has_bom = false;               //!< BOM の有無
	bool is_binary = false;             //!< get_MemoryCodePage が -1 (バイナリ) と判定した

	Int64 file_size = 0;                //!< 実際のファイルサイズ
	Int64 read_size = 0;                //!< 実際に読み込んだバイト数 (kMaxViewBytes で切り詰められうる)
	bool  truncated = false;            //!< file_size > read_size (先頭だけ読んだ)

	std::vector<UnicodeString> lines;   //!< 改行で分割した内容 (各要素に改行コードは含まない)
};

/**
 * @brief ファイルを開いて表示用に読み込む
 * @details 先頭 max_bytes バイトだけを TFileStream→TMemoryStream::CopyFrom で
 *          読み、get_MemoryCodePage() でコードページを判定する
 *          (src/usr_file_inf.cpp の get_top_line() と同じ手順)。
 *          コードページが不明 (0) のときは get_MemoryStrins() と同じく 932
 *          (Shift_JIS) にフォールバックする。
 * @param path ファイルパス
 * @param max_bytes 読み込み上限バイト数
 * @return LoadResult
 */
LoadResult LoadForView(const UnicodeString &path, Int64 max_bytes = kMaxViewBytes);

/**
 * @brief 1文字の表示幅 (半角=1/全角=2の目安)
 * @details VCL 版 (TxtViewer::add_CharWidth/get_StrWidth) はフォントの実測
 *          (TCanvas::TextWidth) を使うが、ここは wx 非依存のロジックとして
 *          テストできるよう、Unicode のブロック範囲によるおおまかな判定に
 *          単純化した (推測・要検証。等幅フォントで全角相当のグリフが
 *          半角のちょうど2倍幅で描かれる前提)。
 * @param c 文字
 * @return int 1 または 2
 */
int CharDisplayWidth(wchar_t c);

/**
 * @brief 1行を指定した表示幅 (半角換算) で複数の表示行に折り返す
 * @param line 対象の1行 (改行コードを含まない)
 * @param width_cols 折り返し幅 (半角換算)。0以下なら折り返さない
 * @param tab_width タブ幅 (半角換算、既定4)
 * @return std::vector<UnicodeString> 折り返し後の各行 (最低1行を返す)
 */
std::vector<UnicodeString> WrapLine(const UnicodeString &line, int width_cols, int tab_width = 4);

}  // namespace text_viewer_core

#endif  // NYANFI_GUI_TEXT_VIEWER_CORE_H
