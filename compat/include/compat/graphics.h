/**
 * @file compat/graphics.h
 * @brief Vcl.Graphics / Vcl.Themes 相当の互換シム (TColor / Graphics::TBitmap /
 *        TStyleManager)
 *
 * 対象コードでの実測: TColor 88 / clNone 5 / clWindow 7 / clWindowText 7 /
 * ColorToRGB 9 / Graphics::TBitmap 26 / TStyleManager::ActiveStyle 17。
 *
 * TColor:
 *   Delphi と同じ 32bit 値 (0x00BBGGRR)。システム色は最上位ビットが立ち、下位
 *   1 バイトが Win32 の COLOR_* インデックスになる (Delphi の実装と同じ)。
 *   使われている clXXX 定数は usr_color.cpp / Global.cpp / HistFrm.cpp /
 *   InspectFrm.cpp 等の実使用箇所から全数洗い出した。
 *
 * Graphics::TBitmap:
 *   usr_wic.cpp を中心に実際に使われているメンバ (PixelFormat / SetSize /
 *   ScanLine[] / Width / Height / Handle / Canvas / Empty / AlphaFormat /
 *   Assign) のみを実装している。24bpp (pf24bit) だけが実際に使われているため、
 *   ScanLine の実装も 24bpp DIB 前提。他の PixelFormat 値は列挙のみで
 *   SetSize 時の実データは 24bpp として確保される (報告に明記)。
 *
 * TCanvas / TPen / TBrush / TFont:
 *   usr_wic.cpp 以外に imgv_thread.cpp / UserFunc.cpp / usr_scale.cpp でも
 *   Graphics::TBitmap::Canvas 経由で使われているのを確認したため、そこで
 *   実際に呼ばれているメンバ (CopyRect / StretchDraw / Lock / Unlock /
 *   MoveTo / LineTo / FillRect / Pen / Brush / Handle) を実装した。ただし
 *   これらのファイル自体を通しでビルド検証してはいない (フォーム依存の型を
 *   多数参照しているため Phase 0 の対象外)。
 *
 * TStyleManager:
 *   Phase 0 の指示どおり GetSystemColor を ::GetSysColor へマップする最小実装。
 *   VCL Styles のダーク/ライト切替機構そのものは実装していない。
 *   ここは Phase 2 で wxWidgets のダークモード対応に置き換える対象。
 *   RegisterStyleHook / Engine / TrySetStyle (MainFrm.cpp, NyanFi.cpp が使用)
 *   は GUI 生成後の話であり Phase 0 では未実装 (報告に明記)。
 *
 * Graphics::TIcon は Global.cpp 等 GUI ファイルで使われているが、担当範囲の
 * 実測対象 (usr_wic.cpp 等) には現れず、今回は未実装 (報告に明記)。
 */
#ifndef NYANFI_COMPAT_GRAPHICS_H
#define NYANFI_COMPAT_GRAPHICS_H

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// Delphi TColor 相当 (32bit 符号あり整数。0x00BBGGRR / システム色は最上位ビット)
using TColor = Int32;

//---------------------------------------------------------------------------
/// RGB 実値を得る (System.UITypes::ColorToRGB 相当)。システム色は GetSysColor で解決する
TColor ColorToRGB(TColor color);

/// 色を "clXXX" 名または "$BBGGRR" 形式の文字列にする (簡易実装。src/ での使用は 1 箇所)
UnicodeString ColorToString(TColor color);
/// ColorToString の逆変換 (src/ では未使用。API 完全性のために実装)
TColor StringToColor(const UnicodeString &s);

