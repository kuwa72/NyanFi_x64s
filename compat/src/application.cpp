/**
 * @file application.cpp
 * @brief TApplication 互換シムの実装
 */
#include "compat/application.h"

//---------------------------------------------------------------------------
namespace {
TApplication g_application;
}  // namespace

TApplication *Application = &g_application;

HINSTANCE HInstance = ::GetModuleHandleW(nullptr);

//---------------------------------------------------------------------------
void TApplication::ProcessMessages()
{
	MSG msg;
	while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			terminated_ = true;
			break;
		}
		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);
	}
}

//---------------------------------------------------------------------------
bool TApplication::GetActive() const
{
	// VCL の Application->Active は「自アプリのウィンドウがアクティブか」。
	// 前面ウィンドウの所有プロセスが自分かどうかで判定する。
	const HWND fore = ::GetForegroundWindow();
	if (fore == nullptr) return false;

	DWORD pid = 0;
	::GetWindowThreadProcessId(fore, &pid);
	return pid == ::GetCurrentProcessId();
}

//---------------------------------------------------------------------------
UnicodeString TApplication::GetExeName() const
{
	wchar_t buf[MAX_PATH * 4] = {0};
	const DWORD len = ::GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
	return UnicodeString(buf, static_cast<int>(len));
}
