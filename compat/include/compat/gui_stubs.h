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

/// Vcl.StdCtrls::TListBoxStyle 相当 (実測: lbVirtualOwnerDraw のみ使用。Global.cpp:6618)
enum TListBoxStyle {
	lbStandard, lbOwnerDrawFixed, lbOwnerDrawVariable, lbVirtual, lbVirtualOwnerDraw
};

/// Vcl.Forms::TFormStyle 相当 (実測: fsStayOnTop のみ使用。Global.cpp:15250)
enum TFormStyle { fsNormal, fsMDIChild, fsMDIForm, fsStayOnTop };

/// Vcl.Forms::TCloseAction 相当 (実測: FormClose ハンドラの引数型としてのみ出現。
/// InpDir.h:42 / InpExDlg.h:72 ほか)
enum TCloseAction { caNone, caHide, caFree, caMinimize };

/// Vcl.Controls::TDragState 相当 (実測: UserMdl.h:255 の DragOver ハンドラの引数型のみ)
enum TDragState { dsDragEnter, dsDragLeave, dsDragMove };

/// Vcl.Controls::TDragObject 相当 (実測: UserMdl.h:253 の StartDrag ハンドラの
/// 引数型 `TDragObject *&` としてのみ出現。メンバアクセスは無い)
class TDragObject : public TObject {
};

//---------------------------------------------------------------------------
/// Vcl.Controls::TOwnerDrawState の要素 (Delphi の TOwnerDrawStateType)
/// 実測: odSelected (Global.h:1783, Global.cpp:8943,9054) と
///       odFocused (Global.cpp:12707,12719) のみ使用
enum TOwnerDrawStateType {
	odSelected, odGrayed, odDisabled, odChecked, odFocused, odDefault, odHotLight,
	odInactive, odNoAccel, odNoFocusRect, odReserved1, odReserved2, odComboBoxEdit
};
/// TOwnerDrawStateType の集合 (Vcl.Controls::TOwnerDrawState 相当)
using TOwnerDrawState = Set<TOwnerDrawStateType, odSelected, odComboBoxEdit>;

/// Vcl.Grids::TGridDrawState の要素
/// 実測: gdSelected (Global.h:1787) のみ使用
enum TGridDrawStateType { gdSelected, gdFocused, gdFixed, gdRowSelected, gdHotTrack, gdPressed };
/// TGridDrawStateType の集合 (Vcl.Grids::TGridDrawState 相当)
using TGridDrawState = Set<TGridDrawStateType, gdSelected, gdPressed>;

//---------------------------------------------------------------------------
/// イベント型 (compat/events.h の TClosureEvent 参照。__closure 拡張が無い
/// 標準 C++ での代替なので、実際の発火は出来ない。詳細は events.h 参照)
using TNotifyEvent = TClosureEvent<TObject *>;
using TMouseEvent = TClosureEvent<TObject *, TMouseButton, TShiftState, int, int>;
/// Vcl.Controls::TMouseMoveEvent 相当 (OnMouseMove は Button を持たない別シグネチャ)
using TMouseMoveEvent = TClosureEvent<TObject *, TShiftState, int, int>;

/**
 * @brief TMessage 相当 (最小実装)
 * @details 実測: usr_scrpanel.h は Msg / Result のみ、Phase 3 で読む
 *          InpExDlg.h:86 ほか 20 箇所の `WmMenuChar` が `msg.WParamHi` を使う。
 *          Winapi.Messages の TMessage と同じく共用体で重ねてある。
 * @warning 64bit では WParamLo/WParamHi は WParam の下位 32bit にしか重ならず、
 *          LParamLo 以降は LParam とはずれる (VCL の定義自体がそうなっている)。
 *          既存コードが使っているのは WParamHi (= HIWORD(WParam)) だけなので
 *          そのまま写した。
 */
struct TMessage {
	unsigned Msg = 0;
	union {
		struct {
			WORD WParamLo;
			WORD WParamHi;
			WORD LParamLo;
			WORD LParamHi;
			WORD ResultLo;
			WORD ResultHi;
		};
		struct {
			NativeUInt WParam;
			NativeInt LParam;
			NativeInt Result;
		};
	};

	TMessage() : WParam(0), LParam(0), Result(0) {}
};
/// Vcl.Controls::TWndMethod 相当 (ウィンドウ・プロシージャの差し替え用)
using TWndMethod = TClosureEvent<TMessage &>;

