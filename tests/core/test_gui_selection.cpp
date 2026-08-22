/**
 * @file tests/core/test_gui_selection.cpp
 * @brief gui/selection.cpp (一覧の選択操作) のテスト
 *
 * @details VCL の該当実装 (src/MainFrm.cpp の Sel*ActionExecute) を読んで
 *          合わせた挙動を固定する。特に間違えやすい3点を明示的に見る:
 *            - SelAllFile は「全選択」ではなくトグルで、ディレクトリは常に解除
 *            - SelReverseAll はディレクトリも対象
 *            - SelSameExt は追加ではなく「一致するものだけを選択し直す」
 */
#include "doctest/doctest.h"

#include "gui/selection.h"

namespace {

FileItem file_of(const UnicodeString &name, bool marked = false)
{
	FileItem it;
	it.name = name;
	it.marked = marked;
	return it;
}

FileItem dir_of(const UnicodeString &name, bool marked = false)
{
	FileItem it;
	it.name = name;
	it.is_dir = true;
	it.size = -1;
	it.marked = marked;
	return it;
}

FileItem parent_item()
{
	FileItem it;
	it.name = _T("..");
	it.is_dir = true;
	it.is_parent = true;
	return it;
}

/// ".." / dir1 / a.txt / b.txt / c.dat
std::vector<FileItem> sample()
{
	return {parent_item(), dir_of(_T("dir1")), file_of(_T("a.txt")),
	        file_of(_T("b.txt")), file_of(_T("c.dat"))};
}

}  // namespace

//===========================================================================
// 反転
//===========================================================================

TEST_CASE("ReverseAll: ディレクトリも反転する")
{
	// MainFrm.cpp:25284 は is_dir で除外していない
	std::vector<FileItem> v = sample();
	v[2].marked = true;

	selection::ReverseAll(v);

	CHECK_FALSE(v[0].marked);  // ".." は常に対象外
	CHECK(v[1].marked);        // ディレクトリも反転する
	CHECK_FALSE(v[2].marked);
	CHECK(v[3].marked);
	CHECK(v[4].marked);
}

TEST_CASE("ReverseFiles: ディレクトリは触らない")
{
	std::vector<FileItem> v = sample();
	selection::ReverseFiles(v);

	CHECK_FALSE(v[0].marked);
	CHECK_FALSE(v[1].marked);  // ディレクトリは変わらない
	CHECK(v[2].marked);
	CHECK(v[3].marked);
	CHECK(v[4].marked);
}

//===========================================================================
// 全選択 (トグル)
//===========================================================================

TEST_CASE("ToggleAllFiles: 選択0件なら全ファイルを選択する")
{
	std::vector<FileItem> v = sample();
	selection::ToggleAllFiles(v);

	CHECK_FALSE(v[0].marked);
	CHECK_FALSE(v[1].marked);  // **ディレクトリは選択しない**
	CHECK(v[2].marked);
	CHECK(v[3].marked);
	CHECK(v[4].marked);
}

TEST_CASE("ToggleAllFiles: 1件でも選択があれば全解除する")
{
	// 「全選択」ではなくトグル (MainFrm.cpp:24829 の GetSelCount(lst)==0)
	std::vector<FileItem> v = sample();
	v[3].marked = true;

	selection::ToggleAllFiles(v);
	CHECK(selection::MarkedCount(v) == 0);
}

TEST_CASE("ToggleAllFiles: ディレクトリの選択は常に解除される")
{
	std::vector<FileItem> v = sample();
	v[1].marked = true;  // ディレクトリだけ選択済み

	selection::ToggleAllFiles(v);
	// 選択が1件あるので全解除の側に倒れる
	CHECK_FALSE(v[1].marked);
	CHECK(selection::MarkedCount(v) == 0);
}

TEST_CASE("ToggleAllItems: ディレクトリも含めてトグルする")
{
	std::vector<FileItem> v = sample();
	selection::ToggleAllItems(v);

	CHECK_FALSE(v[0].marked);  // ".." だけは対象外
	CHECK(v[1].marked);
	CHECK(v[4].marked);

	selection::ToggleAllItems(v);
	CHECK(selection::MarkedCount(v) == 0);
}

TEST_CASE("ClearAll: すべて解除する")
{
	std::vector<FileItem> v = sample();
	selection::ToggleAllItems(v);
	REQUIRE(selection::MarkedCount(v) > 0);

	selection::ClearAll(v);
	CHECK(selection::MarkedCount(v) == 0);
}

//===========================================================================
// 同じ拡張子 / 同じ名前
//===========================================================================

