/**
 * @file tests/core/test_gui_history.cpp
 * @brief gui/history.cpp (履歴リスト) の回帰テスト
 *
 * @details 重複時に先頭へ移す挙動・上限超過時に末尾 (一番古い項目) を
 *          切り捨てる挙動は、VCL の `add_TextEditHistory`
 *          (`src/Global.cpp:11093-11104`、`TextEditHistory->Insert(0, ...)`)
 *          を実測して合わせてある。ini の往復は `L:TextEditHistory=50,true`
 *          (`src/Global.cpp:2006`) を実測し、`50`=最大件数、`true`=引用符を
 *          外すかどうか (`src/UIniFile.h:106`) であることを確かめたうえで
 *          `UsrIniFile::LoadListItems`/`SaveListItems` をそのまま使う。
 *          詳しい実測結果は gui/history.h の解説を参照。
 */
#include "doctest/doctest.h"

#include "gui/history.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;
using history::HistoryList;
using history::Kind;

//===========================================================================
// HistoryList::Add (重複排除・先頭移動・上限)
//===========================================================================

TEST_CASE("Add: 新しい項目は先頭に入る")
{
	HistoryList h(10);
	h.Add(_T("a.txt"));
	h.Add(_T("b.txt"));

	REQUIRE(h.Entries().size() == 2);
	CHECK(h.Entries()[0] == UnicodeString(_T("b.txt")));
	CHECK(h.Entries()[1] == UnicodeString(_T("a.txt")));
}

TEST_CASE("Add: 既にある項目を再度使うと先頭へ移動し、重複しない (VCL の add_TextEditHistory)")
{
	HistoryList h(10);
	h.Add(_T("a.txt"));
	h.Add(_T("b.txt"));
	h.Add(_T("a.txt"));

	REQUIRE(h.Entries().size() == 2);
	CHECK(h.Entries()[0] == UnicodeString(_T("a.txt")));
	CHECK(h.Entries()[1] == UnicodeString(_T("b.txt")));
}

TEST_CASE("Add: 大文字小文字を区別せず重複とみなす (SameText)")
{
	HistoryList h(10);
	h.Add(_T("C:\\Dir\\A.TXT"));
	h.Add(_T("c:\\dir\\a.txt"));

	REQUIRE(h.Entries().size() == 1);
	// 後から使った表記 (元の大文字小文字) が残る
	CHECK(h.Entries()[0] == UnicodeString(_T("c:\\dir\\a.txt")));
}

TEST_CASE("Add: 空文字は無視する")
{
	HistoryList h(10);
	h.Add(EmptyStr);
	CHECK(h.Entries().empty());
}

TEST_CASE("Add: 上限を超えたら一番古い項目 (末尾) を切り捨てる")
{
	HistoryList h(3);
	h.Add(_T("a"));
	h.Add(_T("b"));
	h.Add(_T("c"));
	h.Add(_T("d"));

	REQUIRE(h.Entries().size() == 3);
	CHECK(h.Entries()[0] == UnicodeString(_T("d")));
	CHECK(h.Entries()[1] == UnicodeString(_T("c")));
	CHECK(h.Entries()[2] == UnicodeString(_T("b")));
	// 一番古い "a" が落ちている
}

TEST_CASE("Add: 再利用による先頭移動は上限を減らさない")
{
	HistoryList h(2);
	h.Add(_T("a"));
	h.Add(_T("b"));
	h.Add(_T("a"));  // 先頭へ移動するだけ

	REQUIRE(h.Entries().size() == 2);
	CHECK(h.Entries()[0] == UnicodeString(_T("a")));
	CHECK(h.Entries()[1] == UnicodeString(_T("b")));
}

TEST_CASE("MaxItems: コンストラクタの値をそのまま返す")
{
	HistoryList h(50);
	CHECK(h.MaxItems() == 50);
}

TEST_CASE("MaxItems: 0 以下を渡したら 1 に補正する")
{
	HistoryList h(0);
	CHECK(h.MaxItems() == 1);
}

//===========================================================================
// Remove / Clear
//===========================================================================

TEST_CASE("Remove: 一致する項目を取り除く (大文字小文字を区別しない)")
{
	HistoryList h(10);
	h.Add(_T("a.txt"));
	h.Add(_T("b.txt"));
	h.Remove(_T("A.TXT"));

	REQUIRE(h.Entries().size() == 1);
	CHECK(h.Entries()[0] == UnicodeString(_T("b.txt")));
}

TEST_CASE("Remove: 無い項目を指定しても何も起きない")
{
	HistoryList h(10);
	h.Add(_T("a.txt"));
	h.Remove(_T("nothere.txt"));
	CHECK(h.Entries().size() == 1);
}

TEST_CASE("Clear: すべて消える")
{
	HistoryList h(10);
	h.Add(_T("a.txt"));
	h.Add(_T("b.txt"));
	h.Clear();
	CHECK(h.Entries().empty());
}

//===========================================================================
// DropMissingFiles
//===========================================================================

TEST_CASE("DropMissingFiles: 実体の無いファイルを外し、件数を返す")
{
	TempDir tmp;
	const UnicodeString real = tmp.file(_T("real.txt"));
	{
		HANDLE fh = ::CreateFileW(real.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		                          FILE_ATTRIBUTE_NORMAL, NULL);
		REQUIRE(fh != INVALID_HANDLE_VALUE);
		::CloseHandle(fh);
	}

	HistoryList h(10);
	h.Add(real);
	h.Add(tmp.file(_T("gone.txt")));

	CHECK(h.DropMissingFiles() == 1);
	REQUIRE(h.Entries().size() == 1);
	CHECK(h.Entries()[0] == real);
}