//---------------------------------------------------------------------------
/**
 * @brief C++Builder の sysmac.h 相当のメッセージマップ・マクロ
 * @details `src/InpExDlg.h:99` ほか 20 以上のフォームが
 *          @code
 *              BEGIN_MESSAGE_MAP
 *                  VCL_MESSAGE_HANDLER(WM_MENUCHAR, TMessage, WmMenuChar)
 *              END_MESSAGE_MAP(TForm)
 *          @endcode
 *          の形で使っている。C++Builder の定義と同じく、Dispatch() を
 *          オーバーライドする switch 文へ展開する。
 * @note 展開後の Dispatch は inline な仮想関数になるので、vtable の
 *       キー関数問題 (未定義の非 inline 仮想関数があると vtable が
 *       どこにも生成されない) は起きない。
 */
#define BEGIN_MESSAGE_MAP                                       \
	virtual void __fastcall Dispatch(void *Message)             \
	{                                                           \
		switch (static_cast<TMessage *>(Message)->Msg) {

#define VCL_MESSAGE_HANDLER(msg, type, meth) \
	case msg:                                \
		meth(*static_cast<type *>(Message)); \
		break;

#define END_MESSAGE_MAP(base)              \
	default:                               \
		base::Dispatch(Message);           \
		break;                             \
		}                                  \
	}

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
	// Tag は基底の TComponent (compat/classes.h) が持つ。ここに重ねて宣言すると
	// **基底を隠して静かに壊れる**: src/Global.cpp の BringOptionByTag は
	// `TComponent *cp = fp->Components[i]; ... *(bool*)cp->Tag` と TComponent 側を
	// 読むので、派生側に書いた値が見えずオプションが全部無視される。
	// コンパイルは通るので気づけない (規約2 と同じ罠)

	TAlign Align = alNone;
	TAnchors Anchors;
	TStyleElements StyleElements;

	/// 割り当てられたアクション (Global.cpp:6497 `if (cp->Action) cp->Action->Update();`)
	TAction *Action = nullptr;

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

	//-- Phase 3 で Global.cpp から要求されたメンバ ------------------------
	// 実測した呼び出し箇所:
	//   ControlCount / Controls : Global.cpp:2718-2719 (TPanel), 2740-2741 (TForm),
	//                             6481-6482 (TToolBar), 14785-14788 (TTabSheet/TGroupBox)
	//   Focused()               : Global.cpp:8943,8949,12720,12733,15125
	//   SetFocus()              : Global.cpp:15453
	//   Dispatch()              : InpExDlg.h:86 ほか (`TForm::Dispatch(&msg)`)

	/// 子コントロールの数 (VCL では TWinControl のプロパティ)
	/// @warning 宣言のみ
	int GetControlCount() const;
	/// @warning 宣言のみ
	TControl *GetControl(int index) const;

	compat::ROProperty<TWinControl, int, &TWinControl::GetControlCount> ControlCount{this};

	/// Controls[i] (読取専用の添字プロパティ)
	class ControlsProperty {
	public:
		explicit ControlsProperty(TWinControl *owner) : owner_(owner) {}
		TControl *operator[](int index) const { return owner_->GetControl(index); }

	private:
		TWinControl *owner_;
	};
	ControlsProperty Controls{this};

	/// @warning 宣言のみ (入力フォーカスを持っているか)
	bool Focused() const;
	/// @warning 宣言のみ (入力フォーカスを移す)
	void SetFocus();

	/// メッセージを既定の処理へ回す (BEGIN_MESSAGE_MAP が展開する Dispatch の
	/// 基底側)。VCL では TObject の仮想関数だが、compat/classes.h には手を
	/// 入れない方針なのでここに置いた。
	/// @warning 宣言のみ。**あえて非仮想にしてある** — 未定義の非 inline 仮想
	///          関数を足すと GCC のキー関数規則で TWinControl の vtable が
	///          どこにも生成されず、既存のビルドがリンクできなくなるため。
	///          派生側 (BEGIN_MESSAGE_MAP) が inline な仮想関数として
	///          あらためて宣言するので、既存コードの書き方は変わらない。
	void __fastcall Dispatch(void *message);

	/// 右クリックメニュー (Global.cpp:15118 は非 NULL 判定にしか使わない)
	TPopupMenu *PopupMenu = nullptr;

	HWND Handle = nullptr;
	/// ウィンドウ・プロシージャの差し替え (usr_scrpanel.cpp がサブクラス化に使用。
	/// 実際のメッセージポンプは無いため、代入/比較/退避ができるだけでよい)
	TWndMethod WindowProc;
};

