/**
 * @file gui/key_map.cpp
 * @brief キー割り当て表の実装 (ini 読み込みを含む、wx 非依存の部分)
 *
 * @details wx に依存する KeyStrOf() の実装だけは gui/key_map_wx.cpp に分けてある。
 *          こちらは UsrIniFile と usr_str.h/usr_cmdlist.h/usr_key.h の
 *          UnicodeString ベースの API しか使わないため wx 無しでも単体テストできる
 *          (tests/core/test_gui_settings.cpp、CMakeLists.txt の
 *          `nyanfi_gui_core` ライブラリ)。
 */
#include "gui/key_map.h"

#include <memory>

#include "UIniFile.h"
#include "usr_cmdlist.h"
#include "usr_key.h"
#include "usr_str.h"

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

	// 並べ替え・絞り込み。"S" は src/Global.cpp の既定キー表にある実際の割り当て
	// ("F:S=SortDlg") と同じ。Ctrl+M / Ctrl+U は既定キー表に対応する記載が無く
	// (パスマスクはコンボボックス操作が前提で単独のキー割り当てが見当たらない)、
	// Phase 2 骨格向けに新規で決めたもの (推測)
	Assign(_T("S"), _T("SortDlg"));
	Assign(_T("Ctrl+M"), _T("SetPathMask"));
	Assign(_T("Ctrl+U"), _T("ClearMask"));

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
// KeyMap::KeyStrOf() は wx に依存するため gui/key_map_wx.cpp に定義がある
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/**
 * @brief KeyFuncList の1エントリを解析する
 * @details 形式の実測結果は gui/key_map.h 冒頭のコメントを参照。
 *          "F:" (ファイルペイン) 以外のモード、SELECT+ 付き、2ストローク
 *          ('~' を含む) は Phase 2 骨格が対応する仕組みを持たないため、
 *          誤動作させるより読み飛ばす方を選び false を返す。
 */
bool KeyMap::ParseKeyFuncListEntry(
	const UnicodeString &name, const UnicodeString &value,
	UnicodeString &key_str_out, UnicodeString &command_out)
{
	// モード文字 "F:" (ファイルペイン) 以外は非対応
	static const UnicodeString kModePrefix = _T("F:");
	if (!StartsText(kModePrefix, name)) return false;

	UnicodeString key_str = name.SubString(kModePrefix.Length() + 1);
	if (key_str.IsEmpty()) return false;

	// SELECT+ (選択操作の修飾子。src/Global.cpp の KeyStr_SELECT = "SELECT+")
	// は対応する選択モデルが無いため非対応
	if (StartsText(_T("SELECT+"), key_str)) return false;

	// 2ストロークキー ("Ctrl+K~D" 等) は非対応
	if (ContainsStr(key_str, _T("~"))) return false;

	if (value.IsEmpty()) return false;

	key_str_out = key_str;
	command_out = value;
	return true;
}

//---------------------------------------------------------------------------
/**
 * @brief ini の KeyFuncList セクションから読み込み、既定の割り当てに上書きする
 * @details 読み込みは寛容にする (ini が無い/セクションが無い/1行も解釈でき
 *          ないときは何もせず既定のまま)。書き込みは一切行わない
 *          (UsrIniFile::UpdateFile を呼ばない) ので、VCL 版の既存 ini を
 *          読むだけなら壊れる心配が無い。
 */
void KeyMap::LoadFromIni(const UnicodeString &ini_path)
{
	if (!FileExists(ini_path)) return;

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	std::unique_ptr<TStringList> lst(new TStringList());
	ini->ReadSection(_T("KeyFuncList"), lst.get());

	for (int i = 0; i < lst->Count; ++i) {
		UnicodeString key_str, command;
		if (ParseKeyFuncListEntry(lst->Names[i], lst->ValueFromIndex[i], key_str, command)) {
			Assign(key_str, command);
		}
	}
}
