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

#include <cmath>

#include "compat/config.h"
#include "compat/set.h"
#include "compat/types.h"

//---------------------------------------------------------------------------
// C ランタイムの数学定数 (M_PI ほか)
//
// C++Builder の math.h は M_PI 等を無条件に定義していたため、既存コードは
// 宣言なしで使っている (src/Global.cpp:10465,10468,15552-15555 /
// src/Splash.cpp:41 / src/CalcDlg.cpp:298,309 の計 8箇所)。
//
// mingw-w64 の math.h はこれらを `_USE_MATH_DEFINES` が定義されている場合に
// しか出さない。compat/config.h が先に <math.h> を取り込んでしまっており、
// あとから _USE_MATH_DEFINES を定義して再インクルードしても
// インクルードガードで弾かれる (実際に確認した) ので、ここで定義する。
// 値は C99 / glibc / MSVC と同じもの。
//---------------------------------------------------------------------------
#ifndef M_E
#	define M_E 2.71828182845904523536
#endif
#ifndef M_LOG2E
#	define M_LOG2E 1.44269504088896340736
#endif
#ifndef M_LOG10E
#	define M_LOG10E 0.434294481903251827651
#endif
#ifndef M_LN2
#	define M_LN2 0.693147180559945309417
#endif
#ifndef M_LN10
#	define M_LN10 2.30258509299404568402
#endif
#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#	define M_PI_2 1.57079632679489661923
#endif
#ifndef M_PI_4
#	define M_PI_4 0.785398163397448309616
#endif
#ifndef M_1_PI
#	define M_1_PI 0.318309886183790671538
#endif
#ifndef M_2_PI
#	define M_2_PI 0.636619772367581343076
#endif
#ifndef M_2_SQRTPI
#	define M_2_SQRTPI 1.12837916709551257390
#endif
#ifndef M_SQRT2
#	define M_SQRT2 1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#	define M_SQRT1_2 0.707106781186547524401
#endif

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

/// 非数か (System.Math::IsNan)。
/// 実測: src/InspectFrm.cpp:224,250 が `IsNan(v)? "NAN" : ...` と表示に使う
inline bool IsNan(double value) { return std::isnan(value); }
/// 無限大か (System.Math::IsInfinite)。
/// 実測: src/CalcDlg.cpp:262 が計算結果の妥当性チェックに使う
inline bool IsInfinite(double value) { return std::isinf(value); }

//---------------------------------------------------------------------------
// 値の比較 (System.Math) — 戻り値は System.Types の TValueRelationship
//
// **src/ での直接呼び出しは 0 件** (grep で確認)。src が使っているのは
// System.DateUtils の CompareDate / CompareDateTime / CompareTime だけで、
// TValueRelationship と EqualsValue 等の定数はそちらの戻り値の受け手として
// 出てくる。ここでは System.Math の契約として一式そろえておく。
//---------------------------------------------------------------------------
/// Delphi の DoubleResolution (1E-15 * FuzzFactor(1000))
constexpr double DoubleResolution = 1.0E-12;

/// 誤差を許した等値判定。epsilon<=0 のときは Delphi と同じく
/// `Max(Min(|a|,|b|) * DoubleResolution, DoubleResolution)` を使う
bool SameValue(double a, double b, double epsilon = 0.0);

TValueRelationship CompareValue(int a, int b);
TValueRelationship CompareValue(Int64 a, Int64 b);
TValueRelationship CompareValue(double a, double b, double epsilon = 0.0);

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

//---------------------------------------------------------------------------
// 乱数
//
// 正確には Delphi の System 単位 (System.hpp) の関数だが、compat 側に
// System 単位そのものに対応するヘッダが無いため、数値まわりということで
// ここに置く。System.Math.hpp / System.hpp のどちらから来ても vcl_shim.h が
// 全部取り込むので、呼び出し側からは区別できない。
//
// 実呼び出し箇所 (grep 実測):
//   Randomize 2 (src/Global.cpp:1178 / src/task_thread.cpp:1113)
//   Random(n) 5 (src/Global.cpp:11386 / src/usr_excmd.cpp:1494,1496 /
//                src/task_thread.cpp:1134,1153,1154)
//
// **スレッド安全性はシム独自の判断**: src/task_thread.cpp はワーカースレッドの
// 中で Randomize / Random を呼ぶ。Delphi の RandSeed はプロセス共通の
// グローバル変数でスレッド安全ではないが、ここでは乱数エンジンを
// thread_local にしてデータ競合を避ける。Randomize() は**呼んだスレッドの
// エンジンだけ**を再シードする。乱数列そのものは Delphi の LCG とは
// 一致しない (一致させる必要のある用途は src に無い)。
//---------------------------------------------------------------------------
/// 呼び出したスレッドの乱数エンジンを現在時刻などで再シードする
void Randomize();

/**
 * @brief 0 以上 range 未満の一様乱数
 * @param range 上限 (含まない)
 * @return [0, range) の整数。range<=0 のときは 0
 *         (Delphi では未定義動作。ここは落ちない方を選んだ)
 */
int Random(int range);

#endif  // NYANFI_COMPAT_MATH_H