//---------------------------------------------------------------------------
/**
 * @brief TCustomEdit 相当 (TEdit / TLabeledEdit / TMaskEdit / TMemo の共通基底)
 * @details Phase 3 で追加。Global.cpp:7749 と 14787-14797 が
 *          `op->InheritsFrom(__classid(TCustomEdit))` で判定してから
 *          `((TCustomEdit*)cp)->Text` / `->SelStart` / `->SelLength` を触るため、
 *          前方宣言だけでは足りず実体が要る。
 *          Text / NumbersOnly はもともと TEdit / TLabeledEdit / TMaskEdit が
 *          個別に持っていたものをここへ集約した (VCL でも TCustomEdit の
 *          プロパティ)。既存の `ep->Text` / `ep->NumbersOnly` の書き方は変わらない。
 */
class TCustomEdit : public TWinControl {
public:
	UnicodeString Text;
	bool NumbersOnly = false;
	/// 選択開始位置 (0 起点。Global.cpp:14788,14797)
	int SelStart = 0;
	/// 選択文字数 (Global.cpp:14788)
	int SelLength = 0;
};

/// TEdit 相当 (最小実装。Text / Font は TCustomEdit / TControl から継承)
class TEdit : public TCustomEdit {
};

/// TLabeledEdit 相当 (最小実装)
class TLabeledEdit : public TCustomEdit {
};

/// TMaskEdit 相当 (最小実装)
class TMaskEdit : public TCustomEdit {
};

/// TMemo 相当 (最小実装)
class TMemo : public TCustomEdit {
};

/**
 * @brief TRichEdit 相当 (最小実装)
 * @details Lines は Global.cpp:9495-9501 が
 *          `TempRichEdit->Lines->LoadFromFile()` / `->Text` / `->Count` /
 *          `->Strings[i]` の形で使う。VCL の TCustomMemo と同じくコンストラクタで
 *          自前に生成して所有する (TComboBox / TCustomListBox と同じ扱い)。
 */
class TRichEdit : public TCustomEdit {
public:
	TRichEdit() : Lines(new TStringList()) {}
	~TRichEdit() override { delete Lines; }

	TStrings *Lines;
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
	/// ドロップダウンを開いているか (Global.cpp:14754 が読み書きする)
	bool DroppedDown = false;
	/// 入力補完を行うか (Global.cpp:2791)
	bool AutoComplete = true;
};

//---------------------------------------------------------------------------
/// TPanel 相当 (最小実装。Font は TControl から継承)
class TPanel : public TWinControl {
public:
	explicit TPanel(TComponent *owner = nullptr) : TWinControl(owner) {}

	TPanelBevel BevelOuter = bvRaised;
};

/// TToolBar 相当 (最小実装。Font は TControl から継承)
/// 3 色は Global.cpp:6477-6479 の setup_ToolBar が設定する
class TToolBar : public TWinControl {
public:
	TColor GradientStartColor = clBtnFace;
	TColor GradientEndColor = clBtnFace;
	TColor HotTrackColor = clBtnFace;
};

/// TSplitter 相当 (最小実装。Color は TControl から継承。
/// Global.cpp:6491 が `((TSplitter*)cp)->Color = ...` とするだけ)
class TSplitter : public TControl {
};

//---------------------------------------------------------------------------
/// Vcl.ComCtrls::TTabPosition 相当 (実測: tpBottom のみ使用。Global.cpp:12655,12677)
enum TTabPosition { tpTop, tpBottom, tpLeft, tpRight };

/**
 * @brief TCustomTabControl 相当 (TTabControl / TPageControl の共通基底)
 * @details Global.h:2834 の `draw_OwnerTab(TCustomTabControl *Control, ...)` が
 *          引数型として要求する。実体のメンバアクセスは TTabControl へ
 *          キャストしてから行われる (Global.cpp:12645)。
 */
class TCustomTabControl : public TWinControl {
};

