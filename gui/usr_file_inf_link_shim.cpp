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
 *   1. 意味を変えずに用意できるもの → 本物を使う
 *      LoadUsrMsg/UserAbort/TextAbort (src/usr_msg.h) の実体は src/usr_msg.cpp
 *      に移植済み (port/phase2)。メッセージ文字列テーブルと Abort 系関数は
 *      GUI に依存しないため (`scripts/probe.sh usr_msg` で確認済み)、
 *      cmake/phase0_sources.cmake に載せて nyanfi_core にリンクしている。
 *      以前ここにあった「メッセージIDを含む簡易文字列を返す簡易版」は削除した
 *      (docs/port/phase0-report.md §13.8 で指摘されていた不一致の解消)。
 *      メッセージボックス表示そのもの (msgbox_ERR 等) は VCL の
 *      CreateMessageDialog/TForm::ShowModal に依存するため src/usr_msg_dlg.cpp
 *      に分離したままで、こちらは未移植 (呼ばれたらリンクエラー)。
 *      現状 gui/ からは呼ばれていない。
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
// 1. 本物を使う (LoadUsrMsg/UserAbort/TextAbort は src/usr_msg.cpp が提供。
//    ここでは何もしない)
//---------------------------------------------------------------------------

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
