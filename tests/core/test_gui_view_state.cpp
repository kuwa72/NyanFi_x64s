/**
 * @file tests/core/test_gui_view_state.cpp
 * @brief gui/view_state.cpp (表示切り替えの計算) のテスト
 */
#include "doctest/doctest.h"

#include "gui/view_state.h"

TEST_CASE("ClampRatio: 片側が潰れない範囲に丸める")
{
	CHECK(view_state::ClampRatio(0.5) == doctest::Approx(0.5));
	CHECK(view_state::ClampRatio(0.0) == doctest::Approx(view_state::kMinRatio));
	CHECK(view_state::ClampRatio(1.0) == doctest::Approx(view_state::kMaxRatio));
	CHECK(view_state::ClampRatio(-3.0) == doctest::Approx(view_state::kMinRatio));
}

TEST_CASE("MoveBorder: 左右に1段ずつ動く")
{
	CHECK(view_state::MoveBorder(0.5, -1) == doctest::Approx(0.5 - view_state::kBorderStep));
	CHECK(view_state::MoveBorder(0.5, 1) == doctest::Approx(0.5 + view_state::kBorderStep));
	CHECK(view_state::MoveBorder(0.5, 0) == doctest::Approx(0.5));
}

TEST_CASE("MoveBorder: 端まで行っても潰れない")
{
	double r = 0.5;
	for (int i = 0; i < 100; i++) r = view_state::MoveBorder(r, -1);
	CHECK(r == doctest::Approx(view_state::kMinRatio));

	for (int i = 0; i < 200; i++) r = view_state::MoveBorder(r, 1);
	CHECK(r == doctest::Approx(view_state::kMaxRatio));
}

TEST_CASE("WidenSide: 広げる側で取り分が反転する")
{
	// MainFrm.cpp:27645 の `if (tag==1) r = 1.0 - r;`
	CHECK(view_state::WidenSide(true) == doctest::Approx(0.75));
	CHECK(view_state::WidenSide(false) == doctest::Approx(0.25));

	// EqualListWidth は WidenCurList に "50" と "Left" を渡したもの
	// (MainFrm.cpp:17244)
	CHECK(view_state::WidenSide(true, 0.5) == doctest::Approx(0.5));
	CHECK(view_state::WidenSide(false, 0.5) == doctest::Approx(0.5));
}

TEST_CASE("IsListedByAttr: 隠し属性とシステム属性は独立に効く")
{
	const int normal = faArchive;
	const int hidden = faHidden;
	const int system = faSysFile;
	const int both = faHidden | faSysFile;

	// どちらも表示しない設定
	CHECK(view_state::IsListedByAttr(normal, false, false));
	CHECK_FALSE(view_state::IsListedByAttr(hidden, false, false));
	CHECK_FALSE(view_state::IsListedByAttr(system, false, false));
	CHECK_FALSE(view_state::IsListedByAttr(both, false, false));

	// 隠しだけ表示する
	CHECK(view_state::IsListedByAttr(hidden, true, false));
	CHECK_FALSE(view_state::IsListedByAttr(system, true, false));
	// **両方の属性が立っているファイルは、片方が off なら出ない**
	CHECK_FALSE(view_state::IsListedByAttr(both, true, false));

	// 両方表示する
	CHECK(view_state::IsListedByAttr(both, true, true));
}