/**
 * @brief TTabControl 相当 (最小実装。Font は TControl から継承)
 * @details 実測: Global.cpp:12646 (`tp->Canvas`)、12655,12677 (`tp->TabPosition`)、
 *          12670 (`tp->Tabs->Strings[idx]`)、ShareDlg.cpp:306 (`TabIndex`)。
 *          Tabs は VCL と同じくコンストラクタで自前に生成して所有する。
 */
class TTabControl : public TCustomTabControl {
public:
	TTabControl() : Canvas(new TCanvas()), Tabs(new TStringList()) {}
	~TTabControl() override
	{
		delete Canvas;
		delete Tabs;
	}

	TCanvas *const Canvas;
	TStrings *Tabs;
	int TabIndex = -1;
	TTabPosition TabPosition = tpTop;
};

/// TTabSheet 相当 (最小実装。ControlCount / Controls は TWinControl から継承。
/// Global.cpp:2699-2700 の ApplyOptionByTag(TTabSheet*) が引数型として要求する)
class TTabSheet : public TWinControl {
};

/// TTreeView 相当 (最小実装。Font は TControl から継承)
class TTreeView : public TWinControl {
};

//---------------------------------------------------------------------------
/**
 * @brief THeaderSection 相当 (ヘッダーの 1 区画)
 * @details 実測: Global.cpp:2780 (`Items[i]->Text = ...`)、12541 (`sp->Index`)、
 *          12538 (`sp->Text` の読み取り)、FileExtDlg.cpp:142 ほか (`->Width`)。
 *          VCL では TCollectionItem 派生だが、compat には TCollection が無いので
 *          TPersistent 直下に置き Index を自前で持つ。
 */
class THeaderSection : public TPersistent {
public:
	UnicodeString Text;
	int Index = 0;
	int Width = 0;
};

/// THeaderSections 相当 (THeaderSection のコレクション)
/// 実測: Global.cpp:2779,12541 (`->Count`)、2780 (`->Items[i]`)
class THeaderSections : public TPersistent {
public:
	/// @warning 宣言のみ
	int GetCount() const;
	/// @warning 宣言のみ
	THeaderSection *GetItem(int index) const;

	compat::ROProperty<THeaderSections, int, &THeaderSections::GetCount> Count{this};

	/// Items[i] (読取専用の添字プロパティ。返った THeaderSection への書き込みは可)
	class ItemsProperty {
	public:
		explicit ItemsProperty(THeaderSections *owner) : owner_(owner) {}
		THeaderSection *operator[](int index) const { return owner_->GetItem(index); }

	private:
		THeaderSections *owner_;
	};
	ItemsProperty Items{this};
};

/// THeaderControl 相当 (最小実装。Font は TControl から継承)
/// 実測: Global.cpp:2779-2780 / 12526,12541 (`->Sections`, `->Canvas`)
class THeaderControl : public TWinControl {
public:
	THeaderControl() : Canvas(new TCanvas()), Sections(new THeaderSections()) {}
	~THeaderControl() override
	{
		delete Canvas;
		delete Sections;
	}

	TCanvas *const Canvas;
	THeaderSections *const Sections;
};

//---------------------------------------------------------------------------
/// TStatusPanel 相当 (ステータスバーの 1 区画。実測: Global.cpp:7069 の `->Text`)
class TStatusPanel : public TPersistent {
public:
	UnicodeString Text;
	int Width = 0;
};

/// TStatusPanels 相当 (実測: Global.cpp:7068 の `->Count`、7069 の `->Items[i]`)
class TStatusPanels : public TPersistent {
public:
	/// @warning 宣言のみ
	int GetCount() const;
	/// @warning 宣言のみ
	TStatusPanel *GetItem(int index) const;

	compat::ROProperty<TStatusPanels, int, &TStatusPanels::GetCount> Count{this};

	class ItemsProperty {
	public:
		explicit ItemsProperty(TStatusPanels *owner) : owner_(owner) {}
		TStatusPanel *operator[](int index) const { return owner_->GetItem(index); }

	private:
		TStatusPanels *owner_;
	};
	ItemsProperty Items{this};
};

/// TStatusBar 相当 (最小実装。Font / ClientHeight は TControl から継承)
class TStatusBar : public TWinControl {
public:
	TStatusBar() : Panels(new TStatusPanels()) {}
	~TStatusBar() override { delete Panels; }

	TStatusPanels *const Panels;

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	void Repaint() {}
};

