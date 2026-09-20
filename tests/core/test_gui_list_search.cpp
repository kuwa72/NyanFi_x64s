/**
 * @file tests/core/test_gui_list_search.cpp
 * @brief gui/list_search.h/.cpp (L モードのログエラー移動と S モードの
 *        インクリメンタルサーチ操作) の回帰テスト
 *
 * @details gui/list_search.h/.cpp は wx に依存しない (nyanfi_gui_core)
 *          ため、GUI (wxWidgets) 無しでもここでテストできる。
 *          VCL 版との対応 (実測箇所) は各 TEST_CASE のコメントを参照。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/list_search.h"

using namespace list_search;

//===========================================================================
// L モード: NextErr / PrevErr (MainFrm.cpp::ExeCommandL、13377行の正規表現)
//===========================================================================

TEST_CASE("IsErrorLine: E/C/W のログ行をエラーと判定する")
{
	// VCL のパターンは "^.>([ECW]|(     [45]\\d{2})) .*"
	// gui/log_win.cpp の FormatLine は " >" + 時刻 + 状態文字 + 本文の形なので、
	// 先頭の " >" が "^.>" に、状態文字が "[ECW]" に一致する
	CHECK(IsErrorLine(_T(" >E COPY foo.txt")));
	CHECK(IsErrorLine(_T(" >C COPY foo.txt")));
	CHECK(IsErrorLine(_T(" >W COPY foo.txt")));
}

TEST_CASE("IsErrorLine: 通常のログ行はエラーではない")
{
	CHECK_FALSE(IsErrorLine(_T(" > COPY foo.txt")));
	CHECK_FALSE(IsErrorLine(_T(" >S COPY foo.txt")));
	CHECK_FALSE(IsErrorLine(_T(" >O COPY foo.txt")));
	CHECK_FALSE(IsErrorLine(_T(" >N COPY foo.txt")));
}

TEST_CASE("IsErrorLine: HTTP 状態コード風の行 (5xx/4xx) もエラーと判定する")
{
	CHECK(IsErrorLine(_T(" >     404 foo.txt")));
	CHECK(IsErrorLine(_T(" >     500 foo.txt")));
	CHECK_FALSE(IsErrorLine(_T(" >     200 foo.txt")));
}

TEST_CASE("IsErrorLine: 空行・装飾なしの詳細行はエラーではない")
{
	CHECK_FALSE(IsErrorLine(EmptyStr));
	// LogFileInfo の詳細行 (AddRaw) には " >" の接頭辞が無い
	CHECK_FALSE(IsErrorLine(_T("  FLINFO C:\\foo.txt")));
}

TEST_CASE("FindNextError: 現在位置より後の最初のエラーへ進む (折り返さない)")
{
	const std::vector<UnicodeString> lines{
		_T(" > COPY a.txt"),
		_T(" >E COPY b.txt"),
		_T(" > COPY c.txt"),
		_T(" >W COPY d.txt"),
	};
	// VCL は idx0+1 から下方向へ探し、無ければ beep (ここでは -1)
	CHECK(FindNextError(lines, 0, true) == 1);
	CHECK(FindNextError(lines, 1, true) == 3);
	CHECK(FindNextError(lines, 3, true) == -1);
}

TEST_CASE("FindNextError: 前方向は現在位置より前の最初のエラーへ戻る (折り返さない)")
{
	const std::vector<UnicodeString> lines{
		_T(" >E COPY a.txt"),
		_T(" > COPY b.txt"),
		_T(" >W COPY c.txt"),
	};
	CHECK(FindNextError(lines, 2, false) == 0);
	CHECK(FindNextError(lines, 1, false) == 0);
	CHECK(FindNextError(lines, 0, false) == -1);
}

TEST_CASE("FindNextError: 空の一覧・範囲外の開始位置では -1")
{
	const std::vector<UnicodeString> empty;
	CHECK(FindNextError(empty, 0, true) == -1);
	CHECK(FindNextError(empty, 0, false) == -1);

	const std::vector<UnicodeString> lines{_T(" >E COPY a.txt")};
	CHECK(FindNextError(lines, 99, true) == -1);
	CHECK(FindNextError(lines, -99, false) == -1);
}

//===========================================================================
// S モード: MigemoMode / NormalMode (MainFrm.cpp::FileListIncSearch、12065行)
//===========================================================================

TEST_CASE("ToggleMigemoMode: 辞書が無ければ ON にできない (VCL の DictReady ガード)")
{
	// VCL: is_Migemo = (MigemoMode)? (!is_Migemo && DictReady) : false
	CHECK(ToggleMigemoMode(false, false) == false);
	CHECK(ToggleMigemoMode(true, false) == false);
}

TEST_CASE("ToggleMigemoMode: 辞書があれば反転する")
{
	CHECK(ToggleMigemoMode(false, true) == true);
	CHECK(ToggleMigemoMode(true, true) == false);
}

TEST_CASE("SetNormalMode: 常に OFF に戻る")
{
	CHECK(SetNormalMode(true) == false);
	CHECK(SetNormalMode(false) == false);
}

//===========================================================================
// S モード: KeywordHistory (MainFrm.cpp::FileListIncSearch、12097行)
//===========================================================================

TEST_CASE("FilterKeywordHistory: 一覧に一致する候補だけを残す (順序は保つ)")
{
	const std::vector<UnicodeString> history{
		_T("report"), _T("zzz_no_match"), _T("readme"),
	};
	const std::vector<UnicodeString> names{
		_T("report.pdf"), _T("readme.txt"),
	};
	const std::vector<UnicodeString> filtered = FilterKeywordHistory(history, names);
	REQUIRE(filtered.size() == 2);
	CHECK(filtered[0] == UnicodeString(_T("report")));
	CHECK(filtered[1] == UnicodeString(_T("readme")));
}

TEST_CASE("FilterKeywordHistory: 1件も一致しなければ空 (VCL は Abort して beep)")
{
	const std::vector<UnicodeString> history{_T("zzz")};
	const std::vector<UnicodeString> names{_T("readme.txt")};
	CHECK(FilterKeywordHistory(history, names).empty());
}

//===========================================================================
// S モード: IncMatchSelect (MainFrm.cpp::set_IncSeaStt(true)、11945行)
//===========================================================================

TEST_CASE("CollectMatchedIndices: 一致している位置だけを集める")
{
	CHECK(CollectMatchedIndices({false, true, false, true}) == std::vector<int>{1, 3});
	CHECK(CollectMatchedIndices({false, false}).empty());
}

//===========================================================================
// S モード: IncSearchTop (MainFrm.cpp::FileListIncSearch、12160行の csr_top)
//===========================================================================

TEST_CASE("FindFromTop: 先頭から探し直す (先頭の項目自体も対象に含める)")
{
	const std::vector<UnicodeString> names{_T("apple"), _T("banana"), _T("avocado")};
	// VCL は s_idx=0 から find_NextIncSea (ループありなら先頭にも戻る)。
	// ここでは「先頭を含めて最初の一致」を返す単純化 (動作は等価)
	CHECK(FindFromTop(names, _T("avo")) == 2);
	CHECK(FindFromTop(names, _T("app")) == 0);
	CHECK(FindFromTop(names, _T("zzz")) == -1);
	CHECK(FindFromTop(names, EmptyStr) == -1);
}
