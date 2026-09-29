/**
 * @file tests/core/test_gui_color_settings.cpp
 * @brief gui/color_settings.cpp (配色ダイアログの判断ロジック) のテスト
 *
 * @details VCL の該当実装は `src/ColDlg.cpp` (`TColorDlg`)。
 *          配色項目の一覧は `FormCreate` の `ColorListBox->Items->Text`
 *          (37項目) を実測。無効化できるのは `DisableColActionUpdate` の
 *          `contained_wd_i("fgSelItem|bdrLine|Indent2|bdrFold|bdrFixed|fgPair")`
 *          に一致する6項目だけ。無効値は `col_None` (`DisableColActionExecute`
 *          の `IntToStr(col_None)` を実測)。色の読み書きは
 *          `RefColBtnClick` の `ToIntDef` / `IntToStr` を実測。
 */
#include "doctest/doctest.h"

#include "gui/color_settings.h"

TEST_CASE("ColorSettings: 項目は37件で先頭と末尾が一致する")
{
	const std::vector<color_settings::ColorItem> &items = color_settings::ColorItems();
	CHECK(items.size() == 37);
	CHECK(items.front().key == UnicodeString(_T("bgView")));
	CHECK(items.front().caption == UnicodeString(_T("背景色")));
	CHECK(items.back().key == UnicodeString(_T("Error")));
}

TEST_CASE("ColorSettings: FindItem はキーで引ける。無ければ nullptr")
{
	const color_settings::ColorItem *found = color_settings::FindItem(_T("Cursor"));
	REQUIRE(found != nullptr);
	CHECK(found->caption == UnicodeString(_T("ラインカーソルの色")));
	CHECK(color_settings::FindItem(_T("NoSuchKey")) == nullptr);
}

TEST_CASE("ColorSettings: 無効化できるのは6項目だけ")
{
	for (const wchar_t *key :
	     {L"fgSelItem", L"bdrLine", L"Indent2", L"bdrFold", L"bdrFixed", L"fgPair"}) {
		CHECK(color_settings::CanDisable(key));
	}
	CHECK(!color_settings::CanDisable(_T("bgView")));
	CHECK(!color_settings::CanDisable(_T("Cursor")));
	CHECK(!color_settings::CanDisable(_T("")));
}

TEST_CASE("ColorSettings: 色値の読み書きは ToIntDef/IntToStr と同じ")
{
	CHECK(color_settings::ParseColorValue(_T("255"), 0) == 255);
	CHECK(color_settings::ParseColorValue(_T(""), 7) == 7);
	CHECK(color_settings::ParseColorValue(_T("abc"), 7) == 7);
	CHECK(color_settings::FormatColorValue(255) == UnicodeString(_T("255")));
}

TEST_CASE("ColorSettings: EnsureEntries は37件に正規化する")
{
	std::vector<color_settings::ColorEntry> cur;
	cur.push_back({UnicodeString(_T("Cursor")), 123});
	const std::vector<color_settings::ColorEntry> fixed =
		color_settings::EnsureEntries(cur, 0);
	REQUIRE(fixed.size() == 37);
	CHECK(fixed.front().key == UnicodeString(_T("bgView")));
	const color_settings::ColorEntry *cursor = color_settings::FindEntry(fixed, _T("Cursor"));
	REQUIRE(cursor != nullptr);
	CHECK(cursor->color == 123);
}

TEST_CASE("ColorSettings: DisableEntry は対象外で false、対象で無効値を入れる")
{
	std::vector<color_settings::ColorEntry> entries =
		color_settings::EnsureEntries({}, 0);
	CHECK(!color_settings::DisableEntry(entries, _T("bgView")));
	CHECK(color_settings::DisableEntry(entries, _T("fgPair")));
	const color_settings::ColorEntry *fgpair =
		color_settings::FindEntry(entries, _T("fgPair"));
	REQUIRE(fgpair != nullptr);
	CHECK(fgpair->color == color_settings::DisabledColor());
}

//===========================================================================
// FormatColorPickerValue: カラーピッカーの色書式化 (ColorPicker)
// VCL: src/ColPicker.cpp:215-241 (UpdateStt の FmtComboBox 13形式)
//===========================================================================

TEST_CASE("FormatColorPickerValue: RGB(255,0,0) を各形式で書式化")
{
	const int red = 0x000000FF;  // TColor RGB(255,0,0)
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Rgb)
		== UnicodeString(_T("255,0,0")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Colorref)
		== UnicodeString(_T("0x000000ff")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Delphi)
		== UnicodeString(_T("$000000FF")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Vb)
		== UnicodeString(_T("&H000000FF&")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Html)
		== UnicodeString(_T("#ff0000")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::RgbParen)
		== UnicodeString(_T("RGB(0xff,0x00,0x00)")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::RgbDec)
		== UnicodeString(_T("RGB(255,0,0)")));
}

TEST_CASE("FormatColorPickerValue: HSV/HSL 形式 (赤は H0 S100 V100)")
{
	const int red = 0x000000FF;
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Hsv)
		== UnicodeString(_T("0,100,100")));
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Hsl)
		== UnicodeString(_T("0,100,50")));
}

TEST_CASE("FormatColorPickerValue: BITMAP 形式")
{
	const int red = 0x000000FF;
	CHECK(color_settings::FormatColorPickerValue(red, color_settings::ColorFormat::Bitmap32)
		== UnicodeString(_T("R255 G0 B0")));
}
