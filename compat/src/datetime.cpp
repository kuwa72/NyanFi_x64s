/**
 * @file compat/src/datetime.cpp
 * @brief compat/datetime.h の実装
 */
#include "compat/datetime.h"

#include <cmath>
#include <cwchar>
#include <cwctype>
#include <vector>

#include "compat/exception.h"  // EConvertError (ヘッダではなく実装側でのみ使う)

namespace {

//---------------------------------------------------------------------------
// 暦計算 (Howard Hinnant の civil_from_days / days_from_civil アルゴリズム)
// 参考: http://howardhinnant.github.io/date_algorithms.html
// 1970-01-01 を 0 とする通し日数を返す/受け取る。純カレンダー計算のみで
// 浮動小数誤差が入らないため、Delphi の EncodeDate/DecodeDate と一致する。
//---------------------------------------------------------------------------
constexpr long long kUnixToDelphiEpochDays = 25569;  // 1899-12-30 -> 1970-01-01 の日数

long long DaysFromCivil(int y, unsigned m, unsigned d)
{
	y -= (m <= 2) ? 1 : 0;
	const long long era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400);                      // [0, 399]
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;            // [0, 365]
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                     // [0, 146096]
	return era * 146097 + static_cast<long long>(doe) - 719468;
}

void CivilFromDays(long long z, int &y, unsigned &m, unsigned &d)
{
	z += 719468;
	const long long era = (z >= 0 ? z : z - 146096) / 146097;
	const unsigned doe = static_cast<unsigned>(z - era * 146097);                    // [0, 146096]
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;      // [0, 399]
	const long long yy = static_cast<long long>(yoe) + era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                    // [0, 365]
	const unsigned mp = (5 * doy + 2) / 153;                                         // [0, 11]
	d = doy - (153 * mp + 2) / 5 + 1;                                                // [1, 31]
	m = mp + (mp < 10 ? 3 : -9);                                                     // [1, 12]
	y = static_cast<int>(yy + (m <= 2 ? 1 : 0));
}

/// Delphi 通し日数 (1899-12-30 = 0) から (y, m, d) を得る
void DecodeDateSerial(long long serial, unsigned short &y, unsigned short &m, unsigned short &d)
{
	int yy = 0;
	unsigned mm = 0, dd = 0;
	CivilFromDays(serial - kUnixToDelphiEpochDays, yy, mm, dd);
	y = static_cast<unsigned short>(yy);
	m = static_cast<unsigned short>(mm);
	d = static_cast<unsigned short>(dd);
}

/// (y, m, d) から Delphi 通し日数 (1899-12-30 = 0) を得る
long long EncodeDateSerial(unsigned short y, unsigned short m, unsigned short d)
{
	return DaysFromCivil(y, m, d) + kUnixToDelphiEpochDays;
}

/// 値の整数部 (日付) を負値にも対応する形で取り出す (floor 相当)
long long FloorToInt(double value)
{
	return static_cast<long long>(std::floor(value));
}

/// 0 時からの経過ミリ秒 (0 <= ms < 86400000) を取り出す
long long DayMillis(double value)
{
	const double dayFrac = value - std::floor(value);
	long long ms = static_cast<long long>(std::llround(dayFrac * 86400000.0));
	if (ms < 0) ms = 0;
	if (ms >= 86400000) ms = 86400000 - 1;
	return ms;
}

/// 文字列から数字だけを取り出して整数配列にする (StrToDateTime の簡易実装用)
std::vector<int> ExtractDigitGroups(const UnicodeString &s)
{
	std::vector<int> groups;
	const wchar_t *p = s.c_str();
	int cur = 0;
	bool has = false;
	for (; *p; ++p) {
		if (std::iswdigit(static_cast<wint_t>(*p))) {
			cur = cur * 10 + (*p - L'0');
			has = true;
		}
		else {
			if (has) groups.push_back(cur);
			cur = 0;
			has = false;
		}
	}
	if (has) groups.push_back(cur);
	return groups;
}

bool ContainsAny(const UnicodeString &s, const wchar_t *chars)
{
	const wchar_t *p = s.c_str();
	for (; *p; ++p) {
		for (const wchar_t *c = chars; *c; ++c) {
			if (*p == *c) return true;
		}
	}
	return false;
}

}  // namespace

