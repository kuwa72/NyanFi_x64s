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
//前方宣言 (TWinControl / TAction / TPopupMenu を、実体を定義する前に
//ポインタメンバとして持つため)。vcl_shim.h 経由なら先に読まれているが、
//このヘッダ単体でも読めるように明示的に含める (tests/compat から直接
//インクルードして落ちた)
#include "compat/vcl_forward.h"

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

/// ウィンドウハンドルから VCL のコントロールを引く。
/// 実測: `FindControl(get_window_from_pos())` (UserMdl.cpp:380,401) と
/// MainFrm.cpp:1637 の 3箇所。
/// @warning 宣言のみ (規約4)。シムは HWND とコントロールの対応表を持たない。
///          nullptr を返す実装にすると「常に別のコントロール」と判定されて
///          静かに挙動が変わる (UserMdl.cpp:380 はスポイトの色取得の判定)
class TWinControl;
TWinControl *FindControl(HWND handle);

/// フォルダ選択ダイアログの追加オプション (Vcl.FileCtrl)。
/// 実測: `TSelectDirExtOpts() << sdNewUI << sdShowShares` (UserMdl.cpp:946) のみ
enum TSelectDirExtOpt { sdNewUI, sdShowShares, sdNewFolder, sdShowEdit, sdValidateDir };
using TSelectDirExtOpts = Set<TSelectDirExtOpt, sdNewUI, sdValidateDir>;

/// フォルダ選択ダイアログ (Vcl.FileCtrl::SelectDirectory)。
/// @warning 宣言のみ。ダイアログを実際に出す経路なので実装しない (規約4)
bool SelectDirectory(const UnicodeString &caption, const UnicodeString &root,
                     UnicodeString &directory, TSelectDirExtOpts options, TForm *parent);

/// Vcl.Controls::TDragMode 相当 (実測: UserMdl.cpp:138 の `DragMode==dmAutomatic`)
enum TDragMode { dmManual, dmAutomatic };

/// Vcl.Controls::TDragState 相当 (実測: UserMdl.h:255 の DragOver ハンドラの引数型のみ)
enum TDragState { dsDragEnter, dsDragLeave, dsDragMove };

/// Vcl.Controls::TDragObject 相当 (実測: UserMdl.h:253 の StartDrag ハンドラの
/// 引数型 `TDragObject *&` としてのみ出現。メンバアクセスは無い)
class TDragObject : public TObject {
};

//---------------------------------------------------------------------------
/// 通常の矢印カーソル (実測: InpCmds.cpp:449 の `Screen->Cursor = crArrow;` のみ)。
/// 値は Delphi の crXXX 表 (crDefault=0 / crNone=-1 / crArrow=-2 / … /
/// crHourGlass=-11) に従う。既に compat/controls.h にある crHourGlass=-11 と
/// 同じ表なので整合している。
/// @note 本来は crDefault / crHourGlass と並べて compat/controls.h に置くのが
///       自然。Phase 3b の分担でこのファイルしか触れなかったためここに置いた
constexpr TCursor crArrow = -2;

/**
 * @brief Vcl.Controls::TMouse 相当 (グローバル Mouse)
 * @details 実測: src 全体で使われているのは `Mouse->CursorPos` の読み書きだけ
 *          (読み 20 / 書き 4。usr_shell.cpp:241、UserFunc.cpp:262、
 *          CalcDlg.cpp:946-947、ColPicker.cpp:89,98、MainFrm.cpp ほか)。
 *
 *          これは Win32 の ::GetCursorPos / ::SetCursorPos そのものなので、
 *          GUI フレームワークが無くても**本当の実装**が書ける。宣言のみに
 *          しても代わりに書くべき処理が存在しないため、ここは実装した
 *          (規約4 が禁じているのは「中身の無い no-op で未実装を隠すこと」)。
 * @warning この経路の自動テストは無い。テストで ::SetCursorPos を呼ぶと
 *          実機のマウスカーソルが飛ぶため意図的に書いていない (規約9 の
 *          「未検証」として報告に明記した)。
 */
class TMouse {
public:
	TPoint GetCursorPos() const
	{
		::POINT p{};
		if (!::GetCursorPos(&p)) return TPoint(0, 0);
		return TPoint(p.x, p.y);
	}
	void SetCursorPos(const TPoint &value) { ::SetCursorPos(value.x, value.y); }

	/**
	 * @brief `Mouse->CursorPos` 専用のプロキシ
	 * @details 汎用の RWProperty ではなく専用にしてあるのは、src が
	 *          `Mouse->CursorPos.x` / `.y` と**データメンバとして**触るため
	 *          (UserMdl.cpp:404 / MainFrm.cpp:2221,3088,3139)。
	 *          プロキシはメンバ関数しか転送できないので `.x` を通せない。
	 *
	 *          汎用のプロパティ全部に x/y を生やすと、値型が何であっても
	 *          メンバが増えて紛らわしい。ここだけに閉じた形にした。
	 */
	class CursorPosProperty {
	public:
		/// `.x` / `.y` (読み取り専用。読むたびに実際のカーソル位置を取る)
		class Axis {
		public:
			Axis(const TMouse *owner, bool isY) : owner_(owner), isY_(isY) {}
			operator int() const
			{
				const TPoint p = owner_->GetCursorPos();
				return isY_? p.Y : p.X;
			}

