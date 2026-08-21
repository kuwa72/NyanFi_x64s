/**
 * @file tests/core/test_gui_external.cpp
 * @brief gui/external.cpp のテスト
 *
 * @details 実際に起動はしない。**何をどう起動するか**だけを固定する。
 *          引数の組み立てを間違えると「起動はするが違う場所が開く」という
 *          気づきにくい壊れ方をするので、そこを見る。
 */
#include "doctest/doctest.h"

#include "gui/external.h"

TEST_CASE("ShellLaunchSpec: cmd と PowerShell は作業ディレクトリで渡す")
{
	// VCL の既定も作業ディレクトリを渡すだけ (MainFrm.cpp:14455 / 24049)
	const auto cmd = external::ShellLaunchSpec(external::ShellKind::CommandPrompt,
	                                            _T("C:\\work\\"));
	CHECK(cmd.file == UnicodeString(_T("cmd.exe")));
	CHECK(cmd.parameters.IsEmpty());
	CHECK(cmd.directory == UnicodeString(_T("C:\\work")));  // 末尾の区切りは落とす

	const auto ps = external::ShellLaunchSpec(external::ShellKind::PowerShell, _T("C:\\work"));
	CHECK(ps.file == UnicodeString(_T("powershell.exe")));
	CHECK(ps.directory == UnicodeString(_T("C:\\work")));
}

TEST_CASE("ShellLaunchSpec: Windows Terminal は -d で渡す")
{
	// wt.exe は作業ディレクトリを渡しても中のシェルに効かない
	const auto wt = external::ShellLaunchSpec(external::ShellKind::WindowsTerminal,
	                                           _T("C:\\my work\\"));
	CHECK(wt.file == UnicodeString(_T("wt.exe")));
	// 空白を含むパスが引用符で囲まれていること
	CHECK(wt.parameters == UnicodeString(_T("-d \"C:\\my work\"")));
}

TEST_CASE("ExplorerLaunchSpec: ディレクトリはそこを開く")
{
	const auto s = external::ExplorerLaunchSpec(_T("C:\\work\\"), true);
	CHECK(s.file == UnicodeString(_T("explorer.exe")));
	CHECK(s.parameters == UnicodeString(_T("\"C:\\work\"")));
}

TEST_CASE("ExplorerLaunchSpec: ファイルは選択した状態で開く")
{
	// /select, を付けないと「ファイルを実行してしまう」
	const auto s = external::ExplorerLaunchSpec(_T("C:\\work\\a.txt"), false);
	CHECK(s.parameters == UnicodeString(_T("/select,\"C:\\work\\a.txt\"")));
}

TEST_CASE("ExplorerLaunchSpec: 特殊フォルダの指定はそのまま渡す")
{
	// MainFrm.cpp:22596
	CHECK(external::ExplorerLaunchSpec(_T("shell:RecycleBinFolder"), true).parameters
	      == UnicodeString(_T("shell:RecycleBinFolder")));
	CHECK(external::ExplorerLaunchSpec(_T("::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"), true).parameters
	      == UnicodeString(_T("::{20D04FE0-3AEA-1069-A2D8-08002B30309D}")));
	CHECK(external::ExplorerLaunchSpec(_T("/n"), true).parameters == UnicodeString(_T("/n")));
}

TEST_CASE("ExplorerLaunchSpec: 空白を含むパスが引用符で囲まれる")
{
	// 囲み忘れると「別の場所が開く」で気づきにくい
	const auto s = external::ExplorerLaunchSpec(_T("C:\\Program Files\\x.txt"), false);
	CHECK(ContainsStr(s.parameters, _T("\"C:\\Program Files\\x.txt\"")));
}
