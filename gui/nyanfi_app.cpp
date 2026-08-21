/**
 * @file gui/nyanfi_app.cpp
 * @brief アプリケーションのエントリポイント
 *
 * 本フォークの存在理由は「ライト/ダークモード表示」だった (README)。VCL 版は
 * VCL Styles で実現していたが、wxWidgets 3.3 は Windows のダークモードを正式に
 * サポートしているので、`MSWEnableDarkMode()` を呼ぶだけでよい。
 * 各ペインの配色も wxSystemSettings 由来なので自動で追従する。
 */
#include <wx/wx.h>

#include "gui/main_frame.h"

/**
 * @brief NyanFi (wx 版) のアプリケーション
 */
class NyanFiApp : public wxApp {
public:
	bool OnInit() override
	{
		if (!wxApp::OnInit()) return false;

#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)
		// Windows のライト/ダーク設定に追従させる。
		// これは wxWidgets 3.3 で正式サポートされた API で、本フォークが VCL Styles で
		// 実現していたライト/ダーク切替の置き換えにあたる (issue #1 の GUI 選定理由)。
		MSWEnableDarkMode();
#endif

		MainFrame *frame = new MainFrame();
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(NyanFiApp);
