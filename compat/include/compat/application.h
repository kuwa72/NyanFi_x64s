/**
 * @file compat/application.h
 * @brief VCL のグローバル Application (TApplication) の互換シム
 *
 * ロジック層から使われているのは 3 つだけで、いずれも GUI フレームワークに
 * 依存せず Win32 だけで実装できる:
 *   Application->Active           … 自プロセスのウィンドウが前面か (usr_key.cpp:135,271, usr_wic.cpp:54)
 *   Application->ProcessMessages()… メッセージポンプを回す (usr_key.cpp:271)
 *   Application->ExeName          … 実行ファイルのフルパス (usr_migemo.cpp:20)
 *
 * Phase 2 で wxWidgets へ移る際は wxApp / wxTheApp に置き換える。
 */
#ifndef NYANFI_COMPAT_APPLICATION_H
#define NYANFI_COMPAT_APPLICATION_H

#include <memory>
#include <vector>

#include "compat/config.h"
#include "compat/controls.h"
// TScreen::Fonts (TStrings) / DesktopRect (TRect) / TApplication::HintColor (TColor)
// のために実体が要る。classes.h と graphics.h はどちらも application.h を
// 含まないので循環しない (vcl_shim.h では application.h の方が先)
#include "compat/classes.h"
#include "compat/graphics.h"
#include "compat/property.h"
#include "compat/ustring.h"
#include "compat/vcl_forward.h"

//前方宣言 (compat/graphics.h が namespace Graphics に完全定義する。ここではポインタで
//持つだけなので前方宣言で足りる)
namespace Graphics {
class TFont;
}  // namespace Graphics
using ::Graphics::TFont;

/**
 * @brief TApplication 互換 (Phase 0 で必要な最小限)
 */
class TApplication {
public:
	TApplication() = default;

	/// 保留中のウィンドウメッセージを処理する
	void ProcessMessages();

	/// 自プロセスのウィンドウが前面にあるか
	bool GetActive() const;

	/// 実行ファイルのフルパス
	UnicodeString GetExeName() const;

	/// 実行ファイルのファイル名部分 (VCL の Title 相当ではないので参考値)
	bool GetTerminated() const { return terminated_; }
	void SetTerminated(bool value) { terminated_ = value; }

	compat::ROProperty<TApplication, bool, &TApplication::GetActive> Active{this};
	compat::ROProperty<TApplication, UnicodeString, &TApplication::GetExeName> ExeName{this};
	compat::RWValueProperty<TApplication, bool, &TApplication::GetTerminated, &TApplication::SetTerminated>
		Terminated{this};

	/// メインフォーム (UIniFile.cpp / usr_scale.h が ReadScaledInteger 等の既定
	/// コントロール解決に使う。Phase 0/1 では実フォームが無いため常に nullptr
	/// になり得る。呼び出し側は NULL チェックの上で使うこと)
	TForm *MainForm = nullptr;

	/// 既定フォント (usr_scale.cpp::AssignScaledFont が既定値として使用)
	TFont *DefaultFont = nullptr;

	/// ヒント (ツールチップ) を出すか。
	/// 実測: `Application->ShowHint = ShowTooltip;` の代入だけ
	/// (Global.cpp:2071 / OptDlg.cpp:4602)。読む箇所は src に無い。
	/// wx 側は自前でツールチップを扱うので、ここは値を保持するだけ
	bool ShowHint = true;

	/// ヒントの背景色。実測: Global.cpp:11026 が配色表の既定値として1回読むだけ。
	/// VCL の既定は clInfoBk (システムカラー COLOR_INFOBK) なので同じ値にする
	TColor HintColor = static_cast<TColor>(::GetSysColor(COLOR_INFOBK));

	/// ヘルプファイル (.chm) のパス。
	/// 実測: Global.cpp:2182 で `ChangeFileExt(ExeName, ".chm")` を代入し、
	/// 15516 / 15525 で `"ms-its:" + HelpFile` として読む
	UnicodeString HelpFile;

	/// メインウィンドウのハンドル。
	///
	/// 実測: `src/grep_thread.cpp:167` が **ワーカースレッドから
	/// `::SendMessage(Application->MainFormHandle, WM_NYANFI_GREP_END, ...)` で
	/// 完了を通知する**のと、`src/About.cpp:43` がダイアログの親にする 2箇所。
	///
	/// @warning **GUI 側が起動時に代入すること。** VCL は MainForm から自動で
	///          決まるが、シムには実フォームが無いので設定されない。
	///          NULL のままだと `SendMessage(NULL, ...)` が**エラーも出さずに
	///          0 を返す**ため、grep の完了通知が黙って届かなくなる。
	///          `gui/nyanfi_app.cpp` の起動処理で `MainFrame` のハンドルを入れる。
	HWND MainFormHandle = NULL;

