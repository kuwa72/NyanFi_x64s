/**
 * @file tests/core/test_gui_backup_settings.cpp
 * @brief gui/backup_settings のバックアップ設定・CSVのテスト
 */
#include "doctest/doctest.h"

#include "gui/backup_settings.h"

TEST_CASE("backup_settings: 日付条件の書式を検証する")
{
	backup_settings::DateKind kind = backup_settings::DateKind::None;
	UnicodeString normalized;
	UnicodeString error;
	CHECK(backup_settings::ParseDateCondition(_T(""), kind, normalized, error));
	CHECK(kind == backup_settings::DateKind::None);
	CHECK(backup_settings::ParseDateCondition(_T(">=2024/01/31"), kind, normalized, error));
	CHECK(kind == backup_settings::DateKind::AfterOrEqual);
	CHECK(backup_settings::ParseDateCondition(_T("<1D"), kind, normalized, error));
	CHECK(kind == backup_settings::DateKind::Before);
	CHECK(backup_settings::ParseDateCondition(_T("TD"), kind, normalized, error));
	CHECK(kind == backup_settings::DateKind::OnOrEqual);
	CHECK_FALSE(backup_settings::ParseDateCondition(_T("2024/01/01"), kind, normalized, error));
	CHECK_FALSE(backup_settings::ParseDateCondition(_T(">2024/99/99"), kind, normalized, error));
}

TEST_CASE("backup_settings: 設定の CSV を往復する")
{
	backup_settings::Setup s;
	s.name = _T("daily, main=prod");
	s.options.include_mask = _T("*.cpp;*.h, backup");
	s.options.exclude_mask = _T("tmp");
	s.options.skip_dirs = _T("build,out");
	s.options.sub_dirs = true;
	s.options.mirror = true;
	s.options.sync = false;
	s.options.date_condition = _T(">=1D");
	s.options.confirm = false;

	const UnicodeString record = backup_settings::FormatSetupRecord(s);
	backup_settings::Setup parsed;
	CHECK(backup_settings::ParseSetupRecord(record, parsed));
	CHECK(parsed.name == s.name);
	CHECK(parsed.options.include_mask == s.options.include_mask);
	CHECK(parsed.options.exclude_mask == s.options.exclude_mask);
	CHECK(parsed.options.skip_dirs == s.options.skip_dirs);
	CHECK(parsed.options.sub_dirs);
	CHECK(parsed.options.mirror);
	CHECK_FALSE(parsed.options.sync);
	CHECK(parsed.options.date_condition == s.options.date_condition);
	CHECK(parsed.options.confirm);  // VCL のSetup CSVには SureCheckBox を含めない
}

TEST_CASE("backup_settings: 設定一覧の追加・削除・検索")
{
	std::vector<backup_settings::Setup> list;
	backup_settings::Options o;
	o.include_mask = _T("*.txt");
	backup_settings::UpsertSetup(list, _T("one"), o);
	backup_settings::UpsertSetup(list, _T("two"), o);
	REQUIRE(list.size() == 2);
	CHECK(backup_settings::FindSetupIndex(list, _T("TWO")) == 0);
	o.mirror = true;
	backup_settings::UpsertSetup(list, _T("one"), o);
	REQUIRE(list.size() == 2);
	CHECK(list[1].options.mirror);
	CHECK(backup_settings::DeleteSetup(list, _T("one")));
	CHECK(list.size() == 1);
	CHECK_FALSE(backup_settings::DeleteSetup(list, _T("missing")));
}

TEST_CASE("backup_settings: パスとオプションを検証する")
{
	UnicodeString error;
	backup_settings::Options o;
	o.date_condition = _T(">1D");
	CHECK(backup_settings::ValidateOptions(o, _T("C:\\src"), _T("D:\\dst"), error));
	CHECK_FALSE(backup_settings::ValidateOptions(o, _T("C:\\src"), _T("C:\\src"), error));
	CHECK_FALSE(backup_settings::ValidateOptions(o, _T(""), _T("D:\\dst"), error));
	o.date_condition = _T("bad");
	CHECK_FALSE(backup_settings::ValidateOptions(o, _T("C:\\src"), _T("D:\\dst"), error));
}

TEST_CASE("backup_settings: 同期先を解決しコマンドファイルを生成する")
{
	std::vector<sync_dirs::SyncEntry> entries(1);
	entries[0].title = _T("mirror");
	entries[0].enabled = true;
	entries[0].dirs = {_T("D:\\"), _T("E:\\")};
	const auto targets = backup_settings::ResolveDestinations(_T("D:\\dst"), true, entries);
	REQUIRE(targets.size() == 2);
	CHECK(targets[0] == UnicodeString(_T("D:\\dst\\")));
	CHECK(targets[1] == UnicodeString(_T("E:\\dst\\")));
	const auto no_sync = backup_settings::ResolveDestinations(_T("D:\\dst"), false, entries);
	REQUIRE(no_sync.size() == 1);
	CHECK(no_sync[0] == UnicodeString(_T("D:\\dst")));

	backup_settings::Setup s;
	s.name = _T("daily");
	const UnicodeString cmd = backup_settings::MakeCommandText(_T("C:\\src"), _T("D:\\dst"), s);
	CHECK(cmd.Pos(_T("BackUp_\"daily\"")) > 0);
	CHECK(cmd.Pos(_T("ChangeDir_\"C:\\src\"")) > 0);
}