namespace Graphics {

//---------------------------------------------------------------------------
// clXXX 定数。値は Delphi の Vcl.Graphics / System.UITypes と同じもの。
// システム色は SC(0x80000000) | Win32 の COLOR_* を使う (windows.h のマクロ)。
//---------------------------------------------------------------------------
constexpr TColor SC = static_cast<TColor>(0x80000000);

//標準 16 色 (Windows 20色パレットの基本色)
constexpr TColor clBlack   = TColor(0x000000);
constexpr TColor clMaroon  = TColor(0x000080);
constexpr TColor clGreen   = TColor(0x008000);
constexpr TColor clOlive   = TColor(0x008080);
constexpr TColor clNavy    = TColor(0x800000);
constexpr TColor clPurple  = TColor(0x800080);
constexpr TColor clTeal    = TColor(0x808000);
constexpr TColor clGray    = TColor(0x808080);
constexpr TColor clSilver  = TColor(0xC0C0C0);
constexpr TColor clRed     = TColor(0x0000FF);
constexpr TColor clLime    = TColor(0x00FF00);
constexpr TColor clYellow  = TColor(0x00FFFF);
constexpr TColor clBlue    = TColor(0xFF0000);
constexpr TColor clFuchsia = TColor(0xFF00FF);
constexpr TColor clAqua    = TColor(0xFFFF00);
constexpr TColor clWhite   = TColor(0xFFFFFF);

//旧 Windows 20色パレットの残り (推測箇所: clSkyBlue の値は手元で Delphi ヘッダを
//参照できず記憶に基づく。誤りがあれば要修正)
constexpr TColor clSkyBlue = TColor(0xF0CAA6);

//GDI 標準ブラシ由来と推測される色 (推測箇所: DKGRAY_BRUSH/LTGRAY_BRUSH の色に
//合わせた。Delphi 公式ヘッダでの値を確認できていない)
constexpr TColor clDkGray = TColor(0x00404040);
constexpr TColor clLtGray = TColor(0x00C0C0C0);

//CSS3 Web カラー (clWebXXX)。src/ で使われているものだけを実装
//(推測箇所: いずれも CSS3 名前付きカラーの標準 RGB 値から BGR 変換したもの。
// Delphi の Vcl.Graphics.pas 上の実際の定義値と突き合わせてはいない)
constexpr TColor clWebDeepPink     = TColor(0x9314FF);	//CSS DeepPink    #FF1493
constexpr TColor clWebFirebrick    = TColor(0x2222B2);	//CSS Firebrick   #B22222
constexpr TColor clWebIndigo       = TColor(0x82004B);	//CSS Indigo      #4B0082
constexpr TColor clWebLimeGreen    = TColor(0x32CD32);	//CSS LimeGreen   #32CD32
constexpr TColor clWebSaddleBrown  = TColor(0x13458B);	//CSS SaddleBrown #8B4513

//システム色 (Win32 の COLOR_* インデックスへ委譲する)
constexpr TColor clWindow            = TColor(SC | COLOR_WINDOW);
constexpr TColor clWindowText        = TColor(SC | COLOR_WINDOWTEXT);
constexpr TColor clWindowFrame       = TColor(SC | COLOR_WINDOWFRAME);
constexpr TColor clBtnFace           = TColor(SC | COLOR_BTNFACE);
constexpr TColor clBtnText           = TColor(SC | COLOR_BTNTEXT);
constexpr TColor clBtnShadow         = TColor(SC | COLOR_BTNSHADOW);
constexpr TColor clGrayText          = TColor(SC | COLOR_GRAYTEXT);
constexpr TColor clHighlight         = TColor(SC | COLOR_HIGHLIGHT);
constexpr TColor clHighlightText     = TColor(SC | COLOR_HIGHLIGHTTEXT);
constexpr TColor clMenu              = TColor(SC | COLOR_MENU);
constexpr TColor clMenuText          = TColor(SC | COLOR_MENUTEXT);
constexpr TColor clAppWorkSpace      = TColor(SC | COLOR_APPWORKSPACE);
#ifdef COLOR_MENUHILIGHT
constexpr TColor clMenuHighlight     = TColor(SC | COLOR_MENUHILIGHT);
#else
constexpr TColor clMenuHighlight     = TColor(SC | 29);
#endif

/// 「色無し」を表す特別な値。ColorToRGB では通常の値として扱われる (Delphi と同じ)
constexpr TColor clNone = TColor(0x1FFFFFFF);
/// 「既定色を使う」を表す特別な値
constexpr TColor clDefault = TColor(0x20000000);

//---------------------------------------------------------------------------
/// Graphics::TPixelFormat 相当。実際に使われているのは pf24bit のみ
enum TPixelFormat { pfDevice, pf1bit, pf4bit, pf8bit, pf15bit, pf16bit, pf24bit, pf32bit, pfCustom };

/// Graphics::TAlphaFormat 相当
enum TAlphaFormat { afIgnored, afDefined, afPremultiplied };

class TCanvas;

//---------------------------------------------------------------------------
/**
 * @brief Graphics::TBitmap 相当
 * @details CreateDIBSection による 24bpp トップダウン DIB を実体に持つ。
 *          ScanLine[i] は常に上から i 行目 (VCL の TBitmap と同じ向き)。
 */
class TBitmap : public TPersistent {
public:
	TBitmap();
	~TBitmap() override;
	TBitmap(const TBitmap &) = delete;
	TBitmap &operator=(const TBitmap &) = delete;