	/// 現在アクティブなフォームのウィンドウハンドル。
	/// 実測: Global.cpp:3511 / UserFunc.cpp:1453 / UserMdl.cpp の CloseIME が
	/// **Win32 の HWND としてそのまま渡す**用途だけ。VCL の TForm を辿る必要が
	/// 無いので ::GetActiveWindow() で返す。
	/// 差: VCL は自プロセスのフォームだけを見るが、GetActiveWindow も
	/// 呼び出しスレッドがアタッチされたキューのアクティブウィンドウを返すため
	/// 同等になる (他プロセスのウィンドウは返らない)
	HWND GetActiveFormHandle() const { return ::GetActiveWindow(); }
	compat::ROProperty<TApplication, HWND, &TApplication::GetActiveFormHandle>
		ActiveFormHandle{this};

private:
	bool terminated_ = false;
};

/// VCL のグローバル Application 相当。プロセス内で 1 つ
extern TApplication *Application;

/// C++Builder のグローバル HInstance (自モジュールのインスタンスハンドル)
extern HINSTANCE HInstance;

//---------------------------------------------------------------------------
/**
 * @brief Vcl.Forms::TScreen 相当 (最小実装)
 * @details 実測: UIniFile.cpp の LoadFormPos/LoadPosInfo (Width/Height/
 *          MonitorCount によるフォーム位置補正)、usr_scale.h の GetCurPPI
 *          (ActiveForm)、UserFunc.h::cursor_HourGlass/cursor_Default (Cursor)。
 *          Width/Height は ::GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) から
 *          実測する。MonitorCount は ::GetSystemMetrics(SM_CMONITORS)。
 *          ActiveForm / Cursor は実フォーム/実カーソルが無い Phase 0/1 では
 *          設定されない (既定値のまま)。
 */
class TScreen {
public:
	int GetWidth() const { return ::GetSystemMetrics(SM_CXSCREEN); }
	int GetHeight() const { return ::GetSystemMetrics(SM_CYSCREEN); }
	int GetMonitorCount() const { return ::GetSystemMetrics(SM_CMONITORS); }

	compat::ROProperty<TScreen, int, &TScreen::GetWidth> Width{this};
	compat::ROProperty<TScreen, int, &TScreen::GetHeight> Height{this};
	compat::ROProperty<TScreen, int, &TScreen::GetMonitorCount> MonitorCount{this};

	/// アクティブフォーム (Phase 0/1 には実フォームが無いため常に nullptr)
	TForm *ActiveForm = nullptr;
	/// マウスカーソル形状 (実際の描画は行わない。UserFunc.h::cursor_HourGlass 等が
	/// 読み書きするだけの値として使う)
	TCursor Cursor = crDefault;

	/// 仮想デスクトップ全体の矩形 (マルチモニタを含む)。
	/// 実測: Global.cpp:14722 と ModalScr.cpp:26 がウィンドウ位置の
	/// クランプに使う。`SM_XVIRTUALSCREEN` 系から作る
	TRect GetDesktopRect() const
	{
		const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
		const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
		return TRect(x, y,
		             x + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
		             y + ::GetSystemMetrics(SM_CYVIRTUALSCREEN));
	}
	compat::ROProperty<TScreen, TRect, &TScreen::GetDesktopRect> DesktopRect{this};

	/// インストール済みフォント名の一覧。
	/// 実測: Global.cpp:5979 の `Screen->Fonts->IndexOf(font_name)` 1箇所だけ
	/// (指定フォントが存在するかの判定)。EnumFontFamiliesEx で作って保持する
	TStrings *GetFonts() const;
	compat::ROProperty<TScreen, TStrings *, &TScreen::GetFonts> Fonts{this};

	/// 生成済みフォームの一覧。
	/// **登録は行わない** (Phase 3 で実フォームを作るまで常に空)。
	/// 実測: AppDlg.cpp / Global.cpp が `for (i<FormCount) Forms[i]` で
	/// 全フォームを走査する用途だけ。空なら走査が0回になるだけで害が無い。
	/// 規約4 の「呼んだら落とす」ではなく空を返すのは、走査そのものは
	/// 正常系だから (フォームが無い状態と区別がつかないのは承知の上)
	int GetFormCount() const { return static_cast<int>(forms_.size()); }
	TForm *GetFormAt(int index)
	{
		return (index >= 0 && index < static_cast<int>(forms_.size()))? forms_[index] : nullptr;
	}
	void PutFormAt(int index, TForm *form)
	{
		if (index >= 0 && index < static_cast<int>(forms_.size())) forms_[index] = form;
	}

	compat::ROProperty<TScreen, int, &TScreen::GetFormCount> FormCount{this};
	compat::IndexedPtrProperty<TScreen, TForm, &TScreen::GetFormAt, &TScreen::PutFormAt> Forms{this};

private:
	std::vector<TForm *> forms_;
	mutable std::unique_ptr<TStrings> fonts_;
};

/// VCL のグローバル Screen 相当。プロセス内で 1 つ
extern TScreen *Screen;

#endif  // NYANFI_COMPAT_APPLICATION_H
