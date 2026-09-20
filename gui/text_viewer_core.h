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
 * @param forced_code_page 0以外なら判定を上書きして強制デコードする
 *        (TxtViewer の ChangeCodePage→OpenTxtViewer(..., code_page, ...) 相当)
 * @return LoadResult
 */
LoadResult LoadForView(const UnicodeString &path, Int64 max_bytes = kMaxViewBytes,
                       int forced_code_page = 0);

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

//---------------------------------------------------------------------------
// Vモード操作の純関数 (src/TxtViewer.cpp 由来。行単位ビューア向けに単純化)
//---------------------------------------------------------------------------
/**
 * @brief カーソル行を delta だけ動かし 0..count-1 に収める
 * @details TTxtViewer::CursorDown/CursorUp は CurPos.y を 0..MaxDispLine-1 に
 *          収める (TxtViewer.cpp:3369-3392)。行単位ビューアでは同じことを
 *          current_line_ に対して行う。空ファイル (count<=0) では 0 を返す
 */
int StepLine(int cur, int delta, int count);

/**
 * @brief 1ページ分の行数 (MovePage相当)
 * @details TTxtViewer::MovePage は LineCount+1 行進む (TxtViewer.cpp:3395)。
 *          これは表示行数-1 と同値。0以下が来たら1を返す
 */
int PageStepLines(int visible_rows);

/**
 * @brief キーワードを含む行を次/前に探す (SearchDown/SearchUp相当)
 * @details 大小文字を区別しない (TextViewer::SearchForward と同じ簡略化。
 *          VCL 版の isCase/isRegEx 切替は非対応)。自行を飛ばし、次の行から
 *          探して末尾/先頭で折り返す。見つからなければ -1
 * @param lines 全行
 * @param kwd キーワード (空なら -1)
 * @param from_line 基準行 (0ベース。ここは含めず次から探す)
 * @param down true=下方向 / false=上方向
 */
int FindNextLine(const std::vector<UnicodeString> &lines, const UnicodeString &kwd,
                 int from_line, bool down);

/**
 * @brief 栞マークをトグルした新しい一覧を返す (MarkLine相当)
 * @details TTxtViewer::MarkLine は "lno;" 形式の文字列でトグルする
 *          (TxtViewer.cpp:4800)。ここでは0ベース行番号の vector で同じ
 *          トグルを行う。結果は昇順ソート済み
 */
std::vector<int> ToggleMark(const std::vector<int> &marks, int line);

/** @brief 全マーク解除 (ClearMark相当)。空の一覧を返す */
std::vector<int> ClearMarkList(const std::vector<int> &marks);

/**
 * @brief 次/前のマーク行を探す (FindMarkDown/FindMarkUp相当)
 * @details TTxtViewer::ExeCommand の FindMarkDown/Up は MarkListStr を走査し、
 *          現在行より大きい最小/小さい最大の行番号へ飛ぶ (TxtViewer.cpp:5196)。
 *          ここでは0ベースで同じことを行う。無ければ -1
 */
int FindMarkNext(const std::vector<int> &marks, int cur_line, bool down);

/**
 * @brief 行番号ジャンプ先を0ベースで返す (JumpLine相当)
 * @details テキスト表示の JumpLine は1ベース絶対指定のみで "+|=" を含むと
 *          Abort する (TxtViewer.cpp:4763-4787)。バイナリ表示は "+-n" の
 *          相対指定を受け付ける (TxtViewer.cpp:4751)。ここでは両方を受け付け:
 *          "+n/-n" は相対、"n" は1ベース絶対。範囲外・非数値・空は -1
 */
int ParseJumpLine(const UnicodeString &param, int cur_0based, int count);

/**
 * @brief 次の文字コードを返す (change_CodePage相当)
 * @details TxtViewer.cpp:4023 の循環表 (932→50220→20932→1252→65001→1200→932)。
 *          不明値は932へ
 */
int NextCodePage(int cur_code_page);

/**
 * @brief 文字コードパラメータを解釈する (change_CodePage相当)
 * @details 表にある値だけ有効で、それ以外は0 (無効)。空なら次へ進む
 */
int ParseCodePageParam(const UnicodeString &param, int cur_code_page);

/**
 * @brief 移動行数を解釈する (get_MovePrmの数値部分相当)
 * @details TxtViewer.cpp:4065 の get_MovePrm は HP/FP/MW 等の記号も扱うが、
 *          行単位ビューアでは数値のみ対応し、それ以外・0以下・空は1を返す
 */
int ParseMoveCount(const UnicodeString &param, int visible_rows);

}  // namespace text_viewer_core

#endif  // NYANFI_GUI_TEXT_VIEWER_CORE_H
