/**
 * @file tests/compat/test_datetime.cpp
 * @brief compat/datetime.h の単体テスト (doctest)
 */
#include "doctest/doctest.h"

#include "compat/datetime.h"
#include "compat/exception.h"  // EConvertError (StrToDateTime の失敗時例外)

#include <cmath>

TEST_CASE("TDateTime は 1899-12-30 を 0 とする通し日数")
{
	CHECK(static_cast<double>(TDateTime(0)) == doctest::Approx(0.0));
	CHECK(static_cast<double>(TDateTime(1899, 12, 30)) == doctest::Approx(0.0));
	CHECK(static_cast<double>(TDateTime(1899, 12, 31)) == doctest::Approx(1.0));
}

TEST_CASE("1970-01-01 は Delphi epoch から 25569 日 (Unix epoch との差)")
{
	// usr_exif.cpp: dt += (TDateTime(1970, 1, 1) - TDateTime(0));
	double diff = TDateTime(1970, 1, 1) - TDateTime(0);
	CHECK(diff == doctest::Approx(25569.0));
}

TEST_CASE("EncodeDate/DecodeDate の往復")
{
	TDateTime dt = EncodeDate(2024, 2, 29);  // うるう年
	unsigned short y, m, d;
	DecodeDate(dt, y, m, d);
	CHECK(y == 2024);
	CHECK(m == 2);
	CHECK(d == 29);
}

TEST_CASE("EncodeTime/DecodeTime の往復")
{
	TDateTime t = EncodeTime(23, 59, 58, 123);
	unsigned short h, mi, s, ms;
	DecodeTime(t, h, mi, s, ms);
	CHECK(h == 23);
	CHECK(mi == 59);
	CHECK(s == 58);
	CHECK(ms == 123);
}

TEST_CASE("7引数コンストラクタ (usr_str.cpp の str_to_DateTime と同じ形)")
{
	TDateTime dt(2024, 1, 2, 3, 4, 5, 0);
	unsigned short y, m, d, h, mi, s, ms;
	DecodeDate(dt, y, m, d);
	DecodeTime(dt, h, mi, s, ms);
	CHECK(y == 2024);
	CHECK(m == 1);
	CHECK(d == 2);
	CHECK(h == 3);
	CHECK(mi == 4);
	CHECK(s == 5);
}

TEST_CASE("FormatDateTime: 対象コードで実測された書式")
{
	TDateTime dt(2024, 3, 5, 9, 8, 7, 0);
	CHECK(FormatDateTime("yyyy'/'mm'/'dd hh:nn:ss", dt) == "2024/03/05 09:08:07");
	CHECK(FormatDateTime("hh:nn:ss", dt) == "09:08:07");
	CHECK(FormatDateTime("yyyymmddhhnnss", dt) == "20240305090807");
	CHECK(FormatDateTime("yyyy'/'mm'/'dd", dt) == "2024/03/05");

	TDateTime dt2(2024, 3, 5, 9, 8, 7, 250);
	CHECK(FormatDateTime("hh:nn:ss.zzz", dt2) == "09:08:07.250");
}

TEST_CASE("IncMonth/IncYear は月末を補正する (Delphi 仕様)")
{
	TDateTime jan31(2024, 1, 31);
	TDateTime feb = IncMonth(jan31, 1);
	unsigned short y, m, d;
	DecodeDate(feb, y, m, d);
	CHECK(y == 2024);
	CHECK(m == 2);
	CHECK(d == 29);  // 2024年はうるう年なので 29 に補正

	TDateTime leapDay(2024, 2, 29);
	TDateTime nextYear = IncYear(leapDay, 1);
	DecodeDate(nextYear, y, m, d);
	CHECK(y == 2025);
	CHECK(m == 2);
	CHECK(d == 28);  // 2025年は平年なので 28 に補正
}

TEST_CASE("IncDay / YearOf / MonthOf / DayOf / HourOf")
{
	TDateTime dt(2024, 12, 31, 23, 0, 0, 0);
	TDateTime next = IncDay(dt, 1);
	CHECK(YearOf(next) == 2025);
	CHECK(MonthOf(next) == 1);
	CHECK(DayOf(next) == 1);
	CHECK(HourOf(next) == 23);
}

TEST_CASE("DateTimeToSystemTime / SystemTimeToDateTime の往復")
{
	TDateTime dt(2024, 6, 15, 12, 34, 56, 789);
	TSystemTime st;
	DateTimeToSystemTime(dt, st);
	CHECK(st.wYear == 2024);
	CHECK(st.wMonth == 6);
	CHECK(st.wDay == 15);
	CHECK(st.wHour == 12);
	CHECK(st.wMinute == 34);
	CHECK(st.wSecond == 56);
	CHECK(st.wMilliseconds == 789);

	TDateTime back = SystemTimeToDateTime(st);
	CHECK(static_cast<double>(back) == doctest::Approx(static_cast<double>(dt)));
}

