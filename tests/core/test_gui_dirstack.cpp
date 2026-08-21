/**
 * @file tests/core/test_gui_dirstack.cpp
 * @brief gui/navigation.cpp の DirStack / NextDriveOf のテスト
 */
#include "doctest/doctest.h"

#include <set>

#include "gui/navigation.h"

namespace {

/// 存在するディレクトリを差し替えられるようにする
std::function<bool(const UnicodeString &)> exists_in(const std::set<UnicodeString> &alive)
{
	return [alive](const UnicodeString &p) { return alive.count(p) != 0; };
}

const auto all_exist = [](const UnicodeString &) { return true; };

}  // namespace

TEST_CASE("DirStack: 後入れ先出し (先頭に積む)")
{
	// VCL は Insert(0, ...) なので、最後に積んだものが最初に出る
	DirStack st;
	CHECK(st.IsEmpty());

	st.Push(_T("C:\\a"), 1);
	st.Push(_T("C:\\b"), 2);
	CHECK(st.Count() == 2);

	DirStack::Entry e;
	REQUIRE(st.Pop(e, all_exist));
	CHECK(e.path == UnicodeString(_T("C:\\b")));
	CHECK(e.cursor == 2);

	REQUIRE(st.Pop(e, all_exist));
	CHECK(e.path == UnicodeString(_T("C:\\a")));
	CHECK(st.IsEmpty());
}

TEST_CASE("DirStack: 空なら Pop は false")
{
	DirStack st;
	DirStack::Entry e;
	CHECK_FALSE(st.Pop(e, all_exist));
}

TEST_CASE("DirStack: 存在しなくなったディレクトリは読み飛ばす")
{
	// MainFrm.cpp:23715。消えたディレクトリでスタックが詰まらないようにする
	DirStack st;
	st.Push(_T("C:\\alive"), 0);
	st.Push(_T("C:\\gone1"), 0);
	st.Push(_T("C:\\gone2"), 0);

	DirStack::Entry e;
	REQUIRE(st.Pop(e, exists_in({_T("C:\\alive")})));
	CHECK(e.path == UnicodeString(_T("C:\\alive")));
	CHECK(st.IsEmpty());  // 飛ばした分も取り除かれている
}

TEST_CASE("DirStack: 全部消えていたら false")
{
	DirStack st;
	st.Push(_T("C:\\gone"), 0);
	DirStack::Entry e;
	CHECK_FALSE(st.Pop(e, exists_in({})));
	CHECK(st.IsEmpty());
}

TEST_CASE("DirStack: 空のパスは積まない")
{
	DirStack st;
	st.Push(EmptyStr, 0);
	CHECK(st.IsEmpty());
}

//===========================================================================
// NextDriveOf
//===========================================================================

TEST_CASE("NextDriveOf: 現在より辞書順で大きい最初のドライブ")
{
	// VCL は一覧中の位置を +1 するのではない (MainFrm.cpp:22368)
	const std::vector<UnicodeString> d = {_T("C:\\"), _T("D:\\"), _T("E:\\")};

	CHECK(NextDriveOf(d, _T("C:\\"), true) == UnicodeString(_T("D:\\")));
	CHECK(NextDriveOf(d, _T("D:\\"), true) == UnicodeString(_T("E:\\")));
	CHECK(NextDriveOf(d, _T("E:\\"), true) == UnicodeString(_T("C:\\")));  // 先頭へ回る
}

TEST_CASE("NextDriveOf: 現在のドライブが一覧に無くても動く")
{
	// 取り外した直後など。位置を +1 する実装だと破綻する
	const std::vector<UnicodeString> d = {_T("C:\\"), _T("E:\\")};
	CHECK(NextDriveOf(d, _T("D:\\"), true) == UnicodeString(_T("E:\\")));
	CHECK(NextDriveOf(d, _T("D:\\"), false) == UnicodeString(_T("C:\\")));
}

TEST_CASE("NextDriveOf: 前へ回る")
{
	const std::vector<UnicodeString> d = {_T("C:\\"), _T("D:\\"), _T("E:\\")};
	CHECK(NextDriveOf(d, _T("E:\\"), false) == UnicodeString(_T("D:\\")));
	CHECK(NextDriveOf(d, _T("C:\\"), false) == UnicodeString(_T("E:\\")));  // 末尾へ回る
}

TEST_CASE("NextDriveOf: 一覧が空なら空文字列")
{
	CHECK(NextDriveOf({}, _T("C:\\"), true).IsEmpty());
}
