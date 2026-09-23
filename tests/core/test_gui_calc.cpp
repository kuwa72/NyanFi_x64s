/**
 * @file tests/core/test_gui_calc.cpp
 * @brief gui/calc.cpp の wx 非依存式評価テスト
 */
#include "doctest/doctest.h"

#include <cmath>

#include "gui/calc.h"

namespace {

bool near_value(const calc::EvalResult &result, long double expected)
{
	return result.ok && std::fabs(static_cast<double>(result.number - expected)) < 1e-9;
}

}  // namespace

TEST_CASE("Calc: 四則演算と優先順位")
{
	calc::EvalResult r = calc::Evaluate(_T("2 + 3 * 4"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 14.0L));

	r = calc::Evaluate(_T("(2 + 3) * 4"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 20.0L));

	// VCL と同じく ^ は左結合
	r = calc::Evaluate(_T("2 ^ 3 ^ 2"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 64.0L));
}

TEST_CASE("Calc: 数値リテラルと16進数")
{
	CHECK(calc::IsHexString(_T("0xFF")));
	CHECK(calc::IsHexString(_T("0Xabc")));
	CHECK_FALSE(calc::IsHexString(_T("FF")));
	CHECK(calc::IsIntegerLiteral(_T("-12,345")));
	CHECK_FALSE(calc::IsIntegerLiteral(_T("1.5")));

	calc::EvalResult r = calc::Evaluate(_T("1,000 + 2"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 1002.0L));
	CHECK(r.value == UnicodeString(_T("1,002")));

	r = calc::Evaluate(_T("0xA + 0x2"));
	REQUIRE(r.ok);
	CHECK(r.is_hex);
	CHECK(r.value == UnicodeString(_T("0xc")));
}

TEST_CASE("Calc: 三角関数の角度単位")
{
	calc::EvalOptions deg;
	deg.angle_mode = calc::AngleMode::Deg;
	calc::EvalResult r = calc::Evaluate(_T("SIN(30)"), deg);
	REQUIRE(r.ok);
	CHECK(std::fabs(static_cast<double>(r.number - 0.5L)) < 1e-12);

	calc::EvalOptions rad;
	rad.angle_mode = calc::AngleMode::Rad;
	r = calc::Evaluate(_T("SIN(PI / 2)"), rad);
	REQUIRE(r.ok);
	CHECK(std::fabs(static_cast<double>(r.number - 1.0L)) < 1e-12);

	calc::EvalOptions grad;
	grad.angle_mode = calc::AngleMode::Grad;
	r = calc::Evaluate(_T("SIN(100)"), grad);
	REQUIRE(r.ok);
	CHECK(std::fabs(static_cast<double>(r.number - 1.0L)) < 1e-12);
}

TEST_CASE("Calc: 関数の優先順位と括弧")
{
	calc::EvalResult r = calc::Evaluate(_T("ABS(-3) + CEIL(1.2)"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 5.0L));

	r = calc::Evaluate(_T("FLOOR(3.9)"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 3.0L));
}

TEST_CASE("Calc: ビット演算と GCD/LCM")
{
	calc::EvalResult r = calc::Evaluate(_T("12 AND 10"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 8.0L));

	r = calc::Evaluate(_T("12 XOR 10"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 6.0L));

	r = calc::Evaluate(_T("12 OR 10"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 14.0L));

	r = calc::Evaluate(_T("GCD(12, 18)"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 6.0L));

	r = calc::Evaluate(_T("LCM(12, 18)"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 36.0L));

	// VCL の GCD/LCM 語 também infix として使える。
	r = calc::Evaluate(_T("12 GCD 18"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 6.0L));
	r = calc::Evaluate(_T("12 LCM 18"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 36.0L));
}

TEST_CASE("Calc: 階乗と定義域エラー")
{
	calc::EvalResult r = calc::Evaluate(_T("5!"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 120.0L));

	r = calc::Evaluate(_T("(-1)!"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
}

TEST_CASE("Calc: 時間リテラルと Now")
{
	CHECK(calc::IsTimeString(_T("01:30")));
	CHECK(calc::IsTimeString(_T("+1:02:03")));
	CHECK_FALSE(calc::IsTimeString(_T("1:60")));

	calc::EvalResult r = calc::Evaluate(_T("01:30 + 00:45"));
	REQUIRE(r.ok);
	CHECK(r.is_time);
	CHECK(near_value(r, 8100.0L));
	CHECK(r.value == UnicodeString(_T("2:15:00")));

	r = calc::Evaluate(_T("-01:30"));
	REQUIRE(r.ok);
	CHECK(r.is_time);
	CHECK(near_value(r, -5400.0L));
	CHECK(r.value == UnicodeString(_T("-1:30:00")));

	r = calc::Evaluate(_T("1+1:30"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 5401.0L));

	calc::EvalOptions now;
	now.now = _T("12:34:56");
	r = calc::Evaluate(_T("Now"), now);
	REQUIRE(r.ok);
	CHECK(r.value == UnicodeString(_T("12:34:56")));
}

TEST_CASE("Calc: 複数行入力はプラスで連結する")
{
	CHECK(calc::JoinInput(_T("1\r\n2\r\n3")) == UnicodeString(_T("1+2+3")));
	CHECK(calc::JoinInput(_T(" 2 * 3 ")) == UnicodeString(_T("2 * 3")));
}

TEST_CASE("Calc: 割り算と括弧のエラー")
{
	calc::EvalResult r = calc::Evaluate(_T("1 / 0"));
	CHECK_FALSE(r.ok);
	CHECK(r.error == UnicodeString(_T("0による割り算")));

	r = calc::Evaluate(_T("(1 + 2"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());

	r = calc::Evaluate(_T("1 $ 2"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
}

TEST_CASE("Calc: 定数と単位変換")
{
	calc::EvalResult r = calc::Evaluate(_T("1KB + 1MB"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 1024.0L + 1024.0L * 1024.0L));

	long double converted = 0.0L;
	REQUIRE(calc::ConvertUnit(2.0L, calc::Unit::Kibibyte, calc::Unit::Byte, converted));
	CHECK(converted == 2048.0L);
	REQUIRE(calc::ConvertUnit(90.0L, calc::Unit::Degree, calc::Unit::Radian, converted));
	CHECK(std::fabs(static_cast<double>(converted - 1.5707963267948966L)) < 1e-12);
}

TEST_CASE("Calc: 16進/10進と NOT の補助関数")
{
	calc::EvalResult r = calc::Evaluate(_T("0x10"));
	REQUIRE(r.ok);
	CHECK(r.is_hex);
	CHECK(r.value == UnicodeString(_T("0x10")));

	CHECK(calc::BitwiseNot(0) == UnicodeString(_T("-1")));
	CHECK(calc::ToggleHex(_T("0x10")) == UnicodeString(_T("16")));
	CHECK(calc::ToggleHex(_T("16")) == UnicodeString(_T("0x10")));
}

TEST_CASE("Calc: 代入式の左辺を無視する")
{
	calc::EvalResult r = calc::Evaluate(_T("answer = 6 * 7"));
	REQUIRE(r.ok);
	CHECK(near_value(r, 42.0L));
}

TEST_CASE("Calc: NaN と異常値を拒否する")
{
	calc::EvalResult r = calc::Evaluate(_T("LN(0)"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());

	r = calc::Evaluate(_T("ASIN(2)"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
}
