/**
 * @file compat/math.h
 * @brief System.Math 相当の互換シム
 *
 * 対象コード (src/CalcDlg.cpp, src/DrvGraph.cpp など) での実測: Sign 1 /
 * Ceil 1 / Floor 2。Phase 0 対象の 15 ファイル (usr_*.cpp 等) では実際には
 * 未使用だったが、System.Math は vcl_shim.h 経由で全翻訳単位に見えるため
 * 契約として一式実装する。
 *
 * 依存: compat/config.h のみ。
 */
#ifndef NYANFI_COMPAT_MATH_H
#define NYANFI_COMPAT_MATH_H

#include "compat/config.h"
#include "compat/set.h"

//---------------------------------------------------------------------------
// 定数
//---------------------------------------------------------------------------
constexpr int MaxInt = 2147483647;
constexpr int MinInt = -2147483647 - 1;  //!< シム独自の追加。実 Delphi の System 単位には無いが
                                          //!< MaxInt との対比で契約に含める (report 参照)

//---------------------------------------------------------------------------
// 比較
//---------------------------------------------------------------------------
/// Delphi の Math.Max / Math.Min は Integer/Int64/Double の個別オーバーロード
/// だが、シムでは汎用テンプレートで代替する (対象コードでの直接呼び出しは無し)。
template <class T>
constexpr T Max(T a, T b)
{
	return (a > b) ? a : b;
}

template <class T>
constexpr T Min(T a, T b)
{
	return (a < b) ? a : b;
}

//---------------------------------------------------------------------------
// 指数・対数
//---------------------------------------------------------------------------
double Power(double base, double exponent);
double Log10(double x);
double Log2(double x);

//---------------------------------------------------------------------------
// 符号 / ゼロ判定
//---------------------------------------------------------------------------
/// Delphi の TValueSign (-1 / 0 / 1) 相当
template <class T>
constexpr int Sign(T value)
{
	return (value > T(0)) - (value < T(0));
}

/// Epsilon<=0 の場合は厳密な 0 判定 (Delphi の IsZero(A, 0) と同じ)
bool IsZero(double value, double epsilon = 0.0);

//---------------------------------------------------------------------------
// 丸め
//---------------------------------------------------------------------------
/// 対象コードでの直接呼び出しは無いため、RoundTo / SimpleRoundTo は同一の
/// 単純な実装 (value * 10^-digit を四捨五入して戻す) にしている。実 RTL の
/// RoundTo は誤差テーブルを使うより精密な実装だが、Phase 0 では簡略化する。
double RoundTo(double value, int digit);
double SimpleRoundTo(double value, int digit = -2);

//---------------------------------------------------------------------------
// 天井 / 床
//---------------------------------------------------------------------------
Int64 Ceil(double value);
Int64 Floor(double value);

//---------------------------------------------------------------------------
// 浮動小数点例外のマスク (System.Math)
//
// usr_shell.h:168 が `TFPUExceptionMask FpuTmpMask;` をメンバに持つ。
// シェル拡張の呼び出し前後で FPU 例外を抑止するために使われる。
//---------------------------------------------------------------------------
/// 浮動小数点例外の種類
enum TFPUException { exInvalidOp, exDenormalized, exZeroDivide, exOverflow, exUnderflow, exPrecision };

/// TFPUException の集合
using TFPUExceptionMask = Set<TFPUException, exInvalidOp, exPrecision>;

TFPUExceptionMask GetExceptionMask();               //!< 現在のマスクを取得
TFPUExceptionMask SetExceptionMask(TFPUExceptionMask mask);  //!< マスクを設定し、直前の値を返す

#endif  // NYANFI_COMPAT_MATH_H
