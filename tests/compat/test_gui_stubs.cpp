/**
 * @file tests/compat/test_gui_stubs.cpp
 * @brief compat/gui_stubs.h のうち「宣言のみ」ではなく実データ / 実計算を
 *        持たせた部分のテスト
 *
 * gui_stubs.h の GUI コントロールは原則としてメンバ関数の定義を書かない
 * (規約4)。ここでテストするのは、その例外として**実装した**次の 2 つだけ:
 *   - TStringGrid::Cells[col][row] (ColWidths / RowHeights と同じ実データ)
 *   - TForm::BoundsRect への代入と CenteredRect (Left/Top/Width/Height への展開)
 *
 * 宣言のみのメンバ (SelectAll / ItemRect / ClientToScreen / ModalResult /
 * Monitor / CalcHintRect / ActivateHint ほか) は**呼ぶとリンクエラーになる**
 * のが仕様なので、ここからは呼ばない。
 */
#include "doctest/doctest.h"

#include "compat/gui_stubs.h"

//===========================================================================
// TStringGrid::Cells
//===========================================================================
TEST_CASE("TStringGrid::Cells: 列が先、行が後の順で読み書きできる")
{
	TStringGrid grid;
	grid.Cells[0][0] = UnicodeString("A1");
	grid.Cells[1][0] = UnicodeString("B1");
	grid.Cells[0][1] = UnicodeString("A2");

	CHECK(UnicodeString(grid.Cells[0][0]) == UnicodeString("A1"));
	CHECK(UnicodeString(grid.Cells[1][0]) == UnicodeString("B1"));
	CHECK(UnicodeString(grid.Cells[0][1]) == UnicodeString("A2"));
	//列と行を取り違えていないこと
	CHECK(UnicodeString(grid.Cells[1][1]).IsEmpty());
}

TEST_CASE("TStringGrid::Cells: 未設定のセルは空文字列を返す")
{
	TStringGrid grid;
	CHECK(UnicodeString(grid.Cells[0][0]).IsEmpty());
	CHECK(UnicodeString(grid.Cells[7][9]).IsEmpty());
	//読んだだけでは領域を作らない (書いていない隣も空のまま)
	CHECK(UnicodeString(grid.Cells[7][8]).IsEmpty());
}

TEST_CASE("TStringGrid::Cells: 添字は自動的に伸びる (ColCount/RowCount とは独立)")
{
	TStringGrid grid;
	CHECK(grid.ColCount == 2);	//Delphi の既定値のまま
	CHECK(grid.RowCount == 2);

	grid.Cells[10][20] = UnicodeString("far");
	CHECK(UnicodeString(grid.Cells[10][20]) == UnicodeString("far"));
	//伸ばした途中のセルは空
	CHECK(UnicodeString(grid.Cells[5][20]).IsEmpty());
	//ColCount / RowCount は連動しない (実 VCL と違う点。宣言のコメント参照)
	CHECK(grid.ColCount == 2);
	CHECK(grid.RowCount == 2);
}

TEST_CASE("TStringGrid::Cells: 負の添字は無視され、壊れない")
{
	TStringGrid grid;
	grid.Cells[-1][0] = UnicodeString("x");
	grid.Cells[0][-1] = UnicodeString("y");
	CHECK(UnicodeString(grid.Cells[-1][0]).IsEmpty());
	CHECK(UnicodeString(grid.Cells[0][-1]).IsEmpty());
	CHECK(UnicodeString(grid.Cells[0][0]).IsEmpty());
}

TEST_CASE("TStringGrid::Cells: セル同士の代入 (Cells[a][b] = Cells[c][d]) ができる")
{
	TStringGrid grid;
	grid.Cells[0][0] = UnicodeString("src");
	grid.Cells[2][3] = grid.Cells[0][0];
	CHECK(UnicodeString(grid.Cells[2][3]) == UnicodeString("src"));
	//上書き
	grid.Cells[2][3] = UnicodeString("dst");
	CHECK(UnicodeString(grid.Cells[2][3]) == UnicodeString("dst"));
	CHECK(UnicodeString(grid.Cells[0][0]) == UnicodeString("src"));
}

TEST_CASE("TStringGrid::Cells: 値のメンバをそのまま呼べる (IsEmpty / Length)")
{
	TStringGrid grid;
	grid.Cells[1][1] = UnicodeString("abc");
	CHECK(grid.Cells[1][1].IsEmpty() == false);
	CHECK(grid.Cells[1][1].Length() == 3);
	CHECK(grid.Cells[0][0].IsEmpty() == true);
}