		private:
			const TMouse *owner_;
			bool isY_;
		};

		explicit CursorPosProperty(TMouse *owner)
			: x(owner, false), y(owner, true), X(owner, false), Y(owner, true), owner_(owner) {}
		CursorPosProperty(const CursorPosProperty &) = delete;

		operator TPoint() const { return owner_->GetCursorPos(); }
		TPoint get() const { return owner_->GetCursorPos(); }

		CursorPosProperty &operator=(const TPoint &value)
		{
			owner_->SetCursorPos(value);
			return *this;
		}

		Axis x, y, X, Y;

	private:
		TMouse *owner_;
	};

	CursorPosProperty CursorPos{this};
};

namespace compat {
/// グローバル Mouse の実体 (inline 変数なので複数の翻訳単位に含めても 1 つ)
inline TMouse mouse_instance;
}  // namespace compat

/// VCL のグローバル Mouse 相当。プロセス内で 1 つ
inline TMouse *const Mouse = &compat::mouse_instance;

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

// ドラッグ＆ドロップのイベント。
// 実測: src/UserMdl.h:253-257 の宣言に合わせてある
//   ListBoxStartDrag(TObject *Sender, TDragObject *&DragObject)
//   ListBoxDragOver (TObject *Sender, TObject *Source, int X, int Y, TDragState State, bool &Accept)
//   ListBoxDragDrop (TObject *Sender, TObject *Source, int X, int Y)
//   ListBoxEndDrag  (TObject *Sender, TObject *Target, int X, int Y)
// TDragObject / TDragState はこのファイルの上方 (117-122行) で定義済み
/// 右クリックメニュー (実測: UserMdl.h:258 の宣言に合わせてある)
using TContextPopupEvent = TClosureEvent<TObject *, const TPoint &, bool &>;
using TStartDragEvent = TClosureEvent<TObject *, TDragObject *&>;
using TDragOverEvent = TClosureEvent<TObject *, TObject *, int, int, TDragState, bool &>;
using TDragDropEvent = TClosureEvent<TObject *, TObject *, int, int>;
using TEndDragEvent = TClosureEvent<TObject *, TObject *, int, int>;
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

//---------------------------------------------------------------------------
// メッセージ別の構造体 (Winapi.Messages 相当)
//
// VCL は WM_* ごとに TMessage と同じ大きさの構造体を用意し、
// VCL_MESSAGE_HANDLER で `*static_cast<type *>(Message)` として被せる。
// **TMessage と同じレイアウトでなければならない** (別物を被せることになる)。
// src が使うのは3種だけ (grep 済み)。
//---------------------------------------------------------------------------

/// WM_DROPFILES。実測: src/AppDlg.cpp:305,311 が `(HDROP)msg.Drop` として使う
struct TWMDropFiles {
	unsigned Msg;
	NativeUInt Drop;	//!< HDROP (wParam)
	NativeInt Unused;
	NativeInt Result;
};

/// WM_SYSCOMMAND。実測: src/HistFrm.h:38 / MainFrm.h:2158 の
/// `if (SysCom.CmdType==SC_CLOSE)`
struct TWMSysCommand {
	unsigned Msg;
	NativeUInt CmdType;	//!< wParam (SC_CLOSE など)
	NativeInt Key;
	NativeInt Result;
};

/// WM_GETMINMAXINFO。実測: src/MainFrm.cpp:1973-1985 が
/// `msg.MinMaxInfo->ptMaxSize` などを書き換える
struct TWMGetMinMaxInfo {
	unsigned Msg;
	NativeUInt Unused;
	::MINMAXINFO *MinMaxInfo;	//!< lParam
	NativeInt Result;
};