/// TCheckBox 相当 (最小実装)
class TCheckBox : public TWinControl {
public:
	bool Checked = false;
	UnicodeString Caption;
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
	/// Global.cpp:2810-2811 が読み書きする
	bool Enabled = true;
	/// @warning 宣言のみ (OnUpdate を発火して表示状態を更新する。Global.cpp:6497)
	void Update();
};

/// TActionList 相当 (最小実装。src/ ではフォームのメンバとして保持されるだけで
/// メンバアクセスは無い。UserMdl.h:98 / InpDir.h:26 ほか)
class TActionList : public TComponent {
};

//---------------------------------------------------------------------------
// Vcl.StdActns の編集系標準アクション。
// 実測: UserMdl.h:100-105 がメンバとして保持するだけで、メンバアクセスは無い。
//---------------------------------------------------------------------------
/// Vcl.StdActns::TEditAction 相当 (編集系標準アクションの共通基底)
class TEditAction : public TAction {
};

class TEditCopy : public TEditAction {
};
class TEditCut : public TEditAction {
};
class TEditPaste : public TEditAction {
};
class TEditDelete : public TEditAction {
};
class TEditSelectAll : public TEditAction {
};
class TEditUndo : public TEditAction {
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
class TMetafile : public Graphics::TGraphic {
public:
	/// @warning 宣言のみ
	void LoadFromFile(const UnicodeString &fileName);

	int Width = 0;
	int Height = 0;
	/// 画像が空か (Global.cpp:15162 の copy_to_Clipboard(TMetafile*) が読む。
	/// VCL は読取専用プロパティだが、実データが無いシムでは既定値のまま)
	bool Empty = true;
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
	/// 仮想リストボックス (Style==lbVirtual*) のときだけ意味を持つ項目数の設定。
	/// 実データを持たないシムでは「正しく実装できない」ので宣言のみにしてある
	/// (Global.cpp:6619 `lp->Count = lst? lst->Count : 0;`)
	/// @warning 宣言のみ
	void SetCount(int value);
	compat::RWValueProperty<TCustomListBox, int, &TCustomListBox::GetCount, &TCustomListBox::SetCount>
		Count{this};

	TStrings *Items;
	int ItemIndex = -1;
	int ItemHeight = 16;
	int TopIndex = 0;
	int ScrollWidth = 0;
	/// 複数選択を許すか (Global.cpp:15049,15056,15062)
	bool MultiSelect = false;
	TListBoxStyle Style = lbStandard;

	/// @warning 宣言のみ (Items を空にし ItemIndex も戻す。Global.cpp:6623,8930)
	void Clear();
	/// @warning 宣言のみ (選択をすべて解除する。Global.cpp:15073)
	void ClearSelection();
	/// 選択中の項目数 (Global.cpp:15057,15062)
	/// @warning 宣言のみ
	int GetSelCount() const;
	compat::ROProperty<TCustomListBox, int, &TCustomListBox::GetSelCount> SelCount{this};

	/// Selected[i] の読み書き (Global.cpp:15051,15066)。
	/// 選択状態は実 GUI 側が持つものなので、getter/setter とも宣言のみにした
	/// @warning 宣言のみ
	bool GetSelected(int index) const;
	/// @warning 宣言のみ
	void SetSelected(int index, bool value);

	class SelectedProperty {
	public:
		class Ref {
		public:
			Ref(TCustomListBox *owner, int index) : owner_(owner), index_(index) {}
			operator bool() const { return owner_->GetSelected(index_); }
			Ref &operator=(bool value)
			{
				owner_->SetSelected(index_, value);
				return *this;
			}

		private:
			TCustomListBox *owner_;
			int index_;
		};

		explicit SelectedProperty(TCustomListBox *owner) : owner_(owner) {}
		Ref operator[](int index) const { return Ref(owner_, index); }

	private:
		TCustomListBox *owner_;
	};
	SelectedProperty Selected{this};
};

/**
 * @brief TListBox 相当 (最小実装)
 * @details Canvas は Global.cpp の描画系 (set_ListBoxItemHi / draw_InfListBox /
 *          draw_ColorListBox / draw_ListCursor ほか計 10 箇所) が
 *          `lp->Canvas->Font` / `->FillRect()` の形で使う。TCheckListBox と
 *          同じく描画先を持たない既定の TCanvas (DC 無し) を持たせてある。
 */
class TListBox : public TCustomListBox {
public:
	TListBox() : Canvas(new TCanvas()) {}
	~TListBox() override { delete Canvas; }

