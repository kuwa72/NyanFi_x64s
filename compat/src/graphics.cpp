/**
 * @file graphics.cpp
 * @brief compat/graphics.h の実装。TBitmap は 24bpp のトップダウン DIB
 *        (CreateDIBSection、負の biHeight) を実体に持つ。
 */
#include "compat/graphics.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

//---------------------------------------------------------------------------
TColor ColorToRGB(TColor color)
{
	//Delphi の ColorToRGB と同じロジック: 最上位ビットが立っていれば
	//システム色 (下位 1 バイトが Win32 の COLOR_* インデックス)
	if (color < 0) return static_cast<TColor>(::GetSysColor(static_cast<int>(color & 0xFF)));
	return color;
}

//---------------------------------------------------------------------------
UnicodeString ColorToString(TColor color)
{
	//簡易実装: clXXX 名の逆引きテーブルまでは持たず "$BBGGRR" 形式で返す
	//(src/ での使用は ColPicker.cpp の 1 箇所のみで、名前解決までは要求されていない)
	UnicodeString s;
	s.sprintf(_T("$%06X"), static_cast<unsigned>(color) & 0xFFFFFF);
	return s;
}

//---------------------------------------------------------------------------
TColor StringToColor(const UnicodeString &s)
{
	//"$BBGGRR" / "0xBBGGRR" 形式のみ対応する簡易実装 (src/ では未使用)
	UnicodeString t = s;
	if (!t.IsEmpty() && (t[1] == L'$' || t[1] == L'#')) t = t.SubString(2);
	return static_cast<TColor>(wcstol(t.c_str(), nullptr, 16));
}

//---------------------------------------------------------------------------
namespace Graphics {

namespace {

/// 24bpp DIB の 1 行あたりバイト数 (DWORD 境界に切り上げ)
int stride_of(int width)
{
	return ((width * 3 + 3) / 4) * 4;
}

}  // namespace

//---------------------------------------------------------------------------
TBitmap::TBitmap() : Canvas(new TCanvas()) {}

//---------------------------------------------------------------------------
TBitmap::~TBitmap()
{
	release_bitmap();
	delete Canvas;
}

//---------------------------------------------------------------------------
void TBitmap::release_bitmap()
{
	Canvas->SetHandleValue(nullptr);
	if (hbmp_) {
		::DeleteObject(hbmp_);
		hbmp_ = nullptr;
	}
	bits_ = nullptr;
	width_ = 0;
	height_ = 0;
	stride_ = 0;
}

//---------------------------------------------------------------------------
void TBitmap::SetSize(int width, int height)
{
	release_bitmap();
	if (width <= 0 || height <= 0) return;

	//Phase 0 では pf24bit のみを実データとして扱う (使用箇所の実測に基づく)
	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;	//負値でトップダウン DIB にする (ScanLine[0]=最上行)
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	void *bits = nullptr;
	HBITMAP hbmp = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!hbmp || !bits) return;

	hbmp_ = hbmp;
	bits_ = bits;
	width_ = width;
	height_ = height;
	stride_ = stride_of(width);

	HDC dc = ::CreateCompatibleDC(nullptr);
	if (dc) ::SelectObject(dc, hbmp_);
	Canvas->SetHandleValue(dc);
}

//---------------------------------------------------------------------------
void TBitmap::SetPixelFormat(TPixelFormat fmt)
{
	//Phase 0 は 24bpp 固定の実装。他の値は列挙のみ受理し実データは変えない
	//(usr_wic.cpp 等で実際に使われているのは pf24bit のみ)
	pixel_format_ = fmt;
}

//---------------------------------------------------------------------------
void *TBitmap::scanline_at(int index) const
{
	if (!bits_ || index < 0 || index >= height_) return nullptr;
	return static_cast<unsigned char *>(bits_) + static_cast<std::size_t>(index) * static_cast<std::size_t>(stride_);
}

