/**
 * @file compat/gui_stubs.h
 * @brief ロジック層のファイルが GUI コントロールをポインタで受け取り、数個の
 *        プロパティ/メソッドだけに触れている箇所を通すための「宣言だけ」の
 *        スタブ群
 *
 * 設計方針 (重要):
 *   - **メンバ「関数」は宣言のみ書き、本体(定義)は書かない**。呼び出し側の
 *     コードは `-fsyntax-only` (scripts/probe.sh) では問題なく通るが、実際に
 *     このパスをリンクしようとすると「未定義参照」でリンクエラーになる。
 *     これは意図的な設計であり、「実装が無いのに動いているように見える」
 *     状態を静かに作らないための安全策である (呼び出しても平気で通る
 *     no-op を書かない)。
 *   - データメンバ (Color / Font / Text / Width / Height など、単純な値の
 *     読み書きだけで副作用が無いもの) は普通に宣言してよい。Get/Set が
 *     単純な代入以上の処理を必要とする場合のみ「メンバ関数」として扱い
 *     宣言のみにする。
 *   - ここにある型はすべて **Phase 2 で wxWidgets のコントロールに置き換える
 *     対象** である。
 *   - compat/vcl_forward.h が TForm 等をグローバル名前空間へ前方宣言済みなので、
 *     ここではその実体 (完全な定義) を同じくグローバル名前空間に与える
 *     (namespace で包むと前方宣言と別の型になってしまうため)。
 *   - TCanvas / TBitmap / TColor / TFont / TRect は compat/graphics.h が
 *     既に実装しているので、ここでは再定義せず include して使う。
 *
 * 実際に使われているメンバの根拠 (build-probe/<name>.log と該当行を確認して確定):
 *   - TEdit / TLabeledEdit / TMaskEdit:
 *       usr_color.cpp:286-328 (`->Color`, `->Font->Color`, `->NumbersOnly`,
 *       `->Text.ToIntDef()`, `->Text.IsEmpty()`)
 *   - TComboBox / TComboBoxStyle:
 *       usr_cmdlist.cpp:511-987 (`->Clear()`, `->Style = csDropDown(List)`,
 *       `->Text`, `->Enabled`)、usr_key.cpp:35-46 (`->Text`, `->LockDrawing()`,
 *       `->Items->Assign()/Insert()/IndexOf()`, `->ItemIndex`, `->UnlockDrawing()`)
 *   - TControl:
 *       usr_key.cpp:118-126 (`->Perform(Msg, wParam, lParam)`)
 *   - TWinControl:
 *       LockDrawing/UnlockDrawing は WM_SETREDRAW の Perform を包む最近の VCL の
 *       便利メソッド (旧コードでは `Perform(WM_SETREDRAW, ...)` を直書きしている
 *       箇所が Global.cpp/MainFrm.cpp に残っているのを確認した)
 *   - TPanel:
 *       usr_str.cpp:1744-1757 (`->Handle` (HWND), `->ClientRect`, `->Font`)
 *   - TDirect2DCanvas:
 *       usr_str.cpp:1746-1748,1755 (コンストラクタ `(HDC, TRect)`、
 *       `::Supported()`。`Font` / `TextWidth()` は Graphics::TCanvas から継承)
 *   - TMetafile:
 *       usr_file_inf.cpp:987 (`new TMetafile()` → `->LoadFromFile()`,
 *       `->Width`, `->Height`, `->CreatedBy`, `->Description`)。
 *       **注意**: `new TMetafile()` 自体はコンパイルが通る (デフォルトコンス
 *       トラクタは暗黙生成のため) が、`LoadFromFile()` は宣言のみなので
 *       実際にこの関数が呼ばれる経路がリンクされると未定義参照でリンクが
 *       落ちる。EMF/WMF の実パースは Phase 0 の対象外。
 *   - TListBox / TImage / TForm:
 *       usr_shell.h でポインタ/参照としてのみ現れる (現在の probe 対象
 *       15 ファイルの範囲ではメンバへのアクセスは無い)。今後のファイル追加で
 *       必要になった時点で拡張する。
 */