static_assert(sizeof(TWMDropFiles) == sizeof(TMessage), "TMessage と同じ大きさでなければならない");
static_assert(sizeof(TWMSysCommand) == sizeof(TMessage), "TMessage と同じ大きさでなければならない");
static_assert(sizeof(TWMGetMinMaxInfo) == sizeof(TMessage), "TMessage と同じ大きさでなければならない");

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

	/// ドラッグの開始方法 (dmManual / dmAutomatic)。
	/// 実測: UserMdl.cpp:138 が `DragMode==dmAutomatic` を見るだけ
	TDragMode DragMode = dmManual;

	/// 再描画。実測: UserMdl.cpp / usr_hintwin.cpp などが呼ぶ。
	/// **本物の描画経路がシムに無いので何もしない** (ヘッダ冒頭の
	/// 「real no-op」の扱い。呼んでも表示が変わらないだけで壊れない)
	void Repaint() {}
	void Invalidate() {}

	/// ツールチップに出す文字列。VCL では TControl のプロパティ。
	/// 実測: `->Hint` が src 全体で 10箇所 (UserMdl.cpp の 5箇所を含む)
	UnicodeString Hint;

	//-- ドラッグ＆ドロップのイベント -------------------------------------
	// 実測: UserMdl.cpp が TListBox / TCheckListBox に対して代入するだけで、
	// 発火させる側 (実コントロール) はシムに無い。値を保持する
	TStartDragEvent OnStartDrag;
	TEndDragEvent OnEndDrag;
	TDragOverEvent OnDragOver;
	TDragDropEvent OnDragDrop;

	/// @warning 宣言のみ。実際に呼び出す経路がリンクされると未定義参照になる
	NativeInt Perform(unsigned msg, NativeInt wParam, NativeInt lParam);

	/// @warning 宣言のみ (実処理は Z オーダーの変更。テストからは呼ばれない想定)
	void BringToFront();

	/// @brief クライアント座標をスクリーン座標へ変換する
	/// @details 実測: UserFunc.cpp:104,255 / usr_hintwin.cpp:51,52 /
	///          OptDlg.cpp:2439 / NewDlg.cpp:37 / MainFrm.cpp 多数。
	///          実処理は自ウィンドウの位置が要る (Win32 の ::ClientToScreen は
	///          HWND が必須で、TControl は非ウィンドウのコントロールも含む)。
	///          Left/Top を足すだけの近似を書くと**親のスクロール量や
	///          ウィンドウ枠のぶんだけ静かにずれる**ので、宣言のみにした
	/// @warning 宣言のみ
	TPoint ClientToScreen(const TPoint &point);

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

	/// 選択されている文字列。**代入すると選択範囲がその文字列で置き換わる**
	/// (VCL の意味論)。実測: UserMdl.cpp が 13箇所で読み書きする
	/// (貼り付け / ファイル名の挿入など)。
	///
	/// @warning アクセサは宣言のみ (規約4)。素のデータメンバにすると
	///          「代入したのにテキストが変わらない」で静かに壊れる。
	///          実際のキャレットと選択は実コントロールが持つので、
	///          ヘッドレスでは実装できない
	UnicodeString GetSelText() const;
	void SetSelText(const UnicodeString &value);
	compat::RWProperty<TCustomEdit, UnicodeString, &TCustomEdit::GetSelText,
	                   &TCustomEdit::SetSelText> SelText{this};

	/// @brief テキスト全体を選択する
	/// @details 実測: UserFunc.cpp:322,383 (ChangeSelFileNameEdit /
	///          ChangeSelCmdEdit の「全体を選択」)、InpExDlg.cpp:218,226,316、
	///          RenDlg.cpp:497、MemoFrm.cpp:53。
	///          `SelStart = 0; SelLength = Text.Length();` と書けてしまうが、
	///          実 GUI では選択の反映 (キャレット移動と再描画) を伴う。
	///          データメンバだけ書き換える偽物を置くと、実装漏れが
	///          隠れるので宣言のみにした (規約4)
	/// @warning 宣言のみ
	void SelectAll();
};

/// TEdit 相当 (最小実装。Text / Font は TCustomEdit / TControl から継承)
class TEdit : public TCustomEdit {
};

/// TLabeledEdit 相当 (最小実装)
class TLabeledEdit : public TCustomEdit {
};

/// TMaskEdit 相当 (最小実装)
class TMaskEdit : public TCustomEdit {
public:
	/// マスクを除いた素の入力文字列。実測: UserMdl.cpp:547 が Text と
	/// 長さを比べるだけ。VCL では Text と別物だが、マスクを実装していないので
	/// **同じ値を返す** (マスク付きの入力欄では差が出る。報告書 §19)
	UnicodeString GetEditText() const { return Text; }
	void SetEditText(const UnicodeString &v) { Text = v; }
	compat::RWProperty<TMaskEdit, UnicodeString, &TMaskEdit::GetEditText,
	                   &TMaskEdit::SetEditText> EditText{this};

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
	/// 右クリックメニューのイベント。実測: UserMdl.cpp が代入するだけ
	/// (`ComboBoxContextPopup(TObject*, const TPoint&, bool&)`。UserMdl.h:258)
	TContextPopupEvent OnContextPopup;

	/// 選択されている文字列。代入すると選択範囲が置き換わる (VCL の意味論)。
	/// 実測: UserMdl.cpp が 10箇所で読み書きする。
	/// @warning アクセサは宣言のみ (規約4)。TCustomEdit::SelText と同じ理由
	UnicodeString GetSelText() const;
	void SetSelText(const UnicodeString &value);
	compat::RWProperty<TComboBox, UnicodeString, &TComboBox::GetSelText,
	                   &TComboBox::SetSelText> SelText{this};

	/// 実 VCL の TComboBox は Items (TStrings) をコンストラクタで自前に生成
	/// して所有する (TCustomListBox と同じ理由)。Canvas は TListBox と同じく
	/// 描画先を持たない既定の TCanvas (DC 無し)
	TComboBox() : Items(new TStringList()), Canvas(new TCanvas()) {}
	~TComboBox() override
	{
		delete Items;
		delete Canvas;
	}

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

	//-- Phase 3b で InpCmds.cpp / UserFunc.cpp から要求されたメンバ -------
	// VCL では TCustomCombo (TCustomEdit ではない) が持つプロパティなので、
	// 継承関係は変えずにここへ足した。TCustomEdit 側と同じく「単純な値の
	// 読み書き」なのでデータメンバでよい (gui_stubs.h 冒頭の設計方針)

