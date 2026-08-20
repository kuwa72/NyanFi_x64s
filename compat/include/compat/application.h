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

#include "compat/config.h"
#include "compat/controls.h"
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
};

/// VCL のグローバル Screen 相当。プロセス内で 1 つ
extern TScreen *Screen;

#endif  // NYANFI_COMPAT_APPLICATION_H
