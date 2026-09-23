/**
 * @file gui/cv_enc.h
 * @brief 文字コード変換ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/CvEncDlg.cpp` (`TCvTxtEncDlg`) と
 *          `src/MainFrm.cpp:29378` の `ConvertTextEncActionExecute` を実測した。
 *
 *          未移植 (未実装扱い):
 *          - XML/HTML の charset 宣言書き換え (MainFrm.cpp:29433)
 *          - VCL のユーザー編集メニュー、ダイアログ位置と設定の保存
 */
#ifndef NYANFI_GUI_CV_ENC_H
#define NYANFI_GUI_CV_ENC_H

#include <vector>

#include "usr_str.h"

namespace cv_enc {

/// SaveCodePages (UserMdl.cpp:25) から UTF-8N を除いた並び
inline const std::vector<UnicodeString> &EncodingNames()
{
	static const std::vector<UnicodeString> names = {
		_T("Shift_JIS"), _T("UTF-8"), _T("UTF-16"),
		_T("UTF-16(BE)"), _T("EUC-JP"), _T("ISO-2022-JP")
	};
	return names;
}

inline int NormalizeSelection(int index, int count)
{
	if (count <= 0 || index < 0 || index >= count) return 0;
	return index;
}

/// VCL (CvEncDlg.cpp:23) の並びに対応するコードページ
inline int CodePageFor(int index)
{
	switch (NormalizeSelection(index, static_cast<int>(EncodingNames().size()))) {
	case 0:  return 932;
	case 1:  return 65001;
	case 2:  return 1200;
	case 3:  return 1201;
	case 4:  return 20932;
	default: return 50220;
	}
}

/// VCL (CvEncDlg.cpp:58): 文字コード名に UTF が含まれる場合だけ BOM を選べる
inline bool BomAvailable(const UnicodeString &encoding)
{
	return ContainsText(encoding, _T("UTF"));
}

/// VCL (MainFrm.cpp:29472): 0=CR/LF, 1=LF, 2=CR
inline UnicodeString LineBreakFor(int index)
{
	if (index == 1) return _T("\n");
	if (index == 2) return _T("\r");
	return _T("\r\n");
}

inline bool CanSubmit(int selection)
{
	return selection >= 0 && selection < static_cast<int>(EncodingNames().size());
}

/// UTF-16(BE) の宣言名だけは VCL と同じく UTF-16 に揃える
inline UnicodeString DeclarationCharset(const UnicodeString &encoding)
{
	return SameText(encoding, _T("UTF-16(BE)"))? _T("UTF-16") : encoding;
}

/// VCL (MainFrm.cpp:29390) の表題補足
inline UnicodeString TitleSuffix(int selected_count, const UnicodeString &current_path)
{
	if (selected_count > 0) {
		UnicodeString out;
		out.sprintf(_T(" - 選択 %d"), selected_count);
		return out;
	}
	const UnicodeString name = ExtractFileName(current_path);
	return name.IsEmpty()? EmptyStr : _T(" - ") + name;
}

}  // namespace cv_enc

#endif  // NYANFI_GUI_CV_ENC_H
