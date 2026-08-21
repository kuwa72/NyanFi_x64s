/**
 * @file gui/vcl_gui_bridge.cpp
 * @brief 移植済みロジック層が参照する VCL GUI シンボルの橋渡し
 *
 * ロジック層のうち数ファイルには、GUI コントロールを引数に取る関数が混ざって
 * いる (`get_WidthInPanel(TPanel*)`、`perform_Key(TControl*)`、
 * `assign_KeyList(TComboBox*)` など)。`compat/gui_stubs.h` はそれらの型を
 * 宣言だけで持っているので、**呼ばなければ**リンクは通る — と思いたいところだが、
 * GNU ld のセクション回収はシンボル解決の後に走るため、未使用の関数からの参照でも
 * リンクエラーになる。そのためここで定義を与える。
 *
 * 方針は 2 通りに分けてある。
 *
 *   1. 意味を変えずに無害化できるもの → そのまま実装する
 *      LockDrawing / UnlockDrawing は再描画の抑止 (最適化) にすぎないので何もしない。
 *
 *   2. 実装が必要だが、まだ移植していないもの → 呼ばれたら確実に落とす
 *      静かに誤った値を返すより、呼ばれたことが分かる方がよい。Phase 2/3 で
 *      対応する wx のウィジェットができた時点で、ここを置き換える。
 *
 * つまりこのファイルの「2.」の一覧が、GUI 移植の残作業そのものになっている。
 */
#include <stdexcept>

#include <wx/wx.h>

//---------------------------------------------------------------------------
// 1. 無害化できるもの
//---------------------------------------------------------------------------

/// 再描画の抑止。VCL では WM_SETREDRAW のラッパで、省いても表示は正しい
void TWinControl::LockDrawing()
{
}

/// 再描画の再開
void TWinControl::UnlockDrawing()
{
}

/// Direct2D で描けるか。wx 版では使わないので常に false
/// (これが false の間、usr_str.cpp の D2D 経路には入らない)
bool TDirect2DCanvas::Supported()
{
	return false;
}

//---------------------------------------------------------------------------
// 2. 未移植 (呼ばれたら落とす)
//---------------------------------------------------------------------------

namespace {

[[noreturn]] void not_ported(const char *what)
{
	// GUI の移植が済んでいない経路に入った。静かに誤動作させない
	wxLogError(_T("%s は wx 版で未実装です"), what);
	throw std::logic_error(what);
}

}  // namespace

/// メッセージ送出。移植するなら wxWindow の HWND に対する ::SendMessage になる
NativeInt TControl::Perform(unsigned, NativeInt, NativeInt)
{
	not_ported("TControl::Perform");
}

/// Direct2D キャンバス。Supported() が false なので到達しない
TDirect2DCanvas::TDirect2DCanvas(HDC, const TRect &)
{
	not_ported("TDirect2DCanvas");
}

/// フォームが表示モニタからはみ出していたら収まるように調整 (UserFunc.cpp)。
/// src/UIniFile.cpp の LoadPosInfo(TForm*, ...) から参照されるが、gui/settings.cpp
/// (Settings) は TForm を使わない独自の永続化なのでこの経路は呼ばれない。
/// UIniFile.cpp を (LoadPosInfo 以外の目的で) リンクに含めた時点で GNU ld が
/// この未定義参照を要求するため、ここで「未移植」として明示する。
void adjust_form_pos(TForm *)
{
	not_ported("adjust_form_pos");
}