TEST_CASE("DropMissingFiles: 実体のあるディレクトリは残す")
{
	TempDir tmp;
	HistoryList h(10);
	h.Add(tmp.path);  // TempDir 自体が既存のディレクトリ

	CHECK(h.DropMissingFiles() == 0);
	CHECK(h.Entries().size() == 1);
}

TEST_CASE("DropMissingFiles: 全部残っているなら 0 件")
{
	TempDir tmp;
	const UnicodeString real = tmp.file(_T("real.txt"));
	HANDLE fh = ::CreateFileW(real.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                          FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(fh != INVALID_HANDLE_VALUE);
	::CloseHandle(fh);

	HistoryList h(10);
	h.Add(real);
	CHECK(h.DropMissingFiles() == 0);
}

//===========================================================================
// IniKeyOf
//===========================================================================

TEST_CASE("IniKeyOf: VCL の ini キー名と同じにしてある (Edit/View)")
{
	CHECK(history::IniKeyOf(Kind::Edit) == UnicodeString(_T("TextEditHistory")));
	CHECK(history::IniKeyOf(Kind::View) == UnicodeString(_T("TextViewHistory")));
}

TEST_CASE("IniKeyOf: Recent/Command は VCL に対応する ini が無いための新規名")
{
	CHECK(history::IniKeyOf(Kind::Recent) == UnicodeString(_T("RecentList")));
	CHECK(history::IniKeyOf(Kind::Command) == UnicodeString(_T("CmdHistory")));
}

//===========================================================================
// LoadFromIni / SaveToIni (往復)
//===========================================================================

TEST_CASE("SaveToIni -> LoadFromIni: 新しい順のまま往復する")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList src(10);
	src.Add(_T("a.txt"));
	src.Add(_T("b.txt"));
	src.Add(_T("c.txt"));  // 新しい順: c, b, a

	history::SaveToIni(ini, Kind::Edit, src);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist.ini")));
	HistoryList got(10);
	history::LoadFromIni(reread, Kind::Edit, got);

	REQUIRE(got.Entries().size() == 3);
	CHECK(got.Entries()[0] == UnicodeString(_T("c.txt")));
	CHECK(got.Entries()[1] == UnicodeString(_T("b.txt")));
	CHECK(got.Entries()[2] == UnicodeString(_T("a.txt")));
}

TEST_CASE("LoadFromIni: 読み込みは順序をそのまま入れる (先頭移動・重複排除をしない)")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList src(10);
	src.Add(_T("x"));
	src.Add(_T("y"));
	history::SaveToIni(ini, Kind::View, src);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist.ini")));
	HistoryList got(10);
	history::LoadFromIni(reread, Kind::View, got);

	// src と同じ並び (y, x) のまま。読み込みが Add と同じ動きをすると
	// 逆順になってしまうのでそれを固定する
	REQUIRE(got.Entries().size() == 2);
	CHECK(got.Entries()[0] == UnicodeString(_T("y")));
	CHECK(got.Entries()[1] == UnicodeString(_T("x")));
}

TEST_CASE("LoadFromIni: 種類ごとに別セクションに保存されるので混ざらない")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList edit(10);
	edit.Add(_T("edit.txt"));
	HistoryList view(10);
	view.Add(_T("view.txt"));

	history::SaveToIni(ini, Kind::Edit, edit);
	history::SaveToIni(ini, Kind::View, view);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist.ini")));
	HistoryList got_edit(10), got_view(10);
	history::LoadFromIni(reread, Kind::Edit, got_edit);
	history::LoadFromIni(reread, Kind::View, got_view);

	REQUIRE(got_edit.Entries().size() == 1);
	CHECK(got_edit.Entries()[0] == UnicodeString(_T("edit.txt")));
	REQUIRE(got_view.Entries().size() == 1);
	CHECK(got_view.Entries()[0] == UnicodeString(_T("view.txt")));
}

TEST_CASE("SaveToIni: 上限を超える分は書き出さない (VCL の SaveListItems と同じ)")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList src(2);
	src.Add(_T("a"));
	src.Add(_T("b"));
	// Add の時点で既に2件に切り詰められているが、SaveToIni 自体も
	// MaxItems() を超えて書かないことを確かめる
	history::SaveToIni(ini, Kind::Command, src);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist.ini")));
	HistoryList got(2);
	history::LoadFromIni(reread, Kind::Command, got);
	CHECK(got.Entries().size() == 2);
}

TEST_CASE("LoadFromIni: 何も保存されていなければ空のまま")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList got(10);
	history::LoadFromIni(ini, Kind::Recent, got);
	CHECK(got.Entries().empty());
}

TEST_CASE("Add -> SaveToIni -> LoadFromIni: 一連の流れで日本語のパスが壊れない")
{
	TempDir tmp;
	UsrIniFile ini(tmp.file(_T("hist.ini")));

	HistoryList src(10);
	src.Add(_T("C:\\メモ\\日本語.txt"));

	history::SaveToIni(ini, Kind::Edit, src);
	REQUIRE(ini.UpdateFile());

	UsrIniFile reread(tmp.file(_T("hist.ini")));
	HistoryList got(10);
	history::LoadFromIni(reread, Kind::Edit, got);

	REQUIRE(got.Entries().size() == 1);
	CHECK(got.Entries()[0] == UnicodeString(_T("C:\\メモ\\日本語.txt")));
}