//---------------------------------------------------------------------------
TDateTime::TDateTime(unsigned short year, unsigned short month, unsigned short day)
    : value_(static_cast<double>(EncodeDateSerial(year, month, day)))
{
}

TDateTime::TDateTime(unsigned short hour, unsigned short min, unsigned short sec, unsigned short msec)
    : value_((static_cast<double>(hour) * 3600.0 + static_cast<double>(min) * 60.0 +
              static_cast<double>(sec)) / 86400.0 + static_cast<double>(msec) / 86400000.0)
{
}

TDateTime::TDateTime(unsigned short year, unsigned short month, unsigned short day, unsigned short hour,
                      unsigned short min, unsigned short sec, unsigned short msec)
    : value_(static_cast<double>(EncodeDateSerial(year, month, day)) +
              (static_cast<double>(hour) * 3600.0 + static_cast<double>(min) * 60.0 +
               static_cast<double>(sec)) / 86400.0 + static_cast<double>(msec) / 86400000.0)
{
}

TDateTime::TDateTime(const UnicodeString &s) : value_(StrToDateTime(s).Val()) {}

//---------------------------------------------------------------------------
TDateTime Now()
{
	SYSTEMTIME st;
	::GetLocalTime(&st);
	return SystemTimeToDateTime(st);
}

TDateTime Date()
{
	SYSTEMTIME st;
	::GetLocalTime(&st);
	st.wHour = 0;
	st.wMinute = 0;
	st.wSecond = 0;
	st.wMilliseconds = 0;
	return SystemTimeToDateTime(st);
}

TDateTime Time()
{
	TDateTime now = Now();
	return TDateTime(now.Val() - std::floor(now.Val()));
}

//---------------------------------------------------------------------------
TDateTime EncodeDate(unsigned short year, unsigned short month, unsigned short day)
{
	return TDateTime(year, month, day);
}

TDateTime EncodeTime(unsigned short hour, unsigned short min, unsigned short sec, unsigned short msec)
{
	return TDateTime(hour, min, sec, msec);
}

void DecodeDate(const TDateTime &dt, unsigned short &year, unsigned short &month, unsigned short &day)
{
	DecodeDateSerial(FloorToInt(dt.Val()), year, month, day);
}

void DecodeTime(const TDateTime &dt, unsigned short &hour, unsigned short &min, unsigned short &sec,
                 unsigned short &msec)
{
	long long ms = DayMillis(dt.Val());
	hour = static_cast<unsigned short>(ms / 3600000);
	ms %= 3600000;
	min = static_cast<unsigned short>(ms / 60000);
	ms %= 60000;
	sec = static_cast<unsigned short>(ms / 1000);
	msec = static_cast<unsigned short>(ms % 1000);
}

