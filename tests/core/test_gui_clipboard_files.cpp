/**
 * @file tests/core/test_gui_clipboard_files.cpp
 * @brief gui/clipboard_files.cpp のテスト
 *
 * @details 貼り付けは破壊的な操作なので、**弾く条件**を重点的に固定する。
 */
#include "doctest/doctest.h"

#include "gui/clipboard_files.h"

TEST_CASE("IsMoveEffect: ビットで判定する")
{
	// VCL は `is_move = (*ep & DROPEFFECT_MOVE)` (MainFrm.cpp:28702)。
	// エクスプローラは複数のビットを立てることがあるので、値の一致で
	// 判定してはいけない
	CHECK(clipboard_files::IsMoveEffect(DROPEFFECT_MOVE));
	CHECK(clipboard_files::IsMoveEffect(DROPEFFECT_MOVE | DROPEFFECT_LINK));
	CHECK(clipboard_files::IsMoveEffect(DROPEFFECT_MOVE | DROPEFFECT_COPY));

	CHECK_FALSE(clipboard_files::IsMoveEffect(DROPEFFECT_COPY));
	CHECK_FALSE(clipboard_files::IsMoveEffect(0));
}

TEST_CASE("ValidatePasteTargets: 普通のパスは通す")
{
	const auto r = clipboard_files::ValidatePasteTargets(
		{_T("C:\\src\\a.txt"), _T("C:\\src\\b.txt")}, _T("D:\\dst"));

	CHECK(r.accepted.size() == 2);
	CHECK(r.rejected.empty());
}

TEST_CASE("ValidatePasteTargets: 自分自身への貼り付けを弾く")
{
	const auto r = clipboard_files::ValidatePasteTargets({_T("C:\\work")}, _T("C:\\work"));
	CHECK(r.accepted.empty());
	REQUIRE(r.rejected.size() == 1);
	CHECK(ContainsText(r.rejected[0], _T("自分自身")));
}

TEST_CASE("ValidatePasteTargets: 配下への貼り付けを弾く (無限再帰になる)")
{
	const auto r = clipboard_files::ValidatePasteTargets({_T("C:\\work")}, _T("C:\\work\\sub"));
	CHECK(r.accepted.empty());
	CHECK(r.rejected.size() == 1);
}

TEST_CASE("ValidatePasteTargets: 元と同じディレクトリへの貼り付けを弾く")
{
	// 何も起きないので弾く
	const auto r = clipboard_files::ValidatePasteTargets({_T("C:\\src\\a.txt")}, _T("C:\\src"));
	CHECK(r.accepted.empty());
	REQUIRE(r.rejected.size() == 1);
	CHECK(ContainsText(r.rejected[0], _T("同じディレクトリ")));
}

TEST_CASE("ValidatePasteTargets: 末尾の区切りの有無で結果が変わらない")
{
	const auto a = clipboard_files::ValidatePasteTargets({_T("C:\\work\\")}, _T("C:\\work"));
	const auto b = clipboard_files::ValidatePasteTargets({_T("C:\\work")}, _T("C:\\work\\"));
	CHECK(a.accepted.empty());
	CHECK(b.accepted.empty());
}

TEST_CASE("ValidatePasteTargets: 弾いた分は黙って捨てず理由つきで返す")
{
	// 一部だけ弾かれたときに気づけるようにする
	const auto r = clipboard_files::ValidatePasteTargets(
		{_T("C:\\ok\\a.txt"), _T("C:\\dst")}, _T("C:\\dst"));

	CHECK(r.accepted.size() == 1);
	CHECK(r.rejected.size() == 1);
	CHECK(ContainsText(r.rejected[0], _T("C:\\dst")));
}

TEST_CASE("ValidatePasteTargets: 似た名前のディレクトリを誤って弾かない")
{
	// "C:\work" と "C:\work2" は別物。前方一致だけで判定すると弾いてしまう
    const auto r = clipboard_files::ValidatePasteTargets({_T("C:\\work")}, _T("C:\\work2"));
	CHECK(r.accepted.size() == 1);
	CHECK(r.rejected.empty());
}