	/// 選択開始位置 (0 起点)。
	/// 実測: InpCmds.cpp:193,230,256,376,460 / UserFunc.cpp:347,348,355,359
	int SelStart = 0;
	/// 選択文字数。実測: InpCmds.cpp:229,255,375,459 / UserFunc.cpp:347,348,356
	int SelLength = 0;

	/// @brief 入力欄のテキスト全体を選択する
	/// @details 実測: UserFunc.cpp:352 (ChangeSelCmdComboBox の「全体を選択」)、
	///          InpExDlg.cpp:235,323。TCustomEdit::SelectAll と同じ理由で宣言のみ
	/// @warning 宣言のみ
	void SelectAll();

	/// オーナードローの描画先。
	/// 実測: InpCmds.cpp:345 の SubComboBoxDrawItem が
	/// `TCanvas *cv = SubComboBox->Canvas;` として FillRect / TextOut / TextWidth
	/// に使う (候補一覧の自前描画)
	TCanvas *const Canvas;
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

/**
 * @brief Vcl.ExtCtrls::TBevel 相当 (見た目だけの区切り線・枠)
 * @details 実測: `src/OptDlg.h:764-766` が 3つ保持するだけで、
 *          `.cpp` からのメンバアクセスは1件も無い (grep 済み)。
 *          描画専用の部品なのでメンバは足さない。
 */
class TBevel : public TControl {
};

/**
 * @brief Vcl.ExtCtrls::TColorBox 相当 (色を選ぶコンボボックス)
 * @details 実測: `Selected` 2箇所 / `Color` 1箇所だけ。
 *          Color は TControl 側にあるので Selected だけを足す。
 */
class TColorBox : public TWinControl {
public:
	/// 選択中の色
	TColor Selected = clBlack;
};

/// Vcl.Forms の TForm::OnHelp が受け取る第2引数。
/// 実測: `bool __fastcall FormHelp(WORD Command, THelpEventData Data, bool &CallHelp)`
/// の形でフォーム側が受けるだけで、**中身を読む箇所は1つも無い** (grep 済み)。
/// VCL では NativeInt (ヘルプの文脈 ID かキーワード文字列のポインタ)。
using THelpEventData = NativeInt;

/**
 * @brief Vcl.ComCtrls::TToolButton 相当
 * @details ツールバー上のボタン。フォームのヘッダが 79箇所で
 *          `TToolButton *XxxBtn;` として保持している。
 *
 *          実測 (`*Btn->` / `*Button->` のメンバアクセスを集計):
 *          Checked 93 / Enabled 33 / Visible 13 / Hint 10 / SetFocus 9 /
 *          Action 4 / Top 4 / Width 3 / Left 3 / Click 3 / Height 2 / Tag 1。
 *          Enabled / Visible / Hint / Top / Left / Width / Height / Action /
 *          SetFocus は TControl・TWinControl 側にあるので、ここは Checked と
 *          Click だけでよい。
 */
class TToolButton : public TControl {
public:
	/// 押し込み状態 (実測 93箇所。ほとんどが表示モードの ON/OFF)
	bool Checked = false;

	/// グループ化された排他ボタンか
	bool Grouped = false;
	/// 区切り (tbsSeparator) などのスタイル
	int Style = 0;

	/// @warning 宣言のみ。押下を模擬するとアクションが走ってしまうので、
	///          実装せずリンクエラーにする (規約4)
	void Click();
};

/// TSplitter 相当 (最小実装。Color は TControl から継承。
/// Global.cpp:6491 が `((TSplitter*)cp)->Color = ...` とするだけ)
class TSplitter : public TControl {
public:
	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	/// (gui_stubs.h 冒頭の設計方針を参照)。
	/// 実測: UserFunc.cpp:1331 の set_SplitterWidht が幅を変えた後に呼ぶ
	void Repaint() {}
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
	/// 列幅の下限 / 上限 (Vcl.ComCtrls::THeaderSection の MinWidth / MaxWidth)。
	/// 実測: UserFunc.cpp:787-788 の set_HeaderSecWidth が「一旦、固定を解除」
	/// として `MinWidth = 0; MaxWidth = 10000;` を書き、809-810 で幅を固定する
	/// ために両方へ同じ値を入れる。**既定値はこの src 側の「解除」の書き方に
	/// 合わせた** (Delphi の既定も同じ 0 / 10000)。
	/// 幅の実制約は実 GUI 側が行うため、ここでは値を保持するだけ
	int MinWidth = 0;
	int MaxWidth = 10000;
};

/// THeaderSections 相当 (THeaderSection のコレクション)
/// 実測: Global.cpp:2779,12541 (`->Count`)、2780 (`->Items[i]`)
class THeaderSections : public TPersistent {
public:
	/// @warning 宣言のみ
	THeaderSection *GetItem(int index) const;

