/**
 * @file gui/key_map_wx.cpp
 * @brief KeyMap::KeyStrOf() (wx のキーイベント → NyanFi のキー名) の実装
 *
 * @details KeyMap のそれ以外のメンバ (ini 読み込みや割り当て表そのもの) は
 *          wx に依存しないため gui/key_map.cpp に置き、wx を必要とする
 *          この関数だけを分けてある。理由は gui/CMakeLists.txt と
 *          CMakeLists.txt (ルート) の `nyanfi_gui_core` の説明を参照。
 */
#include "gui/key_map.h"

#include <wx/wx.h>

#include "usr_key.h"

//---------------------------------------------------------------------------
// gui/key_map.cpp の VkFromWxKeyCode() は wx をリンクせずに単体テストできるよう
// wxKeyCode の値を数値で書き写している。本物とずれていないかをここで確認する。
// wx を更新して値が変わったら、この static_assert でコンパイルが止まる。
// (メッセージが英語なのは static_assert が narrow リテラルしか取れないため。
//  コンパイル時にしか出ないので ACP 依存の実害は無いが、規約1 の機械チェックに
//  例外を作らないよう ASCII にしてある)
//---------------------------------------------------------------------------
static_assert(WXK_START    == 300,           "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_PAUSE    == WXK_START + 10, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_END      == WXK_START + 12, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_HOME     == WXK_START + 13, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_LEFT     == WXK_START + 14, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_UP       == WXK_START + 15, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_RIGHT    == WXK_START + 16, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_DOWN     == WXK_START + 17, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_INSERT   == WXK_START + 22, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_NUMPAD0  == WXK_START + 24, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_NUMPAD9  == WXK_START + 33, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_MULTIPLY == WXK_START + 34, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_ADD      == WXK_START + 35, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_SUBTRACT == WXK_START + 37, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_DECIMAL  == WXK_START + 38, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_DIVIDE   == WXK_START + 39, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_F1       == WXK_START + 40, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_F12      == WXK_START + 51, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_PAGEUP   == WXK_START + 66, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_PAGEDOWN == WXK_START + 67, "wxKeyCode value changed; update VkFromWxKeyCode");
static_assert(WXK_DELETE   == 127,            "wxKeyCode value changed; update VkFromWxKeyCode");

//---------------------------------------------------------------------------
UnicodeString KeyMap::KeyStrOf(const wxKeyEvent &event)
{
	// wx の修飾キー状態を VCL の TShiftState に移す。移植済みの
	// get_KeyStr(WORD, TShiftState) をそのまま使えるようにするため
	TShiftState shift;
	if (event.ShiftDown()) shift << ssShift;
	if (event.ControlDown()) shift << ssCtrl;
	if (event.AltDown()) shift << ssAlt;

	// MSW では GetRawKeyCode() が WM_KEYDOWN の wParam = 仮想キーコードその物に
	// なる (wx 3.3.3 src/msw/window.cpp の MSWInitAnyKeyEvent で
	// `event.m_rawCode = (wxUint32) wParam;`。wxEVT_CHAR_HOOK もこれを通る)。
	// VCL 版の TForm::OnKeyDown が受け取る値と同じなので、get_KeyStr() には
	// これを渡すのが正しい。
	//
	// GetKeyCode() を渡してはいけない。あれは仮想キーコードではなく、矢印や
	// F キーは WXK_START (300) からの連番になる。英数字だけ偶然 VK と同値なので
	// 「G は効くのに上下キーが無反応」という壊れ方をする (報告書 §16.5)。
	WORD vk = static_cast<WORD>(event.GetRawKeyCode());

	// raw code が取れない環境向けの保険 (MSW では通らない)。OEM キーは
	// wx が ASCII に畳んでしまっているので戻せない
	if (vk == 0) vk = VkFromWxKeyCode(event.GetKeyCode());

	return get_KeyStr(vk, shift);
}