TEST_CASE("TryStrToDateTime: 日付+時刻/日付のみ/時刻のみ")
{
	TDateTime dt;
	REQUIRE(TryStrToDateTime("2024/03/05 09:08:07", dt));
	unsigned short y, m, d, h, mi, s, ms;
	DecodeDate(dt, y, m, d);
	DecodeTime(dt, h, mi, s, ms);
	CHECK(y == 2024);
	CHECK(m == 3);
	CHECK(d == 5);
	CHECK(h == 9);
	CHECK(mi == 8);
	CHECK(s == 7);

	REQUIRE(TryStrToDateTime("2024-03-05", dt));
	DecodeDate(dt, y, m, d);
	CHECK(y == 2024);
	CHECK(m == 3);
	CHECK(d == 5);

	REQUIRE(TryStrToDateTime("09:08:07", dt));
	DecodeTime(dt, h, mi, s, ms);
	CHECK(h == 9);
	CHECK(mi == 8);
	CHECK(s == 7);
}

TEST_CASE("StrToDateTime は解析できない文字列で例外を送出する")
{
	CHECK_THROWS_AS(StrToDateTime("not a date"), EConvertError);
}

TEST_CASE("Now/Date は妥当な範囲の値を返す (Phase 0 の実行環境に依存しないゆるいチェック)")
{
	TDateTime now = Now();
	unsigned short y, m, d;
	DecodeDate(now, y, m, d);
	CHECK(y >= 2024);
	CHECK(y < 2100);

	TDateTime today = Date();
	unsigned short h, mi, s, ms;
	DecodeTime(today, h, mi, s, ms);
	CHECK(h == 0);
	CHECK(mi == 0);
	CHECK(s == 0);
}

TEST_CASE("DaysInMonth (UserFunc.cpp が使用。Phase 0 の対象外ファイルだが契約として用意)")
{
	CHECK(DaysInMonth(TDateTime(2024, 2, 1)) == 29);  // うるう年
	CHECK(DaysInMonth(TDateTime(2023, 2, 1)) == 28);
	CHECK(DaysInMonth(TDateTime(2024, 4, 1)) == 30);
}

//---------------------------------------------------------------------------
// System.DateUtils の追加分 (Phase 3 / Global.cpp のために足したもの)
//---------------------------------------------------------------------------
TEST_CASE("DateOf/TimeOf は日付部分と時刻部分に分ける")
{
	TDateTime dt = TDateTime(2024, 5, 17, 13, 45, 30, 500);
	CHECK(static_cast<double>(DateOf(dt)) == doctest::Approx(static_cast<double>(TDateTime(2024, 5, 17))));

	unsigned short h, mi, s, ms;
	DecodeTime(TimeOf(dt), h, mi, s, ms);
	CHECK(h == 13);
	CHECK(mi == 45);
	CHECK(s == 30);
	CHECK(ms == 500);
}

TEST_CASE("Today は Date と同じ (時刻 0:00 の当日)")
{
	// src/Global.cpp:14157 が `int n = dt - Today();` と整数日差を採るので、
	// 小数部が残っていないことが要件になる
	TDateTime t = Today();
	CHECK(static_cast<double>(t) == doctest::Approx(static_cast<double>(Date())));
	CHECK(static_cast<double>(t) == doctest::Approx(std::trunc(static_cast<double>(t))));
}

TEST_CASE("IsSameDay/IsToday は時刻を無視して日付だけ見る")
{
	TDateTime a = TDateTime(2024, 5, 17, 0, 0, 0, 0);
	TDateTime b = TDateTime(2024, 5, 17, 23, 59, 59, 999);
	TDateTime c = TDateTime(2024, 5, 18, 0, 0, 0, 0);
	CHECK(IsSameDay(a, b));
	CHECK_FALSE(IsSameDay(a, c));

	CHECK(IsToday(Now()));
	CHECK_FALSE(IsToday(IncDay(Today(), -1)));
}

TEST_CASE("IncMilliSecond/IncSecond/IncMinute/IncHour")
{
	TDateTime base = TDateTime(2024, 5, 17, 10, 0, 0, 0);

	unsigned short h, mi, s, ms;
	DecodeTime(IncHour(base, 12), h, mi, s, ms);
	CHECK(h == 22);

	// src/RenDlg.cpp:1183 の `IncHour(DateOf(Now()), 12)` = 当日の正午
	DecodeTime(IncHour(DateOf(base), 12), h, mi, s, ms);
	CHECK(h == 12);
	CHECK(mi == 0);
	CHECK(s == 0);

	DecodeTime(IncMinute(base, 90), h, mi, s, ms);
	CHECK(h == 11);
	CHECK(mi == 30);

	DecodeTime(IncSecond(base, -1), h, mi, s, ms);
	CHECK(h == 9);
	CHECK(mi == 59);
	CHECK(s == 59);

	DecodeTime(IncMilliSecond(base, 1500), h, mi, s, ms);
	CHECK(s == 1);
	CHECK(ms == 500);
}

