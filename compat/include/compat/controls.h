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

//---------------------------------------------------------------------------
/// Vcl.Controls::TAlign 相当 (コントロールの自動配置)
enum TAlign { alNone, alTop, alBottom, alLeft, alRight, alClient, alCustom };

/// Vcl.Controls::TAnchorKind 相当
enum TAnchorKind { akLeft, akTop, akRight, akBottom };
/// TAnchorKind の集合 (Vcl.Controls::TAnchors 相当)
using TAnchors = Set<TAnchorKind, akLeft, akBottom>;

/// Vcl.Controls::TStyleElement 相当 (実測: usr_swatch.cpp が seClient のみ使用)
enum TStyleElement { seFont, seClient, seBorder };
/// TStyleElement の集合 (Vcl.Controls::TStyleElements 相当)
using TStyleElements = Set<TStyleElement, seFont, seBorder>;

/// Vcl.Extctrls::TPanelBevel 相当
enum TPanelBevel { bvNone, bvLowered, bvRaised, bvSpace };

/// Vcl.Forms::TWindowState 相当
enum TWindowState { wsNormal, wsMinimized, wsMaximized };

/// Vcl.Forms::TFormBorderStyle 相当
enum TFormBorderStyle { bsNone, bsSingle, bsSizeable, bsDialog, bsSizeToolWin, bsToolWindow };

/// Vcl.Controls::TCursor 相当 (Win32 の IDC_* とは無関係な Delphi 独自の負値体系。
/// 実測: crDefault / crHourGlass のみ使用)
using TCursor = int;
constexpr TCursor crDefault = 0;
constexpr TCursor crHourGlass = -11;

//---------------------------------------------------------------------------
// System.Rtti 相当の最小限 (UserFunc.h::class_is_CustomEdit が
// `op->InheritsFrom(__classid(TCustomEdit))` の形で使用。実際に呼ばれる
// 経路は無い (未使用の inline 関数) が、型チェックのために用意する)。
// TClass 自体は compat/classes.h (TObject::InheritsFrom と対) で定義済み。
//---------------------------------------------------------------------------
#define __classid(cls) (static_cast<TClass>(nullptr))

//---------------------------------------------------------------------------
// モーダルダイアログの結果 (System.UITypes 相当)。usr_msg.h の
// msgbox_Y_N_C() / msgbox_Retry() の戻り値型として参照される。
// 値は Delphi の mrXxx と同じ (mrNone = 0 で、以降 ID_OK.. の順)。
//---------------------------------------------------------------------------
using TModalResult = int;

constexpr TModalResult mrNone = 0;
constexpr TModalResult mrOk = IDOK;
constexpr TModalResult mrCancel = IDCANCEL;
constexpr TModalResult mrAbort = IDABORT;
constexpr TModalResult mrRetry = IDRETRY;
constexpr TModalResult mrIgnore = IDIGNORE;
constexpr TModalResult mrYes = IDYES;
constexpr TModalResult mrNo = IDNO;
constexpr TModalResult mrClose = IDCLOSE;
constexpr TModalResult mrHelp = IDHELP;
constexpr TModalResult mrTryAgain = IDTRYAGAIN;
constexpr TModalResult mrContinue = IDCONTINUE;
constexpr TModalResult mrAll = mrContinue + 1;
constexpr TModalResult mrNoToAll = mrAll + 1;
constexpr TModalResult mrYesToAll = mrNoToAll + 1;

namespace System {
namespace UITypes {
using ::TModalResult;
}  // namespace UITypes
namespace Classes {
using ::TAlignment;
using ::TShiftState;
using ::TShiftStateEnum;
}  // namespace Classes
}  // namespace System

#endif  // NYANFI_COMPAT_CONTROLS_H
