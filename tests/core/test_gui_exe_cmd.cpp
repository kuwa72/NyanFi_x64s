/**
 * @file tests/core/test_gui_exe_cmd.cpp
 * @brief gui/exe_cmd.h (TExeCmdDlg の判断ロジック) のテスト
 */
#include "doctest/doctest.h"

#include "gui/exe_cmd.h"

TEST_CASE("exe_cmd: FN/LC 初期入力モード")
{
	CHECK(get_extension(_T("C:\\bin\\tool.exe")) == _T(".exe"));
	CHECK(exe_cmd::IsExecutablePath(_T("C:\\bin\\tool.exe")) == true);
	CHECK(exe_cmd::IsExecutablePath(_T("C:\\bin\\tool.cmd")) == true);
	CHECK(exe_cmd::IsExecutablePath(_T("C:\\bin\\notes.txt")) == false);
	CHECK(exe_cmd::ParseInitialInput(EmptyStr) == exe_cmd::InitialInput::None);
	CHECK(exe_cmd::ParseInitialInput(_T("FN")) == exe_cmd::InitialInput::CurrentFile);
	CHECK(exe_cmd::ParseInitialInput(_T("LC")) == exe_cmd::InitialInput::LastCommand);
	CHECK(exe_cmd::ParseInitialInput(_T("LC;FN")) == exe_cmd::InitialInput::CurrentFile);
	CHECK(exe_cmd::ParseInitialInput(_T("FNX")) == exe_cmd::InitialInput::None);
}

TEST_CASE("exe_cmd: カーソル位置のファイル名から初期値を作る")
{
	exe_cmd::CommandSeed normal = exe_cmd::MakeSeed(
		exe_cmd::InitialInput::CurrentFile, _T("C:\\dir\\sample file.txt"), false, {});
	CHECK(normal.text == _T(" \"C:\\dir\\sample file.txt\""));
	CHECK(normal.caret == 0);

	exe_cmd::CommandSeed executable = exe_cmd::MakeSeed(
		exe_cmd::InitialInput::CurrentFile, _T("C:\\bin\\tool.exe"), true, {});
	CHECK(executable.text == _T("C:\\bin\\tool.exe "));
	CHECK(executable.caret == static_cast<int>(executable.text.Length()));

	exe_cmd::CommandSeed last = exe_cmd::MakeSeed(
		exe_cmd::InitialInput::LastCommand, EmptyStr, false,
		{_T("first"), _T("second")});
	CHECK(last.text == _T("first"));
	CHECK(last.caret == 5);
}

TEST_CASE("exe_cmd: 実行ボタン条件")
{
	exe_cmd::Options opt;
	CHECK(exe_cmd::CanSubmit(opt) == false);
	opt.command = _T("dir");
	CHECK(exe_cmd::CanSubmit(opt) == true);
	opt.save_stdout = true;
	CHECK(exe_cmd::CanSubmit(opt) == false);
	opt.save_name = _T("out.txt");
	CHECK(exe_cmd::CanSubmit(opt) == true);
}

TEST_CASE("exe_cmd: 権限昇格中は標準出力オプションを無効にする")
{
	exe_cmd::Options opt;
	CHECK(exe_cmd::OutputOptionsEnabled(opt) == true);
	opt.run_as = true;
	CHECK(exe_cmd::OutputOptionsEnabled(opt) == false);
	opt.run_as = false;
	opt.uac_dialog = true;
	CHECK(exe_cmd::OutputOptionsEnabled(opt) == false);
}

TEST_CASE("exe_cmd: 履歴を先頭へ移す")
{
	std::vector<UnicodeString> history = {_T("first"), _T("second")};
	exe_cmd::PromoteHistory(history, _T("second"));
	CHECK(history == std::vector<UnicodeString>{_T("second"), _T("first")});
	exe_cmd::PromoteHistory(history, _T("third"));
	CHECK(history == std::vector<UnicodeString>{_T("third"), _T("second"), _T("first")});
	exe_cmd::PromoteHistory(history, EmptyStr);
	CHECK(history.size() == 3);
}

TEST_CASE("exe_cmd: 標準出力の保存先")
{
	CHECK(exe_cmd::SaveOutputPath(_T("C:\\work"), _T("out.txt")) == _T("C:\\work\\out.txt"));
	CHECK(exe_cmd::SaveOutputPath(_T("C:\\work"), _T("C:\\other\\out.txt")) == _T("C:\\other\\out.txt"));
}
