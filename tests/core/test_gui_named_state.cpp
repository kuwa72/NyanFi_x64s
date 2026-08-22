/**
 * @file tests/core/test_gui_named_state.cpp
 * @brief gui/named_state.cpp (タブグループ / 結果リスト / 検索設定の保存と読み込み) のテスト
 *
 * 書式は VCL の以下を実測して合わせてある (詳細は gui/named_state.h の冒頭コメント):
 *   - タブグループ: `SaveTabGroupActionExecute`/`LoadTabGroupActionExecute` (MainFrm.cpp)。
 *     ただし VCL の TabList CSV とは非互換の新規 ini 書式 (gui/tabs.h の TabState に
 *     フィールドを合わせたため)
 *   - 結果リスト: `SaveAsResultListActionExecute`/`LoadResultListActionExecute` (MainFrm.cpp)。
 *     UTF-8 + BOM、`;[ResultList]` ヘッダ + `パス<TAB>別名` の項目行 (.nwl と同じ)
 *   - 検索設定: `save_FindSettings`/`load_FindSettings` (Global.cpp:5192/5360)。
 *     ini の "FindSettings" セクション (拡張検索の項目は対象外)
 */
#include "doctest/doctest.h"

#include "gui/named_state.h"

#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;
using named_state::FindSet;
using named_state::Kind;

namespace {

void mkfile(const UnicodeString &path, const std::string &body = std::string())
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	if (!body.empty()) {
		DWORD written = 0;
		::WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, NULL);
	}
	::CloseHandle(h);
}

std::string read_all(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	std::string out;
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

TabState MakeTab(const UnicodeString &dir0, const UnicodeString &dir1, SortKey key = SortKey::Name,
                  bool descending = false, bool dirs_first = true)
{
	TabState state;
	state.panes[0].directory = dir0;
	state.panes[0].sort_key = key;
	state.panes[0].sort_descending = descending;
	state.panes[0].dirs_first = dirs_first;
	state.panes[1].directory = dir1;
	return state;
}

}  // namespace

//===========================================================================
// ExtensionOf
//===========================================================================
TEST_CASE("ExtensionOf: 種類ごとの既定拡張子 (F_FILTER_INI / F_FILTER_TXT)")
{
	CHECK(named_state::ExtensionOf(Kind::TabGroup) == UnicodeString(_T(".ini")));
	CHECK(named_state::ExtensionOf(Kind::ResultList) == UnicodeString(_T(".txt")));
	CHECK(named_state::ExtensionOf(Kind::FindSet) == UnicodeString(_T(".ini")));
}

//===========================================================================
// タブグループ: WriteTabGroupIni / ReadTabGroupIni
//===========================================================================
TEST_CASE("WriteTabGroupIni/ReadTabGroupIni: 別インスタンスで読み直すと同じ内容になる")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("tg.ini");

	std::vector<TabState> tabs = {
		MakeTab(_T("C:\\A\\"), _T("C:\\B\\"), SortKey::Name, false, true),
		MakeTab(_T("D:\\C\\"), _T("D:\\D\\"), SortKey::Size, true, false),
	};

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteTabGroupIni(*ini, tabs, 1);
		REQUIRE(ini->UpdateFile(true));
	}

	std::vector<TabState> loaded;
	int current = -1;
	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		REQUIRE(named_state::ReadTabGroupIni(*ini, loaded, current));
	}

	REQUIRE(loaded.size() == 2);
	CHECK(current == 1);
	CHECK(loaded[0].panes[0].directory == UnicodeString(_T("C:\\A\\")));
	CHECK(loaded[0].panes[1].directory == UnicodeString(_T("C:\\B\\")));
	CHECK(loaded[0].panes[0].sort_key == SortKey::Name);
	CHECK(loaded[0].panes[0].sort_descending == false);
	CHECK(loaded[1].panes[0].directory == UnicodeString(_T("D:\\C\\")));
	CHECK(loaded[1].panes[0].sort_key == SortKey::Size);
	CHECK(loaded[1].panes[0].sort_descending == true);
	CHECK(loaded[1].panes[0].dirs_first == false);
}

TEST_CASE("ReadTabGroupIni: セクションが無ければ false (呼び出し側の変数はそのまま)")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("empty.ini");
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));

	std::vector<TabState> loaded;
	int current = 0;
	CHECK(named_state::ReadTabGroupIni(*ini, loaded, current) == false);
	CHECK(loaded.empty());
}