TEST_CASE("MilliSecondsBetween/SecondsBetween/DaysBetween は常に非負")
{
	TDateTime a = TDateTime(2024, 5, 17, 10, 0, 0, 0);
	TDateTime b = TDateTime(2024, 5, 17, 10, 0, 2, 500);

	CHECK(MilliSecondsBetween(a, b) == 2500);
	CHECK(MilliSecondsBetween(b, a) == 2500);  // 引数の順に依らない
	CHECK(SecondsBetween(a, b) == 2);          // 切り捨て

	CHECK(DaysBetween(TDateTime(2024, 5, 17), TDateTime(2024, 5, 20)) == 3);
	CHECK(DaysBetween(TDateTime(2024, 5, 20), TDateTime(2024, 5, 17)) == 3);
	// 端数の日は切り捨て (src/FindDlg.cpp が相対日数として使う)
	CHECK(DaysBetween(TDateTime(2024, 5, 17, 0, 0, 0, 0), TDateTime(2024, 5, 18, 12, 0, 0, 0)) == 1);
}

TEST_CASE("WithinPastMilliSeconds はタイムスタンプの許容誤差判定 (境界を含む)")
{
	// src の既定 TimeTolerance=2000ms。src/Global.cpp:3624 is_NewerTime() ほか
	const Int64 tolerance = 2000;
	TDateTime a = TDateTime(2024, 5, 17, 10, 0, 0, 0);

	CHECK(WithinPastMilliSeconds(a, a, tolerance));
	CHECK(WithinPastMilliSeconds(a, TDateTime(2024, 5, 17, 10, 0, 1, 999), tolerance));
	CHECK(WithinPastMilliSeconds(a, TDateTime(2024, 5, 17, 10, 0, 2, 0), tolerance));   // 境界は含む
	CHECK_FALSE(WithinPastMilliSeconds(a, TDateTime(2024, 5, 17, 10, 0, 2, 1), tolerance));
	// 「Past」という名前だが、未来向きでも真になる (RTL と同じ)
	CHECK(WithinPastMilliSeconds(TDateTime(2024, 5, 17, 10, 0, 2, 0), a, tolerance));
}

TEST_CASE("CompareDate は日付部分だけを比べる")
{
	TDateTime d17am = TDateTime(2024, 5, 17, 1, 0, 0, 0);
	TDateTime d17pm = TDateTime(2024, 5, 17, 23, 0, 0, 0);
	TDateTime d18 = TDateTime(2024, 5, 18, 0, 0, 0, 0);

	CHECK(CompareDate(d17am, d17pm) == EqualsValue);
	CHECK(CompareDate(d17am, d18) == LessThanValue);
	CHECK(CompareDate(d18, d17pm) == GreaterThanValue);

	// 名前空間つきの呼び方 (src/Global.cpp:5677 / src/UserFunc.cpp:516)
	CHECK(Dateutils::CompareDate(d17am, d18) == LessThanValue);
	CHECK(System::Dateutils::CompareDate(d17am, d18) == LessThanValue);
}

TEST_CASE("CompareDateTime は 1ms の分解能で比べる")
{
	TDateTime a = TDateTime(2024, 5, 17, 10, 0, 0, 0);
	TDateTime b = TDateTime(2024, 5, 17, 10, 0, 0, 1);

	CHECK(CompareDateTime(a, a) == EqualsValue);
	CHECK(CompareDateTime(a, b) == LessThanValue);
	CHECK(CompareDateTime(b, a) == GreaterThanValue);
	// 1ms 未満の差は同値
	CHECK(CompareDateTime(a, TDateTime(static_cast<double>(a) + OneMillisecond / 4)) == EqualsValue);
}

TEST_CASE("CompareTime は時刻部分だけを比べる")
{
	// src/Global.cpp:5714 は再生時間 (ms) を 86400000 で割った「日の小数部」を渡す
	TTime t1 = 3600000.0 / 86400000.0;  // 1 時間
	TTime t2 = 7200000.0 / 86400000.0;  // 2 時間

	CHECK(CompareTime(t1, t1) == EqualsValue);
	CHECK(CompareTime(t1, t2) == LessThanValue);
	CHECK(CompareTime(t2, t1) == GreaterThanValue);

	// 日付が違っても時刻が同じなら EqualsValue
	CHECK(CompareTime(TDateTime(2024, 5, 17, 10, 0, 0, 0), TDateTime(2030, 1, 1, 10, 0, 0, 0)) == EqualsValue);
}

TEST_CASE("DateTimeToMilliseconds は 1899-12-30 からの通算ミリ秒")
{
	CHECK(DateTimeToMilliseconds(TDateTime(1899, 12, 30)) == 0);
	CHECK(DateTimeToMilliseconds(TDateTime(1899, 12, 31)) == 86400000);
	CHECK(DateTimeToMilliseconds(TDateTime(1899, 12, 30, 0, 0, 1, 0)) == 1000);
}