	TCanvas *const Canvas;

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	void Invalidate() {}
	void Repaint() {}
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

	TStringGrid() : Canvas(new TCanvas()) {}
	~TStringGrid() override { delete Canvas; }

	int FixedCols = 1;
	int FixedRows = 1;
	int ColCount = 2;
	int RowCount = 2;
	int LeftCol = 0;
	int TopRow = 0;
	int VisibleRowCount = 0;
	int VisibleColCount = 0;
	/// カーソル行 (Global.cpp:12732)
	int Row = 0;
	/// 既定の行高 (Global.cpp:2764 が設定する)
	int DefaultRowHeight = 24;

	/// Global.cpp:12735 が `gp->Canvas` へ線を引く
	TCanvas *const Canvas;

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
/// @note Picture の型は Phase 3 で Graphics::TBitmap* → Graphics::TPicture* に
///       直した。実呼び出しは `ip->Picture->Bitmap->SetSize(...)`
///       (usr_shell.cpp:1783 / imgv_thread.cpp:122 / HistFrm.cpp:30) と
///       `Image1->Picture->Assign(...)` (SubView.cpp:57,95,110,116) で、
///       いずれも TPicture 越しのアクセスだった。VCL の TImage は Picture を
///       自前に生成して所有するが、ここでは生成しない (規約4 の対象外なので
///       nullptr になる。Phase 3 の GUI 実装で実体を持たせる)
class TImage : public TControl {
public:
	Graphics::TPicture *Picture = nullptr;
	/// SubView.cpp:111,117 が設定する
	bool Transparent = false;
};

/// TLabel 相当 (最小実装。実 VCL では TGraphicControl 経由で TControl を継承する。
/// Font は TControl から継承)
class TLabel : public TControl {
public:
	UnicodeString Caption;
};

//---------------------------------------------------------------------------
/**
 * @brief TShape 相当 (最小実装)
 * @details 実 VCL では TGraphicControl 経由で TControl を継承する。
 *          実測: Global.cpp:2743-2753 の SetToolWinBorder が
 *          `sp->Brush->Color` / `sp->Pen->Color` / `->Align` / `->Width` /
 *          `->Height` / `->Visible` を触る。MarkList.cpp:57-77 は加えて
 *          コンストラクタ `TShape(TComponent*)` / `->Parent` / `->Tag` /
 *          `->BoundsRect` / `->Pen->Width` / `->Brush->Style` /
 *          `->BringToFront()` を使う (BoundsRect は TControl 側に未実装。
 *          MarkList.cpp は今回のビルド対象外なので残作業として報告した)。
 *          Pen / Brush は TCanvas と同じくアロー呼び出しのため生ポインタで所有する。
 */
class TShape : public TControl {
public:
	explicit TShape(TComponent *owner = nullptr) : TControl(owner), Pen(new TPen()), Brush(new TBrush()) {}
	~TShape() override
	{
		delete Pen;
		delete Brush;
	}

	TPen *const Pen;
	TBrush *const Brush;
};

//---------------------------------------------------------------------------
/**
 * @brief TSpeedButton 相当 (最小実装)
 * @details 実 VCL では TGraphicControl 経由で TControl を継承する。
 *          実測: Global.cpp:12588-12615 の set_ButtonMark が `->Align` /
 *          `->Height` / `->Width` / `->Glyph` を、12626-12639 の
 *          set_BtnTextStyle が `->Caption` / `->Glyph` / `->ClientWidth` /
 *          `->ClientHeight` / `->ClientRect` / `->Font` を使う
 *          (Font / ClientRect ほかは TControl から継承)。
 *          Glyph は VCL と同じくコンストラクタで自前に生成して所有する。
 */
class TSpeedButton : public TControl {
public:
	explicit TSpeedButton(TComponent *owner = nullptr) : TControl(owner), Glyph(new Graphics::TBitmap()) {}
	~TSpeedButton() override { delete Glyph; }