TEST_CASE("ReadTabGroupIni: Current が範囲外なら 0 に補正する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("bad_current.ini");
	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteTabGroupIni(*ini, {MakeTab(_T("C:\\A\\"), _T("C:\\B\\"))}, 0);
		ini->WriteInteger(_T("TabGroup"), _T("Current"), 99);  // 手で壊す
		REQUIRE(ini->UpdateFile(true));
	}

	std::vector<TabState> loaded;
	int current = -1;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadTabGroupIni(*ini, loaded, current));
	CHECK(current == 0);
}

//===========================================================================
// タブグループ: SaveTabGroup / LoadTabGroup (ファイル経由)
//===========================================================================
TEST_CASE("SaveTabGroup/LoadTabGroup: ファイル経由でも往復する")
{
	TempDir tmp;
	const UnicodeString path = tmp.path + _T("group") + named_state::ExtensionOf(Kind::TabGroup);

	std::vector<TabState> tabs = {
		MakeTab(_T("C:\\X\\"), _T("C:\\Y\\")),
		MakeTab(_T("C:\\Z\\"), EmptyStr, SortKey::Date, true, true),
	};
	UnicodeString error;
	REQUIRE(named_state::SaveTabGroup(path, tabs, 1, error));

	std::vector<TabState> loaded;
	int current = -1;
	REQUIRE(named_state::LoadTabGroup(path, loaded, current, error));
	REQUIRE(loaded.size() == 2);
	CHECK(current == 1);
	CHECK(loaded[1].panes[0].directory == UnicodeString(_T("C:\\Z\\")));
	CHECK(loaded[1].panes[1].directory.IsEmpty());
	CHECK(loaded[1].panes[0].sort_key == SortKey::Date);
}

TEST_CASE("SaveTabGroup: 保存先が空なら失敗を理由付きで返す")
{
	std::vector<TabState> tabs = {TabState()};
	UnicodeString error;
	CHECK(named_state::SaveTabGroup(EmptyStr, tabs, 0, error) == false);
	CHECK(!error.IsEmpty());
}

TEST_CASE("LoadTabGroup: 無いファイルは理由を返して false")
{
	TempDir tmp;
	std::vector<TabState> loaded;
	int current = 0;
	UnicodeString error;
	CHECK(named_state::LoadTabGroup(tmp.path + _T("nothere.ini"), loaded, current, error) == false);
	CHECK(!error.IsEmpty());
}

//===========================================================================
// 結果リスト: ParseResultListLines / FormatResultListLines
//===========================================================================
TEST_CASE("ParseResultListLines: 先頭行が ;[ResultList] でなければ空 (不正な形式)")
{
	const std::vector<UnicodeString> lines = {_T("C:\\a.txt\t")};
	UnicodeString title;
	CHECK(named_state::ParseResultListLines(lines, title).empty());
}

TEST_CASE("ParseResultListLines: Find_Path を拾い、項目をパスと別名に分ける")
{
	const std::vector<UnicodeString> lines = {
		_T(";[ResultList]"), _T(";Find_Path=C:\\search\\"), _T("D:\\other\\a.txt\tエー")
	};
	UnicodeString title;
	const std::vector<FileItem> items = named_state::ParseResultListLines(lines, title);

	CHECK(title == UnicodeString(_T("C:\\search\\")));
	REQUIRE(items.size() == 1);
	CHECK(items[0].full_path == UnicodeString(_T("D:\\other\\a.txt")));
	CHECK(items[0].name == UnicodeString(_T("a.txt")));
	CHECK(items[0].alias == UnicodeString(_T("エー")));
	CHECK(items[0].is_dir == false);
}

TEST_CASE("ParseResultListLines: 末尾が区切り文字ならディレクトリ (末尾は落とす)")
{
	const std::vector<UnicodeString> lines = {_T(";[ResultList]"), _T(";Find_Path="), _T("D:\\sub\\\tフォルダ")};
	UnicodeString title;
	const std::vector<FileItem> items = named_state::ParseResultListLines(lines, title);

	REQUIRE(items.size() == 1);
	CHECK(items[0].is_dir == true);
	CHECK(items[0].full_path == UnicodeString(_T("D:\\sub")));
}

