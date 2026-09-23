/**
 * @file tests/core/test_gui_new_file.cpp
 * @brief gui/new_file.h (TNewFileDlg の判断ロジック) のテスト
 */
#include "doctest/doctest.h"

#include "gui/new_file.h"

TEST_CASE("new_file: テンプレートから名前と選択範囲を決める")
{
	CHECK(new_file::NameFromTemplate(_T("C:\\tpl\\sample.txt")) == _T("sample.txt"));
	CHECK(new_file::StemSelectionLength(_T("sample.txt")) == 6);
	CHECK(new_file::StemSelectionLength(_T(".nyanfi")) == 0);
	CHECK(new_file::StemSelectionLength(_T("README")) == 6);
}

TEST_CASE("new_file: 作成できる条件と出力先")
{
	CHECK(new_file::CanSubmit(EmptyStr, _T("out.txt")) == false);
	CHECK(new_file::CanSubmit(_T("C:\\tpl\\sample.txt"), EmptyStr) == false);
	CHECK(new_file::CanSubmit(_T("C:\\tpl\\sample.txt"), _T("out.txt")) == true);
	CHECK(new_file::OutputPath(_T("C:\\dest"), _T("out.txt")) == _T("C:\\dest\\out.txt"));
	CHECK(new_file::OutputPath(_T("C:\\dest\\"), _T("out.txt")) == _T("C:\\dest\\out.txt"));
}

TEST_CASE("new_file: テンプレートの履歴を先頭へ移す")
{
	std::vector<UnicodeString> history = {_T("a.tpl"), _T("b.tpl")};
	new_file::PromoteHistory(history, _T("b.tpl"));
	CHECK(history == std::vector<UnicodeString>{_T("b.tpl"), _T("a.tpl")});
	new_file::PromoteHistory(history, _T("c.tpl"));
	CHECK(history == std::vector<UnicodeString>{_T("c.tpl"), _T("b.tpl"), _T("a.tpl")});
	new_file::PromoteHistory(history, EmptyStr);
	CHECK(history.size() == 3);
}

TEST_CASE("new_file: 作成後の既定コマンド")
{
	CHECK(new_file::DefaultPostCommand(false) == _T("OpenByWin"));
	CHECK(new_file::DefaultPostCommand(true).IsEmpty());
	CHECK(new_file::TemplateDirectory(_T("C:\\tpl\\sample.txt")) == _T("C:\\tpl\\"));
}
