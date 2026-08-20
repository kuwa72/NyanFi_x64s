/**
 * @file gui/usr_file_inf_link_shim.cpp
 * @brief src/usr_file_inf.cpp が (呼ばない関数を含めて) 参照するシンボルの橋渡し
 *
 * @details gui/vcl_gui_bridge.cpp と同じ理由・同じ方針の橋渡しだが、こちらは
 * wxWidgets に依存しない (core_tests からも呼ぶため。gui/CMakeLists.txt の
 * `nyanfi` 実行ファイルだけでなく `nyanfi_core` 自体に必要)。
 *
 * # なぜ必要か
 *
 * gui/file_info.cpp は src/usr_file_inf.cpp の一部の関数 (get_PdfVer 等) だけを
 * 呼ぶが、GNU ld の静的ライブラリはオブジェクトファイル単位で取り込まれるため、
 * 1つでも呼べば usr_file_inf.cpp.obj 全体が取り込まれ、**呼んでいない関数**
 * (get_AppInf / get_duration / get_DllExpFunc / get_MetafileInf 等) が参照する
 * シンボルも含めてリンク時に解決が必要になる
 * (`--gc-sections` はシンボル解決の後に走るため防げない。CLAUDE.md 規約5、
 * gui/vcl_gui_bridge.cpp のコメントと同じ事情)。
 *
 * 方針は gui/vcl_gui_bridge.cpp と同じ2通り。
 *
 *   1. 意味を変えずに用意できるもの → 簡易実装する
 *      LoadUsrMsg/UserAbort/TextAbort (src/usr_msg.h) の実体 (src/usr_msg.cpp)
 *      はメッセージボックス表示と一体で GUI 依存のため未移植
 *      (`scripts/probe.sh usr_msg` で確認済み)。実際に使われる先
 *      (get_PngInf 等の `catch (EAbort&)` 節、get_IconInf 等の異常系) は
 *      文字列を組み立てて例外に載せるか警告行に足すだけなので、実際の
 *      日本語文言 (usr_msg.cpp の大きなメッセージテーブル) の代わりに
 *      メッセージIDを含む簡易な文字列を返す簡易版で十分機能する
 *      (診断用と割り切る。表示文言の完全一致は求めない)。
 *
 *   2. 呼ばれたら確実に落とす → gui/file_info.cpp では呼んでいない経路
 *      - `UserShell::get_PropInf` / `get_Duration`: 実体 (src/usr_shell.cpp)
 *        は `IShellFolder` 等のシェル統合で GUI/COM に強く依存し未移植
 *        (`scripts/probe.sh usr_shell` で確認済み)。呼ぶのは get_AppInf /
 *        get_duration で、gui/file_info.cpp はどちらも呼ばない
 *        (gui/file_info.h の「GUI依存で使わなかった関数」を参照)。
 *      - `TMetafile::LoadFromFile`: `compat/gui_stubs.h` の「宣言のみ」の
 *        GUI スタブ (CLAUDE.md 規約4)。呼ぶのは get_MetafileInf
 *        (.wmf/.emf) で、gui/file_info.cpp では対象外にしている
 *        (gui/file_info.h を参照)。
 *
 * `get_DllExpFunc` が使う imagehlp の関数 (`MapAndLoad` 等) は実装が要る
 * スタブではなく実在の Win32 API (imagehlp.dll) なので、CMakeLists.txt で
 * `imagehlp` をリンクするだけで解決する (ここでは何もしない)。
 */
#include "usr_msg.h"
#include "usr_shell.h"

namespace {

/// 呼ばれたら確実に落とす (GUI/COM 依存で未移植のため。gui/vcl_gui_bridge.cpp
/// の not_ported() と同じ考え方。wx に依存しないので wxLogError は使わない)
[[noreturn]] void NotPorted(const UnicodeString &what)
{
	throw Exception(UnicodeString(_T("GUI/COM 依存のため wx 版では未実装です: ")) + what);
}

}  // namespace

//---------------------------------------------------------------------------
// 1. 簡易実装 (src/usr_msg.h)
//---------------------------------------------------------------------------
UnicodeString LoadUsrMsg(int id, UnicodeString s)
{
	UnicodeString msg;
	msg.sprintf(_T("エラー(#%d)"), id);
	if (!s.IsEmpty()) msg += _T(": ") + s;
	return msg;
}

UnicodeString LoadUsrMsg(int id, const _TCHAR *s)
{
	return LoadUsrMsg(id, UnicodeString(s));
}

UnicodeString LoadUsrMsg(int id, int id_s)
{
	return LoadUsrMsg(id, LoadUsrMsg(id_s));
}

/// Delphi の Abort() 相当 (メッセージID版)。呼び出し側はいずれも catch (...) や
/// catch (EAbort&) で受けるだけなので、例外を投げれば実際の動作は変わらない
void UserAbort(unsigned id)
{
	throw EAbort(LoadUsrMsg(static_cast<int>(id)));
}

/// Delphi の Abort() 相当 (メッセージ文字列版)
void TextAbort(const _TCHAR *msg)
{
	throw EAbort(UnicodeString(msg));
}

//---------------------------------------------------------------------------
// 2. 呼ばれたら落とす (未移植)
//---------------------------------------------------------------------------
UserShell *usr_SH = NULL;

UnicodeString UserShell::get_PropInf(UnicodeString, TStringList *, UnicodeString, bool)
{
	NotPorted(_T("UserShell::get_PropInf"));
}

int UserShell::get_Duration(UnicodeString)
{
	NotPorted(_T("UserShell::get_Duration"));
}

void TMetafile::LoadFromFile(const UnicodeString &)
{
	NotPorted(_T("TMetafile::LoadFromFile"));
}
