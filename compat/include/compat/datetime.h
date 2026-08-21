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
 * 依存: compat/config.h, compat/types.h, compat/ustring.h のみ。compat/sysutils.h には
 * 依存しない (sysutils.h 側が本ヘッダを include するため、循環を避ける)。
 * TSearchRec (sysutils.h) は TimeStamp フィールドとして TDateTime を持つ。
 */
#ifndef NYANFI_COMPAT_DATETIME_H
#define NYANFI_COMPAT_DATETIME_H

#include "compat/config.h"
#include "compat/types.h"
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
/**
 * @brief System.SysUtils::TFormatSettings 相当 (日付・時刻の名前表)
 * @details 実測: `src/UserFunc.cpp:449` が
 *          `remove_text(fmt, "$EN")? TFormatSettings::Create("en-US") : TFormatSettings::Create()`
 *          として **ユーザ書式に `$EN` が付いていたら英語の曜日名・月名で出す**
 *          ために使う。`src/task_thread.cpp:236,1882` は既定のまま
 *          `FormatDateTime("hh:nn:ss.zzz ", Now(), fs)` に渡すだけ。
 *
 *          名前は `GetLocaleInfoEx` から取る。曜日の添字は **0 = 日曜**
 *          (Delphi の `ShortDayNames[1]` = 日曜 に合わせた 0 始まり)。
 *          Windows の `LOCALE_SDAYNAME1` は月曜なので、詰め替えでずらしている。
 */
struct TFormatSettings {
	UnicodeString ShortDayNames[7];    //!< [0]=日 .. [6]=土
	UnicodeString LongDayNames[7];
	UnicodeString ShortMonthNames[12]; //!< [0]=1月 .. [11]=12月
	UnicodeString LongMonthNames[12];
	UnicodeString TimeAMString;
	UnicodeString TimePMString;

	/// 利用者の既定ロケールで作る
	static TFormatSettings Create();
	/// ロケール名を指定して作る ("en-US" など)
	static TFormatSettings Create(const UnicodeString &localeName);
};

/// FormatDateTime 互換。対応トークン:
///   y/yy/yyyy, m/mm (月番号), mmm/mmmm (月名), d/dd (日), ddd/dddd (曜日名),
///   h/hh, n/nn, s/ss, z/zzz, ampm/am/pm
/// ' または " で囲んだ区間はリテラルとして通す (Delphi の書式仕様)。
///
/// @note 名前を使うトークン (mmm/mmmm/ddd/dddd/ampm) は TFormatSettings を
///       渡さない版では**利用者の既定ロケール**の名前になる (Delphi と同じ)。
UnicodeString FormatDateTime(const UnicodeString &format, const TDateTime &dt);
UnicodeString FormatDateTime(const UnicodeString &format, const TDateTime &dt,
                              const TFormatSettings &settings);
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

//---------------------------------------------------------------------------
// 日付・時刻の切り出し (System.DateUtils)
//
// 実呼び出し箇所 (grep 実測):
//   Today       8 (src/Global.cpp:5407,14133,14147,14157 / src/FindDlg.cpp:238,538)
//   DateOf      2 (src/RenDlg.cpp:1176,1183)
//   IsToday     1 (src/UserFunc.cpp:429)
//---------------------------------------------------------------------------
/// 日付部分だけを取り出す (Delphi の DateOf = Trunc)。
/// 1899-12-30 より前 (負値) では Trunc が 0 方向に丸めるため Delphi と同じく
/// 直感に反する結果になる。対象コードは現代日付しか扱わないのでそのままにする。
TDateTime DateOf(const TDateTime &dt);
/// 時刻部分だけを取り出す (Delphi の TimeOf = Frac)
TDateTime TimeOf(const TDateTime &dt);
/// 今日の日付 (時刻は 0:00)。Delphi の Today は Date と同じ
TDateTime Today();
/// 同じ日か。Delphi は IncDay を使った範囲判定だが、ここでは DateOf の一致で
/// 判定する (正の TDateTime では等価)
bool IsSameDay(const TDateTime &value, const TDateTime &basis);
/// 今日か
bool IsToday(const TDateTime &value);

//---------------------------------------------------------------------------
// 加算 (System.DateUtils)
//
// 実呼び出し箇所: IncHour 1 (src/RenDlg.cpp:1183)
//---------------------------------------------------------------------------
/// ミリ秒単位で加算する。Delphi の IncMilliSecond と同じく、負の TDateTime では
/// 符号を反転して加算する (Delphi 実装をそのまま踏襲)
TDateTime IncMilliSecond(const TDateTime &dt, Int64 numberOfMilliSeconds = 1);
TDateTime IncSecond(const TDateTime &dt, Int64 numberOfSeconds = 1);
TDateTime IncMinute(const TDateTime &dt, Int64 numberOfMinutes = 1);
TDateTime IncHour(const TDateTime &dt, Int64 numberOfHours = 1);

