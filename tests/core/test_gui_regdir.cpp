/**
 * @file tests/core/test_gui_regdir.cpp
 * @brief gui/regdir.cpp (登録ディレクトリの判断ロジック) のテスト
 *
 * @details VCL の該当実装は `src/DirDlg.cpp` (`TRegDirDlg`) と
 *          `src/Global.cpp` (`get_RegDirItem` / `move_top_RegDirItem`)。
 *          登録ディレクトリ一覧 (`RegDirList`) の1行は `REGDIR_CSVITMCNT = 4`
 *          の CSV (`key,title,path,user`、`src/Global.h`)。
 *            - キー (`[0]`) は ChangeRegDir のパラメータ (`ActionParam[1]`、
 *              `MainFrm.cpp::ChangeRegDirActionExecute` を実測) で、
 *              大文字小文字を区別せず探す (`SameText` を実測)
 *            - タイトル (`[1]`) が `"-"` の行はセパレータ
 *              (`is_separator` を実測)。選択できず、パスは空扱い
 *              (`GetCurDirItem` が `is_separator` の行で空を返すのを実測)
 *            - 使用後に先頭へ動かすときセパレータをまたがない
 *              (`move_top_RegDirItem` を実測)
 *            - 一覧のキー押下は一致が1件だけのときに確定し、複数あるときは
 *              最後の一致へカーソルを動かすだけ
 *              (`RegDirListBoxKeyPress` の f_cnt 分岐を実測)
 */
#include "doctest/doctest.h"

#include <memory>

#include "gui/regdir.h"
#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

regdir::RegDirItem item_of(const wchar_t *key, const wchar_t *title, const wchar_t *path,
                           const wchar_t *user = L"")
{
	regdir::RegDirItem it;
	it.key = key;
	it.title = title;
	it.path = path;
	it.user = user;
	return it;
}

std::vector<regdir::RegDirItem> list_of_3()
{
	return {
		item_of(L"A", L"ツール", L"C:\\Tools\\"),
		item_of(L"B", L"資料", L"D:\\Docs\\"),
		item_of(L"C", L"音楽", L"E:\\Music\\"),
	};
}

}  // namespace

//===========================================================================
// ParseRecord / FormatRecord (REGDIR_CSVITMCNT = 4: key,title,path,user)
//===========================================================================

TEST_CASE("RegDir: CSVレコードを往復できる")
{
	const regdir::RegDirItem it = item_of(L"A", L"ツール", L"C:\\Tools\\", L"user1");
	const regdir::RegDirItem back = regdir::ParseRecord(regdir::FormatRecord(it));
	CHECK(back.key == UnicodeString(_T("A")));
	CHECK(back.title == UnicodeString(_T("ツール")));
	CHECK(back.path == UnicodeString(_T("C:\\Tools\\")));
	CHECK(back.user == UnicodeString(_T("user1")));
}

TEST_CASE("RegDir: ユーザ名の無いレコードも読める")
{
	const regdir::RegDirItem back = regdir::ParseRecord(UnicodeString(_T("B,資料,D:\\Docs\\,")));
	CHECK(back.key == UnicodeString(_T("B")));
	CHECK(back.title == UnicodeString(_T("資料")));
	CHECK(back.path == UnicodeString(_T("D:\\Docs\\")));
	CHECK(back.user.IsEmpty());
}

//===========================================================================
// IsSeparator / SelectablePath (is_separator / GetCurDirItem を実測)
//===========================================================================

TEST_CASE("RegDir: タイトルが - の行はセパレータでパスは選べない")
{
	const regdir::RegDirItem sep = item_of(L"", L"-", L"", L"");
	CHECK(regdir::IsSeparator(sep));
	CHECK(regdir::SelectablePath(sep).IsEmpty());

	const regdir::RegDirItem normal = item_of(L"A", L"ツール", L"C:\\Tools\\");
	CHECK_FALSE(regdir::IsSeparator(normal));
	CHECK(regdir::SelectablePath(normal) == UnicodeString(_T("C:\\Tools\\")));
}

//===========================================================================
// MoveTop (move_top_RegDirItem を実測: セパレータをまたいで先頭へ行かない)
//===========================================================================

TEST_CASE("RegDir: 先頭グループの項目は先頭へ動く")
{
	std::vector<regdir::RegDirItem> v = list_of_3();
	CHECK(regdir::MoveTop(v, 2));
	CHECK(v[0].key == UnicodeString(_T("C")));
	CHECK(v[1].key == UnicodeString(_T("A")));
	CHECK(v[2].key == UnicodeString(_T("B")));
}

TEST_CASE("RegDir: セパレータの後ろの項目はセパレータ直後までしか動かない")
{
	std::vector<regdir::RegDirItem> v = {
		item_of(L"A", L"ツール", L"C:\\Tools\\"),
		item_of(L"", L"-", L"", L""),
		item_of(L"B", L"資料", L"D:\\Docs\\"),
		item_of(L"C", L"音楽", L"E:\\Music\\"),
	};
	CHECK(regdir::MoveTop(v, 3));
	CHECK(v[0].key == UnicodeString(_T("A")));
	CHECK(v[1].title == UnicodeString(_T("-")));
	CHECK(v[2].key == UnicodeString(_T("C")));
	CHECK(v[3].key == UnicodeString(_T("B")));
}

