/**
 * @file gui/text_display.cpp
 * @brief gui/text_display.h の実装 (wx 非依存)
 */
#include "gui/text_display.h"

namespace text_display {

//---------------------------------------------------------------------------
bool ToggleValue(bool cur, const UnicodeString &param)
{
	if (SameText(param, _T("ON"))) return true;
	if (SameText(param, _T("OFF"))) return false;
	return !cur;
}

//---------------------------------------------------------------------------
int ParseTabWidth(const UnicodeString &param, int fallback)
{
	const int v = param.ToIntDef(fallback);
	if (v < 0) return 0;
	if (v > kMaxTabWidth) return kMaxTabWidth;
	return v;
}

//---------------------------------------------------------------------------
int ParseFoldWidth(const UnicodeString &param, int fallback)
{
	const int v = param.ToIntDef(fallback);
	if (v < 0) return 0;
	if (v > kMaxFoldWidth) return kMaxFoldWidth;
	return v;
}

//---------------------------------------------------------------------------
int ParseMargin(const UnicodeString &param, int fallback)
{
	const int v = param.ToIntDef(fallback);
	if (v < 0) return 0;
	if (v > kMaxMargin) return kMaxMargin;
	return v;
}

//---------------------------------------------------------------------------
int ParseScrollLines(const UnicodeString &param, int fallback)
{
	const int v = param.ToIntDef(fallback);
	if (v < 1) return 1;
	return v;
}

//---------------------------------------------------------------------------
TailParam ParseTailParam(const UnicodeString &param)
{
	TailParam out;
	UnicodeString buf = param;
	// VCL の remove_top_text(ActionParam, "R") と同じく、先頭の R を逆順印に取る
	if (StartsText(_T("R"), buf)) {
		out.reverse = true;
		buf.Delete(1, 1);
	}
	out.limit_lines = buf.ToIntDef(kDefaultTailLines);
	if (out.limit_lines < 1) out.limit_lines = 1;
	return out;
}

}  // namespace text_display
