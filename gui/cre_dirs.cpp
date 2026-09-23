/**
 * @file gui/cre_dirs.cpp
 * @brief gui/cre_dirs.h の実装
 */
#include "gui/cre_dirs.h"

#include "usr_file_ex.h"

namespace cre_dirs {

//---------------------------------------------------------------------------
DateUnit ResolveDateUnit(const UnicodeString &format)
{
	// VCL CreDirsDlg.cpp:141-142 の順序を保つ。大文字小文字は区別する。
	if (ContainsStr(format, _T("d"))) return DateUnit::Day;
	if (ContainsStr(format, _T("m"))) return DateUnit::Month;
	return DateUnit::Year;
}

//---------------------------------------------------------------------------
bool CanAddSerial(const UnicodeString &start, int increment)
{
	return !start.IsEmpty() && increment > 0;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> AddSerial(const std::vector<UnicodeString> &lines,
                                     int start, int increment, bool before,
                                     int number_width)
{
	std::vector<UnicodeString> out = lines;
	if (number_width <= 0) number_width = 1;
	int number = start;
	for (UnicodeString &line : out) {
		UnicodeString serial;
		serial.sprintf(_T("%0*u"), number_width, static_cast<unsigned int>(number));
		line = before ? serial + line : line + serial;
		number += increment;
	}
	return out;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> AddText(const std::vector<UnicodeString> &lines,
                                   const UnicodeString &text, bool before)
{
	std::vector<UnicodeString> out = lines;
	for (UnicodeString &line : out) line = before ? text + line : line + text;
	return out;
}

//---------------------------------------------------------------------------
bool AddDate(const std::vector<UnicodeString> &lines, const TDateTime &date,
             const UnicodeString &format, bool before,
             std::vector<UnicodeString> &out, UnicodeString &error_out)
{
	out.clear();
	error_out = EmptyStr;
	if (format.IsEmpty()) {
		error_out = _T("日付の書式を入力してください");
		return false;
	}

	try {
		const DateUnit unit = ResolveDateUnit(format);
		TDateTime current = date;
		for (const UnicodeString &line : lines) {
			const UnicodeString rendered = FormatDateTime(format, current);
			out.push_back(before ? rendered + line : line + rendered);
			if (unit == DateUnit::Day) current = IncDay(current, 1);
			else if (unit == DateUnit::Month) current = IncMonth(current, 1);
			else current = IncYear(current, 1);
		}
		return true;
	}
	catch (...) {
		out.clear();
		error_out = _T("日付の書式または日付が不正です");
		return false;
	}
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> CreatableEntries(const std::vector<UnicodeString> &lines)
{
	std::vector<UnicodeString> out;
	for (const UnicodeString &raw : lines) {
		const UnicodeString line = raw.Trim();
		if (line.IsEmpty() || ExtractFileName(line).IsEmpty()) continue;
		out.push_back(line);
	}
	return out;
}

//---------------------------------------------------------------------------
Validation ValidateEntries(const std::vector<UnicodeString> &lines)
{
	Validation result;
	result.total = static_cast<int>(lines.size());
	for (const UnicodeString &raw : lines) {
		const UnicodeString line = raw.Trim();
		if (line.IsEmpty() || ExtractFileName(line).IsEmpty()) {
			result.blank++;
		}
		else {
			result.creatable++;
		}
	}
	result.valid = result.creatable > 0;
	return result;
}

}  // namespace cre_dirs
