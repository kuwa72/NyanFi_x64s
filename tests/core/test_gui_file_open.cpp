/**
 * @file tests/core/test_gui_file_open.cpp
 * @brief gui/file_open.h/.cpp のうち wx 非依存の対象決め (OpenByWin) の回帰テスト
 *
 * @details 実際に開く部分 (ShellExecuteExW) は実行環境に依存するため対象外。
 * ここで見るのは「何を開くか」だけで、VCL 版の該当は
 * `src/MainFrm.cpp` の `OpenByWinActionExecute` (22631行)。
 */
#include "doctest/doctest.h"

#include "gui/file_open.h"

using namespace file_open;

//===========================================================================
// OpenByWin: ResolveOpenTarget (MainFrm.cpp:22631)
//===========================================================================

TEST_CASE("ResolveOpenTarget: パラメータがあればそれを開く")
{
	// VCL は URL/mailto ならそのまま、そうでなければパラメータのファイル名。
	// どちらも「書かれているものをそのまま開く」なので区別しない
	CHECK(ResolveOpenTarget(_T("https://example.com/x"), _T("C:\\work\\a.txt"))
	      == UnicodeString(_T("https://example.com/x")));
	CHECK(ResolveOpenTarget(_T("C:\\other\\b.txt"), _T("C:\\work\\a.txt"))
	      == UnicodeString(_T("C:\\other\\b.txt")));
}

TEST_CASE("ResolveOpenTarget: パラメータが空ならカーソル位置を開く")
{
	CHECK(ResolveOpenTarget(EmptyStr, _T("C:\\work\\a.txt")) == UnicodeString(_T("C:\\work\\a.txt")));
}

TEST_CASE("ResolveOpenTarget: どちらも空なら空 (呼び出し側が警告を出す)")
{
	CHECK(ResolveOpenTarget(EmptyStr, EmptyStr).IsEmpty());
}