//---------------------------------------------------------------------------
// 期間 (System.DateUtils)
//
// 実呼び出し箇所:
//   MilliSecondsBetween     1 (src/GenInfDlg.cpp:768)
//   DaysBetween             2 (src/FindDlg.cpp:238,538)
//   WithinPastMilliSeconds  8 (src/Global.cpp:3624,16080,16418 / src/SameDlg.cpp:84 /
//                              src/usr_excmd.cpp:49 / src/MainFrm.cpp:14632,16648 /
//                              src/task_thread.cpp:1721)
//---------------------------------------------------------------------------
/// TDateTime を「1899-12-30 0:00 からの通算ミリ秒」に直す (Delphi の
/// DateTimeToMilliseconds 相当)。丸めは四捨五入
Int64 DateTimeToMilliseconds(const TDateTime &dt);

/// 2 時点の差 (ミリ秒)。Delphi と同じく **常に非負** (絶対値)
Int64 MilliSecondsBetween(const TDateTime &aNow, const TDateTime &aThen);
/// 2 時点の差 (秒)。切り捨て、常に非負
Int64 SecondsBetween(const TDateTime &aNow, const TDateTime &aThen);
/// 2 時点の差 (日)。切り捨て、常に非負
int DaysBetween(const TDateTime &aNow, const TDateTime &aThen);

/**
 * @brief 2 時点の差が指定ミリ秒以内か
 * @details Delphi の実装 `MilliSecondsBetween(ANow, AThen) <= AMilliSeconds`
 *          をそのまま写している (境界値は **含む**)。前後どちらでも真になる
 *          (関数名の "Past" に反するが、これが RTL の挙動)。
 *          src では「タイムスタンプの許容誤差 (TimeTolerance、既定 2000ms)
 *          の範囲で同一とみなす」判定に使われている。
 * @param aNow          比較する時刻の片方
 * @param aThen         比較する時刻のもう片方
 * @param aMilliSeconds 許容するミリ秒
 * @return 差が aMilliSeconds 以下なら true
 */
bool WithinPastMilliSeconds(const TDateTime &aNow, const TDateTime &aThen, Int64 aMilliSeconds);

//---------------------------------------------------------------------------
// 比較 (System.DateUtils) — 戻り値は System.Types の TValueRelationship
//
// 実呼び出し箇所:
//   CompareDate      5 (src/Global.cpp:5677,6384,6999 / src/UserFunc.cpp:516 /
//                       src/task_thread.cpp:746)
//   CompareDateTime  5 (src/Global.cpp:2329,2335 / src/usr_excmd.cpp:1147,1817 /
//                       src/MainFrm.cpp:4299)
//   CompareTime      1 (src/Global.cpp:5714)
//---------------------------------------------------------------------------
/// 1 ミリ秒 (TDateTime の日単位表現)。Delphi の OneMillisecond 相当
constexpr double OneMillisecond = 1.0 / 86400000.0;

bool SameDateTime(const TDateTime &a, const TDateTime &b);  //!< 差が 1ms 未満か
bool SameTime(const TDateTime &a, const TDateTime &b);      //!< 時刻部分の差が 1ms 未満か

/// 日付部分だけを比べる
TValueRelationship CompareDate(const TDateTime &a, const TDateTime &b);
/// 日時を 1ms の分解能で比べる
TValueRelationship CompareDateTime(const TDateTime &a, const TDateTime &b);
/// 時刻部分だけを 1ms の分解能で比べる
TValueRelationship CompareTime(const TDateTime &a, const TDateTime &b);

//---------------------------------------------------------------------------
// 名前空間つきの呼び出しへの対応
//
// src には `Dateutils::CompareDateTime(...)` (src/Global.cpp:2329) と
// `System::Dateutils::CompareDate(...)` (src/UserFunc.cpp:516,
// src/task_thread.cpp:746) の両方の書き方がある。C++Builder が生成する
// hpp の名前空間名は Pascal 単位名の先頭大文字だけを残した `Dateutils`
// (小文字の u) なので、それに合わせる。
//---------------------------------------------------------------------------
namespace System {
namespace Dateutils {
using ::CompareDate;
using ::CompareDateTime;
using ::CompareTime;
using ::DateOf;
using ::DateTimeToMilliseconds;
using ::DayOf;
using ::DaysBetween;
using ::DaysInMonth;
using ::HourOf;
using ::IncDay;
using ::IncHour;
using ::IncMilliSecond;
using ::IncMinute;
using ::IncMonth;
using ::IncSecond;
using ::IncYear;
using ::IsSameDay;
using ::IsToday;
using ::MilliSecondsBetween;
using ::MonthOf;
using ::SameDateTime;
using ::SameTime;
using ::SecondsBetween;
using ::TimeOf;
using ::Today;
using ::WithinPastMilliSeconds;
using ::YearOf;
}  // namespace Dateutils
using namespace Dateutils;
}  // namespace System

namespace Dateutils = System::Dateutils;
namespace DateUtils = System::Dateutils;

#endif  // NYANFI_COMPAT_DATETIME_H