	UnicodeString Caption;
	Graphics::TBitmap *const Glyph;
};

/// TButton 相当 (最小実装)
/// 実測: MarkList.cpp:54 (`((TButton*)cp)->Caption`)、OptDlg.cpp:1709
/// (`((TButton*)Sender)->Tag`。Tag は TComponent 側の担当)
class TButton : public TWinControl {
public:
	UnicodeString Caption;
};

/// TRadioButton 相当 (最小実装。src/ ではフォームのメンバとして保持されるのと
/// SrtModDlg.cpp:238 の `SubModeRadioGroup->Buttons[i]` の受け型としてのみ出現)
class TRadioButton : public TWinControl {
public:
	bool Checked = false;
	UnicodeString Caption;
};

/// TGroupBox 相当 (最小実装)
/// 実測: MarkList.cpp:52 (`->Caption`)、Global.cpp:14788-14789 (`->ControlCount`
/// / `->Controls[j]`。いずれも TWinControl から継承)
class TGroupBox : public TWinControl {
public:
	UnicodeString Caption;
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
	/// Owner を取るコンストラクタ (THintWindow の派生 UsrTooltipWindow /
	/// UsrHintWindow が `THintWindow(AOwner)` の形で基底へ転送するため必要)
	explicit TForm(TComponent *owner = nullptr) : TWinControl(owner) {}

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

	//-- Phase 3 で Global.cpp から要求されたメンバ ------------------------
	/// タイトル (Global.cpp:11154 の set_FormTitle が設定する)
	UnicodeString Caption;
	/// Global.cpp:15250 が fsStayOnTop と比較する
	TFormStyle FormStyle = fsNormal;
	/// ドッキングしていない (単独ウィンドウの) 状態か。Global.cpp:15250
	bool Floating = true;
	/// アクティブか。Global.h:1798 / Global.cpp:15250 付近が読む
	bool Active = false;

	/// @warning 宣言のみ (フォームを閉じる。Global.cpp:14743,15464)
	void Close();
	/// @warning 宣言のみ (モーダル表示。Global.cpp:14813 が戻り値を mrOk と比較する)
	TModalResult ShowModal();
};

//---------------------------------------------------------------------------
/**
 * @brief Vcl.Controls::THintWindow 相当 (最小実装)
 * @details 実測: Global.h:3160 の UsrTooltipWindow と usr_hintwin.h:19 の
 *          UsrHintWindow が継承し、コンストラクタ `THintWindow(AOwner)` へ
 *          転送する。派生側の Paint() が `ClientRect` (TControl 継承) /
 *          `Caption` (TForm 継承) / `Canvas` を使う (Global.cpp:16541-16549)。
 *          VCL では Canvas は TCustomForm 側のプロパティだが、実測で必要なのは
 *          THintWindow 経由だけなのでここに置いた。
 */
class THintWindow : public TForm {
public:
	explicit THintWindow(TComponent *owner = nullptr) : TForm(owner), Canvas(new TCanvas()) {}
	~THintWindow() override { delete Canvas; }

	TCanvas *const Canvas;
};

//---------------------------------------------------------------------------
/**
 * @brief Vcl.ExtCtrls::TTimer 相当 (最小実装)
 * @details 実測: src/ 全体で `->Enabled` 19 / `->Tag` 7 / `->Interval` 5
 *          (Tag は TComponent 側)。Global.cpp:9092 は `->Tag` だけを使うが、
 *          不完全型のままでは通らないので実体を置く。OnTimer の発火機構は
 *          無い (メッセージループが無いため)。
 */
class TTimer : public TComponent {
public:
	bool Enabled = true;
	int Interval = 1000;
	TNotifyEvent OnTimer;
};

//---------------------------------------------------------------------------
// Vcl.Dialogs / Vcl.ExtDlgs の共通ダイアログ。
// 実測: UserMdl.h:99,106,168-173 がデータモジュールのメンバとして保持するだけで、
//       Global.cpp からのメンバアクセスは 1 箇所も無い。したがってメンバは
//       宣言しない (必要になった時点で実呼び出し箇所を grep して足すこと)。
//---------------------------------------------------------------------------
/// Vcl.Dialogs::TCommonDialog 相当
class TCommonDialog : public TComponent {
};

class TOpenDialog : public TCommonDialog {
};
class TSaveDialog : public TOpenDialog {
};
/// Vcl.Dialogs::TSaveTextFileDialog 相当
class TSaveTextFileDialog : public TSaveDialog {
};
/// Vcl.ExtDlgs::TOpenPictureDialog 相当
class TOpenPictureDialog : public TOpenDialog {
};
/// TColorDialog 相当。CustomColors はユーザー定義色を "ColorA=..." の形で持つ
/// TStrings で、Global.cpp:10315 が ini の読み書き対象の一覧へ登録する。
/// VCL と同じくコンストラクタで生成して所有する (TComboBox::Items と同じ扱い)
class TColorDialog : public TCommonDialog {
public:
	TColorDialog() : CustomColors(new TStringList()) {}
	~TColorDialog() override { delete CustomColors; }