	void Assign(TPersistent *source) override;

	void SetSize(int width, int height);
	int GetWidth() const { return width_; }
	int GetHeight() const { return height_; }
	bool GetEmpty() const { return width_ == 0 || height_ == 0; }

	TPixelFormat GetPixelFormat() const { return pixel_format_; }
	void SetPixelFormat(TPixelFormat fmt);

	TAlphaFormat GetAlphaFormat() const { return alpha_format_; }
	void SetAlphaFormat(TAlphaFormat fmt) { alpha_format_ = fmt; }

	HBITMAP GetHandle() const { return hbmp_; }

	/// ScanLine[] プロパティ (1 行分の先頭ポインタ。上から index 行目)
	class ScanLineProperty {
	public:
		explicit ScanLineProperty(TBitmap *owner) : owner_(owner) {}
		void *operator[](int index) const { return owner_->scanline_at(index); }

	private:
		TBitmap *owner_;
	};

	compat::ROProperty<TBitmap, int, &TBitmap::GetWidth> Width{this};
	compat::ROProperty<TBitmap, int, &TBitmap::GetHeight> Height{this};
	compat::ROProperty<TBitmap, bool, &TBitmap::GetEmpty> Empty{this};
	compat::RWValueProperty<TBitmap, TPixelFormat, &TBitmap::GetPixelFormat, &TBitmap::SetPixelFormat> PixelFormat{
		this};
	compat::RWValueProperty<TBitmap, TAlphaFormat, &TBitmap::GetAlphaFormat, &TBitmap::SetAlphaFormat> AlphaFormat{
		this};
	compat::ROProperty<TBitmap, HBITMAP, &TBitmap::GetHandle> Handle{this};
	ScanLineProperty ScanLine{this};
	/// bmp->Canvas->Xxx (アロー呼び出し) と TCanvas *cv = bmp->Canvas; (生ポインタ
	/// 代入) の両方の呼び出し形が src/ にあるため、生ポインタとして公開する。
	/// ポインタ自体は TBitmap の生存期間中不変で、指す先の HDC だけを
	/// SetSize()/SetPixelFormat() 実行時に張り替える。
	TCanvas *const Canvas;

private:
	void *scanline_at(int index) const;
	void release_bitmap();

	int width_ = 0;
	int height_ = 0;
	int stride_ = 0;	//!< 1 行のバイト数 (24bpp, 4 バイト境界に切り上げ)
	TPixelFormat pixel_format_ = pf24bit;
	TAlphaFormat alpha_format_ = afIgnored;
	HBITMAP hbmp_ = nullptr;
	void *bits_ = nullptr;	//!< CreateDIBSection の DIB ピクセルデータ (トップダウン)
};

//---------------------------------------------------------------------------
// TRect / Rect() — Vcl.Graphics 系 API (CopyRect 等) が要求するため最小限を実装
// (実際の Delphi では System.Types 相当だが、依存先を増やさないためここに置く)
//---------------------------------------------------------------------------
struct TRect {
	int Left = 0;
	int Top = 0;
	int Right = 0;
	int Bottom = 0;