	/// @brief セクション数
	/// @details **プロパティのプロキシではなく素の int にしてある。**
	///          UserFunc.cpp:748,765 が `std::min(hp->Sections->Count, gp->ColCount)`
	///          と書いており、プロキシ型では std::min のテンプレート実引数推定が
	///          両辺で食い違って通らない (C++Builder の `__property int Count` は
	///          読むと int の右辺値になるのでそのまま通っていた)。
	///
	/// @warning **値は 0 のまま更新されない。** 実データを持たない
	///          (GetItem() が宣言のみ) ので、数えようがないため。
	///
	///          Count を読む 7箇所のうち 6箇所は同じ処理で `Items[i]` も触るので、
	///          GetItem() の未定義参照でリンク時に落ちる
	///          (UserFunc.cpp:748,756,765,773 / Global.cpp:2779 / EditHistDlg.cpp:178)。
	///
	///          **残る 1箇所は静かに挙動が変わる**: `Global.cpp:12542` の
	///          `if (sp->Index < hp->Sections->Count-1)` は Items を触らないため、
	///          Count が 0 だと条件が常に偽になり**ヘッダの区切り線が描かれない**。
	///          リンクエラーにならないので気づけない。実データを持つ実装に
	///          差し替えるときに解消する (報告書 §19)。
	int Count = 0;

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
	TStatusBar() : Panels(new TStatusPanels()), Canvas(new TCanvas()) {}
	~TStatusBar() override
	{
		delete Panels;
		delete Canvas;
	}

	TStatusPanels *const Panels;

	/// パネル幅の計算に使う描画コンテキスト。
	/// 実測: UserFunc.cpp:828,838 の set_SttBarPanelWidth が
	/// `cv->Font->Assign(sp->Font)` してから TextWidth で幅を測る。
	/// TListBox / TCheckListBox と同じく描画先を持たない既定の TCanvas (DC 無し)
	TCanvas *const Canvas;

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
	/// メニュー/ツールバーに出すか。実測: UserMdl.cpp が 25箇所で読み書きする
	bool Visible = true;
	/// メニュー項目などに出す文言
	UnicodeString Caption;
	/// @warning 宣言のみ (OnUpdate を発火して表示状態を更新する。Global.cpp:6497)
	void Update();
	/// @warning 宣言のみ。**実行するとコマンドが走る**ので実装しない (規約4)。
	///          実測: UserMdl.cpp の 3箇所
	void Execute();
};

/// TActionList 相当 (最小実装。src/ ではフォームのメンバとして保持されるだけで
/// メンバアクセスは無い。UserMdl.h:98 / InpDir.h:26 ほか)
class TActionList : public TComponent {
};

//---------------------------------------------------------------------------
/**
 * @brief Vcl.Menus::TMenuItem 相当 (最小実装)
 * @details 実測した使われ方:
 *            - `new TMenuItem(親)` → `->Caption` / `->OnClick` を設定して
 *              `親->Add(mp)` (UserMdl.cpp:96-110, 783-787, 828-833)
 *            - `->Clear()` (UserMdl.cpp:779)
 *            - `->Count` / `->Items[i]` / `->Action` / `->Visible` /
 *              `->Caption` (UserFunc.cpp:1578-1596 の reduction_MenuLine、
 *              UserMdl.cpp:428-433)
 *          Caption / Visible / OnClick は単純な値の保持なのでデータメンバ、
 *          子項目の増減 (Add / Clear / Count / Items) は実メニューの
 *          構築を伴うので宣言のみ (規約4)。
 */
class TMenuItem : public TComponent {
public:
	explicit TMenuItem(TComponent *owner = nullptr) : TComponent(owner) {}

	UnicodeString Caption;
	bool Visible = true;
	bool Enabled = true;
	bool Checked = false;
	/// 割り当てられたアクション (UserFunc.cpp:1580 `if (ip->Action) ip->Action->Update();`)
	TAction *Action = nullptr;
	TNotifyEvent OnClick;

	/// @warning 宣言のみ (子項目を末尾に追加する)
	void Add(TMenuItem *item);
	/// @warning 宣言のみ (子項目をすべて削除する)
	void Clear();
	/// @warning 宣言のみ
	int GetCount() const;
	/// @warning 宣言のみ
	TMenuItem *GetItem(int index) const;

	compat::ROProperty<TMenuItem, int, &TMenuItem::GetCount> Count{this};

	/// Items[i] (読取専用の添字プロパティ。返った TMenuItem への書き込みは可)
	class ItemsProperty {
	public:
		explicit ItemsProperty(TMenuItem *owner) : owner_(owner) {}
		TMenuItem *operator[](int index) const { return owner_->GetItem(index); }

	private:
		TMenuItem *owner_;
	};
	ItemsProperty Items{this};
};

/**
 * @brief Vcl.Menus::TPopupMenu 相当 (最小実装)
 * @details 実測: `->Items` (ルート項目。UserMdl.cpp:428,478、
 *          UserMdl.cpp:790,838 の `reduction_MenuLine(EditPopupMenuC->Items)`)
 *          と `->Popup(x, y)` (UserFunc.cpp:106、UserMdl.cpp:481) だけ。
 *          VCL の TMenu と同じくルート項目を自前に生成して所有する
 *          (TComboBox の Items と同じ扱い)。
 */
class TPopupMenu : public TComponent {
public:
	explicit TPopupMenu(TComponent *owner = nullptr) : TComponent(owner), Items(new TMenuItem()) {}
	~TPopupMenu() override { delete Items; }

	TMenuItem *const Items;

