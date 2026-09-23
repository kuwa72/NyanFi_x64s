/**
 * @file gui/find_txt.h
 * @brief テキストビューア内検索の判定・検索 (wx 非依存)
 *
 * @details VCL の実測元は `src/FindTxtDlg.cpp:25-319`、
 *          `src/MainFrm.cpp:19167-19171` (FindTextActionExecute)、
 *          `src/TxtViewer.cpp:5396-5399` (FindText を利用可能判定する箇所)、
 *          `src/usr_cmdlist.cpp:439` (V:FindText) です。
 *          検索語の状態解決と行探索はここへ切り出し、wx ダイアログと
 *          TextViewer は同じ関数を使う。
 *
 *          未移植 (未実装扱い):
 *          - Migemo 辞書を使う検索 (usr_Migemo は wx 非依存 core ではない)
 *          - バイナリバイト列検索と実際の強調描画
 *          - VCL の検索履歴/ini 位置、GeneralInfoDlg 側の検索
 */
#ifndef NYANFI_GUI_FIND_TXT_H
#define NYANFI_GUI_FIND_TXT_H

#include <vector>

#include "usr_str.h"

namespace find_txt {

/// src/FindTxtDlg.dfm:110-124 の方向ラジオ
enum class Direction {
	Up = 0,
	Down = 1,
};

/// 検索ダイアログの入力状態
struct Options {
	UnicodeString keyword;
	bool case_sensitive = false;
	bool whole_word = false;
	bool regex = false;
	bool migemo = false;
	bool bytes = false;
	bool highlight = true;
	bool close_after = false;
	Direction direction = Direction::Down;
	int code_page = 932;
};

/// バイナリ表示かどうかから各操作部品の有効状態を決める
struct Availability {
	bool binary_panel = false;
	bool bytes = false;
	bool word = false;
	bool regex = false;
	bool migemo = false;
	bool highlight = false;
	bool code_page = false;
};
Availability ResolveAvailability(bool binary);

/// 排他条件とコードページを正規化する (FindOptChanged/Migemo/RegEx click 相当)
Options Normalize(const Options &in, bool binary);

/// 検索式的妥当性を確認する。失敗時は error_out に理由を入れる
bool Validate(const Options &opt, UnicodeString &error_out);

/// 1行を条件照合する内部公開関数 (テストと TextViewer から使う)
bool LineMatches(const UnicodeString &line, const Options &opt);

/**
 * @brief 現在行の次/前行を検索する
 * @param from_line 0始まりの基準行 (最初は次行から。折り返し後は現在行も再評価)
 * @param direction Down/Up。端では VCL と同じく折り返す
 * @return 見つかった行 (0始まり)。無ければ -1
 */
int FindNextLine(const std::vector<UnicodeString> &lines, const Options &opt,
                 int from_line, Direction direction = Direction::Down);

/// Direction の index 変換
int DirectionIndex(Direction direction);
Direction DirectionFromIndex(int index);

}  // namespace find_txt

#endif  // NYANFI_GUI_FIND_TXT_H
