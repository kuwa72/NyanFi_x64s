/**
 * @file gui/view_state.cpp
 * @brief 表示切り替えの計算 (根拠と VCL の該当行は gui/view_state.h を参照)
 */
#include "gui/view_state.h"

namespace view_state {

//---------------------------------------------------------------------------
double ClampRatio(double ratio)
{
	if (ratio < kMinRatio) return kMinRatio;
	if (ratio > kMaxRatio) return kMaxRatio;
	return ratio;
}

//---------------------------------------------------------------------------
double MoveBorder(double ratio, int direction)
{
	if (direction == 0) return ClampRatio(ratio);
	return ClampRatio(ratio + (direction < 0? -kBorderStep : kBorderStep));
}

//---------------------------------------------------------------------------
double WidenSide(bool widen_left, double share)
{
	return ClampRatio(widen_left? share : 1.0 - share);
}

//---------------------------------------------------------------------------
bool IsListedByAttr(int attr, bool show_hidden, bool show_system)
{
	if (!show_hidden && (attr & faHidden) != 0) return false;
	if (!show_system && (attr & faSysFile) != 0) return false;
	return true;
}

}  // namespace view_state