#ifndef NYANFI_COMPAT_GUI_STUBS_H
#define NYANFI_COMPAT_GUI_STUBS_H

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/graphics.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// TComboBoxStyle 相当 (実測: csDropDown / csDropDownList のみ使用。他は列挙のみ)
enum TComboBoxStyle { csSimple, csDropDown, csDropDownList, csOwnerDrawFixed, csOwnerDrawVariable };

//---------------------------------------------------------------------------
/// TControl 相当 (最小実装)
class TControl : public TPersistent {
public:
	/// @warning 宣言のみ。実際に呼び出す経路がリンクされると未定義参照になる
	NativeInt Perform(unsigned msg, NativeInt wParam, NativeInt lParam);

	bool Enabled = true;
	TColor Color = clWindow;
	TRect ClientRect;
};

/// TWinControl 相当 (最小実装)
class TWinControl : public TControl {
public:
	/// @warning 宣言のみ (実処理は Perform(WM_SETREDRAW, 0, 0) 相当)
	void LockDrawing();
	/// @warning 宣言のみ
	void UnlockDrawing();

	HWND Handle = nullptr;
};

//---------------------------------------------------------------------------
/// TEdit 相当 (最小実装)
class TEdit : public TWinControl {
public:
	TFont *Font = nullptr;
	UnicodeString Text;
	bool NumbersOnly = false;
};

/// TLabeledEdit 相当 (最小実装)
class TLabeledEdit : public TWinControl {
public:
	TFont *Font = nullptr;
	UnicodeString Text;
	bool NumbersOnly = false;
};

/// TMaskEdit 相当 (最小実装)
class TMaskEdit : public TWinControl {
public:
	TFont *Font = nullptr;
	UnicodeString Text;
	bool NumbersOnly = false;
};

//---------------------------------------------------------------------------
/// TComboBox 相当 (最小実装)
class TComboBox : public TWinControl {
public:
	/// @warning 宣言のみ
	void Clear();

	TComboBoxStyle Style = csDropDown;
	UnicodeString Text;
	TStrings *Items = nullptr;
	int ItemIndex = -1;
};

//---------------------------------------------------------------------------
/// TPanel 相当 (最小実装)
class TPanel : public TWinControl {
public:
	TFont *Font = nullptr;
};

//---------------------------------------------------------------------------
/**
 * @brief TDirect2DCanvas 相当
 * @details Direct2D の実体は無く、Graphics::TCanvas の GDI 実装をそのまま流用
 *          する (Font / TextWidth 等は基底の実装が使われる)。コンストラクタと
 *          Supported() は宣言のみ。
 */
class TDirect2DCanvas : public TCanvas {
public:
	/// @warning 宣言のみ (Direct2D 未実装)
	TDirect2DCanvas(HDC dc, const TRect &rect);
	/// @warning 宣言のみ。呼ばれた場合にリンクエラーで気付けるようにしてある
	static bool Supported();
};

//---------------------------------------------------------------------------
/**
 * @brief TMetafile 相当 (EMF/WMF 画像)
 * @details `new TMetafile()` (暗黙のデフォルトコンストラクタ) はコンパイルが
 *          通るが、`LoadFromFile()` は宣言のみなのでこの経路が実際にリンク
 *          されると未定義参照でリンクが落ちる。EMF/WMF の実パースは Phase 0
 *          の対象外 (Phase 2 で wxWidgets 側の画像処理に置き換える)。
 */
class TMetafile : public TPersistent {
public:
	/// @warning 宣言のみ
	void LoadFromFile(const UnicodeString &fileName);

	int Width = 0;
	int Height = 0;
	UnicodeString CreatedBy;
	UnicodeString Description;
};

//---------------------------------------------------------------------------
// usr_shell.h でポインタ/参照としてのみ使われる型。現状メンバアクセスは無い。
//---------------------------------------------------------------------------
/// TListBox 相当 (最小実装)
class TListBox : public TWinControl {
public:
	TStrings *Items = nullptr;
};

/// TImage 相当 (最小実装)
class TImage : public TControl {
public:
	Graphics::TBitmap *Picture = nullptr;
};

/// TForm 相当 (最小実装)
class TForm : public TWinControl {
};

#endif  // NYANFI_COMPAT_GUI_STUBS_H
