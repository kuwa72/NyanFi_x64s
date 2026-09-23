/**
 * @file gui/sort_mode.h
 * @brief ソートダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/SrtModDlg.cpp` (`TSortModeDlg`) と
 *          `src/MainFrm.cpp:26222-26384` (`SortDlgActionExecute`) の設定解決を
 *          純粋関数として切り出したもの。wx の入力ダイアログは
 *          `gui/sort_mode_dialog.h` が担当する。
 *
 *          実測した VCL の並び (SortIdStr = `FEDSAU`) と、
 *          `XNX` / `XNI` を含むディレクトリ指定を保持する。
 *
 *          未移植 (未実装扱い):
 *          - 2段目の実際の比較 (FilePane は単一 SortKey のみ)
 *          - 論理比較・自然順・優先拡張子リストの実際の比較
 *          - 更新日時アクセラレータ/同じキーで閉じる操作
 *          - 結果リストの場所順 (`SortDlg_L`)、画像ビューア固有の並べ替え
 */
#ifndef NYANFI_GUI_SORT_MODE_H
#define NYANFI_GUI_SORT_MODE_H

#include <array>

#include "gui/file_item.h"

namespace sort_mode {

/// VCL の SortMode[tag] (0〜5)
enum class Mode {
	Name = 0,
	Extension = 1,
	Date = 2,
	Size = 3,
	Attribute = 4,
	None = 5,
};

/// VCL の DirSortMode[tag] (0〜5、XNI のアイコン指定は 6)
enum class DirectoryMode {
	SameAsFile = 0,
	Name = 1,
	Date = 2,
	Size = 3,
	Attribute = 4,
	Mixed = 5,
	Icon = 6,
};

/// ソートダイアログで編集する設定
struct Options {
	Mode mode = Mode::Name;
	DirectoryMode dir_mode = DirectoryMode::SameAsFile;
	bool natural = true;
	bool descending_name = false;
	bool descending_old = false;
	bool descending_small = false;
	bool descending_attribute = false;
	bool both = false;
	bool logical = false;
	bool acc_date_time = false;
	bool same_close = false;
	bool show_extended = false;
	UnicodeString extension_list;
	// VCL の SubSortMode[0..4]。0 は名前順で「なし」固定。
	std::array<Mode, 5> secondary = {Mode::None, Mode::Name, Mode::Extension,
	                                 Mode::Extension, Mode::Extension};
};

/// コマンドパラメータを適用した結果
struct ParamResult {
	Options options;
	bool recognized = false;
	bool changed = false;
	/// `L` (結果リストの場所順) は wx の FilePane では未実装
	bool path_sort = false;
};

/// FilePane に渡す既存 API 用の値
struct PaneSettings {
	SortKey key = SortKey::Name;
	bool descending = false;
	bool dirs_first = true;
};

/// 範囲外の添字を VCL と同じ既定値に丸める
Mode FromIndex(int index);
DirectoryMode DirectoryFromIndex(int index);
int ToIndex(Mode mode);
int ToIndex(DirectoryMode mode);

/// 2段ソートの選択可否 (VCL PrimeComboBoxClick 相当)
bool IsSubModeEnabled(int primary, int secondary);
int NormalizeSubMode(int primary, int secondary);

/// 値域と保存時に不正な値を正規化する
Options Normalize(Options options);

/// `SortDlg` の ActionParam を適用する
ParamResult ApplyParam(const Options &current, const UnicodeString &param);

/// 現在の設定から FilePane の単一キーの設定を作る
PaneSettings ToPaneSettings(const Options &options);
SortKey KeyForMode(Mode mode);
bool DescendingForMode(const Options &options, Mode mode);

}  // namespace sort_mode

#endif  // NYANFI_GUI_SORT_MODE_H