TEST_CASE("TStringGrid: Col / GridLineWidth の既定値")
{
	TStringGrid grid;
	CHECK(grid.Col == 0);
	CHECK(grid.Row == 0);
	CHECK(grid.GridLineWidth == 1);	//Delphi の TCustomGrid の既定
}

//===========================================================================
// TForm::BoundsRect
//===========================================================================
TEST_CASE("TForm::BoundsRect: Left/Top/Width/Height から矩形を組み立てる")
{
	TForm frm;
	frm.Left = 10;
	frm.Top = 20;
	frm.Width = 300;
	frm.Height = 200;

	TRect rc = frm.BoundsRect;
	CHECK(rc.Left == 10);
	CHECK(rc.Top == 20);
	CHECK(rc.Right == 310);
	CHECK(rc.Bottom == 220);
	//フィールドを直接読む書き方 (UIniFile.cpp) も同じ値
	CHECK(frm.BoundsRect.Right == 310);
	CHECK(frm.BoundsRect.Bottom == 220);
}

TEST_CASE("TForm::BoundsRect への代入は Left/Top/Width/Height へ展開される")
{
	TForm frm;
	frm.BoundsRect = TRect(100, 50, 400, 250);
	CHECK(frm.Left == 100);
	CHECK(frm.Top == 50);
	CHECK(frm.Width == 300);
	CHECK(frm.Height == 200);
	//往復して同じ矩形に戻る
	TRect rc = frm.BoundsRect;
	CHECK(rc.Left == 100);
	CHECK(rc.Top == 50);
	CHECK(rc.Right == 400);
	CHECK(rc.Bottom == 250);
}

TEST_CASE("TForm::BoundsRect::CenteredRect: TRect のものへ委譲する")
{
	//src/UserFunc.cpp:93 の show_ModalDlg と同じ形:
	//  dlg->BoundsRect = frm->BoundsRect.CenteredRect(dlg->BoundsRect);
	TForm parent;
	parent.Left = 0;
	parent.Top = 0;
	parent.Width = 1000;
	parent.Height = 800;

	TForm dlg;
	dlg.Left = 500;
	dlg.Top = 500;
	dlg.Width = 200;
	dlg.Height = 100;

	dlg.BoundsRect = parent.BoundsRect.CenteredRect(dlg.BoundsRect);
	CHECK(dlg.Left == 400);
	CHECK(dlg.Top == 350);
	CHECK(dlg.Width == 200);
	CHECK(dlg.Height == 100);
}

//===========================================================================
// 既定値 (実 GUI が無い状態で src が読む値)
//===========================================================================
TEST_CASE("THeaderSection: 幅制約の既定は「制約なし」(0 / 10000)")
{
	//src/UserFunc.cpp:787-788 の set_HeaderSecWidth が「固定の解除」として
	//書き込む値と同じ。既定値がこれと食い違うと、初期状態だけ挙動が変わる
	THeaderSection sec;
	CHECK(sec.MinWidth == 0);
	CHECK(sec.MaxWidth == 10000);
	CHECK(sec.Width == 0);
}

TEST_CASE("THeaderSections::Count は素の int (std::min にそのまま渡せる)")
{
	THeaderSections secs;
	CHECK(secs.Count == 0);
	//UserFunc.cpp:748 の `std::min(hp->Sections->Count, gp->ColCount)` と同じ形
	int col_count = 3;
	CHECK(std::min(secs.Count, col_count) == 0);
	secs.Count = 5;
	CHECK(std::min(secs.Count, col_count) == 3);
}

TEST_CASE("TComboBox / TCustomEdit: 選択位置の既定値")
{
	TComboBox cb;
	CHECK(cb.SelStart == 0);
	CHECK(cb.SelLength == 0);
	CHECK(cb.ItemIndex == -1);
	REQUIRE(cb.Canvas != nullptr);

	TEdit ed;
	CHECK(ed.SelStart == 0);
	CHECK(ed.SelLength == 0);
}

TEST_CASE("TCustomListBox: Canvas は基底が 1 つだけ持つ (派生で隠していない)")
{
	//TListBox / TCheckListBox が自前の Canvas を重ねて宣言していると、
	//TCustomListBox* 越しに触ったときだけ別の TCanvas を見ることになる
	//(TControl::Tag と同じ罠)。同一であることを固定する
	TListBox lb;
	TCustomListBox *base = &lb;
	CHECK(base->Canvas == lb.Canvas);

	TCheckListBox clb;
	TCustomListBox *base2 = &clb;
	CHECK(base2->Canvas == clb.Canvas);
}
