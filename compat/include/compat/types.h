/**
 * @file compat/types.h
 * @brief System.Types 相当の互換シム (TValueRelationship とその定数)
 *
 * Delphi の `TValueRelationship = -1..1` は System.Types 単位で宣言されて
 * おり、C++Builder の System.Types.hpp では `System::Int8` の別名になる。
 * これを返すのは System.Math の `CompareValue` と System.DateUtils の
 * `CompareDate` / `CompareDateTime` / `CompareTime` の両方なので、
 * compat/datetime.h と compat/math.h の共通の土台として切り出す
 * (vcl_shim.h の取り込み順は datetime.h → math.h で、math.h に置くと
 * datetime.h から使えない)。
 *
 * 実呼び出し箇所 (grep 実測):
 *   TValueRelationship 4 / EqualsValue 8 / GreaterThanValue 5 /
 *   LessThanValue 4 (src/Global.cpp, src/UserFunc.cpp, src/task_thread.cpp,
 *   src/usr_excmd.cpp, src/MainFrm.cpp)。いずれも Compare* の戻り値との
 *   == / != 比較にしか使っていない。
 *
 * 依存: compat/config.h のみ。
 */
#ifndef NYANFI_COMPAT_TYPES_H
#define NYANFI_COMPAT_TYPES_H

#include "compat/config.h"

//---------------------------------------------------------------------------
// 比較結果 (System.Types)
//---------------------------------------------------------------------------
/**
 * @brief Delphi の TValueRelationship (-1 / 0 / 1)
 * @details 実 C++Builder の System.Types.hpp と同じく Int8 (signed char) の
 *          別名にしている。int にはしない: 呼び出し側は == / != でしか使って
 *          いないので幅は影響しないが、実物と型が変わると
 *          `IntToStr(res)` のような後からの追加でオーバーロード解決が
 *          C++Builder と食い違う恐れがあるため (規約2 と同じ理由)。
 */
using TValueRelationship = Int8;

constexpr TValueRelationship LessThanValue = static_cast<TValueRelationship>(-1);
constexpr TValueRelationship EqualsValue = static_cast<TValueRelationship>(0);
constexpr TValueRelationship GreaterThanValue = static_cast<TValueRelationship>(1);

//---------------------------------------------------------------------------
namespace System {
namespace Types {
using ::EqualsValue;
using ::GreaterThanValue;
using ::LessThanValue;
using ::TValueRelationship;
}  // namespace Types
using namespace Types;
}  // namespace System

#endif  // NYANFI_COMPAT_TYPES_H