	/// @warning 宣言のみ (スクリーン座標 (x, y) にポップアップを出す)
	void Popup(int x, int y);
};

//---------------------------------------------------------------------------
/**
 * @brief Vcl.ComCtrls::TUpDown 相当 (最小実装)
 * @details 実測: UserFunc.cpp:736-740 の init_UpDown が `->Position` の
 *          読み書きと `->Associate` (連動する入力欄) の取得だけを行う。
 *          PrnImgDlg.h / MainFrm.h はメンバとして保持するのみ
 */
class TUpDown : public TWinControl {
public:
	int Position = 0;
	int Min = 0;
	int Max = 100;
	/// 連動する入力欄 (UserFunc.cpp:739 が TCustomEdit へ dynamic_cast する)
	TWinControl *Associate = nullptr;
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
	/// 指定した位置にある項目の添字 (無ければ -1)。
	/// 実測: MainFrm.cpp:3011,3211,10795 / UserMdl.cpp。
	/// @warning 宣言のみ (規約4)。可変高と横スクロールがあるので、
	///          項目高さから割り算する近似は静かにずれる
	int ItemAtPos(const TPoint &pos, bool existing);

	/// 実 VCL の TCustomListBox は Items (TStrings) をコンストラクタで自前に
	/// 生成して所有する (呼び出し側が LoadFromFile/Assign 等で直接書き込む
	/// 対象なので、外部の TStringList への差し替えも許すよう生ポインタの
	/// ままにしてある)
	TCustomListBox() : Items(new TStringList()), Canvas(new TCanvas()) {}
	~TCustomListBox() override
	{
		delete Items;
		delete Canvas;
	}

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

	//-- Phase 3b で UserFunc.cpp から要求されたメンバ ---------------------

	/// 描画先。VCL でも Canvas は TCustomListBox のプロパティ。
	/// もとは TListBox / TCheckListBox が別々に持っていたが、
	/// UserFunc.cpp:581-592 の draw_ListItemLine が `TCustomListBox *` のまま
	/// `lp->Canvas->Pen` を触るのでここへ集約した (派生側で重ねて宣言すると
	/// 基底を隠して静かに壊れる。TControl::Tag と同じ罠)
	TCanvas *const Canvas;

	/// @brief index 番目の項目の矩形 (クライアント座標)
	/// @details 実測: UserFunc.cpp:586 の draw_ListItemLine (項目の境界に罫線を
	///          引く)、MainFrm.cpp:3287 (ヒントの CursorRect)。
	///          実 VCL は LB_GETITEMRECT を投げる。TopIndex と ItemHeight から
	///          計算する近似は書けるが、可変高 (lbOwnerDrawVariable) と
	///          横スクロールを無視して**静かにずれる**ので宣言のみにした
	/// @warning 宣言のみ
	TRect ItemRect(int index);
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
	/// カーソル列。実測: UserFunc.cpp:860,1125,1130 (get_GridIndex /
	/// GridCursorLeft / GridCursorRight)、CsvRecFrm.cpp:55,189,199、
	/// MainFrm.cpp:11779,34817-34847。Row と同じく単純な値の読み書き
	int Col = 0;
	/// 既定の行高 (Global.cpp:2764 が設定する)
	int DefaultRowHeight = 24;
	/// 罫線の太さ。実測: UserFunc.cpp:751,768 (ヘッダとセル幅の相互変換で
	/// 罫線ぶんを足し引きする)、MainFrm.cpp:4721,11890-11891。
	/// 既定値 1 は Delphi の TCustomGrid.GridLineWidth の既定に合わせた
	int GridLineWidth = 1;

	/// Global.cpp:12735 が `gp->Canvas` へ線を引く
	TCanvas *const Canvas;

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	/// (gui_stubs.h 冒頭の設計方針を参照)。
	/// 実測: EditHistDlg.h:160 / CmdListDlg.cpp:421 / MainFrm.cpp:34461,34556
	void Invalidate() {}
	void Repaint() {}

	IntArrayProperty ColWidths{&col_widths_, 64};
	IntArrayProperty RowHeights{&row_heights_, 24};

	/**
	 * @brief Cells[col][row] (セルの文字列。読み書きとも実データ)
	 * @details 実測: src 全体で 87 箇所 (EditHistDlg.cpp / UserFunc.cpp:851 の
	 *          clear_GridRow / CsvRecFrm.cpp / DriveDlg.cpp ほか)。
	 *          C++Builder の `__property Cells[int ACol][int ARow]` と同じ
	 *          **列が先**の並びで、`gp->Cells[col][row] = s;` と書かれている。
	 *
	 *          ColWidths / RowHeights と同じく**実データとして実装した**。
	 *          「書いた値がそのまま読める」以上の意味を持たない素の表なので、
	 *          宣言のみにする理由が無い (規約4 が守りたいのは「実処理がある
	 *          のに no-op で隠すこと」)。範囲外の添字は自動的に伸ばし、
	 *          未設定のセルは空文字列を返す (ColCount / RowCount とは連動
	 *          しない。実 VCL は RowCount を増やすと空行が増えるだけなので
	 *          観測できる差は無い)。
	 */
	class CellsProperty {
	public:
		class Ref {
		public:
			Ref(std::vector<std::vector<UnicodeString>> &cells, int col, int row)
				: cells_(cells), col_(col), row_(row) {}

