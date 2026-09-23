/**
 * @file tests/core/test_gui_grep_opt.cpp
 * @brief gui/grep_opt.h の純関数テスト
 */
#include "doctest/doctest.h"

#include "gui/grep_opt.h"

TEST_CASE("grep_opt: 既定値は VCL の Global 初期値に合わせる")
{
	grep_opt::Options o;
	CHECK(o.output_mode == grep_opt::OutputMode::None);
	CHECK(o.edit_mode == grep_opt::EditMode::Search);
	CHECK(o.file_format == UnicodeString(_T("$F $L:")));
	CHECK(o.replacement == UnicodeString(_T(" ／ ")));
	CHECK(o.trim_left);
	CHECK(o.replace_tab);
	CHECK(o.replace_cr);
}

TEST_CASE("grep_opt: 出力モードの index 変換")
{
	CHECK(grep_opt::OutputModeIndex(grep_opt::OutputMode::None) == 0);
	CHECK(grep_opt::OutputModeIndex(grep_opt::OutputMode::File) == 1);
	CHECK(grep_opt::OutputModeIndex(grep_opt::OutputMode::Clipboard) == 2);
	CHECK(grep_opt::OutputModeFromIndex(2) == grep_opt::OutputMode::Clipboard);
	CHECK(grep_opt::OutputModeFromIndex(99) == grep_opt::OutputMode::None);
	CHECK(grep_opt::EditModeIndex(grep_opt::EditMode::Replace) == 1);
	CHECK(grep_opt::EditModeFromIndex(0) == grep_opt::EditMode::Search);
}

TEST_CASE("grep_opt: 置換ページでは挿入語欄を無効化する")
{
	grep_opt::Options o;
	o.output_mode = grep_opt::OutputMode::File;
	o.app_enabled = true;
	o.backup_replace = true;
	o.save_log = true;
	o.edit_mode = grep_opt::EditMode::Replace;

	const grep_opt::EnabledState e = grep_opt::ResolveEnabled(o);
	CHECK(e.output_file);
	CHECK(e.app);
	CHECK(e.app_name);
	CHECK(e.app_dir);
	CHECK(e.backup);
	CHECK(e.log);
	CHECK_FALSE(e.insert_words);
}

TEST_CASE("grep_opt: 空のファイル書式はサンプルの既定へ戻る")
{
	grep_opt::Options o;
	o.file_format = EmptyStr;
	o.insert_before = _T("foo");
	o.insert_after = _T("bar");
	const UnicodeString sample = grep_opt::BuildSample(o);
	CHECK(ContainsStr(sample, _T("D:\\hoge.txt 123:")));
	CHECK(ContainsStr(sample, _T("これは検索のfooマッチ語barです。")));
}

TEST_CASE("grep_opt: サンプルはタブ・行頭空白・改行を置換する")
{
	grep_opt::Options o;
	o.file_format = _T("$F:");
	o.replace_tab = true;
	o.trim_left = true;
	o.replace_cr = true;
	o.replacement = _T("|");
	const UnicodeString sample = grep_opt::BuildSample(o);
	CHECK(ContainsStr(sample, _T("D:\\hoge.txt:")));
	CHECK(ContainsStr(sample, _T("|これは3行目です。")));
	CHECK_FALSE(ContainsStr(sample, _T("\t\tこれは3行目です。")));
}

TEST_CASE("grep_opt: Normalize は範囲外の後端を既定へ戻す")
{
	grep_opt::Options o;
	o.output_mode = static_cast<grep_opt::OutputMode>(9);
	o.edit_mode = static_cast<grep_opt::EditMode>(-1);
	const grep_opt::Options n = grep_opt::Normalize(o);
	CHECK(n.output_mode == grep_opt::OutputMode::None);
	CHECK(n.edit_mode == grep_opt::EditMode::Search);
}
