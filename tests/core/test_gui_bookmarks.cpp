/**
 * @file tests/core/test_gui_bookmarks.cpp
 * @brief gui/bookmarks.cpp (栞マーク) のテスト
 *
 * 「次の栞へ」「前の栞へ」の折り返しは VCL の NextMark/PrevMark
 * (src/MainFrm.cpp:22380 / 23828) を実測して合わせてある。
 * 保存そのものは移植済みの UsrIniFile なので、ここでは包み方だけを見る。
 */
#include "doctest/doctest.h"

#include "gui/bookmarks.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

std::vector<bool> flags(const char *pattern)
{
	std::vector<bool> v;
	for (const char *p = pattern; *p != '\0'; ++p) v.push_back(*p == '*');
	return v;
}

void mkfile(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	::CloseHandle(h);
}

}  // namespace

//===========================================================================
// FindNext / FindPrev
//===========================================================================

TEST_CASE("FindNext: カーソルより後ろの最初の栞へ")
{
	//        0123456
	CHECK(bookmarks::FindNext(flags("..*..*."), 0) == 2);
	CHECK(bookmarks::FindNext(flags("..*..*."), 2) == 5);
}

TEST_CASE("FindNext: 後ろに無ければ先頭側へ折り返す")
{
	// VCL も idx1 が見つからなければ idx0 (カーソル位置以前で最初の栞) へ飛ぶ
	CHECK(bookmarks::FindNext(flags("*.*...."), 5) == 0);
}

TEST_CASE("FindNext: カーソル位置そのものも折り返し先になる")
{
	// カーソルが最後の栞の上にいて、後ろに栞が無い場合。VCL の判定は
	// `i<=c_idx` なのでカーソル位置を含み、結果として動かない
	CHECK(bookmarks::FindNext(flags("....*.."), 4) == 4);
}

TEST_CASE("FindNext: 栞が1つも無ければ -1")
{
	CHECK(bookmarks::FindNext(flags("......."), 3) == -1);
	CHECK(bookmarks::FindNext(std::vector<bool>(), 0) == -1);
}

TEST_CASE("FindPrev: カーソルより前の最後の栞へ")
{
	CHECK(bookmarks::FindPrev(flags("..*..*."), 6) == 5);
	CHECK(bookmarks::FindPrev(flags("..*..*."), 5) == 2);
}

TEST_CASE("FindPrev: 前に無ければ末尾側へ折り返す")
{
	CHECK(bookmarks::FindPrev(flags("....*.*"), 1) == 6);
}

TEST_CASE("FindPrev: 栞が1つも無ければ -1")
{
	CHECK(bookmarks::FindPrev(flags("......."), 3) == -1);
}

//===========================================================================
// ini とのやりとり
//===========================================================================

TEST_CASE("Toggle: 付ける→外すで切り替わる")
{
	TempDir tmp;
	const UnicodeString f = tmp.path + _T("a.txt");
	mkfile(f);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	CHECK(bookmarks::IsMarked(ini, f) == false);
	CHECK(bookmarks::Toggle(ini, f) == true);
	CHECK(bookmarks::IsMarked(ini, f) == true);
	CHECK(bookmarks::Toggle(ini, f) == false);
	CHECK(bookmarks::IsMarked(ini, f) == false);
}

TEST_CASE("Toggle: メモを付けて取り出せる")
{
	TempDir tmp;
	const UnicodeString f = tmp.path + _T("a.txt");
	mkfile(f);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	bookmarks::Toggle(ini, f, _T("あとで読む"));
	CHECK(bookmarks::MemoOf(ini, f) == UnicodeString(_T("あとで読む")));
}

TEST_CASE("All: ディレクトリをまたいだ栞を1本に均してパス順に並べる")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	bookmarks::Toggle(ini, b);
	bookmarks::Toggle(ini, a, _T("めも"));

	const std::vector<bookmarks::Mark> all = bookmarks::All(ini);
	REQUIRE(all.size() == 2);
	CHECK(SameText(all[0].path, a));
	CHECK(all[0].memo == UnicodeString(_T("めも")));
	CHECK(SameText(all[1].path, b));
	// 日時は FileMark が付ける (中身までは固定しない。空でないことだけ見る)
	CHECK(!all[0].stamp.IsEmpty());
}

TEST_CASE("ClearOf: 指定したパスの栞だけを外し、件数を返す")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	bookmarks::Toggle(ini, a);
	bookmarks::Toggle(ini, b);

	std::vector<UnicodeString> targets;
	targets.push_back(a);
	targets.push_back(tmp.path + _T("notmarked.txt"));  // 付いていないものは数えない

	CHECK(bookmarks::ClearOf(ini, targets) == 1);
	CHECK(bookmarks::IsMarked(ini, a) == false);
	CHECK(bookmarks::IsMarked(ini, b) == true);
}

TEST_CASE("MarkedFlags: 並び順のまま真偽値にする")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	bookmarks::Toggle(ini, b);

	std::vector<UnicodeString> paths;
	paths.push_back(a);
	paths.push_back(b);
	paths.push_back(EmptyStr);  // 区切り行などパスを持たないもの

	const std::vector<bool> f = bookmarks::MarkedFlags(ini, paths);
	REQUIRE(f.size() == 3);
	CHECK(f[0] == false);
	CHECK(f[1] == true);
	CHECK(f[2] == false);
}

TEST_CASE("TrimMissing: 実体の消えた栞を外し、件数を返す")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	UsrIniFile ini(tmp.path + _T("t.ini"));
	bookmarks::Toggle(ini, a);
	bookmarks::Toggle(ini, b);
	REQUIRE(bookmarks::All(ini).size() == 2);

	::DeleteFileW(b.c_str());
	CHECK(bookmarks::TrimMissing(ini) == 1);
	CHECK(bookmarks::IsMarked(ini, a) == true);
}

TEST_CASE("栞は ini に書き出して読み直せる")
{
	TempDir tmp;
	const UnicodeString f = tmp.path + _T("a.txt");
	const UnicodeString inipath = tmp.path + _T("t.ini");
	mkfile(f);

	{
		UsrIniFile ini(inipath);
		bookmarks::Toggle(ini, f, _T("メモ"));
		REQUIRE(ini.UpdateFile());
	}
	UsrIniFile again(inipath);
	CHECK(bookmarks::IsMarked(again, f) == true);
	CHECK(again.GetMarkMemo(f) == UnicodeString(_T("メモ")));
}