//---------------------------------------------------------------------------
// FormatDateTime
//
// 対応トークン: y / yy / yyyy, m / mm, d / dd, h / hh, n / nn, s / ss, z / zzz。
// ' または " で囲まれた区間はリテラルとして出力する (Delphi 仕様)。
// それ以外の文字はそのまま通す。AM/PM 等の未使用トークンは対象コードでの
// 実測 (yyyy/mm/dd, hh:nn:ss, yyyymmddhhnnss, hh:nn:ss.zzz 系のみ) に基づき
// 未実装 (無言のスキップではなく、ここに明記する)。
//---------------------------------------------------------------------------
UnicodeString FormatDateTime(const UnicodeString &format, const TDateTime &dt)
{
	unsigned short y, mo, d, h, mi, se, ms;
	DecodeDate(dt, y, mo, d);
	DecodeTime(dt, h, mi, se, ms);

	std::wstring out;
	const wchar_t *p = format.c_str();
	const wchar_t *end = p + format.Length();

	auto countRun = [&](wchar_t token) -> int {
		int n = 0;
		const wchar_t *q = p;
		while (q < end && *q == token) {
			++n;
			++q;
		}
		return n;
	};

	wchar_t buf[16];
	while (p < end) {
		wchar_t c = *p;
		if (c == L'\'' || c == L'"') {
			wchar_t quote = c;
			++p;
			while (p < end && *p != quote) out.push_back(*p++);
			if (p < end) ++p;  // 閉じ引用符を読み飛ばす
			continue;
		}
		if (c == L'y' || c == L'Y') {
			int n = countRun(L'y');
			if (n >= 4) {
				std::swprintf(buf, 16, L"%04u", y);
			}
			else {
				std::swprintf(buf, 16, L"%02u", y % 100);
			}
			out += buf;
			p += n;
			continue;
		}
		if (c == L'm' || c == L'M') {
			int n = countRun(L'm');
			std::swprintf(buf, 16, (n >= 2) ? L"%02u" : L"%u", mo);
			out += buf;
			p += n;
			continue;
		}
		if (c == L'd' || c == L'D') {
			int n = countRun(L'd');
			std::swprintf(buf, 16, (n >= 2) ? L"%02u" : L"%u", d);
			out += buf;
			p += n;
			continue;
		}
		if (c == L'h' || c == L'H') {
			int n = countRun(L'h');
			std::swprintf(buf, 16, (n >= 2) ? L"%02u" : L"%u", h);
			out += buf;
			p += n;
			continue;
		}
		if (c == L'n' || c == L'N') {
			int n = countRun(L'n');
			std::swprintf(buf, 16, (n >= 2) ? L"%02u" : L"%u", mi);
			out += buf;
			p += n;
			continue;
		}
		if (c == L's' || c == L'S') {
			int n = countRun(L's');
			std::swprintf(buf, 16, (n >= 2) ? L"%02u" : L"%u", se);
			out += buf;
			p += n;
			continue;
		}
		if (c == L'z' || c == L'Z') {
			int n = countRun(L'z');
			std::swprintf(buf, 16, L"%03u", ms);
			out += buf;
			p += n;
			continue;
		}
		out.push_back(c);
		++p;
	}
	return UnicodeString(out);
}

UnicodeString DateTimeToStr(const TDateTime &dt)
{
	// 【注意】実 RTL は FormatSettings (ロケール) 依存。本シムは対象コードで
	// 支配的な書式 "yyyy/mm/dd hh:nn:ss" (usr_exif.cpp / UserFunc.cpp 等で
	// 多用) に固定する。ロケール切り替えは Phase 0 の対象外。
	return FormatDateTime("yyyy/mm/dd hh:nn:ss", dt);
}

bool TryStrToDateTime(const UnicodeString &s, TDateTime &result)
{
	// 【注意】実 RTL は FormatSettings に基づく本格的な構文解析を行うが、
	// 本シムは対象コード (usr_str.cpp の str_to_DateTime 独自関数) が
	// 実際に生成/消費する書式に基づき、数字の並びと区切り文字の種類
	// (':' なら時刻、'/' か '-' があれば日付) から簡易的に判定する。
	std::vector<int> g = ExtractDigitGroups(s);
	bool hasColon = ContainsAny(s, L":");
	bool hasDateSep = ContainsAny(s, L"/-");

	if (hasDateSep && (g.size() == 3 || g.size() == 6)) {
		unsigned short y = static_cast<unsigned short>(g[0]);
		unsigned short mo = static_cast<unsigned short>(g[1]);
		unsigned short d = static_cast<unsigned short>(g[2]);
		if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
		if (g.size() == 6) {
			unsigned short h = static_cast<unsigned short>(g[3]);
			unsigned short mi = static_cast<unsigned short>(g[4]);
			unsigned short se = static_cast<unsigned short>(g[5]);
			result = TDateTime(y, mo, d, h, mi, se, 0);
		}
		else {
			result = TDateTime(y, mo, d);
		}
		return true;
	}
	if (hasColon && !hasDateSep && (g.size() == 2 || g.size() == 3)) {
		unsigned short h = static_cast<unsigned short>(g[0]);
		unsigned short mi = static_cast<unsigned short>(g[1]);
		unsigned short se = (g.size() == 3) ? static_cast<unsigned short>(g[2]) : 0;
		if (h > 23 || mi > 59 || se > 59) return false;
		result = TDateTime(h, mi, se, 0);
		return true;
	}
	if (!hasColon && !hasDateSep && g.size() == 1) {
		// 数字のみ = 通し番号 (Delphi の TDateTime を整数として渡すケース)
		result = TDateTime(static_cast<double>(g[0]));
		return true;
	}
	return false;
}