TEST_CASE("SelectSameExt: 一致するものだけを選択し直す")
{
	// 追加ではない (MainFrm.cpp:25337 の `fp->selected = SameText(...)`)
	std::vector<FileItem> v = sample();
	v[4].marked = true;  // c.dat を先に選択しておく

	CHECK(selection::SelectSameExt(v, 2));  // カーソルは a.txt

	CHECK(v[2].marked);
	CHECK(v[3].marked);
	CHECK_FALSE(v[4].marked);  // 先に選択されていた .dat は解除される
	CHECK_FALSE(v[1].marked);  // ディレクトリは対象外
}

TEST_CASE("SelectSameExt: 大文字小文字は区別しない")
{
	std::vector<FileItem> v = {file_of(_T("a.TXT")), file_of(_T("b.txt"))};
	CHECK(selection::SelectSameExt(v, 0));
	CHECK(v[1].marked);
}

TEST_CASE("SelectSameExt: カーソルがディレクトリなら何もしない")
{
	std::vector<FileItem> v = sample();
	CHECK_FALSE(selection::SelectSameExt(v, 1));
	CHECK(selection::MarkedCount(v) == 0);

	CHECK_FALSE(selection::SelectSameExt(v, 0));   // ".."
	CHECK_FALSE(selection::SelectSameExt(v, 99));  // 範囲外
}

TEST_CASE("SelectSameName: 主部が同じファイルを選択する")
{
	std::vector<FileItem> v = {file_of(_T("doc.txt")), file_of(_T("doc.bak")),
	                           file_of(_T("other.txt"))};
	CHECK(selection::SelectSameName(v, 0));
	CHECK(v[0].marked);
	CHECK(v[1].marked);
	CHECK_FALSE(v[2].marked);
}

//===========================================================================
// 文字列 / 日付での選択
//===========================================================================

TEST_CASE("SelectMatching: 名前に含む項目を選択する (大文字小文字を区別しない)")
{
	std::vector<FileItem> v = sample();
	CHECK(selection::SelectMatching(v, _T("TX")) == 2);
	CHECK(v[2].marked);
	CHECK(v[3].marked);
	CHECK_FALSE(v[4].marked);
}

TEST_CASE("SelectMatching: 空文字列なら何もしない")
{
	std::vector<FileItem> v = sample();
	v[2].marked = true;
	CHECK(selection::SelectMatching(v, EmptyStr) == 0);
	CHECK(v[2].marked);  // 触っていない
}

TEST_CASE("SelectByDate: より古い / 同じ日 / より新しい")
{
	std::vector<FileItem> v(3);
	v[0].name = _T("old.txt");
	v[0].stamp = EncodeDate(2026, 8, 1);
	v[1].name = _T("same.txt");
	v[1].stamp = EncodeDate(2026, 8, 21) + EncodeTime(13, 0, 0, 0);  // 時刻あり
	v[2].name = _T("new.txt");
	v[2].stamp = EncodeDate(2026, 9, 1);

	const TDateTime border = EncodeDate(2026, 8, 21);

	CHECK(selection::SelectByDate(v, border, selection::DateCompare::Before) == 1);
	CHECK(v[0].marked);

	// Same は「同じ日」の比較。時刻は見ない
	CHECK(selection::SelectByDate(v, border, selection::DateCompare::Same) == 1);
	CHECK(v[1].marked);

	CHECK(selection::SelectByDate(v, border, selection::DateCompare::After) == 2);
	CHECK(v[1].marked);  // 13:00 は border (0:00) より後
	CHECK(v[2].marked);
}

//===========================================================================
// 選択項目への移動 / 範囲選択
//===========================================================================

TEST_CASE("FindNextMarked: 次と前の選択項目")
{
	std::vector<FileItem> v = sample();
	v[1].marked = true;
	v[4].marked = true;

	CHECK(selection::FindNextMarked(v, 0, true) == 1);
	CHECK(selection::FindNextMarked(v, 1, true) == 4);
	CHECK(selection::FindNextMarked(v, 4, true) == -1);  // 巡回しない

	CHECK(selection::FindNextMarked(v, 4, false) == 1);
	CHECK(selection::FindNextMarked(v, 1, false) == -1);
}

TEST_CASE("FindNextMarked: 選択が無ければ -1")
{
	std::vector<FileItem> v = sample();
	CHECK(selection::FindNextMarked(v, 0, true) == -1);
	CHECK(selection::FindNextMarked(v, 4, false) == -1);
}

