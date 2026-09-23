/**
 * @file tests/core/test_gui_dot_nyan.cpp
 * @brief gui/dot_nyan.cpp の .nyanfi 設定入出力テスト
 */
#include "doctest/doctest.h"

#include "gui/dot_nyan.h"

TEST_CASE("DotNyan: 既定値と順序の判定")
{
	dot_nyan::Options o = dot_nyan::DefaultOptions();
	CHECK(o.sort_mode == 0);
	CHECK(o.no_order);
	CHECK_FALSE(o.natural_order);
	CHECK(o.path_mask.IsEmpty());
	CHECK(o.grep_mask.IsEmpty());
	CHECK_FALSE(o.handled);
	CHECK_FALSE(o.hidden);

	o.no_order = false;
	o.natural_order = true;
	CHECK_FALSE(dot_nyan::IsNoOrder(o));
	o.natural_order = false;
	o.small_order = true;
	CHECK_FALSE(dot_nyan::IsNoOrder(o));
}

TEST_CASE("DotNyan: VCL の三状態表示値を正規化する")
{
	dot_nyan::Options o;
	o.show_hidden = 2;
	o.show_system = 1;
	o.show_byte_size = 2;
	o.show_icon = 3;
	o.sync_lr = 1;
	CHECK(o.show_hidden == 2);
	CHECK(o.show_system == 1);
	CHECK(o.show_byte_size == 2);
	CHECK(o.show_icon == 3);
	CHECK(o.sync_lr == 1);

	o.show_icon = 9;
	CHECK(dot_nyan::NormalizeOptions(o).show_icon == 0);
	o.show_hidden = -1;
	CHECK(dot_nyan::NormalizeOptions(o).show_hidden == 0);
}

TEST_CASE("DotNyan: 設定文字列の読み込み")
{
	const UnicodeString text =
		_T("SortMode=E\r\n")
		_T("NaturalOrder=1\r\n")
		_T("SmallOrder=0\r\n")
		_T("ShowHideAtr=0\r\n")
		_T("ShowSystemAtr=1\r\n")
		_T("ShowByteSize=0\r\n")
		_T("ShowIcon=2\r\n")
		_T("SyncLR=1\r\n")
		_T("PathMask=*.cpp;*.h\r\n")
		_T("GrepMask=error\r\n")
		_T("ListWidth=240\r\n")
		_T("PlaySound=sound.wav\r\n")
		_T("BgImage=bg.png\r\n")
		_T("Description=Test\r\n")
		_T("ExeCommands=@cmd.nyan\r\n")
		_T("Handled=1\r\n")
		_T("Color_bgDirInf=255\r\n");

	const dot_nyan::ParseResult parsed = dot_nyan::ParseConfig(text);
	REQUIRE(parsed.ok);
	CHECK(parsed.options.sort_mode == 2); // E = 拡張子
	CHECK(parsed.options.natural_order);
	CHECK_FALSE(parsed.options.small_order);
	CHECK(parsed.options.show_hidden == 2);
	CHECK(parsed.options.show_system == 1);
	CHECK(parsed.options.show_byte_size == 2);
	CHECK(parsed.options.show_icon == 3);
	CHECK(parsed.options.sync_lr == 1);
	CHECK(parsed.options.path_mask == UnicodeString(_T("*.cpp;*.h")));
	CHECK(parsed.options.grep_mask == UnicodeString(_T("error")));
	CHECK(parsed.options.list_width == UnicodeString(_T("240")));
	CHECK(parsed.options.play_sound == UnicodeString(_T("sound.wav")));
	CHECK(parsed.options.bg_image == UnicodeString(_T("bg.png")));
	CHECK(parsed.options.description == UnicodeString(_T("Test")));
	CHECK(parsed.options.exe_commands == UnicodeString(_T("cmd.nyan")));
	CHECK(parsed.options.handled);
	CHECK(parsed.options.colors[0] == UnicodeString(_T("255")));
}

