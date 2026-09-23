/**
 * @file gui/color_settings.cpp
 * @brief gui/color_settings.h の実装 (wx 非依存)
 */
#include "gui/color_settings.h"

namespace color_settings {

namespace {

// TColorDlg::FormCreate (src/ColDlg.cpp:27-64) の ColorListBox->Items->Text。
// "Comment=|コメントの文字色" の先頭 "|" を含め原文どおり
const wchar_t *kItemText =
	L"bgView=背景色\n"
	L"fgView=文字色\n"
	L"Margin=余白白\n"
	L"bgRuler=ルーラの背景色\n"
	L"fgRuler=ルーラの目盛色\n"
	L"Cursor=ラインカーソルの色\n"
	L"selItem=選択項目の背景色\n"
	L"fgSelItem=選択項目の文字色\n"
	L"bgLineNo=行番号背景色\n"
	L"LineNo=行番号文字色\n"
	L"Mark=行マーク\n"
	L"bdrLine=行番号境界線\n"
	L"Indent=インデントガイド\n"
	L"Indent2=インデントガイド(交互)\n"
	L"bdrFold=折り返し境界線\n"
	L"bdrFixed=固定長表示の縦罫線\n"
	L"Comment=|コメントの文字色\n"
	L"Strings=文字列の文字色\n"
	L"Reserved=予約語の文字色\n"
	L"Symbol=シンボルの文字色\n"
	L"Numeric=数値の文字色\n"
	L"fgEmpBin1=バイナリ強調文字色1\n"
	L"fgEmpBin2=バイナリ強調文字色2\n"
	L"fgEmpBin3=バイナリ強調文字色3\n"
	L"Headline=見出しの文字色\n"
	L"Ruby=ルビ\n"
	L"URL=URLの文字色\n"
	L"LocalLink=ローカルファイルへのリンク\n"
	L"fgEmp=強調文字色\n"
	L"bgEmp=強調背景色\n"
	L"TAB=タブ表示色\n"
	L"CR=改行表示色\n"
	L"HR=横罫線の色\n"
	L"Ctrl=コントロールコード\n"
	L"fgPair=対応する括弧の文字色\n"
	L"Folder=ディレクトリの文字色\n"
	L"Error=エラーの文字色\n";

// DisableColActionUpdate (src/ColDlg.cpp:258) の判定リスト
const wchar_t *kDisablableKeys =
	L"fgSelItem|bdrLine|Indent2|bdrFold|bdrFixed|fgPair";

}  // namespace

//---------------------------------------------------------------------------
const std::vector<ColorItem> &ColorItems()
{
	static const std::vector<ColorItem> items = [] {
		std::vector<ColorItem> v;
		UnicodeString text(kItemText);
		while (!text.IsEmpty()) {
			UnicodeString line = split_tkn(text, _T("\n"));
			if (line.IsEmpty()) continue;
			ColorItem it;
			UnicodeString rest = line;
			it.key = split_tkn(rest, _T("="));
			it.caption = rest;
			v.push_back(it);
		}
		return v;
	}();
	return items;
}

//---------------------------------------------------------------------------
const ColorItem *FindItem(const UnicodeString &key)
{
	for (const ColorItem &it : ColorItems()) {
		if (SameText(it.key, key)) return &it;
	}
	return nullptr;
}

//---------------------------------------------------------------------------
int ParseColorValue(const UnicodeString &s, int def)
{
	return s.ToIntDef(def);
}

//---------------------------------------------------------------------------
UnicodeString FormatColorValue(int color)
{
	return IntToStr(color);
}

//---------------------------------------------------------------------------
int DisabledColor()
{
	return kDisabledColor;
}

//---------------------------------------------------------------------------
bool CanDisable(const UnicodeString &key)
{
	return contained_wd_i(UnicodeString(kDisablableKeys), key);
}

//---------------------------------------------------------------------------
std::vector<ColorEntry> EnsureEntries(const std::vector<ColorEntry> &current, int def)
{
	std::vector<ColorEntry> out;
	for (const ColorItem &it : ColorItems()) {
		ColorEntry e;
		e.key = it.key;
		e.color = def;
		const ColorEntry *found = FindEntry(current, it.key);
		if (found != nullptr) e.color = found->color;
		out.push_back(e);
	}
	return out;
}

//---------------------------------------------------------------------------
const ColorEntry *FindEntry(const std::vector<ColorEntry> &entries,
                            const UnicodeString &key)
{
	for (const ColorEntry &e : entries) {
		if (SameText(e.key, key)) return &e;
	}
	return nullptr;
}

//---------------------------------------------------------------------------
void SetEntry(std::vector<ColorEntry> &entries, const UnicodeString &key, int color)
{
	for (ColorEntry &e : entries) {
		if (SameText(e.key, key)) {
			e.color = color;
			return;
		}
	}
}

//---------------------------------------------------------------------------
bool DisableEntry(std::vector<ColorEntry> &entries, const UnicodeString &key)
{
	if (!CanDisable(key)) return false;
	SetEntry(entries, key, DisabledColor());
	return true;
}

}  // namespace color_settings
