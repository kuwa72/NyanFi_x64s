/**
 * @file tests/compat/test_datetime.cpp
 * @brief compat/datetime.h の単体テスト (doctest)
 */
#include "doctest/doctest.h"

#include "compat/datetime.h"
#include "compat/exception.h"  // EConvertError (StrToDateTime の失敗時例外)

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
