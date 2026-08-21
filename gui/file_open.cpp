/**
 * @file gui/file_open.cpp
 * @brief ファイルを開く処理の実装
 *
 * @details 設計上の判断は gui/file_open.h の冒頭コメントを参照。
 */
#include "gui/file_open.h"

#include <shellapi.h>
#include <shlobj.h>

namespace file_open {

//---------------------------------------------------------------------------
bool OpenStandard(const UnicodeString &full_path, UnicodeString &error_out, HWND owner)
{
	SHELLEXECUTEINFOW sei;
	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask  = SEE_MASK_FLAG_DDEWAIT;  // 起動先が起動を完了するまで待つ (DDE 経由の関連付け対策)
	sei.hwnd   = owner;
	sei.lpVerb = L"open";
	sei.lpFile = full_path.c_str();
	sei.nShow  = SW_SHOWNORMAL;

	if (::ShellExecuteExW(&sei)) return true;

	UnicodeString msg;
	msg.sprintf(_T("開けませんでした (エラーコード: %u)"), static_cast<unsigned int>(::GetLastError()));
	error_out = msg;
	return false;
}

//---------------------------------------------------------------------------
/**
 * @details wxWidgets (MSW) は起動時に OLE を初期化済みのことが多いが、
 * 単体テストなど COM 未初期化の環境から呼ばれた場合に備えて、この関数内でも
 * `CoInitializeEx` を試みる。既に (別スレッドモデルで) 初期化済みを示す
 * `RPC_E_CHANGED_MODE` は無視して続行する。
 */
bool OpenWithDialog(const UnicodeString &full_path, UnicodeString &error_out, HWND owner)
{
	const HRESULT co_hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	const bool co_owned = SUCCEEDED(co_hr);  // S_FALSE (二重初期化) も含め、自分が呼んだ分は必ず対応する CoUninitialize を呼ぶ

	OPENASINFO oai;
	ZeroMemory(&oai, sizeof(oai));
	oai.pcszFile      = full_path.c_str();
	oai.pcszClass     = NULL;
	oai.oaifInFlags   = OAIF_EXEC | OAIF_ALLOW_REGISTRATION;

	const HRESULT hr = ::SHOpenWithDialog(owner, &oai);

	if (co_owned) ::CoUninitialize();

	if (SUCCEEDED(hr)) return true;
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return false;  // 利用者がキャンセル (エラー扱いにしない)

	UnicodeString msg;
	msg.sprintf(_T("アプリケーションから開くに失敗しました (HRESULT: 0x%08X)"), static_cast<unsigned int>(hr));
	error_out = msg;
	return false;
}

}  // namespace file_open