TEST_CASE("RegDir: 先頭や範囲外は動かさない")
{
	std::vector<regdir::RegDirItem> v = list_of_3();
	CHECK_FALSE(regdir::MoveTop(v, 0));
	CHECK_FALSE(regdir::MoveTop(v, -1));
	CHECK_FALSE(regdir::MoveTop(v, 3));
	CHECK(v[0].key == UnicodeString(_T("A")));
}

//===========================================================================
// KeyMatches (RegDirListBoxKeyPress の f_cnt 数え上げを実測)
//===========================================================================

TEST_CASE("RegDir: キーは大文字小文字を区別せず探す")
{
	const std::vector<regdir::RegDirItem> v = list_of_3();
	const std::vector<int> hit = regdir::KeyMatches(v, _T("b"));
	REQUIRE(hit.size() == 1);
	CHECK(hit[0] == 1);
}

TEST_CASE("RegDir: 同じキーが複数あれば全部返す (VCL はカーソル移動だけ)")
{
	std::vector<regdir::RegDirItem> v = list_of_3();
	v.push_back(item_of(L"A", L"別名", L"F:\\Alias\\"));
	const std::vector<int> hit = regdir::KeyMatches(v, _T("A"));
	REQUIRE(hit.size() == 2);
	CHECK(hit[0] == 0);
	CHECK(hit[1] == 3);
}

TEST_CASE("RegDir: 該当なしは空")
{
	CHECK(regdir::KeyMatches(list_of_3(), _T("Z")).empty());
	CHECK(regdir::KeyMatches(list_of_3(), _T("")).empty());
}

//===========================================================================
// MatchesFilter (TRegDirDlg のフィルタ欄の絞り込み相当)
//===========================================================================

TEST_CASE("RegDir: 空フィルタは全部通す (セパレータも)")
{
	CHECK(regdir::MatchesFilter(item_of(L"A", L"ツール", L"C:\\Tools\\"), _T(""), false));
}

TEST_CASE("RegDir: タイトル・パス・キーに部分一致する")
{
	const regdir::RegDirItem it = item_of(L"A", L"ツール", L"C:\\Tools\\");
	CHECK(regdir::MatchesFilter(it, _T("ツール"), false));
	CHECK(regdir::MatchesFilter(it, _T("tools"), false));
	CHECK(regdir::MatchesFilter(it, _T("A"), false));
	CHECK_FALSE(regdir::MatchesFilter(it, _T("音楽"), false));
}

TEST_CASE("RegDir: 既定は OR、指定時は AND で結ぶ (AndOrAction を実測)")
{
	const regdir::RegDirItem it = item_of(L"A", L"ツール集", L"C:\\Tools\\");
	CHECK(regdir::MatchesFilter(it, _T("ツール 音楽"), false));
	CHECK_FALSE(regdir::MatchesFilter(it, _T("ツール 音楽"), true));
	CHECK(regdir::MatchesFilter(it, _T("ツール 集"), true));
}

TEST_CASE("RegDir: 大文字を含むと大小文字を区別する (contains_upper を実測)")
{
	const regdir::RegDirItem it = item_of(L"A", L"tools", L"C:\\Tools\\");
	CHECK(regdir::MatchesFilter(it, _T("Tools"), false));
	CHECK_FALSE(regdir::MatchesFilter(it, _T("TOOLS"), false));
}

//===========================================================================
// RegDirStore (WxGuiRegDir セクションの ini 永続化)
//===========================================================================

TEST_CASE("RegDirStore: 一覧を ini に往復できる")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi_wx.ini"));

	{
		regdir::RegDirStore store;
		store.MutableItems().push_back(item_of(L"A", L"ツール", L"C:\\Tools\\", L"user1"));
		regdir::RegDirItem sep;
		sep.title = _T("-");
		store.MutableItems().push_back(sep);
		store.MutableItems().push_back(item_of(L"B", L"資料", L"D:\\Docs\\"));

		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		store.SaveToIni(*ini);
		CHECK(ini->UpdateFile());
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	regdir::RegDirStore reloaded;
	reloaded.LoadFromIni(*ini);

	REQUIRE(reloaded.Items().size() == 3);
	CHECK(reloaded.Items()[0].key == UnicodeString(_T("A")));
	CHECK(reloaded.Items()[0].path == UnicodeString(_T("C:\\Tools\\")));
	CHECK(reloaded.Items()[0].user == UnicodeString(_T("user1")));
	CHECK(regdir::IsSeparator(reloaded.Items()[1]));
	CHECK(reloaded.Items()[2].title == UnicodeString(_T("資料")));
}

TEST_CASE("RegDirStore: セクションが無い場合は何もしない")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("does_not_exist.ini"));

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	regdir::RegDirStore store;
	store.LoadFromIni(*ini);
	CHECK(store.Items().empty());
}