			operator UnicodeString() const
			{
				if (col_ < 0 || row_ < 0) return UnicodeString();
				if (col_ >= static_cast<int>(cells_.size())) return UnicodeString();
				const std::vector<UnicodeString> &column = cells_[col_];
				if (row_ >= static_cast<int>(column.size())) return UnicodeString();
				return column[row_];
			}

			Ref &operator=(const UnicodeString &value)
			{
				if (col_ < 0 || row_ < 0) return *this;
				if (col_ >= static_cast<int>(cells_.size())) cells_.resize(col_ + 1);
				std::vector<UnicodeString> &column = cells_[col_];
				if (row_ >= static_cast<int>(column.size())) column.resize(row_ + 1);
				column[row_] = value;
				return *this;
			}

			//`gp->Cells[c][r] = gp->Cells[c2][r2];` の形をそのまま通す
			Ref &operator=(const Ref &rhs) { return operator=(static_cast<UnicodeString>(rhs)); }

			//`gp->Cells[c][r].IsEmpty()` などの形をそのまま通す
			NYANFI_PROPERTY_FORWARD_CONST_METHODS

			UnicodeString get() const { return static_cast<UnicodeString>(*this); }

		private:
			std::vector<std::vector<UnicodeString>> &cells_;
			int col_;
			int row_;
		};

		/// Cells[col] — さらに [row] を付けて 1 セルを指す
		class ColumnRef {
		public:
			ColumnRef(std::vector<std::vector<UnicodeString>> &cells, int col) : cells_(cells), col_(col) {}
			Ref operator[](int row) const { return Ref(cells_, col_, row); }

		private:
			std::vector<std::vector<UnicodeString>> &cells_;
			int col_;
		};

		explicit CellsProperty(std::vector<std::vector<UnicodeString>> *cells) : cells_(cells) {}
		ColumnRef operator[](int col) const { return ColumnRef(*cells_, col); }

	private:
		std::vector<std::vector<UnicodeString>> *cells_;
	};

	CellsProperty Cells{&cells_};

private:
	std::vector<int> col_widths_;
	std::vector<int> row_heights_;
	std::vector<std::vector<UnicodeString>> cells_;	//!< cells_[col][row]
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
/// Vcl.Forms::TMonitorDefaultTo 相当。
/// 実測: usr_hintwin.cpp:52 の `Screen->MonitorFromPoint(..., mdNearest)` のみ
/// (src 全体で mdNearest 以外は 0 箇所)。並びは Delphi の宣言順に合わせてある
enum TMonitorDefaultTo { mdNearest, mdNull, mdPrimary };

/**
 * @brief Vcl.Forms::TMonitor 相当 (最小実装)
 * @details 実測した使われ方は 3 つだけ:
 *            - `->BoundsRect`  : UserFunc.cpp:71 (adjust_form_pos)、
 *                                ModalScr.cpp:29、AppDlg.cpp:502,1372、
 *                                MainFrm.cpp:35770
 *            - `->Left`        : usr_hintwin.cpp:52 (ヒントの左端クランプ)
 *            - `->MonitorNum`  : MainFrm.cpp:19499,20966 (情報表示)
 *          いずれも実モニタの列挙 (EnumDisplayMonitors / GetMonitorInfo) が
 *          要るので、値は宣言のみのゲッタから取る。
 *          TScreen 側の `MonitorFromPoint` / `Monitors[]` / `PrimaryMonitor` は
 *          compat/application.h の担当
 */
class TMonitor : public TObject {
public:
	/// @warning 宣言のみ
	int GetLeft() const;
	/// @warning 宣言のみ
	TRect GetBoundsRect() const;
	/// @warning 宣言のみ (0 起点のモニタ番号)
	int GetMonitorNum() const;

	compat::ROProperty<TMonitor, int, &TMonitor::GetLeft> Left{this};
	compat::ROProperty<TMonitor, TRect, &TMonitor::GetBoundsRect> BoundsRect{this};
	compat::ROProperty<TMonitor, int, &TMonitor::GetMonitorNum> MonitorNum{this};
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

		/// @brief フォームの位置と大きさをまとめて設定する
		/// @details 実測: UserFunc.cpp:78 (adjust_form_pos の
		///          `frm->BoundsRect = frm_rc;`)、UserFunc.cpp:93
		///          (show_ModalDlg)、MainFrm.cpp:35770。
		///          Left / Top / Width / Height は TControl の素のデータメンバ
		///          なので、そこへ展開するのが**そのまま正しい実装**になる
		///          (実 GUI では SetBounds でウィンドウが動くが、シムには
		///          動かすウィンドウが無い)
		BoundsRectProperty &operator=(const TRect &r)
		{
			owner_->Left = r.Left;
			owner_->Top = r.Top;
			owner_->Width = r.Width();
			owner_->Height = r.Height();
			return *this;
		}

		/// @brief 自分の中央に r と同じ大きさの矩形を置いたものを返す
		/// @details 実測: UserFunc.cpp:93 の show_ModalDlg
		///          `dlg->BoundsRect = frm->BoundsRect.CenteredRect(dlg->BoundsRect);`。
		///          TRect::CenteredRect へそのまま委譲する
		TRect CenteredRect(const TRect &r) const { return static_cast<TRect>(*this).CenteredRect(r); }

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