TEST_CASE("MarkRange: 範囲を選択する (向きは問わない)")
{
	std::vector<FileItem> v = sample();
	selection::MarkRange(v, 2, 4);  // [2, 4) = a.txt, b.txt
	CHECK(v[2].marked);
	CHECK(v[3].marked);
	CHECK_FALSE(v[4].marked);  // to は含まない

	selection::ClearAll(v);
	selection::MarkRange(v, 4, 2);  // 逆向きでも同じ
	CHECK(v[2].marked);
	CHECK(v[3].marked);
}

TEST_CASE("MarkRange: 範囲外を渡しても落ちない")
{
	std::vector<FileItem> v = sample();
	selection::MarkRange(v, -5, 99);
	CHECK_FALSE(v[0].marked);  // ".." は選択されない
	CHECK(v[1].marked);
	CHECK(v[4].marked);
}

//===========================================================================
// マスク・一覧・日付による選択 (機能群16)
//===========================================================================

TEST_CASE("SelectByMask: 一致するものだけを選択し直す (追加ではない)")
{
	std::vector<FileItem> v = {
		file_of(_T("a.txt")), file_of(_T("b.md")), file_of(_T("c.txt"))
	};
	v[1].marked = true;  // 事前の選択は残さない

	CHECK(selection::SelectByMask(v, _T("*.txt")) == 2);
	CHECK(v[0].marked == true);
	CHECK(v[1].marked == false);
	CHECK(v[2].marked == true);
}

TEST_CASE("SelectByMask: セミコロンで複数指定できる")
{
	std::vector<FileItem> v = {
		file_of(_T("a.txt")), file_of(_T("b.md")), file_of(_T("c.ini"))
	};
	CHECK(selection::SelectByMask(v, _T("*.txt;*.md")) == 2);
}

TEST_CASE("SelectByMask: \"..\" は選択しない")
{
	std::vector<FileItem> v = {parent_item(), file_of(_T("a.txt"))};
	CHECK(selection::SelectByMask(v, _T("*")) == 1);
	CHECK(v[0].marked == false);
}

TEST_CASE("SelectByNames: 名前の一致で選ぶ (大文字小文字は区別しない)")
{
	std::vector<FileItem> v = {
		file_of(_T("Alpha.txt")), file_of(_T("beta.txt")), file_of(_T("gamma.txt"))
	};
	std::vector<UnicodeString> names = {_T("alpha.txt"), _T("GAMMA.TXT"), _T("nothere.txt")};

	CHECK(selection::SelectByNames(v, names) == 2);
	CHECK(v[0].marked == true);
	CHECK(v[1].marked == false);
	CHECK(v[2].marked == true);
}

TEST_CASE("SelectByDateCondition: 書式が不正なら -1 と理由を返す")
{
	std::vector<FileItem> v = {file_of(_T("a.txt"))};
	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("よくわからない"), Now(), error) == -1);
	CHECK(!error.IsEmpty());
}

TEST_CASE("SelectByDateCondition: 絶対指定で古い側を選ぶ")
{
	std::vector<FileItem> v = {file_of(_T("old.txt")), file_of(_T("new.txt"))};
	v[0].stamp = EncodeDate(2020, 1, 1);
	v[1].stamp = EncodeDate(2030, 1, 1);

	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("<2025/01/01"), Now(), error) == 1);
	CHECK(v[0].marked == true);
	CHECK(v[1].marked == false);
}

TEST_CASE("SelectByDateCondition: ディレクトリは常に非選択 (VCL と同じ)")
{
	std::vector<FileItem> v = {dir_of(_T("sub")), file_of(_T("old.txt"))};
	v[0].stamp = EncodeDate(2020, 1, 1);
	v[1].stamp = EncodeDate(2020, 1, 1);

	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("<2025/01/01"), Now(), error) == 1);
	CHECK(v[0].marked == false);
	CHECK(v[1].marked == true);
}

TEST_CASE("FindNextSameName: 名前主部が同じ次のファイルへ")
{
	std::vector<FileItem> v = {
		file_of(_T("doc.txt")), file_of(_T("other.md")),
		file_of(_T("doc.pdf")), file_of(_T("doc.md"))
	};
	CHECK(selection::FindNextSameName(v, 0) == 2);
	CHECK(selection::FindNextSameName(v, 2) == 3);
}

TEST_CASE("FindNextSameName: 後ろに無ければ先頭側へ折り返す")
{
	std::vector<FileItem> v = {
		file_of(_T("doc.txt")), file_of(_T("other.md")), file_of(_T("doc.pdf"))
	};
	CHECK(selection::FindNextSameName(v, 2) == 0);
}