	TRect() = default;
	TRect(int left, int top, int right, int bottom) : Left(left), Top(top), Right(right), Bottom(bottom) {}

	int Width() const { return Right - Left; }
	int Height() const { return Bottom - Top; }
};

inline TRect Rect(int left, int top, int right, int bottom)
{
	return TRect(left, top, right, bottom);
}

//---------------------------------------------------------------------------
/// TPenMode 相当 (実測: pmCopy / pmNot のみ使用)
enum TPenMode {
	pmBlack, pmWhite, pmNop, pmNot, pmCopy, pmNotCopy, pmMergePenNot, pmMaskPenNot,
	pmMergeNotPen, pmMaskNotPen, pmMerge, pmNotMerge, pmMask, pmNotMask, pmXor, pmNotXor,
};
/// TPenStyle 相当 (実測: psSolid / psDot / psDash のみ使用。Win32 の PS_* と
/// 同じ並びなので CreatePen にそのまま渡せる)
enum TPenStyle { psSolid, psDash, psDot, psDashDot, psDashDotDot, psClear, psInsideFrame };
/// TBrushStyle 相当 (実測: bsSolid のみ使用)
enum TBrushStyle { bsSolid, bsClear, bsHorizontal, bsVertical, bsFDiagonal, bsBDiagonal, bsCross, bsDiagCross };

//---------------------------------------------------------------------------
/// TPen 相当 (最小実装)
class TPen {
public:
	TColor Color = clBlack;
	int Width = 1;
	TPenMode Mode = pmCopy;
	TPenStyle Style = psSolid;
};

/// TBrush 相当 (最小実装)
class TBrush {
public:
	TColor Color = clWhite;
	TBrushStyle Style = bsSolid;
};

/// TFont 相当 (最小実装。実測: Color / Height / Assign が usr_str.cpp で使用)
class TFont {
public:
	TColor Color = clWindowText;
	int Height = -12;	//!< Delphi 既定に合わせた仮の値 (負値 = 文字高さ基準)

	/// source の内容をコピーする (usr_str.cpp: cv->Font->Assign(pp->Font) 等)
	void Assign(TFont *source);
};

//---------------------------------------------------------------------------
/**
 * @brief TCanvas 相当 (最小実装)
 * @details GDI の HDC をラップする。imgv_thread.cpp / UserFunc.cpp /
 *          usr_scale.cpp / usr_wic.cpp で実際に使われているメンバのみ実装。
 */
class TCanvas {
public:
	explicit TCanvas(HDC dc = nullptr) : Pen(new TPen()), Brush(new TBrush()), Font(new TFont()), dc_(dc) {}
	~TCanvas()
	{
		delete Pen;
		delete Brush;
		delete Font;
	}
	TCanvas(const TCanvas &) = delete;
	TCanvas &operator=(const TCanvas &) = delete;

	HDC GetHandle() const { return dc_; }
	void SetHandleValue(HDC dc) { dc_ = dc; }	//!< シム独自: DIB 作成後の再バインド用

	void Lock() {}		//!< Phase 0 は単一スレッド前提のため no-op
	void Unlock() {}	//!< 同上

	void MoveTo(int x, int y);
	void LineTo(int x, int y);
	void FillRect(const TRect &rect);
	/// dest 位置に src の srcRect 部分をそのまま複写する (等倍。StretchBlt を使用)
	void CopyRect(const TRect &dest, TCanvas *src, const TRect &srcRect);
	/// graphic (TBitmap) を rect に収まるよう拡大縮小して描画する
	void StretchDraw(const TRect &rect, TBitmap *graphic);
	/// 文字列の表示幅を取得する (GetTextExtentPoint32W。Font->Height を反映した
	/// 一時フォントを選択して測る。実際の描画に使うフォント選択とは別経路)
	int TextWidth(const UnicodeString &s) const;

