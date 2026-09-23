/**
 * @file gui/calc.h
 * @brief 電卓の式評価・数値整形 (wx 非依存)
 *
 * @details VCL 版の `src/CalcDlg.cpp` (`TCalculator`) のうち、入力文字列の
 *          正規化、数値/時間/16進数の解析、括弧・演算子優先順位、関数、
 *          定数、単位変換を移植した部分だけをここに置く。wx のダイアログ
 *          (`gui/calc_dialog.*`) はこの API を表示と入力にだけ使う。
 *
 *          呼び出し元は `src/MainFrm.cpp:14039-14064`、コマンド表は
 *          `src/usr_cmdlist.cpp:343` と `592-594`。ユーザー定義定数・関数、
 *          例外処理の浮動小数点マスク、エラー表示中の 1 秒 Sleep、位置/履歴の
 *          永続化は **未移植 (未実装扱い)**。
 */
#ifndef NYANFI_GUI_CALC_H
#define NYANFI_GUI_CALC_H

#include "compat/ustring.h"

namespace calc {

/// 三角関数の入力/出力単位 (TCalculator::AngleMode)。
enum class AngleMode {
	Deg = 0,
	Rad = 1,
	Grad = 2,
};

/// 式評価のオプション。
struct EvalOptions {
	AngleMode angle_mode = AngleMode::Deg;
	int output_digits = 18;
	bool comma_grouping = false;
	/// テストと再現処理用。空なら 現在時刻を使う。
	UnicodeString now;
};

/// 式評価の結果。エラー時は ok=false、error に VCL 相当の理由を入れる。
struct EvalResult {
	bool ok = false;
	UnicodeString value;
	UnicodeString error;
	long double number = 0.0L;
	bool is_time = false;
	bool is_hex = false;
};

/// データ・時間・角度の単位変換で使う単位。
enum class Unit {
	None,
	Byte,
	Kibibyte,
	Mebibyte,
	Gibibyte,
	Tebibyte,
	Second,
	Minute,
	Hour,
	Degree,
	Radian,
	Gradian,
};

/// VCL の IsTimeStr 相当 (`Now` を含む)。
bool IsTimeString(const UnicodeString &value);

/// VCL の IsHexStr 相当。
bool IsHexString(const UnicodeString &value);

/// VCL の IsHexOrInt 相当 (10進整数または16進整数)。
bool IsIntegerLiteral(const UnicodeString &value);

/// 複数行を VCL と同じく `+` で連結し、各行の空白と外側の引用符を外す。
UnicodeString JoinInput(const UnicodeString &value);

/// 数値を VCL の LongDoubleToStr に近い規則で文字列化する。
UnicodeString FormatValue(long double value, bool is_hex = false, bool is_time = false,
                          int output_digits = 18, bool comma_grouping = false);

/// 式を評価する。wx にも VCL のフォームにも依存しない純関数。
EvalResult Evaluate(const UnicodeString &expression, const EvalOptions &options = EvalOptions());

/// 評価結果の数値だけを取り出す補助。エラー時は 0 を返す。
inline long double EvaluateNumber(const UnicodeString &expression,
                                  const EvalOptions &options = EvalOptions())
{
	return Evaluate(expression, options).number;
}

/// 16進/10進表示を切り替える。解析できない場合は入力文字列をそのまま返す。
UnicodeString ToggleHex(const UnicodeString &value);

/// 整数に対する NOT。VCL の NOT アクション相当。
UnicodeString BitwiseNot(long long value);

/// 同じ次元の単位間で変換する。異なる次元なら false。
bool ConvertUnit(long double value, Unit from, Unit to, long double &result);

/// 名前が長い呼び出し向けの別名 (Evaluate と同じ)。
inline EvalResult Eval(const UnicodeString &expression,
                       const EvalOptions &options = EvalOptions())
{
	return Evaluate(expression, options);
}

}  // namespace calc

#endif  // NYANFI_GUI_CALC_H