	TStrings *CustomColors;
};
class TFontDialog : public TCommonDialog {
};

//---------------------------------------------------------------------------
/**
 * @brief TImageList 相当 (最小実装)
 * @details 実測: Global.cpp:7608,7630 の `add_IconImage(..., TImageList *lst)` が
 *          `lst->AddIcon(icon)` を呼ぶだけ。
 */
class TImageList : public TComponent {
public:
	/// @warning 宣言のみ (追加した位置のインデックスを返す)
	int AddIcon(Graphics::TIcon *icon);
};

/**
 * @brief TImageCollectionSourceItem 相当
 * @details 実測: Global.cpp:7660-7661 / About.cpp:25-26 が
 *          `sp->Image->Assign(bmp.get())` の形で使う。
 *          VCL は Image を自前に生成して所有するが、ここでは生成しない
 *          (この経路は SourceImages->Add() が宣言のみなので到達しない)。
 */
class TImageCollectionSourceItem : public TPersistent {
public:
	Graphics::TBitmap *Image = nullptr;
};

/// TImageCollectionSourceItems 相当 (実測: Global.cpp:7660 の `->Add()`)
class TImageCollectionSourceItems : public TPersistent {
public:
	/// @warning 宣言のみ
	TImageCollectionSourceItem *Add();
};

/// TImageCollectionItem 相当 (実測: Global.cpp:7660 / About.cpp:25 の `->SourceImages`)
class TImageCollectionItem : public TPersistent {
public:
	TImageCollectionSourceItems *SourceImages = nullptr;
};

/// TImageCollectionItems 相当 (実測: Global.cpp:7657,7662 の `->Add()` / `->Count`)
class TImageCollectionItems : public TPersistent {
public:
	/// @warning 宣言のみ
	TImageCollectionItem *Add();
	/// @warning 宣言のみ
	int GetCount() const;

	compat::ROProperty<TImageCollectionItems, int, &TImageCollectionItems::GetCount> Count{this};
};

/// Vcl.BaseImageCollection::TCustomImageCollection 相当
/// (TVirtualImageList::ImageCollection の静的型。Global.cpp:7656 が
///  TImageCollection* へキャストして使う)
class TCustomImageCollection : public TComponent {
};

/// Vcl.ImageCollection::TImageCollection 相当 (実測: Global.cpp:7657,7662 の `->Images`)
class TImageCollection : public TCustomImageCollection {
public:
	TImageCollectionItems *Images = nullptr;
};

/// Vcl.VirtualImageList::TVirtualImageList 相当
/// 実測: Global.cpp:7656 の `lst->ImageCollection` のみ
class TVirtualImageList : public TComponent {
public:
	TCustomImageCollection *ImageCollection = nullptr;
};

//---------------------------------------------------------------------------
/**
 * @brief Vcl.Clipbrd::TClipboard 相当 (最小実装)
 * @details 実測: Global.cpp:10350-10351 (`HasFormat(CF_BITMAP)` /
 *          `i_bmp->Assign(Clipboard())`)、15138 (`AsText = s`)、
 *          15156,15164,15172 (`Assign(pic/mf/bmp)`)、SubView.cpp:95
 *          (`Image1->Picture->Assign(Clipboard())`)。
 *          `Assign()` は TPersistent から継承する (宣言のみ = 未実装)。
 */
class TClipboard : public TPersistent {
public:
	/// @warning 宣言のみ
	UnicodeString GetAsText() const;
	/// @warning 宣言のみ
	void SetAsText(const UnicodeString &s);
	/// @warning 宣言のみ (Win32 のクリップボード形式 CF_* を持っているか)
	bool HasFormat(unsigned format) const;

	compat::RWProperty<TClipboard, UnicodeString, &TClipboard::GetAsText, &TClipboard::SetAsText>
		AsText{this};
};

/// Vcl.Clipbrd::Clipboard() 相当 (プロセス唯一の TClipboard を返す)
/// @warning 宣言のみ
TClipboard *Clipboard();

#endif  // NYANFI_COMPAT_GUI_STUBS_H