//---------------------------------------------------------------------------
void TBitmap::Assign(TPersistent *source)
{
	TBitmap *src = dynamic_cast<TBitmap *>(source);
	if (!src) return;

	pixel_format_ = src->pixel_format_;
	alpha_format_ = src->alpha_format_;
	SetSize(src->width_, src->height_);
	if (bits_ && src->bits_) {
		const int n = std::min(stride_, src->stride_) * height_;
		std::memcpy(bits_, src->bits_, static_cast<std::size_t>(n));
	}
}

//---------------------------------------------------------------------------
void TFont::Assign(TFont *source)
{
	if (!source) return;
	Color = source->Color;
	Height = source->Height;
}

//---------------------------------------------------------------------------
void TCanvas::MoveTo(int x, int y)
{
	if (dc_) ::MoveToEx(dc_, x, y, nullptr);
}

//---------------------------------------------------------------------------
void TCanvas::LineTo(int x, int y)
{
	if (!dc_) return;
	HPEN pen = ::CreatePen(static_cast<int>(Pen->Style), Pen->Width, static_cast<COLORREF>(ColorToRGB(Pen->Color)));
	HGDIOBJ old = ::SelectObject(dc_, pen);
	::LineTo(dc_, x, y);
	::SelectObject(dc_, old);
	::DeleteObject(pen);
}

//---------------------------------------------------------------------------
void TCanvas::FillRect(const TRect &rect)
{
	if (!dc_) return;
	RECT r{rect.Left, rect.Top, rect.Right, rect.Bottom};
	HBRUSH brush = ::CreateSolidBrush(static_cast<COLORREF>(ColorToRGB(Brush->Color)));
	::FillRect(dc_, &r, brush);
	::DeleteObject(brush);
}

//---------------------------------------------------------------------------
void TCanvas::CopyRect(const TRect &dest, TCanvas *src, const TRect &srcRect)
{
	if (!dc_ || !src || !src->GetHandle()) return;
	::StretchBlt(dc_, dest.Left, dest.Top, dest.Width(), dest.Height(), src->GetHandle(), srcRect.Left, srcRect.Top,
	             srcRect.Width(), srcRect.Height(), SRCCOPY);
}

//---------------------------------------------------------------------------
void TCanvas::StretchDraw(const TRect &rect, TBitmap *graphic)
{
	if (!dc_ || !graphic || !graphic->Canvas->GetHandle()) return;
	::StretchBlt(dc_, rect.Left, rect.Top, rect.Width(), rect.Height(), graphic->Canvas->GetHandle(), 0, 0,
	             static_cast<int>(graphic->Width), static_cast<int>(graphic->Height), SRCCOPY);
}

//---------------------------------------------------------------------------
int TCanvas::TextWidth(const UnicodeString &s) const
{
	if (!dc_) return 0;

	//Font->Height を反映した一時フォントを作って選択する (Canvas 自身は現状
	//フォントを自動選択しないため、計測直前にここで反映する)
	HFONT font = ::CreateFontW(-std::abs(Font->Height), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
	                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
	                           nullptr);
	HGDIOBJ old = font ? ::SelectObject(dc_, font) : nullptr;

	SIZE sz{};
	::GetTextExtentPoint32W(dc_, s.c_str(), s.Length(), &sz);

	if (font) {
		::SelectObject(dc_, old);
		::DeleteObject(font);
	}
	return sz.cx;
}

}  // namespace Graphics

//---------------------------------------------------------------------------
namespace Vcl {
namespace Themes {

TColor TStyleServices::GetSystemColor(TColor color) const
{
	//Phase 0 の最小実装: VCL Styles を実装せず ::GetSysColor にマップするだけ。
	//Phase 2 で wxWidgets のダークモード対応に置き換える予定。
	return ColorToRGB(color);
}

TStyleServices g_active_style_instance;
TStyleServices *TStyleManager::ActiveStyle = &g_active_style_instance;

}  // namespace Themes
}  // namespace Vcl
