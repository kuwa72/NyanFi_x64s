/**
 * @file gui/panel_state.cpp
 * @brief gui/panel_state.h の実装 (wx 非依存)
 */
#include "gui/panel_state.h"

#include "usr_str.h"

namespace panel_state {

//---------------------------------------------------------------------------
bool HasToken(const UnicodeString &param, const UnicodeString &token)
{
	if (param.IsEmpty() || token.IsEmpty()) return false;
	TStringDynArray lst = split_strings_semicolon(param);
	for (int i = 0; i < lst.Length; i++) {
		if (SameText(token, lst[i])) return true;
	}
	return false;
}

//---------------------------------------------------------------------------
int NextIconModeFD(int current)
{
	if (current == 0) return 1;
	if (current == 1) return 2;
	return 1;
}

//---------------------------------------------------------------------------
int ToggleIconMode(int current, view_settings::Toggle how)
{
	const bool shown = (current > 0);
	return view_settings::ApplyToggle(shown, how) ? 1 : 0;
}

//---------------------------------------------------------------------------
int ScrollLogIndex(int current, int total, int lines, bool down)
{
	if (total <= 0) return -1;
	if (lines < 1) lines = 1;
	const int next = current + (down ? lines : -lines);
	if (next < 0) return 0;
	if (next > total - 1) return total - 1;
	return next;
}

}  // namespace panel_state
