/**
 * @file compat/controls.h
 * @brief Vcl.Controls / System.Classes の入力系の型 (TShiftState など)
 *
 * usr_key.cpp がキー入力の修飾キー状態に Delphi の集合型を使っている:
 *   TShiftState shift;
 *   if (HIBYTE(::GetAsyncKeyState(VK_SHIFT))) shift << ssShift;
 *   if (Shift.Contains(ssShift)) ...
 * GUI とは独立に成立する型なので Phase 0 で実装しておく。
 */
#ifndef NYANFI_COMPAT_CONTROLS_H
#define NYANFI_COMPAT_CONTROLS_H

#include "compat/config.h"
#include "compat/set.h"

/// 修飾キー・マウスボタンの状態 (Delphi の TShiftStateEnum)
enum TShiftStateEnum {
	ssShift,
	ssAlt,
	ssCtrl,
	ssLeft,
	ssRight,
	ssMiddle,
	ssDouble,
	ssTouch,
	ssPen,
	ssCommand,
	ssHorizontal
};

/// TShiftStateEnum の集合 (Delphi の TShiftState)
using TShiftState = Set<TShiftStateEnum, ssShift, ssHorizontal>;

/// マウスボタン
enum TMouseButton { mbLeft, mbRight, mbMiddle };

/// 文字列の配置
enum TAlignment { taLeftJustify, taRightJustify, taCenter };

namespace System {
namespace Classes {
using ::TAlignment;
using ::TShiftState;
using ::TShiftStateEnum;
}  // namespace Classes
}  // namespace System

#endif  // NYANFI_COMPAT_CONTROLS_H
