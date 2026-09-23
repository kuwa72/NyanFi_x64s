/**
 * @file gui/dot_nyan.h
 * @brief .nyanfi 設定の入力・保存 (wx 非依存)
 *
 * @details VCL の `src/DotDlg.cpp` (`TDotNyanDlg`) の設定行の読み書きと、
 *          `NoOderCheckBox` を含む判断だけを移植する。wx の入力画面は
 *          `gui/dot_nyan_dialog.*`、実体の設定適用は `MainFrame` 側の
 *          未移植機能として扱う。
 *
 *          VCL 呼び出し位置 (grep 実測): `src/MainFrm.cpp:16792-16810`、
 *          コマンド表は `src/usr_cmdlist.cpp:80` と `732-734`。
 *          未移植 (未実装扱い): 色/spuit・画像/音声の参照と再生、コマンドファイル
 *          選択、継承の探索、削除、ファイル属性の複雑な切り替え、設定の適用。
 */
#ifndef NYANFI_GUI_DOT_NYAN_H
#define NYANFI_GUI_DOT_NYAN_H

#include <array>
#include <cstddef>

#include "compat/ustring.h"

namespace dot_nyan {

/// `.nyanfi` の色設定順 (src/DotDlg.cpp:160-161)。
inline constexpr std::array<const wchar_t *, 6> kColorNames = {
	_T("Color_bgDirInf"), _T("Color_fgDirInf"), _T("Color_bgDrvInf"),
	_T("Color_fgDrvInf"), _T("Color_Cursor"),    _T("Color_selItem"),
};

/// ダイアログが入力する設定。0 は VCL の「指定なし」を表す。
struct Options {
	int sort_mode = 0;  //!< SortIdStr "FEDSAU" の 0 始まり位置
	bool no_order = true;
	bool natural_order = false;
	bool dsc_name_order = false;
	bool small_order = false;
	bool old_order = false;
	bool dsc_attr_order = false;

	int show_hidden = 0;  //!< 0=指定なし、1=表示、2=非表示
	int show_system = 0;
	int show_byte_size = 0;
	int show_icon = 0;   //!< 0=指定なし、1=通常、2=ディレクトリのみ
	int sync_lr = 0;

	UnicodeString path_mask;
	UnicodeString grep_mask;
	UnicodeString list_width;
	UnicodeString play_sound;
	UnicodeString bg_image;
	UnicodeString description;
	UnicodeString exe_commands;  //!< @ を除いたコマンドファイル名
	bool handled = false;
	bool hidden = false;          //!< ファイル属性。設定行ではなく保存時に反映
	std::array<UnicodeString, 6> colors {};
};

struct ParseResult {
	bool ok = false;
	Options options;
	UnicodeString error;
};

/// VCL FormShow の新規ファイル時の既定値。
Options DefaultOptions();

/// 0/1/2/3 の値をダイアログの範囲に収める。
Options NormalizeOptions(const Options &options);

/// NoOderCheckBox と同じ「順序指定なし」判定。
bool IsNoOrder(const Options &options);

/// `.nyanfi` の設定文字列を解析する。不明な行は VCL と同じく無視する。
ParseResult ParseConfig(const UnicodeString &text);

/// Options を VCL と同じキー順・CRLF の設定文字列にする。
UnicodeString SerializeConfig(const Options &options);

/// 空の子の項目だけを上位設定から継承する。
Options MergeInherited(const Options &child, const Options &inherited);

/// ディレクトリから `.nyanfi` のファイル名を作る (ユーザー別 suffix は未移植)。
UnicodeString ConfigName(const UnicodeString &directory);

/// UTF-8 (BOM なし) で保存する。
bool SaveConfig(const UnicodeString &path, const Options &options, UnicodeString &error);

/// UTF-8/UTF-16/ACP を読み込んで解析する。
bool LoadConfig(const UnicodeString &path, Options &options, UnicodeString &error);

/// 設定ファイルを削除する。配色・音声・画像・継承の GUI 操作は別。
bool DeleteConfig(const UnicodeString &path);

}  // namespace dot_nyan

#endif  // NYANFI_GUI_DOT_NYAN_H