TEST_CASE("FindNextSameName: 他に無ければ -1 (その場に留まらせない)")
{
	std::vector<FileItem> v = {file_of(_T("only.txt")), file_of(_T("other.md"))};
	CHECK(selection::FindNextSameName(v, 0) == -1);
}

TEST_CASE("FindNextSameName: カーソルがディレクトリなら何もしない")
{
	std::vector<FileItem> v = {dir_of(_T("doc")), file_of(_T("doc.txt"))};
	CHECK(selection::FindNextSameName(v, 0) == -1);
}

TEST_CASE("MaskOfMarked: 選択項目の名前を ; で繋ぐ")
{
	std::vector<FileItem> v = {
		file_of(_T("a.txt")), file_of(_T("b.txt")), file_of(_T("c.txt"))
	};
	v[0].marked = true;
	v[2].marked = true;

	CHECK(selection::MaskOfMarked(v) == UnicodeString(_T("a.txt;c.txt")));
}

TEST_CASE("MaskOfMarked: 選択が無ければ空 (呼び出し側がマスク解除に使う)")
{
	std::vector<FileItem> v = {file_of(_T("a.txt"))};
	CHECK(selection::MaskOfMarked(v).IsEmpty());
}

TEST_CASE("MaskExcludingMarked: 選択されていない方を並べる")
{
	std::vector<FileItem> v = {
		file_of(_T("a.txt")), file_of(_T("b.txt")), file_of(_T("c.txt"))
	};
	v[1].marked = true;

	CHECK(selection::MaskExcludingMarked(v) == UnicodeString(_T("a.txt;c.txt")));
}

TEST_CASE("MaskExcludingMarked: 全部選択されていれば空 (隠すものが残らない)")
{
	std::vector<FileItem> v = {file_of(_T("a.txt"))};
	v[0].marked = true;
	CHECK(selection::MaskExcludingMarked(v).IsEmpty());
}

// get_DateCond (src/UserFunc.cpp:464) の書き写しなので、書式の分岐を一通り見る。
// UserFunc.cpp はリンクできないため写してある (報告書 §24)
TEST_CASE("SelectByDateCondition: 相対指定 (nD/nM/nY) を解釈する")
{
	std::vector<FileItem> v = {file_of(_T("old.txt")), file_of(_T("new.txt"))};
	v[0].stamp = IncDay(Date(), -100);
	v[1].stamp = Date();

	UnicodeString error;
	// "-30D" = 30日前より古いもの
	CHECK(selection::SelectByDateCondition(v, _T("<-30D"), Now(), error) == 1);
	CHECK(v[0].marked == true);
	CHECK(v[1].marked == false);
}

TEST_CASE("SelectByDateCondition: TD は今日")
{
	std::vector<FileItem> v = {file_of(_T("today.txt")), file_of(_T("old.txt"))};
	v[0].stamp = Now();
	v[1].stamp = IncDay(Date(), -5);

	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("TD"), Now(), error) == 1);
	CHECK(v[0].marked == true);
}

TEST_CASE("SelectByDateCondition: CP はカーソル位置の日付")
{
	std::vector<FileItem> v = {file_of(_T("a.txt")), file_of(_T("b.txt"))};
	const TDateTime pivot = EncodeDate(2024, 6, 15);
	v[0].stamp = pivot;
	v[1].stamp = EncodeDate(2024, 6, 16);

	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("CP"), pivot, error) == 1);
	CHECK(v[0].marked == true);
	CHECK(v[1].marked == false);
}

TEST_CASE("SelectByDateCondition: 単位が D/M/Y 以外なら不正")
{
	std::vector<FileItem> v = {file_of(_T("a.txt"))};
	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("<30W"), Now(), error) == -1);
}

TEST_CASE("SelectByDateCondition: <>= で始まらなければ不正")
{
	std::vector<FileItem> v = {file_of(_T("a.txt"))};
	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("2024/01/01"), Now(), error) == -1);
}

TEST_CASE("SelectByDateCondition: 同じ日の比較は時刻を見ない")
{
	std::vector<FileItem> v = {file_of(_T("morning.txt")), file_of(_T("evening.txt"))};
	v[0].stamp = EncodeDate(2024, 6, 15) + EncodeTime(1, 0, 0, 0);
	v[1].stamp = EncodeDate(2024, 6, 15) + EncodeTime(23, 0, 0, 0);

	UnicodeString error;
	CHECK(selection::SelectByDateCondition(v, _T("=2024/06/15"), Now(), error) == 2);
}
