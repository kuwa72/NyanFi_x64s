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
 *   - **例外 (Phase 1 で追加)**: Repaint() / Invalidate() のような「再描画を
 *     要求するだけ」のメソッドは、ヘッドレスな Phase 0/1 では観測可能な
 *     効果が無い (実ウィンドウが無いので描画自体が発生しない) ため、
 *     「本当に何もしない」ことが正しい実装であり、real no-op として定義
 *     する。これは「実装が無いのに動いて見える」の回避対象ではない
 *     (隠すべき未実装のロジックが元々存在しない)。
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
 *
 * Phase 1 (issue #1) で追加した実際に使われているメンバの根拠:
 *   - TCheckListBox:
 *       usr_tag.cpp (IniCheckList/CheckToTags/CountTags): `->Items->Assign()`,
 *       `->Count`, `->Checked[i]` (読み書き), `->ItemIndex`, `->Canvas->TextWidth()`,
 *       `->Repaint()`。usr_scrpanel.cpp: `->TopIndex`, `->ItemHeight`,
 *       `->ClientHeight`, `->WindowProc`。
 *   - TStringGrid:
 *       UIniFile.cpp (LoadGridColWidth/SaveGridColWidth): `->Name`, `->FixedCols`,
 *       `->ColCount`, `->ColWidths[i]`。usr_scrpanel.cpp: `->RowCount`,
 *       `->RowHeights[i]`, `->LeftCol`, `->TopRow`, `->VisibleRowCount`,
 *       `->VisibleColCount`, `->ClientHeight/Width`, `->Perform`, `->WindowProc`。
 *   - TForm:
 *       UIniFile.cpp (LoadFormPos/SaveFormPos/LoadPosInfo/SavePosInfo):
 *       `->Left/Top/Width/Height`, `->Name`, `->BorderStyle`, `->WindowState`,
 *       `->BoundsRect` (読取専用), `->Constraints->MinWidth/MinHeight`。
 *   - TPaintBox:
 *       usr_swatch.cpp / usr_scrpanel.cpp: `->Parent`, `->Align`, `->OnPaint`,
 *       `->OnMouseDown/Move/Up`, `->Canvas`, `->ClientRect/Width/Height`, `->Tag`,
 *       `->Invalidate()`, `->Repaint()`。
 *   - TScrollBar (新規): usr_scrpanel.cpp: `->Max/Min/Position/LargeChange`。
 *   - TCheckBox / TRadioGroup / TAction (新規、実データのみ):
 *       UIniFile.cpp: `->Checked` (TCheckBox/TAction), `->ItemIndex` (TRadioGroup)。
 *   - TToolBar / TTabControl / TTreeView / TRichEdit / TMemo / THeaderControl /
 *     TStatusBar (新規、Font のみ): usr_scale.cpp の ClassNameIs 分岐。
 */
#ifndef NYANFI_COMPAT_GUI_STUBS_H
#define NYANFI_COMPAT_GUI_STUBS_H

#include <vector>

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/controls.h"
#include "compat/events.h"
#include "compat/graphics.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// TComboBoxStyle 相当 (実測: csDropDown / csDropDownList のみ使用。他は列挙のみ)
enum TComboBoxStyle { csSimple, csDropDown, csDropDownList, csOwnerDrawFixed, csOwnerDrawVariable };

//---------------------------------------------------------------------------
/// イベント型 (compat/events.h の TClosureEvent 参照。__closure 拡張が無い
/// 標準 C++ での代替なので、実際の発火は出来ない。詳細は events.h 参照)
using TNotifyEvent = TClosureEvent<TObject *>;
using TMouseEvent = TClosureEvent<TObject *, TMouseButton, TShiftState, int, int>;
/// Vcl.Controls::TMouseMoveEvent 相当 (OnMouseMove は Button を持たない別シグネチャ)
using TMouseMoveEvent = TClosureEvent<TObject *, TShiftState, int, int>;

/// TMessage 相当 (最小実装。Msg / Result のみ実測で使用)
struct TMessage {
	unsigned Msg = 0;
	NativeInt WParam = 0;
	NativeInt LParam = 0;
	NativeInt Result = 0;
};
/// Vcl.Controls::TWndMethod 相当 (ウィンドウ・プロシージャの差し替え用)
using TWndMethod = TClosureEvent<TMessage &>;

//---------------------------------------------------------------------------
/// Vcl.Controls::TSizeConstraints 相当 (最小実装。MinWidth/MinHeight のみ実測で使用)
class TSizeConstraints {
public:
	int MinWidth = 0;
	int MinHeight = 0;
	int MaxWidth = 0;
	int MaxHeight = 0;
};

//---------------------------------------------------------------------------
/**
 * @brief TControl 相当 (最小実装)
 * @details Phase 1 でレイアウト系のプロパティ (Left/Top/Width/Height/Align/
 *          Anchors/ClientWidth/ClientHeight/CurrentPPI 等) を追加した。
 *          実 GUI が無いため、これらは「値を保持するだけ」でよく (設定しても
 *          実際にウィンドウが動くわけではない)、単純なデータメンバとして
 *          実装する。
 */
class TControl : public TComponent {
public:
	explicit TControl(TComponent *owner = nullptr) : TComponent(owner) {}

	/// @warning 宣言のみ。実際に呼び出す経路がリンクされると未定義参照になる
	NativeInt Perform(unsigned msg, NativeInt wParam, NativeInt lParam);

	/// @warning 宣言のみ (実処理は Z オーダーの変更。テストからは呼ばれない想定)
	void BringToFront();

	bool Enabled = true;
	bool Visible = true;
	TColor Color = clWindow;
	TRect ClientRect;
	/// 親コントロール (usr_scrpanel.cpp / usr_swatch.cpp が設定するのみ。
	/// 実際の親子関係に基づくウィンドウ処理は Phase 0/1 には無い)
	TWinControl *Parent = nullptr;

	int Left = 0;
	int Top = 0;
	int Width = 0;
	int Height = 0;
	int ClientWidth = 0;
	int ClientHeight = 0;
	int CurrentPPI = 96;	//!< DEFAULT_PPI (usr_scale.h) と同じ既定値
	bool DoubleBuffered = false;
	NativeInt Tag = 0;

	TAlign Align = alNone;
	TAnchors Anchors;
	TStyleElements StyleElements;

	/// 所有権を持つ (Phase 0/1 では解放タイミングを厳密には管理しない。
	/// テストは短命なプロセス内で完結するためリークは実害が無い)
	TFont *Font = new TFont();
	TSizeConstraints *Constraints = new TSizeConstraints();
};

/// TWinControl 相当 (最小実装)
class TWinControl : public TControl {
public:
	explicit TWinControl(TComponent *owner = nullptr) : TControl(owner) {}

	/// @warning 宣言のみ (実処理は Perform(WM_SETREDRAW, 0, 0) 相当)
	void LockDrawing();
	/// @warning 宣言のみ
	void UnlockDrawing();

	HWND Handle = nullptr;
	/// ウィンドウ・プロシージャの差し替え (usr_scrpanel.cpp がサブクラス化に使用。
	/// 実際のメッセージポンプは無いため、代入/比較/退避ができるだけでよい)
	TWndMethod WindowProc;
};

//---------------------------------------------------------------------------
/// TEdit 相当 (最小実装。Font は TControl から継承)
class TEdit : public TWinControl {
public:
	UnicodeString Text;
	bool NumbersOnly = false;
};

/// TLabeledEdit 相当 (最小実装。Font は TControl から継承)
class TLabeledEdit : public TWinControl {
public:
	UnicodeString Text;
	bool NumbersOnly = false;
};

/// TMaskEdit 相当 (最小実装。Font は TControl から継承)
class TMaskEdit : public TWinControl {
public:
	UnicodeString Text;
	bool NumbersOnly = false;
};

/// TMemo 相当 (最小実装。Font は TControl から継承)
class TMemo : public TWinControl {
};

/// TRichEdit 相当 (最小実装。Font は TControl から継承)
class TRichEdit : public TWinControl {
};

//---------------------------------------------------------------------------
/// TComboBox 相当 (最小実装。Font は TControl から継承)
class TComboBox : public TWinControl {
public:
	/// 実 VCL の TComboBox は Items (TStrings) をコンストラクタで自前に生成
	/// して所有する (TCustomListBox と同じ理由)
	TComboBox() : Items(new TStringList()) {}
	~TComboBox() override { delete Items; }

	/// Items を空にし、Text/ItemIndex も初期状態に戻す (Vcl.StdCtrls::TCustomComboBox::Clear 相当)
	void Clear()
	{
		Items->Clear();
		ItemIndex = -1;
		Text = "";
	}

	TComboBoxStyle Style = csDropDown;
	UnicodeString Text;
	TStrings *Items;
	int ItemIndex = -1;
};

//---------------------------------------------------------------------------
/// TPanel 相当 (最小実装。Font は TControl から継承)
class TPanel : public TWinControl {
public:
	explicit TPanel(TComponent *owner = nullptr) : TWinControl(owner) {}

	TPanelBevel BevelOuter = bvRaised;
};

/// TToolBar 相当 (最小実装。Font は TControl から継承)
class TToolBar : public TWinControl {
};

/// TTabControl 相当 (最小実装。Font は TControl から継承)
class TTabControl : public TWinControl {
};

/// TTreeView 相当 (最小実装。Font は TControl から継承)
class TTreeView : public TWinControl {
};

/// THeaderControl 相当 (最小実装。Font は TControl から継承)
class THeaderControl : public TWinControl {
};

/// TStatusBar 相当 (最小実装。Font / ClientHeight は TControl から継承)
class TStatusBar : public TWinControl {
};

/// TCheckBox 相当 (最小実装)
class TCheckBox : public TWinControl {
public:
	bool Checked = false;
};

/// TRadioGroup 相当 (最小実装)
class TRadioGroup : public TWinControl {
public:
	int ItemIndex = -1;
};

/// TAction 相当 (最小実装。System.Actions::TBasicAction 相当は省略し TComponent 直下に置く)
class TAction : public TComponent {
public:
	bool Checked = false;
};

/// TScrollBar 相当 (最小実装)
class TScrollBar : public TWinControl {
public:
	int Min = 0;
	int Max = 100;
	int Position = 0;
	int LargeChange = 1;
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
/**
 * @brief TCustomListBox 相当 (TListBox / TCheckListBox の共通基底)
 * @details ItemHeight はヘッドレス実行では実際のフォント計測を伴わないため
 *          Delphi の既定値に近い値 (16) を仮の固定値として持つ。
 */
class TCustomListBox : public TWinControl {
public:
	/// 実 VCL の TCustomListBox は Items (TStrings) をコンストラクタで自前に
	/// 生成して所有する (呼び出し側が LoadFromFile/Assign 等で直接書き込む
	/// 対象なので、外部の TStringList への差し替えも許すよう生ポインタの
	/// ままにしてある)
	TCustomListBox() : Items(new TStringList()) {}
	~TCustomListBox() override { delete Items; }

	int GetCount() const { return Items ? Items->Count : 0; }
	compat::ROProperty<TCustomListBox, int, &TCustomListBox::GetCount> Count{this};

	TStrings *Items;
	int ItemIndex = -1;
	int ItemHeight = 16;
	int TopIndex = 0;
	int ScrollWidth = 0;
};

/// TListBox 相当 (最小実装)
class TListBox : public TCustomListBox {
};

/**
 * @brief TCheckListBox 相当 (最小実装)
 * @details Checked[] は Items と同じ添字系列を持つ実データ (std::vector<bool>)
 *          として実装した (usr_tag.cpp の IniCheckList/CheckToTags/CountTags が
 *          実際にチェック状態を読み書きするため、宣言のみでは回帰テストが
 *          書けない)。範囲外アクセスは自動的に拡張し既定値 false を返す。
 *          Canvas は描画先を持たない既定の TCanvas (DC 無し) で、TextWidth は
 *          0 を返す (実フォント計測は Phase 2 で GUI 実体が出来てから)。
 */
class TCheckListBox : public TCustomListBox {
public:
	TCheckListBox() : Canvas(new TCanvas()) {}
	~TCheckListBox() override { delete Canvas; }

	/// Checked[i] の読み書き (auto-resize)
	class CheckedProperty {
	public:
		class Ref {
		public:
			Ref(std::vector<bool> &vec, int index) : vec_(vec), index_(index) {}
			operator bool() const { return index_ >= 0 && index_ < static_cast<int>(vec_.size()) && vec_[index_]; }
			Ref &operator=(bool value)
			{
				if (index_ >= 0) {
					if (index_ >= static_cast<int>(vec_.size())) vec_.resize(index_ + 1, false);
					vec_[index_] = value;
				}
				return *this;
			}

		private:
			std::vector<bool> &vec_;
			int index_;
		};

		explicit CheckedProperty(TCheckListBox *owner) : owner_(owner) {}
		Ref operator[](int index) const { return Ref(owner_->checked_, index); }

	private:
		TCheckListBox *owner_;
	};

	CheckedProperty Checked{this};
	TCanvas *const Canvas;

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	/// (gui_stubs.h 冒頭の設計方針を参照)
	void Repaint() {}
	void Invalidate() {}

private:
	std::vector<bool> checked_;
};

//---------------------------------------------------------------------------
/**
 * @brief TStringGrid 相当 (最小実装)
 * @details ColWidths[] / RowHeights[] は auto-resize する実データとして実装した
 *          (UIniFile.cpp の LoadGridColWidth/SaveGridColWidth が実際に読み書き
 *          するため)。
 */
class TStringGrid : public TWinControl {
public:
	/// 添字プロパティの共通実装 (ColWidths[] / RowHeights[] で使う。既定値は
	/// Delphi の既定セルサイズに近い値)
	class IntArrayProperty {
	public:
		class Ref {
		public:
			Ref(std::vector<int> &vec, int index, int default_value) : vec_(vec), index_(index), default_(default_value) {}
			operator int() const
			{
				return (index_ >= 0 && index_ < static_cast<int>(vec_.size())) ? vec_[index_] : default_;
			}
			Ref &operator=(int value)
			{
				if (index_ >= 0) {
					if (index_ >= static_cast<int>(vec_.size())) vec_.resize(index_ + 1, default_);
					vec_[index_] = value;
				}
				return *this;
			}

		private:
			std::vector<int> &vec_;
			int index_;
			int default_;
		};

		IntArrayProperty(std::vector<int> *vec, int default_value) : vec_(vec), default_(default_value) {}
		Ref operator[](int index) const { return Ref(*vec_, index, default_); }

	private:
		std::vector<int> *vec_;
		int default_;
	};

	int FixedCols = 1;
	int FixedRows = 1;
	int ColCount = 2;
	int RowCount = 2;
	int LeftCol = 0;
	int TopRow = 0;
	int VisibleRowCount = 0;
	int VisibleColCount = 0;

	IntArrayProperty ColWidths{&col_widths_, 64};
	IntArrayProperty RowHeights{&row_heights_, 24};

private:
	std::vector<int> col_widths_;
	std::vector<int> row_heights_;
};

/// TDrawGrid 相当 (最小実装。src/ での実際のメンバアクセスは無い)
class TDrawGrid : public TWinControl {
};

//---------------------------------------------------------------------------
// usr_shell.h でポインタ/参照としてのみ使われる型。現状メンバアクセスは無い。
//---------------------------------------------------------------------------
/// TImage 相当 (最小実装)
class TImage : public TControl {
public:
	Graphics::TBitmap *Picture = nullptr;
};

/// TLabel 相当 (最小実装。実 VCL では TGraphicControl 経由で TControl を継承する。
/// Font は TControl から継承)
class TLabel : public TControl {
};

//---------------------------------------------------------------------------
/**
 * @brief TPaintBox 相当 (最小実装)
 * @details 実 VCL では TGraphicControl 経由で TControl を継承する
 *          (TWinControl ではない = ウィンドウハンドルを持たない) が、
 *          Phase 0/1 で必要なメンバ (Canvas/Parent/Align/OnPaint/OnMouse*)
 *          はいずれも TControl の範囲で足りるため直接継承する。
 */
class TPaintBox : public TControl {
public:
	explicit TPaintBox(TComponent *owner = nullptr) : TControl(owner), Canvas(new TCanvas()) {}
	~TPaintBox() override { delete Canvas; }

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	void Repaint() {}
	void Invalidate() {}

	TCanvas *const Canvas;

	TNotifyEvent OnPaint;
	TMouseEvent OnMouseDown;
	TMouseMoveEvent OnMouseMove;
	TMouseEvent OnMouseUp;
};

//---------------------------------------------------------------------------
/// TForm 相当 (最小実装)
class TForm : public TWinControl {
public:
	/**
	 * @brief BoundsRect (読取専用) の実装
	 * @details UIniFile.cpp は `frm->BoundsRect.Right` / `.Bottom` のように
	 *          `.` で直接フィールドへアクセスするため、単純な compat::ROProperty
	 *          (TRect を値で返す) では `.Right` を転送できない。Left/Top/Width/
	 *          Height の現在値から毎回計算し直す入れ子プロパティにして、
	 *          `TRect` への暗黙変換 (`TRect::Intersect(..., frm->BoundsRect)`)
	 *          も両立させる。
	 */
	class BoundsRectProperty {
	public:
		explicit BoundsRectProperty(TForm *owner) : owner_(owner) {}

		int GetLeft() const { return owner_->Left; }
		int GetTop() const { return owner_->Top; }
		int GetRight() const { return owner_->Left + owner_->Width; }
		int GetBottom() const { return owner_->Top + owner_->Height; }

		compat::ROProperty<BoundsRectProperty, int, &BoundsRectProperty::GetLeft> Left{this};
		compat::ROProperty<BoundsRectProperty, int, &BoundsRectProperty::GetTop> Top{this};
		compat::ROProperty<BoundsRectProperty, int, &BoundsRectProperty::GetRight> Right{this};
		compat::ROProperty<BoundsRectProperty, int, &BoundsRectProperty::GetBottom> Bottom{this};

		operator TRect() const { return TRect(GetLeft(), GetTop(), GetRight(), GetBottom()); }

	private:
		TForm *owner_;
	};
	BoundsRectProperty BoundsRect{this};

	TFormBorderStyle BorderStyle = bsSizeable;
	TWindowState WindowState = wsNormal;
};

#endif  // NYANFI_COMPAT_GUI_STUBS_H