TEST_CASE("ParseResultListLines: パスが空で別名が - なら区切り行")
{
	const std::vector<UnicodeString> lines = {_T(";[ResultList]"), _T(";Find_Path="), _T("\t-")};
	UnicodeString title;
	const std::vector<FileItem> items = named_state::ParseResultListLines(lines, title);

	REQUIRE(items.size() == 1);
	CHECK(items[0].is_separator == true);
	CHECK(items[0].full_path.IsEmpty());
}

TEST_CASE("ParseResultListLines: 別名だけの行は捨てる (.nwl と同じ)")
{
	const std::vector<UnicodeString> lines = {_T(";[ResultList]"), _T(";Find_Path="), _T("\tラベルだけ")};
	UnicodeString title;
	CHECK(named_state::ParseResultListLines(lines, title).empty());
}

TEST_CASE("FormatResultListLines: ヘッダと Find_Path、項目行を組み立てる")
{
	std::vector<FileItem> items(1);
	items[0].full_path = _T("C:\\a.txt");
	items[0].alias = _T("エー");

	const std::vector<UnicodeString> lines = named_state::FormatResultListLines(_T("C:\\dir\\"), items);
	REQUIRE(lines.size() == 3);
	CHECK(lines[0] == UnicodeString(_T(";[ResultList]")));
	CHECK(lines[1] == UnicodeString(_T(";Find_Path=C:\\dir\\")));
	CHECK(lines[2] == UnicodeString(_T("C:\\a.txt\tエー")));
}

TEST_CASE("FormatResultListLines: is_parent (..) の項目は書き出さない")
{
	std::vector<FileItem> items(1);
	items[0].is_parent = true;
	items[0].name = _T("..");

	const std::vector<UnicodeString> lines = named_state::FormatResultListLines(EmptyStr, items);
	CHECK(lines.size() == 2);  // ヘッダ + Find_Path のみ
}

TEST_CASE("FormatResultListLines: ディレクトリは末尾に区切り文字が付く")
{
	std::vector<FileItem> items(1);
	items[0].full_path = _T("C:\\sub");
	items[0].is_dir = true;

	const std::vector<UnicodeString> lines = named_state::FormatResultListLines(EmptyStr, items);
	REQUIRE(lines.size() == 3);
	CHECK(lines[2] == UnicodeString(_T("C:\\sub\\\t")));
}

TEST_CASE("結果リスト: Format と Parse は往復する (タイトルと項目)")
{
	std::vector<FileItem> items(2);
	items[0].full_path = _T("C:\\a.txt");
	items[0].alias = _T("エー");
	items[1].is_separator = true;
	items[1].alias = _T("-");

	const UnicodeString title = _T("C:\\search\\dir\\");
	const std::vector<UnicodeString> lines = named_state::FormatResultListLines(title, items);

	UnicodeString title_back;
	const std::vector<FileItem> back = named_state::ParseResultListLines(lines, title_back);

	CHECK(title_back == title);
	REQUIRE(back.size() == 2);
	CHECK(back[0].full_path == items[0].full_path);
	CHECK(back[0].alias == items[0].alias);
	CHECK(back[1].is_separator == true);
}

//===========================================================================
// 結果リスト: SaveResultList / LoadResultList (ファイル経由)
//===========================================================================
TEST_CASE("SaveResultList: UTF-8 の BOM を付ける (saveto_TextUTF8 相当)")
{
	TempDir tmp;
	const UnicodeString path = tmp.path + _T("r.txt");

	std::vector<FileItem> items;
	UnicodeString error;
	REQUIRE(named_state::SaveResultList(path, EmptyStr, items, error));

	const std::string bytes = read_all(path);
	REQUIRE(bytes.size() >= 3);
	CHECK(static_cast<unsigned char>(bytes[0]) == 0xEF);
	CHECK(static_cast<unsigned char>(bytes[1]) == 0xBB);
	CHECK(static_cast<unsigned char>(bytes[2]) == 0xBF);
}

TEST_CASE("SaveResultList/LoadResultList: 実在する項目は往復し、サイズが埋まる")
{
	TempDir tmp;
	const UnicodeString real = tmp.path + _T("real.txt");
	mkfile(real, "hello");

	std::vector<FileItem> items(1);
	items[0].full_path = real;
	items[0].alias = _T("日本語の別名");

	const UnicodeString path = tmp.path + _T("r.txt");
	UnicodeString error;
	REQUIRE(named_state::SaveResultList(path, _T("C:\\search\\"), items, error));

	UnicodeString title;
	std::vector<FileItem> loaded;
	REQUIRE(named_state::LoadResultList(path, title, loaded, error));

	CHECK(title == UnicodeString(_T("C:\\search\\")));
	REQUIRE(loaded.size() == 1);
	CHECK(loaded[0].full_path == real);
	CHECK(loaded[0].alias == UnicodeString(_T("日本語の別名")));
	CHECK(loaded[0].size == 5);
}

