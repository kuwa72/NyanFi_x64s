/**
 * @file gui/key_map.cpp
 * @brief キー割り当て表の実装
 */
#include "gui/key_map.h"

#include "usr_cmdlist.h"
#include "usr_key.h"

//---------------------------------------------------------------------------
KeyMap::KeyMap() : entries_(new TStringList())
{
	LoadDefaults();
}

//---------------------------------------------------------------------------
/**
 * @brief 既定のキー割り当て
 * @details VCL 版は ini の [Key] セクションから読む。Phase 2 の骨格では
 *          実装済みのコマンドだけを既定として持つ。コマンド名は
 *          usr_cmdlist.cpp のコマンド表と同じ綴りを使う。
 */
void KeyMap::LoadDefaults()
{
	// カーソル移動 (UP/DOWN/LEFT/RIGHT は get_CsrKeyCmd が持っているので書かない)
	Assign(_T("PGUP"), _T("PageUp"));
	Assign(_T("PGDN"), _T("PageDown"));
	Assign(_T("HOME"), _T("CursorTop"));
	Assign(_T("END"), _T("CursorEnd"));

	// 移動・実行
	Assign(_T("ENTER"), _T("Execute"));
	Assign(_T("BKSP"), _T("UpDir"));
	Assign(_T("TAB"), _T("ChangePane"));
	Assign(_T("F5"), _T("Refresh"));

	// マーク
	Assign(_T("SPACE"), _T("MarkItem"));
	Assign(_T("Ctrl+A"), _T("MarkAll"));
	Assign(_T("Ctrl+D"), _T("UnMarkAll"));

	// 表示・終了
	Assign(_T("F1"), _T("ShowKeyList"));
	Assign(_T("F12"), _T("ShowCmdList"));
	Assign(_T("Alt+F4"), _T("Exit"));
	Assign(_T("Ctrl+Q"), _T("Exit"));
}

//---------------------------------------------------------------------------
void KeyMap::Assign(const UnicodeString &key_str, const UnicodeString &command)
{
	const int idx = entries_->IndexOfName(key_str);
	if (idx != -1) {
		entries_->ValueFromIndex[idx] = command;
	}
	else {
		UnicodeString line;
		entries_->Add(line.sprintf(_T("%s=%s"), key_str.c_str(), command.c_str()));
	}
}

//---------------------------------------------------------------------------
UnicodeString KeyMap::Lookup(const UnicodeString &key_str) const
{
	if (key_str.IsEmpty()) return EmptyStr;

	// カーソルキーは移植済みの割り当てをそのまま使う
	const UnicodeString csr = get_CsrKeyCmd(key_str);
	if (!csr.IsEmpty()) return csr;

	return entries_->Values[key_str];
}

//---------------------------------------------------------------------------
UnicodeString KeyMap::KeyStrOf(const wxKeyEvent &event)
{
	// wx の修飾キー状態を VCL の TShiftState に移す。移植済みの
	// get_KeyStr(WORD, TShiftState) をそのまま使えるようにするため
	TShiftState shift;
	if (event.ShiftDown()) shift << ssShift;
	if (event.ControlDown()) shift << ssCtrl;
	if (event.AltDown()) shift << ssAlt;

	// wx の KeyCode は MSW では仮想キーコードと同じ値になる
	const WORD vk = static_cast<WORD>(event.GetKeyCode());
	return get_KeyStr(vk, shift);
}