TEST_CASE("DotNyan: 設定文字列の保存は VCL と同じキー順")
{
	dot_nyan::Options o;
	o.no_order = false;
	o.sort_mode = 2;
	o.natural_order = true;
	o.show_hidden = 2;
	o.show_system = 1;
	o.show_byte_size = 2;
	o.show_icon = 3;
	o.sync_lr = 1;
	o.path_mask = _T("*.cpp");
	o.grep_mask = _T("error");
	o.list_width = _T("240");
	o.play_sound = _T("sound.wav");
	o.bg_image = _T("bg.png");
	o.description = _T("Test");
	o.exe_commands = _T("cmd.nyan");
	o.handled = true;
	o.colors[0] = _T("255");

	const UnicodeString text = dot_nyan::SerializeConfig(o);
	CHECK(text == UnicodeString(
		_T("SortMode=E\r\n")
		_T("NaturalOrder=1\r\nDscNameOrder=0\r\nSmallOrder=0\r\nOldOrder=0\r\nDscAttrOrder=0\r\n")
		_T("ShowHideAtr=0\r\nShowSystemAtr=1\r\nShowByteSize=0\r\nShowIcon=2\r\nSyncLR=1\r\n")
		_T("PathMask=*.cpp\r\nGrepMask=error\r\nListWidth=240\r\nPlaySound=sound.wav\r\n")
		_T("BgImage=bg.png\r\nDescription=Test\r\nExeCommands=@cmd.nyan\r\nHandled=1\r\n")
		_T("Color_bgDirInf=255\r\n")));
}

TEST_CASE("DotNyan: 順序なしと inherited 値の扱い")
{
	dot_nyan::Options o;
	o.no_order = true;
	o.natural_order = true; // no_order が優先
	CHECK(dot_nyan::SerializeConfig(o).Pos(_T("NaturalOrder=")) == 0);

	dot_nyan::Options inherited = dot_nyan::DefaultOptions();
	inherited.path_mask = _T("parent");
	inherited.exe_commands = _T("parent.cmd");
	dot_nyan::Options child;
	child.path_mask = _T("child");
	child.exe_commands = _T("child.cmd");
	const dot_nyan::Options merged = dot_nyan::MergeInherited(child, inherited);
	CHECK(merged.path_mask == UnicodeString(_T("child")));
	CHECK(merged.exe_commands == UnicodeString(_T("child.cmd")));
	CHECK(merged.description == inherited.description);
}

TEST_CASE("DotNyan: 不正な行は可能な範囲で読む")
{
	const dot_nyan::ParseResult parsed = dot_nyan::ParseConfig(
		_T("_sort=ignored\n\n# comment\nNaturalOrder=1\nBrokenLine\n"));
	REQUIRE(parsed.ok);
	CHECK(parsed.options.natural_order);
}

TEST_CASE("DotNyan: ファイル保存と読み込み")
{
	const UnicodeString path = _T("test-dot-nyan-config.tmp");
	dot_nyan::Options o;
	o.path_mask = _T("*.txt");
	o.description = _T("round trip");
	UnicodeString error;
	REQUIRE(dot_nyan::SaveConfig(path, o, error));
	CHECK(error.IsEmpty());

	dot_nyan::Options loaded;
	REQUIRE(dot_nyan::LoadConfig(path, loaded, error));
	CHECK(loaded.path_mask == o.path_mask);
	CHECK(loaded.description == o.description);
	CHECK(dot_nyan::DeleteConfig(path));
}

TEST_CASE("DotNyan: 設定名の解決")
{
	CHECK(dot_nyan::ConfigName(_T("C:\\tmp")) == UnicodeString(_T("C:\\tmp\\.nyanfi")));
	CHECK(dot_nyan::ConfigName(_T("C:\\tmp\\")) == UnicodeString(_T("C:\\tmp\\.nyanfi")));
}
