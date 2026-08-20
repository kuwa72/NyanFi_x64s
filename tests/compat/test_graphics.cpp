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
