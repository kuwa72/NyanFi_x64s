/**
 * @file application.cpp
 * @brief TApplication 互換シムの実装
 */
#include "compat/application.h"

//---------------------------------------------------------------------------
namespace {
TApplication g_application;
TScreen g_screen;
}  // namespace

TApplication *Application = &g_application;
TScreen *Screen = &g_screen;

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

//---------------------------------------------------------------------------
/**
 * @brief インストール済みフォント名の一覧
 * @details 実測: src/Global.cpp:5979 の `Screen->Fonts->IndexOf(font_name)` が
 *          唯一の呼び出しで、「指定されたフォントが存在するか」の判定に使う。
 *          VCL の TScreen.Fonts と同じく **列挙は初回だけ**行い、以後は
 *          保持したリストを返す (VCL も起動時に1回だけ列挙する)。
 *
 *          IndexOf が使えるよう、`Sorted` にはしない代わりに大文字小文字を
 *          無視する必要は無い: 呼び出し側 (Global.cpp) は ini に保存された
 *          フォント名をそのまま渡すため、VCL と同じ完全一致でよい。
 */
TStrings *TScreen::GetFonts() const
{
	if (fonts_) return fonts_.get();

	fonts_.reset(new TStringList());

	struct Ctx {
		TStrings *lst;
	} ctx{fonts_.get()};

	HDC dc = ::GetDC(NULL);
	LOGFONTW lf = {};
	lf.lfCharSet = DEFAULT_CHARSET;

	::EnumFontFamiliesExW(
		dc, &lf,
		[](const LOGFONTW *plf, const TEXTMETRICW *, DWORD, LPARAM param) -> int {
			Ctx *c = reinterpret_cast<Ctx *>(param);
			// '@' で始まるのは縦書き用の別名なので VCL と同じく除く
			if (plf->lfFaceName[0] != L'@') {
				const UnicodeString name(plf->lfFaceName);
				if (c->lst->IndexOf(name) == -1) c->lst->Add(name);
			}
			return 1;
		},
		reinterpret_cast<LPARAM>(&ctx), 0);

	::ReleaseDC(NULL, dc);
	return fonts_.get();
}
