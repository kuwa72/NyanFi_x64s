/**
 * @file gui/pre_same.h
 * @brief 同名時処理の事前指定ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/PreSameDlg.cpp` (`TPreSameNemeDlg`) と
 *          `src/MainFrm.cpp:28454` / `28603` の Copy/Move PR パラメータを実測した。
 *          wx のダイアログは `gui/pre_same_dialog.h`。
 *
 *          未移植 (未実装扱い):
 *          - 「作成時に確認」を同名項目ごとの逐一確認ではなく、衝突一覧の一括確認にする簡略化
 */
#ifndef NYANFI_GUI_PRE_SAME_H
#define NYANFI_GUI_PRE_SAME_H

#include "gui/f_misc_ops.h"
#include "gui/file_ops.h"
#include "usr_str.h"

namespace pre_same {

/// DFM の一覧順。VCL の PreMode と同じ 0 始まり
enum class Mode {
	Automatic = 0,
	Ask = 1,
	Newest = 2,
	Skip = 3,
	AutoRename = 4
};

inline Mode NormalizeMode(int index);

inline bool IsRequested(const UnicodeString &param)
{
	return f_misc_ops::HasParamToken(param, _T("PR"));
}

inline UnicodeString ModeLabel(Mode mode)
{
	switch (NormalizeMode(static_cast<int>(mode))) {
	case Mode::Ask:       return _T("作成時に確認");
	case Mode::Newest:    return _T("最新から上書き");
	case Mode::Skip:      return _T("スキップ");
	case Mode::AutoRename: return _T("自動的に名前を交互.swap");
	default:              return _T("実行者に委ねる");
	}
}

inline Mode NormalizeMode(int index)
{
	if (index < 0 || index > static_cast<int>(Mode::AutoRename)) return Mode::Automatic;
	return static_cast<Mode>(index);
}

/// VCL の CPYMD_* (src/Global.h:331) に対応。自動は -1
inline int LegacyCopyMode(Mode mode)
{
	return mode == Mode::Automatic? -1 : static_cast<int>(mode) - 1;
}

inline file_ops::ConflictPolicy PolicyFor(Mode mode)
{
	switch (NormalizeMode(static_cast<int>(mode))) {
	case Mode::Ask:       return file_ops::ConflictPolicy::Overwrite;
	case Mode::Newest:    return file_ops::ConflictPolicy::NewestWins;
	case Mode::AutoRename: return file_ops::ConflictPolicy::AutoRename;
	default:              return file_ops::ConflictPolicy::SkipExisting;
	}
}

/// 「作成時に確認」だけ、コピー/移動前に衝突是否存在を問い合わせる
inline bool RequiresConfirmation(Mode mode)
{
	return mode == Mode::Ask;
}

}  // namespace pre_same

#endif  // NYANFI_GUI_PRE_SAME_H
