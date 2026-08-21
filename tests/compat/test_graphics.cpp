/**
 * @file tests/compat/test_graphics.cpp
 * @brief Vcl.Graphics / Vcl.Themes (TColor / Graphics::TBitmap / TStyleManager)
 *        互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "compat/graphics.h"

//===========================================================================
// TColor / clXXX / ColorToRGB
//===========================================================================
TEST_CASE("TColor: 標準色は Delphi と同じ 0x00BBGGRR")
{
	CHECK(clWhite == TColor(0xFFFFFF));
	CHECK(clBlack == TColor(0x000000));
	CHECK(clRed == TColor(0x0000FF));	//BGR なので赤は下位バイト
	CHECK(clBlue == TColor(0xFF0000));
}

TEST_CASE("ColorToRGB: 通常色はそのまま返す")
{
	CHECK(ColorToRGB(clWhite) == TColor(0xFFFFFF));
	CHECK(ColorToRGB(TColor(0x123456)) == TColor(0x123456));
}

TEST_CASE("ColorToRGB: システム色 (最上位ビット) は GetSysColor に解決される")
{
	//clWindow は SC|COLOR_WINDOW。実値は環境依存だが、少なくとも
	//「最上位ビットが立ったまま返らない (実 RGB 値になる)」ことは保証する
	TColor rgb = ColorToRGB(clWindow);
	CHECK(rgb >= 0);
}

TEST_CASE("clNone: ColorToRGB では特別扱いされずそのまま返る (Delphi と同じ)")
{
	CHECK(ColorToRGB(clNone) == clNone);
}

//===========================================================================
// Graphics::TBitmap
//===========================================================================
TEST_CASE("TBitmap: SetSize 直後は Width/Height/Empty が正しい")
{
	std::unique_ptr<Graphics::TBitmap> bmp(new Graphics::TBitmap());
	CHECK(bmp->Empty);
	bmp->PixelFormat = pf24bit;
	bmp->SetSize(4, 3);
	CHECK_FALSE(bmp->Empty);
	CHECK(bmp->Width == 4);
	CHECK(bmp->Height == 3);
	CHECK(bmp->Handle != nullptr);
}

TEST_CASE("TBitmap: ScanLine[] に書き込んだピクセルが読み戻せる")
{
	std::unique_ptr<Graphics::TBitmap> bmp(new Graphics::TBitmap());
	bmp->PixelFormat = pf24bit;
	bmp->SetSize(2, 2);

	//24bpp なので 1 ピクセル = 3 バイト (B,G,R の順)
	for (int y = 0; y < 2; ++y) {
		unsigned char *row = static_cast<unsigned char *>(bmp->ScanLine[y]);
		REQUIRE(row != nullptr);
		for (int x = 0; x < 2; ++x) {
			row[x * 3 + 0] = static_cast<unsigned char>(y * 10);	//B
			row[x * 3 + 1] = static_cast<unsigned char>(y * 10 + 1);	//G
			row[x * 3 + 2] = static_cast<unsigned char>(y * 10 + 2);	//R
		}
	}
	unsigned char *row1 = static_cast<unsigned char *>(bmp->ScanLine[1]);
	CHECK(row1[0] == 10);
	CHECK(row1[1] == 11);
	CHECK(row1[2] == 12);
}

TEST_CASE("TBitmap: AlphaFormat の読み書き")
{
	std::unique_ptr<Graphics::TBitmap> bmp(new Graphics::TBitmap());
	CHECK(bmp->AlphaFormat == afIgnored);
	bmp->AlphaFormat = afDefined;
	CHECK(bmp->AlphaFormat == afDefined);
}

TEST_CASE("TBitmap: Assign で内容がコピーされる")
{
	std::unique_ptr<Graphics::TBitmap> src(new Graphics::TBitmap());
	src->PixelFormat = pf24bit;
	src->SetSize(2, 2);
	unsigned char *row = static_cast<unsigned char *>(src->ScanLine[0]);
	row[0] = 1;
	row[1] = 2;
	row[2] = 3;

	Graphics::TBitmap dst;
	dst.Assign(src.get());
	CHECK(dst.Width == 2);
	CHECK(dst.Height == 2);
	unsigned char *drow = static_cast<unsigned char *>(dst.ScanLine[0]);
	REQUIRE(drow != nullptr);
	CHECK(drow[0] == 1);
	CHECK(drow[1] == 2);
	CHECK(drow[2] == 3);
}

TEST_CASE("TBitmap::Canvas: CopyRect で別のビットマップから複写できる (usr_wic.cpp と同じ用法)")
{
	std::unique_ptr<Graphics::TBitmap> src(new Graphics::TBitmap());
	src->PixelFormat = pf24bit;
	src->SetSize(4, 4);
	unsigned char *row = static_cast<unsigned char *>(src->ScanLine[0]);
	row[0] = 9;
	row[1] = 8;
	row[2] = 7;

	std::unique_ptr<Graphics::TBitmap> dst(new Graphics::TBitmap());
	dst->SetSize(4, 4);
	dst->Canvas->CopyRect(Rect(0, 0, 4, 4), src->Canvas, Rect(0, 0, 4, 4));

	unsigned char *drow = static_cast<unsigned char *>(dst->ScanLine[0]);
	CHECK(drow[0] == 9);
	CHECK(drow[1] == 8);
	CHECK(drow[2] == 7);
}

//===========================================================================
// TCanvas: Pen/Brush/FillRect/MoveTo/LineTo (最小実装の疎通確認)
//===========================================================================
TEST_CASE("TCanvas: Pen/Brush の既定値と書き込み")
{
	Graphics::TBitmap bmp;
	bmp.SetSize(4, 4);
	bmp.Canvas->Brush->Color = clRed;
	bmp.Canvas->Brush->Style = bsSolid;
	CHECK(bmp.Canvas->Brush->Color == clRed);

	bmp.Canvas->FillRect(Rect(0, 0, 4, 4));
	unsigned char *row = static_cast<unsigned char *>(bmp.ScanLine[0]);
	REQUIRE(row != nullptr);
	//clRed = 0x0000FF (BGR) なので R バイトのみ 0xFF になる
	CHECK(row[0] == 0);
	CHECK(row[1] == 0);
	CHECK(row[2] == 0xFF);
}

//===========================================================================
// TStyleManager (Phase 0 の最小実装: GetSysColor へのマップ)
//===========================================================================
TEST_CASE("TStyleManager::ActiveStyle->GetSystemColor は ColorToRGB と同じ値になる")
{
	REQUIRE(TStyleManager::ActiveStyle != nullptr);
	CHECK(TStyleManager::ActiveStyle->GetSystemColor(clBtnFace) == ColorToRGB(clBtnFace));
	CHECK(UnicodeString(TStyleManager::ActiveStyle->Name) == UnicodeString("Windows"));
}

//===========================================================================
// TPoint (Phase 3b: 小文字 x / y の別名)
//===========================================================================
TEST_CASE("TPoint: 大文字 X/Y と小文字 x/y は同じ記憶域を指す")
{
	//C++Builder の TPoint は POINT を継承しているため両方の綴りが使える。
	//src の実呼び出しは小文字だけなので、書いた値がもう一方からも見えること
	//(= 無名共用体で重なっていること) を固定する
	TPoint p(3, 7);
	CHECK(p.x == 3);
	CHECK(p.y == 7);
	CHECK(p.X == 3);
	CHECK(p.Y == 7);

	p.x = 11;
	p.y = 13;
	CHECK(p.X == 11);
	CHECK(p.Y == 13);

	p.X = -1;
	p.Y = -2;
	CHECK(p.x == -1);
	CHECK(p.y == -2);
}

TEST_CASE("TPoint: 既定構築は原点、レイアウトは int 2 つのまま")
{
	TPoint p;
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	CHECK(sizeof(TPoint) == 2 * sizeof(int));
}

TEST_CASE("TPoint: コピーしても両方の綴りが追随する")
{
	TPoint a(5, 6);
	TPoint b = a;
	CHECK(b.X == 5);
	CHECK(b.y == 6);
	b.x = 9;
	CHECK(a.x == 5);	//コピーであって参照ではない
	CHECK(b.X == 9);
}

//===========================================================================
// TRect (Phase 3b: SetWidth / SetHeight / CenteredRect / OffsetRect)
//===========================================================================
TEST_CASE("TRect::SetWidth/SetHeight: 左上を動かさずに大きさだけ変える")
{
	TRect r(10, 20, 30, 50);
	CHECK(r.Width() == 20);
	CHECK(r.Height() == 30);

	r.SetWidth(100);
	CHECK(r.Left == 10);
	CHECK(r.Top == 20);
	CHECK(r.Right == 110);
	CHECK(r.Bottom == 50);

	r.SetHeight(5);
	CHECK(r.Top == 20);
	CHECK(r.Bottom == 25);

	//縮める側 (usr_hintwin.cpp は「最小幅に広げる」用途だが、縮小も同じ式)
	r.SetWidth(0);
	CHECK(r.Right == 10);
	CHECK(r.Width() == 0);
}

TEST_CASE("TRect::CenteredRect: r と同じ大きさの矩形を自分の中央に置く")
{
	TRect outer(0, 0, 100, 100);
	TRect inner(0, 0, 20, 40);

	TRect c = outer.CenteredRect(inner);
	CHECK(c.Left == 40);
	CHECK(c.Top == 30);
	CHECK(c.Width() == 20);
	CHECK(c.Height() == 40);

	//自分は変わらない
	CHECK(outer.Left == 0);
	CHECK(outer.Right == 100);

	//inner の位置は無視され、大きさだけが使われる
	TRect moved(500, 600, 520, 640);
	TRect c2 = outer.CenteredRect(moved);
	CHECK(c2.Left == 40);
	CHECK(c2.Top == 30);
	CHECK(c2.Width() == 20);
	CHECK(c2.Height() == 40);
}

TEST_CASE("TRect::CenteredRect: 原点以外の外枠にもオフセットが乗る")
{
	TRect outer(100, 200, 300, 400);	//200x200
	TRect c = outer.CenteredRect(TRect(0, 0, 50, 50));
	CHECK(c.Left == 175);
	CHECK(c.Top == 275);
	CHECK(c.Right == 225);
	CHECK(c.Bottom == 325);
}

TEST_CASE("TRect::CenteredRect: 差が奇数のときは Delphi と同じく切り捨てる")
{
	//(11 - 4) / 2 == 3 (C++ の int 除算 = Delphi の div。どちらも 0 方向へ切り捨て)
	TRect c = TRect(0, 0, 11, 11).CenteredRect(TRect(0, 0, 4, 4));
	CHECK(c.Left == 3);
	CHECK(c.Top == 3);
	CHECK(c.Width() == 4);
}

TEST_CASE("TRect::CenteredRect: 内側が外側より大きいと左上が外へはみ出す")
{
	TRect c = TRect(0, 0, 10, 10).CenteredRect(TRect(0, 0, 30, 30));
	CHECK(c.Left == -10);
	CHECK(c.Top == -10);
	CHECK(c.Width() == 30);
	CHECK(c.Height() == 30);
}

TEST_CASE("OffsetRect: TRect を参照で受けて平行移動する")
{
	TRect r(1, 2, 11, 22);
	OffsetRect(r, 5, -1);
	CHECK(r.Left == 6);
	CHECK(r.Top == 1);
	CHECK(r.Right == 16);
	CHECK(r.Bottom == 21);
	//大きさは変わらない
	CHECK(r.Width() == 10);
	CHECK(r.Height() == 20);
}

TEST_CASE("TPoint: == / != で座標を比較できる")
{
	CHECK(TPoint(1, 2) == TPoint(1, 2));
	CHECK(TPoint(1, 2) != TPoint(1, 3));
	CHECK(TPoint(1, 2) != TPoint(2, 2));
	CHECK_FALSE(TPoint(0, 0) != TPoint(0, 0));
}
