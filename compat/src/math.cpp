/**
 * @file compat/src/math.cpp
 * @brief compat/math.h の実装
 */
#include "compat/math.h"

#include <cmath>
#include <cstdint>
#include <random>

double Power(double base, double exponent)
{
	return std::pow(base, exponent);
}

double Log10(double x)
{
	return std::log10(x);
}

double Log2(double x)
{
	return std::log2(x);
}

bool IsZero(double value, double epsilon)
{
	if (epsilon <= 0.0) return value == 0.0;
	return std::fabs(value) < epsilon;
}

namespace {

double RoundHalfAwayFromZero(double value)
{
	return (value >= 0.0) ? std::floor(value + 0.5) : std::ceil(value - 0.5);
}

double RoundAtDigit(double value, int digit)
{
	const double scale = std::pow(10.0, -digit);
	return RoundHalfAwayFromZero(value * scale) / scale;
}

}  // namespace

double RoundTo(double value, int digit)
{
	return RoundAtDigit(value, digit);
}

double SimpleRoundTo(double value, int digit)
{
	return RoundAtDigit(value, digit);
}

Int64 Ceil(double value)
{
	return static_cast<Int64>(std::ceil(value));
}

Int64 Floor(double value)
{
	return static_cast<Int64>(std::floor(value));
}

//---------------------------------------------------------------------------
// 浮動小数点例外のマスク
//
// Delphi の Get/SetExceptionMask は「マスクに入っている例外は発生させない」
// という意味なので、CRT の _MCW_EM (例外を無効化するビット) と向きが一致する。
//---------------------------------------------------------------------------
namespace {

struct FpuBit {
	TFPUException flag;
	unsigned int bit;
};

constexpr FpuBit kFpuBits[] = {
	{exInvalidOp, _EM_INVALID},   {exDenormalized, _EM_DENORMAL}, {exZeroDivide, _EM_ZERODIVIDE},
	{exOverflow, _EM_OVERFLOW},   {exUnderflow, _EM_UNDERFLOW},   {exPrecision, _EM_INEXACT},
};

unsigned int read_control_word()
{
	unsigned int cw = 0;
	::_controlfp_s(&cw, 0, 0);
	return cw;
}

}  // namespace

TFPUExceptionMask GetExceptionMask()
{
	const unsigned int cw = read_control_word();
	TFPUExceptionMask mask;
	for (const FpuBit &e : kFpuBits) {
		if ((cw & e.bit) != 0) mask << e.flag;
	}
	return mask;
}

TFPUExceptionMask SetExceptionMask(TFPUExceptionMask mask)
{
	const TFPUExceptionMask prev = GetExceptionMask();

	unsigned int bits = 0;
	for (const FpuBit &e : kFpuBits) {
		if (mask.Contains(e.flag)) bits |= e.bit;
	}
	unsigned int cw = 0;
	::_controlfp_s(&cw, bits, _MCW_EM);

	return prev;
}

//---------------------------------------------------------------------------
// 値の比較 (System.Math)
//---------------------------------------------------------------------------
bool SameValue(double a, double b, double epsilon)
{
	if (epsilon <= 0.0) {
		const double lo = std::fabs(a) < std::fabs(b) ? std::fabs(a) : std::fabs(b);
		const double e = lo * DoubleResolution;
		epsilon = (e > DoubleResolution) ? e : DoubleResolution;
	}
	return std::fabs(a - b) <= epsilon;
}

TValueRelationship CompareValue(int a, int b)
{
	if (a == b) return EqualsValue;
	return (a < b) ? LessThanValue : GreaterThanValue;
}

TValueRelationship CompareValue(Int64 a, Int64 b)
{
	if (a == b) return EqualsValue;
	return (a < b) ? LessThanValue : GreaterThanValue;
}

TValueRelationship CompareValue(double a, double b, double epsilon)
{
	if (SameValue(a, b, epsilon)) return EqualsValue;
	return (a < b) ? LessThanValue : GreaterThanValue;
}

//---------------------------------------------------------------------------
// 乱数
//---------------------------------------------------------------------------
namespace {

/// スレッドごとの乱数エンジン。src/task_thread.cpp がワーカースレッドから
/// Random を呼ぶため、グローバル 1 個にするとデータ競合になる
std::mt19937 &rand_engine()
{
	static thread_local std::mt19937 engine{[] {
		std::random_device rd;
		// random_device が使えない環境 (mingw の実装は決定的になり得る) でも
		// 時刻とスレッド ID で差がつくようにしておく
		const std::uint64_t seed = static_cast<std::uint64_t>(rd())
			^ (static_cast<std::uint64_t>(::GetTickCount64()) << 16)
			^ (static_cast<std::uint64_t>(::GetCurrentThreadId()) << 32);
		return static_cast<std::mt19937::result_type>(seed);
	}()};
	return engine;
}

}  // namespace

void Randomize()
{
	std::random_device rd;
	const std::uint64_t seed = static_cast<std::uint64_t>(rd())
		^ (static_cast<std::uint64_t>(::GetTickCount64()) << 16)
		^ (static_cast<std::uint64_t>(::GetCurrentThreadId()) << 32);
	rand_engine().seed(static_cast<std::mt19937::result_type>(seed));
}

int Random(int range)
{
	if (range <= 0) return 0;
	std::uniform_int_distribution<int> dist(0, range - 1);
	return dist(rand_engine());
}
