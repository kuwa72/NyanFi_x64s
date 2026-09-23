/**
 * @file gui/cre_dirs.h
 * @brief 階層的ディレクトリ作成ダイアログの判定・リスト変換 (wx 非依存)
 *
 * @details VCL の実測元は `src/CreDirsDlg.cpp:22-318`、
 *          `src/CreDirsDlg.dfm:57-353`、`src/MainFrm.cpp:15665-15697`、
 *          コマンド表は `src/usr_cmdlist.cpp:57` です。
 *          リストへの連番/文字列/日付の付加と、作成対象になる行の判定だけを
 *          ここへ移す。実作成は既存の `file_ops2::CreateDirs` に委ねる。
 *
 *          未移植 (未実装扱い):
 *          - VCL の Global.cpp 依存の get_SubDirs/ApplyCnvCharList、進捗表示
 *          - リストの保存/読み込み、ドラッグ＆ドロップ、ini の位置保存
 */
#ifndef NYANFI_GUI_CRE_DIRS_H
#define NYANFI_GUI_CRE_DIRS_H

#include <vector>

#include "usr_str.h"
#include "compat/datetime.h"

namespace cre_dirs {

/// 日付書式から VCL が増やす単位を決める (CreDirsDlg.cpp:141-149)
enum class DateUnit {
	Day = 0,
	Month = 1,
	Year = 2,
};
DateUnit ResolveDateUnit(const UnicodeString &format);

/// 連番を付加できる条件 (AddSerActionUpdate 相当)
bool CanAddSerial(const UnicodeString &start, int increment);

/// 各行へ連番を前置き/後置する。start の桁数を保つ
std::vector<UnicodeString> AddSerial(const std::vector<UnicodeString> &lines,
                                     int start, int increment, bool before,
                                     int number_width = 0);

/// 各行へ文字列を前置き/後置する
std::vector<UnicodeString> AddText(const std::vector<UnicodeString> &lines,
                                   const UnicodeString &text, bool before);

/// 日付出力。書式不正/日付不正なら false を返し error_out を設定
bool AddDate(const std::vector<UnicodeString> &lines, const TDateTime &date,
             const UnicodeString &format, bool before,
             std::vector<UnicodeString> &out, UnicodeString &error_out);

/// 空行を除いた、作成に渡す名前
std::vector<UnicodeString> CreatableEntries(const std::vector<UnicodeString> &lines);

/// 作成対象数と空行数
struct Validation {
	int total = 0;
	int creatable = 0;
	int blank = 0;
	bool valid = false;
};
Validation ValidateEntries(const std::vector<UnicodeString> &lines);

/// ダイアログが保持する状態
struct DialogState {
	std::vector<UnicodeString> entries;
	int serial_start = 1;
	int serial_increment = 1;
	bool serial_before = true;
	UnicodeString text;
	bool text_before = true;
	UnicodeString date_format = _T("yyyy/mm/dd");
	UnicodeString date_text;
	bool date_before = true;
	UnicodeString reference_dir;
};

}  // namespace cre_dirs

#endif  // NYANFI_GUI_CRE_DIRS_H
