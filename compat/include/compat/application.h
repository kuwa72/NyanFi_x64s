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
#include "compat/property.h"
#include "compat/ustring.h"

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

private:
	bool terminated_ = false;
};

/// VCL のグローバル Application 相当。プロセス内で 1 つ
extern TApplication *Application;

/// C++Builder のグローバル HInstance (自モジュールのインスタンスハンドル)
extern HINSTANCE HInstance;

#endif  // NYANFI_COMPAT_APPLICATION_H
