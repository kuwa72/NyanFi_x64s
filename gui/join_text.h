/**
 * @file gui/join_text.h
 * @brief テキストファイル結合ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/JoinDlg.cpp` (`TJoinTextDlg`) と
 *          `src/MainFrm.cpp:20151` の `JoinTextActionExecute` を実測した。
 *          ダイアログの選択肢・有効条件・結合順変更だけはこのヘッダに置き、
 *          wx 側は `gui/join_text_dialog.h`、実処理は既存
 *          `gui/text_ops.h` に分離する。
 *
 *          未移植 (未実装扱い):
 *          - テンプレートによる連結 (`ApplyTemplate`, MainFrm.cpp:20088)
 *          - 既存出力ファイルの上書き確認 (移植版は上書きせず失敗)
 *          - VCL のユーザー編集メニュー、ドラッグ操作、位置と設定の保存
 */
#ifndef NYANFI_GUI_JOIN_TEXT_H
#define NYANFI_GUI_JOIN_TEXT_H

#include <cstddef>
#include <vector>

#include "usr_str.h"

namespace join_text {

/// VCL の出力コード順 (src/JoinDlg.cpp:26)
inline constexpr int kEncodingCount = 6;

/// @return 0 始まり。範囲外は先頭の自動へ戻す
inline UnicodeString EncodingName(int index)
{
	switch (index) {
	case 1:  return _T("Shift_JIS");
	case 2:  return _T("ISO-2022-JP");
	case 3:  return _T("EUC-JP");
	case 4:  return _T("UTF-8");
	case 5:  return _T("UTF-16");
	default: return _T("自動(先頭のコードに統一)");
	}
}

inline int EncodingCount() { return kEncodingCount; }

/// @return 一致する選択肢の 0 始まり番号。未知なら 0
inline int EncodingIndex(const UnicodeString &name)
{
	for (int i = 0; i < kEncodingCount; ++i) {
		if (SameText(name, EncodingName(i))) return i;
	}
	return 0;
}

/// @return 自動なら 0、それ以外は指定コードページ。未知の名前も 0
inline int CodePageFor(const UnicodeString &name)
{
	for (int i = 1; i < kEncodingCount; ++i) {
		if (SameText(name, EncodingName(i))) {
			switch (i) {
			case 1:  return 932;
			case 2:  return 50220;
			case 3:  return 20932;
			case 4:  return 65001;
			case 5:  return 1200;
			}
		}
	}
	return 0;
}

/// VCL (TJoinTextDlg::JoinActionUpdate): BOM は UTF 系だけ選べる
inline bool BomAvailable(const UnicodeString &encoding)
{
	return StartsStr(_T("UTF-"), encoding);
}

/// VCL (MainFrm.cpp:20239): 0=CR/LF, 1=LF, 2=CR
inline UnicodeString LineBreakFor(int index)
{
	if (index == 1) return _T("\n");
	if (index == 2) return _T("\r");
	return _T("\r\n");
}

/// VCL (TJoinTextDlg::JoinActionUpdate): 1件以上かつ出力名が必要
inline bool CanSubmit(std::size_t source_count, const UnicodeString &output_name)
{
	return source_count > 0 && !Trim(output_name).IsEmpty();
}

/**
 * @brief 結合するファイル順を1件移動する
 * @return 移動後の位置。端を越えて移動しようとした場合は -1
 * @details VCL は `TStringList::Move` で挿入位置をずらす (UserMdl.cpp:184)。
 *          隣接要素との交換ではなく、対象を.Remove/Insert する
 */
inline int MoveSource(std::vector<UnicodeString> &sources, int index, int delta)
{
	if (index < 0 || static_cast<std::size_t>(index) >= sources.size()) return -1;
	const int target = index + delta;
	if (delta != -1 && delta != 1) return -1;
	if (target < 0 || static_cast<std::size_t>(target) >= sources.size()) return -1;

	UnicodeString value = sources[static_cast<std::size_t>(index)];
	sources.erase(sources.begin() + index);
	sources.insert(sources.begin() + target, value);
	return target;
}

inline bool CanMoveSource(std::size_t source_count, int index, int delta)
{
	if (index < 0 || static_cast<std::size_t>(index) >= source_count) return false;
	const int target = index + delta;
	return (delta == -1 || delta == 1) && target >= 0
	    && static_cast<std::size_t>(target) < source_count;
}

inline bool CanDeleteSource(std::size_t source_count, int index)
{
	return index >= 0 && static_cast<std::size_t>(index) < source_count;
}

/// @return 削除した名前。範囲外なら空
inline UnicodeString RemoveSource(std::vector<UnicodeString> &sources, int index)
{
	if (index < 0 || static_cast<std::size_t>(index) >= sources.size()) return EmptyStr;
	UnicodeString value = sources[static_cast<std::size_t>(index)];
	sources.erase(sources.begin() + index);
	return value;
}

/// VCL は現在のリストから反対ペインのディレクトリへ出力する
inline UnicodeString OutputPath(const UnicodeString &dst_dir, const UnicodeString &name)
{
	return IncludeTrailingPathDelimiter(dst_dir) + name;
}

}  // namespace join_text

#endif  // NYANFI_GUI_JOIN_TEXT_H