TDateTime StrToDateTime(const UnicodeString &s)
{
	TDateTime result;
	if (!TryStrToDateTime(s, result)) {
		throw EConvertError(UnicodeString(L"'" ) + s + L"' is not a valid date and time");
	}
	return result;
}

//---------------------------------------------------------------------------
void DateTimeToSystemTime(const TDateTime &dt, TSystemTime &systemTime)
{
	unsigned short y, mo, d, h, mi, se, ms;
	DecodeDate(dt, y, mo, d);
	DecodeTime(dt, h, mi, se, ms);
	systemTime.wYear = y;
	systemTime.wMonth = mo;
	systemTime.wDay = d;
	systemTime.wHour = h;
	systemTime.wMinute = mi;
	systemTime.wSecond = se;
	systemTime.wMilliseconds = ms;
	systemTime.wDayOfWeek = 0;  // RTL 同様、呼び出し側では未使用の想定
}

TDateTime SystemTimeToDateTime(const TSystemTime &systemTime)
{
	return TDateTime(systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour,
	                  systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);
}

//---------------------------------------------------------------------------
TDateTime IncDay(const TDateTime &dt, int numberOfDays)
{
	return TDateTime(dt.Val() + static_cast<double>(numberOfDays));
}

TDateTime IncMonth(const TDateTime &dt, int numberOfMonths)
{
	unsigned short y, mo, d, h, mi, se, ms;
	DecodeDate(dt, y, mo, d);
	DecodeTime(dt, h, mi, se, ms);

	long long total = static_cast<long long>(y) * 12 + (mo - 1) + numberOfMonths;
	long long ny = total / 12;
	long long nmRaw = total % 12;
	if (nmRaw < 0) {
		nmRaw += 12;
		ny -= 1;
	}
	unsigned short newYear = static_cast<unsigned short>(ny);
	unsigned short newMonth = static_cast<unsigned short>(nmRaw + 1);
	unsigned short maxDay = DaysInMonth(TDateTime(newYear, newMonth, 1));
	unsigned short newDay = (d > maxDay) ? maxDay : d;  // Delphi IncMonth の月末補正

	return TDateTime(newYear, newMonth, newDay, h, mi, se, ms);
}

TDateTime IncYear(const TDateTime &dt, int numberOfYears)
{
	unsigned short y, mo, d, h, mi, se, ms;
	DecodeDate(dt, y, mo, d);
	DecodeTime(dt, h, mi, se, ms);
	unsigned short newYear = static_cast<unsigned short>(static_cast<int>(y) + numberOfYears);
	unsigned short maxDay = DaysInMonth(TDateTime(newYear, mo, 1));
	unsigned short newDay = (d > maxDay) ? maxDay : d;
	return TDateTime(newYear, mo, newDay, h, mi, se, ms);
}

unsigned short YearOf(const TDateTime &dt)
{
	unsigned short y, mo, d;
	DecodeDate(dt, y, mo, d);
	return y;
}

unsigned short MonthOf(const TDateTime &dt)
{
	unsigned short y, mo, d;
	DecodeDate(dt, y, mo, d);
	return mo;
}

unsigned short DayOf(const TDateTime &dt)
{
	unsigned short y, mo, d;
	DecodeDate(dt, y, mo, d);
	return d;
}

unsigned short HourOf(const TDateTime &dt)
{
	unsigned short h, mi, se, ms;
	DecodeTime(dt, h, mi, se, ms);
	return h;
}

unsigned short DaysInMonth(const TDateTime &dt)
{
	static const unsigned short kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	unsigned short y, mo, d;
	DecodeDate(dt, y, mo, d);
	if (mo == 2) {
		bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
		return leap ? 29 : 28;
	}
	if (mo >= 1 && mo <= 12) return kDays[mo - 1];
	return 30;  // 不正な月 (呼ばれない想定だが RTL は範囲チェック例外。ここは保守的な既定値)
}