	//-- Phase 3b で InpCmds.cpp から要求されたメンバ ----------------------

	/// @brief モーダルダイアログの結果
	/// @details 実測: src 全体で 165 箇所。フォームのイベントハンドラの中から
	///          `ModalResult = mrOk;` / `= mrCancel;` と修飾なしで書かれる
	///          (InpCmds.cpp:235,238,246 ほか)。
	///
	///          VCL ではこれに代入すると**モーダルループが終了してフォームが
	///          閉じる**。素のデータメンバにするとダイアログが閉じなくなる
	///          という壊れ方を静かに作るので、規約4 に従い宣言のみのゲッタ /
	///          セッタにしてある
	/// @warning 宣言のみ
	TModalResult GetModalResult() const;
	/// @warning 宣言のみ
	void SetModalResult(TModalResult value);
	compat::RWValueProperty<TForm, TModalResult, &TForm::GetModalResult, &TForm::SetModalResult>
		ModalResult{this};

	/// @brief このフォームが載っているモニタ
	/// @details 実測: UserFunc.cpp:70 (adjust_form_pos)、ModalScr.cpp:29、
	///          MainFrm.cpp:19499,20966,35770。
	///          実モニタの特定 (::MonitorFromWindow) が要るので宣言のみ
	/// @warning 宣言のみ
	TMonitor *GetMonitor() const;
	compat::ROProperty<TForm, TMonitor *, &TForm::GetMonitor> Monitor{this};
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
	explicit THintWindow(TComponent *owner = nullptr)
		: TForm(owner), Canvas(new TCanvas()), Brush(new TBrush()) {}
	~THintWindow() override
	{
		delete Canvas;
		delete Brush;
	}

	TCanvas *const Canvas;

	//-- Phase 3b で usr_hintwin.cpp から要求されたメンバ ------------------
	// UsrHintWindow::ActivateHintEx (usr_hintwin.cpp:34-59) が基底のものとして
	// 修飾なしで呼ぶ 4 つ。

	/// 背景色 (usr_hintwin.cpp:56 `Brush->Color = bg_col;`)。
	/// VCL では TControl のプロパティだが、実測で要るのは THintWindow 経由
	/// だけなので Canvas と同じくここに置いた
	TBrush *const Brush;

	/// @brief hint を maxWidth 以内で表示するのに必要な矩形を求める
	/// @details 実測: usr_hintwin.cpp:47 `CalcHintRect(max_w, msg, NULL)`。
	///          第3引数は VCL の `TCustomData` (= Pointer)。
	///          折り返し計算に実フォントの計測が要るので宣言のみ
	/// @warning 宣言のみ
	TRect CalcHintRect(int maxWidth, const UnicodeString &hint, void *data);

	/// @brief 指定矩形にヒントを表示する
	/// @details 実測: usr_hintwin.cpp:57 `ActivateHint(rc, msg)`
	/// @warning 宣言のみ
	void ActivateHint(const TRect &rect, const UnicodeString &hint);

	/// 実描画が無いヘッドレス実行では観測可能な副作用が無いため real no-op
	/// (usr_hintwin.cpp:58)
	void Repaint() {}
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

/**
 * @brief Vcl.Dialogs::TOpenDialog 相当 (ファイルを開くダイアログ)
 * @details 実測 (src/UserMdl.cpp:928-993 ほか): Title / Filter / FilterIndex /
 *          DefaultExt / FileName / InitialDir / Options を設定して
 *          `Execute()` を呼び、真なら FileName を読む、という使い方だけ。
 *
 *          設定値は素のデータメンバでよい (設定しただけでは何も起きないので
 *          静かに壊れる余地が無い)。**`Execute()` だけが宣言のみ** (規約4)。
 *          呼ぶとリンクエラーになるので、ダイアログを出す経路が未移植のまま
 *          動くことはない。
 */
class TOpenDialog : public TCommonDialog {
public:
	UnicodeString Title;
	UnicodeString Filter;
	int FilterIndex = 1;
	UnicodeString DefaultExt;
	UnicodeString FileName;
	UnicodeString InitialDir;
	TStrings *Files = nullptr;	//!< 複数選択時の一覧 (VCL は自動生成。未実装)

	/// @warning 宣言のみ。ダイアログを実際に出す経路なので実装しない
	bool Execute();
};

class TSaveDialog : public TOpenDialog {
};

/// Vcl.Dialogs::TSaveTextFileDialog 相当。
/// `Encodings` は文字コードの選択肢 (実測: src/UserMdl.cpp:59-60 が
/// Clear() してコードページ名を並べる)。VCL と同じくコンストラクタで生成して所有する
class TSaveTextFileDialog : public TSaveDialog {
public:
	TSaveTextFileDialog() : Encodings(new TStringList()) {}
	~TSaveTextFileDialog() override { delete Encodings; }

	TStrings *Encodings;
	int EncodingIndex = 0;
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
/// TFontDialog 相当。`Font` は選択されたフォント (実測: UserMdl.cpp が
/// 設定してから Execute し、真なら読み戻す)
class TFontDialog : public TCommonDialog {
public:
	TFont *Font = nullptr;	//!< @warning VCL は自動生成するがシムは nullptr (報告書 §19)

	/// @warning 宣言のみ
	bool Execute();
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
