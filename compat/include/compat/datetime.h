/**
 * @file compat/datetime.h
 * @brief System.DateUtils / TDateTime 相当の互換シム
 *
 * Delphi と同じく **1899-12-30 を serial 値 0 とする浮動小数の日付時刻**
 * (`TDateTime = type Double`) を再現する。実 C++Builder の System::TDateTime
 * と同様、`operator double()` と `double` からの非 explicit コンストラクタ
 * だけを用意し、加減算・比較は暗黙の double 変換に委譲する
 * (`a - b` は一旦 double になり、それを TDateTime が受ける += / 代入で
 * 再び TDateTime に戻る)。
 *
 * 依存: compat/config.h, compat/ustring.h のみ。compat/sysutils.h には
 * 依存しない (sysutils.h 側が本ヘッダを include するため、循環を避ける)。
 * TSearchRec (sysutils.h) は TimeStamp フィールドとして TDateTime を持つ。
 */
#ifndef NYANFI_COMPAT_DATETIME_H
#define NYANFI_COMPAT_DATETIME_H

#include "compat/config.h"
#include "compat/ustring.h"

/// Win32 の SYSTEMTIME (windows.h) の Delphi 側の呼び名
using TSystemTime = SYSTEMTIME;

//---------------------------------------------------------------------------
/**
 * @brief Delphi の TDateTime 互換 (内部表現は double、日単位の通し番号)
 * @details 1899-12-30 0:00 を 0.0 とし、1 日を 1.0 とする。時刻は小数部。
 *          TDate / TTime は同一実体の別名 (実 C++Builder の System.hpp も
 *          typedef で同じ扱いにしている)。
 */
class TDateTime {
public:
	TDateTime() : value_(0.0) {}
	TDateTime(double value) : value_(value) {}  //!< NOLINT: 暗黙変換を許す (Delphi 仕様)
	TDateTime(int value) : value_(static_cast<double>(value)) {}  //!< 整数日 (通し番号) からの暗黙変換

	/// 年月日から (時刻は 0:00)
	TDateTime(unsigned short year, unsigned short month, unsigned short day);
	/// 時分秒ミリ秒のみ (日付は 1899-12-30 のまま)
	TDateTime(unsigned short hour, unsigned short min, unsigned short sec, unsigned short msec);
	/// 年月日時分秒ミリ秒
	TDateTime(unsigned short year, unsigned short month, unsigned short day,
	          unsigned short hour, unsigned short min, unsigned short sec, unsigned short msec);
	/// 文字列からの変換 (失敗時 EConvertError 相当 → std::invalid_argument)
	explicit TDateTime(const UnicodeString &s);

	operator double() const { return value_; }  //!< 比較・算術は double 変換に委譲

	TDateTime &operator+=(const TDateTime &rhs)
	{
		value_ += rhs.value_;
		return *this;
	}
	TDateTime &operator-=(const TDateTime &rhs)
	{
		value_ -= rhs.value_;
		return *this;
	}

	double Val() const { return value_; }  //!< シム独自: 明示アクセサ

private:
	double value_;
};

/// TDate / TTime は TDateTime と同一実体 (C++Builder の System.hpp と同じ扱い)
using TDate = TDateTime;
using TTime = TDateTime;

//---------------------------------------------------------------------------
// 現在時刻
//---------------------------------------------------------------------------
TDateTime Now();
TDateTime Date();
TDateTime Time();

//---------------------------------------------------------------------------
// エンコード / デコード
//---------------------------------------------------------------------------
TDateTime EncodeDate(unsigned short year, unsigned short month, unsigned short day);
TDateTime EncodeTime(unsigned short hour, unsigned short min, unsigned short sec, unsigned short msec);
void DecodeDate(const TDateTime &dt, unsigned short &year, unsigned short &month, unsigned short &day);
void DecodeTime(const TDateTime &dt, unsigned short &hour, unsigned short &min, unsigned short &sec,
                 unsigned short &msec);

//---------------------------------------------------------------------------
// 文字列変換
//---------------------------------------------------------------------------
/// FormatDateTime 互換。対応トークン: y/yy/yyyy, m/mm, d/dd, h/hh, n/nn, s/ss, z/zzz。
/// ' または " で囲んだ区間はリテラルとして通す (Delphi の書式仕様)。
UnicodeString FormatDateTime(const UnicodeString &format, const TDateTime &dt);
UnicodeString DateTimeToStr(const TDateTime &dt);  //!< 既定書式 "yyyy/mm/dd hh:nn:ss" を使う (下記【注意】参照)
TDateTime StrToDateTime(const UnicodeString &s);   //!< 失敗時 EConvertError 相当 → std::invalid_argument
bool TryStrToDateTime(const UnicodeString &s, TDateTime &result);

//---------------------------------------------------------------------------
// Win32 SYSTEMTIME との相互変換
//---------------------------------------------------------------------------
void DateTimeToSystemTime(const TDateTime &dt, TSystemTime &systemTime);
TDateTime SystemTimeToDateTime(const TSystemTime &systemTime);

//---------------------------------------------------------------------------
// 加減算・成分抽出
//---------------------------------------------------------------------------
TDateTime IncDay(const TDateTime &dt, int numberOfDays = 1);
TDateTime IncMonth(const TDateTime &dt, int numberOfMonths = 1);
TDateTime IncYear(const TDateTime &dt, int numberOfYears = 1);

unsigned short YearOf(const TDateTime &dt);
unsigned short MonthOf(const TDateTime &dt);
unsigned short DayOf(const TDateTime &dt);
unsigned short HourOf(const TDateTime &dt);

/// シム独自の追加: src/UserFunc.cpp (Phase 0 対象外) が DaysInMonth を使用しているため
/// 契約を先取りして用意する (実装は EncodeDate/DecodeDate と同じ暦計算に基づく)。
unsigned short DaysInMonth(const TDateTime &dt);

#endif  // NYANFI_COMPAT_DATETIME_H