TEST_CASE("LoadResultList: 実体が無い項目は黙って読み飛ばす (VCL と同じ。missing にはしない)")
{
	TempDir tmp;
	const UnicodeString real = tmp.path + _T("real.txt");
	mkfile(real);

	std::vector<FileItem> items(2);
	items[0].full_path = real;
	items[1].full_path = tmp.path + _T("gone.txt");

	const UnicodeString path = tmp.path + _T("r.txt");
	UnicodeString error;
	REQUIRE(named_state::SaveResultList(path, EmptyStr, items, error));

	UnicodeString title;
	std::vector<FileItem> loaded;
	REQUIRE(named_state::LoadResultList(path, title, loaded, error));
	REQUIRE(loaded.size() == 1);
	CHECK(loaded[0].full_path == real);
}

TEST_CASE("LoadResultList: 区切り行は実体確認をせず必ず残る")
{
	TempDir tmp;
	std::vector<FileItem> items(1);
	items[0].is_separator = true;
	items[0].alias = _T("-");

	const UnicodeString path = tmp.path + _T("r.txt");
	UnicodeString error;
	REQUIRE(named_state::SaveResultList(path, EmptyStr, items, error));

	UnicodeString title;
	std::vector<FileItem> loaded;
	REQUIRE(named_state::LoadResultList(path, title, loaded, error));
	REQUIRE(loaded.size() == 1);
	CHECK(loaded[0].is_separator == true);
}

TEST_CASE("LoadResultList: 形式が違うファイルは理由を返して false")
{
	TempDir tmp;
	const UnicodeString path = tmp.path + _T("not_a_result.txt");
	mkfile(path, "\xEF\xBB\xBF" "C:\\a.txt\t\r\n");

	UnicodeString title, error;
	std::vector<FileItem> loaded;
	CHECK(named_state::LoadResultList(path, title, loaded, error) == false);
	CHECK(!error.IsEmpty());
}

TEST_CASE("LoadResultList: 無いファイルは理由を返して false")
{
	TempDir tmp;
	UnicodeString title, error;
	std::vector<FileItem> loaded;
	CHECK(named_state::LoadResultList(tmp.path + _T("nothere.txt"), title, loaded, error) == false);
	CHECK(!error.IsEmpty());
}

//===========================================================================
// 検索設定: WriteFindSetIni / ReadFindSetIni
//===========================================================================
TEST_CASE("検索設定: 通常のパス検索が往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.path = _T("C:\\search\\");
	set.dir_list = _T("C:\\a;C:\\b");
	set.skip_dir = _T("C:\\skip");
	set.target_dir = true;
	set.sub_dir = true;
	set.include_arc = true;
	set.exclude_trash = true;
	set.res_link = true;
	set.dir_link = true;
	set.mask = _T("*.txt;*.log");
	set.keywd = _T("日本語キーワード");
	set.reg_ex = true;
	set.match_and = true;
	set.match_case = true;
	set.path_sort = true;
	set.sort_mode = 3;

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));

	CHECK(loaded.is_tag == false);
	CHECK(loaded.is_mark == false);
	CHECK(loaded.path == set.path);
	CHECK(loaded.dir_list == set.dir_list);
	CHECK(loaded.skip_dir == set.skip_dir);
	CHECK(loaded.target_dir == true);
	CHECK(loaded.sub_dir == true);
	CHECK(loaded.include_arc == true);
	CHECK(loaded.exclude_trash == true);
	CHECK(loaded.res_link == true);
	CHECK(loaded.dir_link == true);
	CHECK(loaded.mask == set.mask);
	CHECK(loaded.keywd == set.keywd);
	CHECK(loaded.reg_ex == true);
	CHECK(loaded.match_and == true);
	CHECK(loaded.match_case == true);
	CHECK(loaded.path_sort == true);
	CHECK(loaded.sort_mode == 3);
}

