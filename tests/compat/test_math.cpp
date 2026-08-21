/**
 * @file tests/compat/test_math.cpp
 * @brief compat/math.h と compat/types.h の単体テスト (doctest)
 *
 * Phase 3 で src/Global.cpp のために足した分 (M_PI / CompareValue /
 * TValueRelationship / Random / Randomize) が中心。Sign / Ceil / Floor など
 * Phase 0 からある分もあわせて固定する。
 */
#include "doctest/doctest.h"

#include "compat/math.h"
#include "compat/types.h"

#include <cmath>
#include <set>

TEST_CASE("M_PI などの数学定数が定義されている (mingw は _USE_MATH_DEFINES が要る)")
{
	// src/Global.cpp:10465 ほかが宣言なしで使う
	CHECK(M_PI == doctest::Approx(3.14159265358979323846));
	CHECK(std::sin(M_PI) == doctest::Approx(0.0).epsilon(1e-12));
	CHECK(M_PI_2 == doctest::Approx(M_PI / 2));
	CHECK(M_E == doctest::Approx(std::exp(1.0)));
	CHECK(M_SQRT2 == doctest::Approx(std::sqrt(2.0)));
}

TEST_CASE("TValueRelationship の定数は -1 / 0 / 1")
{
	CHECK(static_cast<int>(LessThanValue) == -1);
	CHECK(static_cast<int>(EqualsValue) == 0);
	CHECK(static_cast<int>(GreaterThanValue) == 1);
	CHECK(sizeof(TValueRelationship) == 1);  // 実 C++Builder は Int8
}

TEST_CASE("CompareValue (整数)")
{
	CHECK(CompareValue(1, 2) == LessThanValue);
	CHECK(CompareValue(2, 2) == EqualsValue);
	CHECK(CompareValue(3, 2) == GreaterThanValue);

	CHECK(CompareValue(static_cast<Int64>(-5), static_cast<Int64>(5)) == LessThanValue);
	CHECK(CompareValue(static_cast<Int64>(1) << 40, static_cast<Int64>(1) << 40) == EqualsValue);
}

TEST_CASE("CompareValue/SameValue (浮動小数)")
{
	CHECK(CompareValue(1.0, 2.0) == LessThanValue);
	CHECK(CompareValue(2.0, 1.0) == GreaterThanValue);
	CHECK(CompareValue(1.0, 1.0) == EqualsValue);

	// 既定の epsilon (DoubleResolution) の範囲では同値
	CHECK(SameValue(1.0, 1.0 + 1.0e-15));
	CHECK(CompareValue(1.0, 1.0 + 1.0e-15) == EqualsValue);
	CHECK_FALSE(SameValue(1.0, 1.0 + 1.0e-6));

	// epsilon を明示した場合
	CHECK(SameValue(1.0, 1.5, 0.5));
	CHECK(CompareValue(1.0, 1.5, 0.5) == EqualsValue);
	CHECK(CompareValue(1.0, 1.5, 0.4) == LessThanValue);
}

TEST_CASE("Sign / IsZero / Ceil / Floor")
{
	CHECK(Sign(-3) == -1);
	CHECK(Sign(0) == 0);
	CHECK(Sign(3.5) == 1);

	CHECK(IsZero(0.0));
	CHECK_FALSE(IsZero(1.0e-9));
	CHECK(IsZero(1.0e-9, 1.0e-6));

	CHECK(Ceil(1.2) == 2);
	CHECK(Ceil(-1.2) == -1);
	CHECK(Floor(1.8) == 1);
	CHECK(Floor(-1.2) == -2);
}

TEST_CASE("Random(n) は [0, n) に収まる")
{
	// src/task_thread.cpp:1134 は Random(256) をバイト値として使うので
	// 上限を越えないことが要件
	Randomize();
	for (int i = 0; i < 2000; i++) {
		int v = Random(256);
		CHECK(v >= 0);
		CHECK(v < 256);
	}

	// src/Global.cpp:11386 のシャッフルは Random(Count) を添字に使う
	for (int i = 0; i < 200; i++) {
		int v = Random(1);
		CHECK(v == 0);
	}

	// 0 以下は 0 を返す (Delphi では未定義。落ちないことを固定する)
	CHECK(Random(0) == 0);
	CHECK(Random(-1) == 0);
}

TEST_CASE("Random は 1 つの値に張り付かない")
{
	Randomize();
	std::set<int> seen;
	for (int i = 0; i < 500; i++) seen.insert(Random(1000));
	// 500 回引いて 100 種類も出ないなら壊れている
	CHECK(seen.size() > 100);
}

TEST_CASE("FPU 例外マスクの取得と復元")
{
	TFPUExceptionMask saved = GetExceptionMask();
	TFPUExceptionMask all;
	all << exInvalidOp << exDenormalized << exZeroDivide << exOverflow << exUnderflow << exPrecision;

	SetExceptionMask(all);
	CHECK(GetExceptionMask().Contains(exZeroDivide));

	SetExceptionMask(saved);
	CHECK(GetExceptionMask().Contains(exZeroDivide) == saved.Contains(exZeroDivide));
}
