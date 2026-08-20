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
UnicodeString KeyMap::KeyStrOf(const wxKeyEvent &event)
{
	// wx の修飾キー状態を VCL の TShiftState に移す。移植済みの
	// get_KeyStr(WORD, TShiftState) をそのまま使えるようにするため
	TShiftState shift;
	if (event.ShiftDown()) shift << ssShift;
	if (event.ControlDown()) shift << ssCtrl;
	if (event.AltDown()) shift << ssAlt;

	// wx の KeyCode は MSW では仮想キーコードと同じ値になる
	const WORD vk = static_cast<WORD>(event.GetKeyCode());
	return get_KeyStr(vk, shift);
}