TEST_CASE("検索設定: TAG モードが往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.is_tag = true;
	set.tag_all = true;
	set.path = _T("C:\\dir\\");
	set.keywd = _T("tagword");
	set.match_and = true;

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.is_tag == true);
	CHECK(loaded.tag_all == true);
	CHECK(loaded.path == set.path);
	CHECK(loaded.keywd == set.keywd);
	CHECK(loaded.match_and == true);
	// 排他: 他のモードは立たない
	CHECK(loaded.is_mark == false);
	CHECK(loaded.is_dup_icon == false);
	CHECK(loaded.is_hard_link == false);
}

TEST_CASE("検索設定: MARK モードが往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.is_mark = true;
	set.path = _T("C:\\marked\\");

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.is_mark == true);
	CHECK(loaded.path == set.path);
}

TEST_CASE("検索設定: DICON モードが往復する (アイコン一覧)")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.is_dup_icon = true;
	set.icons = _T("a.ico\r\nb.ico\r\nc.ico");  // ini には "/" 区切りで保存される (VCL と同じ)

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.is_dup_icon == true);
	CHECK(loaded.icons == set.icons);
}

TEST_CASE("検索設定: HLINK モードが往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.is_hard_link = true;
	set.link_name = _T("target.txt");

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.is_hard_link == true);
	CHECK(loaded.link_name == set.link_name);
}

TEST_CASE("検索設定: 日時条件 (絶対指定) が往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.path = _T("C:\\dir\\");
	set.dt_mode = 1;
	set.dt_rel = 0;
	set.dt_value = TDateTime(2024, 1, 15);
	set.dt_str = _T("2024/01/15");

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.dt_mode == 1);
	CHECK(loaded.dt_rel == 0);
	CHECK(loaded.dt_str == set.dt_str);
	CHECK(static_cast<double>(loaded.dt_value) == doctest::Approx(static_cast<double>(set.dt_value)));
}

TEST_CASE("検索設定: 日時条件 (相対指定) は読み込み時に今日基準で計算し直す")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.dt_mode = 1;
	set.dt_rel = -30;  // 30日前

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.dt_rel == -30);
	CHECK(static_cast<double>(loaded.dt_value) == doctest::Approx(static_cast<double>(IncDay(Today(), -30))));
}

TEST_CASE("検索設定: サイズ条件と属性条件が往復する")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("fs.ini");

	FindSet set;
	set.sz_mode = 2;
	set.sz_value = 123456789;
	set.at_mode = 1;
	set.at_value = 4;

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		named_state::WriteFindSetIni(*ini, set);
		REQUIRE(ini->UpdateFile(true));
	}

	FindSet loaded;
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	REQUIRE(named_state::ReadFindSetIni(*ini, loaded));
	CHECK(loaded.sz_mode == 2);
	CHECK(loaded.sz_value == 123456789);
	CHECK(loaded.at_mode == 1);
	CHECK(loaded.at_value == 4);
}

TEST_CASE("ReadFindSetIni: セクションが無ければ false")
{
	TempDir tmp;
	const UnicodeString ini_path = tmp.path + _T("empty.ini");
	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));

	FindSet loaded;
	CHECK(named_state::ReadFindSetIni(*ini, loaded) == false);
}

//===========================================================================
// 検索設定: SaveFindSet / LoadFindSet (ファイル経由)
//===========================================================================
TEST_CASE("SaveFindSet/LoadFindSet: ファイル経由でも往復する")
{
	TempDir tmp;
	const UnicodeString path = tmp.path + _T("find") + named_state::ExtensionOf(Kind::FindSet);

	FindSet set;
	set.path = _T("C:\\search\\");
	set.keywd = _T("keyword");
	set.mask = _T("*.cpp");
	set.sub_dir = true;

	UnicodeString error;
	REQUIRE(named_state::SaveFindSet(path, set, error));

	FindSet loaded;
	REQUIRE(named_state::LoadFindSet(path, loaded, error));
	CHECK(loaded.path == set.path);
	CHECK(loaded.keywd == set.keywd);
	CHECK(loaded.mask == set.mask);
	CHECK(loaded.sub_dir == true);
}

TEST_CASE("SaveFindSet: 保存先が空なら失敗を理由付きで返す")
{
	FindSet set;
	UnicodeString error;
	CHECK(named_state::SaveFindSet(EmptyStr, set, error) == false);
	CHECK(!error.IsEmpty());
}

TEST_CASE("LoadFindSet: 無いファイルは理由を返して false")
{
	TempDir tmp;
	FindSet loaded;
	UnicodeString error;
	CHECK(named_state::LoadFindSet(tmp.path + _T("nothere.ini"), loaded, error) == false);
	CHECK(!error.IsEmpty());
}