	/// usr_str.cpp: cv->Handle = hDc; の形で代入されるため読み書き可能にしてある
	compat::RWValueProperty<TCanvas, HDC, &TCanvas::GetHandle, &TCanvas::SetHandleValue> Handle{this};
	TPen *const Pen;	//!< cv->Pen->Color 等のアロー呼び出しのため生ポインタで所有する
	TBrush *const Brush;
	TFont *const Font;

private:
	HDC dc_;
};

}  // namespace Graphics

//---------------------------------------------------------------------------
namespace Vcl {
namespace Themes {

/// TStyleManager::ActiveStyle が返す型 (TCustomStyleServices 相当の最小実装)
class TStyleServices {
public:
	/// Phase 0 では VCL Styles を実装せず ::GetSysColor へマップするだけ
	/// (Phase 2 で wxWidgets のダークモード対応に置き換える予定)
	TColor GetSystemColor(TColor color) const;
	UnicodeString GetName() const { return "Windows"; }

	compat::ROProperty<TStyleServices, UnicodeString, &TStyleServices::GetName> Name{this};
};

/// System.Themes::TStyleManager 相当。ActiveStyle のみ実装
class TStyleManager {
public:
	static TStyleServices *ActiveStyle;
};

}  // namespace Themes
}  // namespace Vcl

namespace System {
namespace UITypes {
using ::ColorToRGB;
using ::ColorToString;
using ::StringToColor;
using ::TColor;
}  // namespace UITypes
using namespace UITypes;
}  // namespace System

using ::Graphics::afDefined;
using ::Graphics::afIgnored;
using ::Graphics::afPremultiplied;
using ::Graphics::clAppWorkSpace;
using ::Graphics::clAqua;
using ::Graphics::clBlack;
using ::Graphics::clBlue;
using ::Graphics::clBtnFace;
using ::Graphics::clBtnShadow;
using ::Graphics::clBtnText;
using ::Graphics::clDefault;
using ::Graphics::clDkGray;
using ::Graphics::clFuchsia;
using ::Graphics::clGray;
using ::Graphics::clGrayText;
using ::Graphics::clGreen;
using ::Graphics::clHighlight;
using ::Graphics::clHighlightText;
using ::Graphics::clLime;
using ::Graphics::clLtGray;
using ::Graphics::clMaroon;
using ::Graphics::clMenu;
using ::Graphics::clMenuHighlight;
using ::Graphics::clMenuText;
using ::Graphics::clNavy;
using ::Graphics::clNone;
using ::Graphics::clOlive;
using ::Graphics::clPurple;
using ::Graphics::clRed;
using ::Graphics::clSilver;
using ::Graphics::clSkyBlue;
using ::Graphics::clTeal;
using ::Graphics::clWebDeepPink;
using ::Graphics::clWebFirebrick;
using ::Graphics::clWebIndigo;
using ::Graphics::clWebLimeGreen;
using ::Graphics::clWebSaddleBrown;
using ::Graphics::clWhite;
using ::Graphics::clWindow;
using ::Graphics::clWindowFrame;
using ::Graphics::clWindowText;
using ::Graphics::clYellow;
using ::Graphics::pf24bit;
using ::Graphics::pf32bit;
using ::Graphics::pfCustom;
using ::Graphics::pfDevice;
using ::Graphics::TAlphaFormat;
using ::Graphics::TPixelFormat;

using ::Graphics::bsClear;
using ::Graphics::bsSolid;
using ::Graphics::pmCopy;
using ::Graphics::pmNot;
using ::Graphics::psDash;
using ::Graphics::psDot;
using ::Graphics::psSolid;
using ::Graphics::Rect;
using ::Graphics::TBrush;
using ::Graphics::TBrushStyle;
using ::Graphics::TCanvas;
using ::Graphics::TFont;
using ::Graphics::TPen;
using ::Graphics::TPenMode;
using ::Graphics::TPenStyle;
using ::Graphics::TRect;

using ::Vcl::Themes::TStyleManager;
using ::Vcl::Themes::TStyleServices;

#endif  // NYANFI_COMPAT_GRAPHICS_H
