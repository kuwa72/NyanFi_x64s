/**
 * @file compat/src/sysutils.cpp
 * @brief compat/sysutils.h の実装
 *
 * 実装方針:
 * - UpperCase/LowerCase と SameText 系は「ASCII のみの大小文字無視」で統一する
 *   (実測 SameText 179 箇所が最多のため、ここを誤ると影響が大きい)。
 * - ustring.h の UnicodeString は別担当が並行実装中のため、UpperCase() /
 *   Trim() などのメンバ関数には依存せず、c_str()/Length()/SubString() など
 *   ustring.h に明記された契約だけを使って自前で文字列処理する。
 */
#include "compat/sysutils.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cwchar>
#include <cwctype>
#include <vector>

namespace {

//---------------------------------------------------------------------------
// ASCII 限定の大小文字変換 (UpperCase/LowerCase/SameText 系で共有)
//---------------------------------------------------------------------------
inline wchar_t AsciiUpperChar(wchar_t c)
{
	return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - 32) : c;
}

inline wchar_t AsciiLowerChar(wchar_t c)
{
	return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + 32) : c;
}

//---------------------------------------------------------------------------
// パス関数共通で使う区切り文字集合
//---------------------------------------------------------------------------
const UnicodeString kPathDriveDelims = UnicodeString(PathDelim) + DriveDelim;
const UnicodeString kExtDelims = UnicodeString(L'.') + PathDelim + DriveDelim;

}  // namespace

//===========================================================================
// Abort
//===========================================================================
[[noreturn]] void Abort()
{
	throw EAbort();
}

//===========================================================================
// グローバル定数
//===========================================================================
const UnicodeString EmptyStr;
const UnicodeString sLineBreak = L"\r\n";

//===========================================================================
// 数値 <-> 文字列
//===========================================================================
UnicodeString IntToStr(Int64 value)
{
	wchar_t buf[32];
	std::swprintf(buf, 32, L"%lld", static_cast<long long>(value));
	return UnicodeString(buf);
}

UnicodeString UIntToStr(UInt64 value)
{
	wchar_t buf[32];
	std::swprintf(buf, 32, L"%llu", static_cast<unsigned long long>(value));
	return UnicodeString(buf);
}

UnicodeString IntToHex(Int64 value, int digits)
{
	if (digits < 1) digits = 1;
	wchar_t buf[32];
	std::swprintf(buf, 32, L"%0*llX", digits, static_cast<unsigned long long>(value));
	return UnicodeString(buf);
}

namespace {

/// 前後の空白を許した上で文字列全体を消費できた場合のみ ok=true にする
long long ParseIntStrict(const UnicodeString &s, bool &ok)
{
	const wchar_t *p = s.c_str();
	while (*p == L' ' || *p == L'\t') ++p;
	int base = 10;
	const wchar_t *start = p;
	if (*p == L'$') {  // Delphi の16進プレフィックス
		++p;
		base = 16;
		start = p;
	}
	wchar_t *endp = nullptr;
	long long v = std::wcstoll(start, &endp, base);
	while (*endp == L' ' || *endp == L'\t') ++endp;
	ok = (endp != start) && (*endp == L'\0');
	return v;
}

double ParseFloatStrict(const UnicodeString &s, bool &ok)
{
	const wchar_t *p = s.c_str();
	while (*p == L' ' || *p == L'\t') ++p;
	wchar_t *endp = nullptr;
	double v = std::wcstod(p, &endp);
	while (*endp == L' ' || *endp == L'\t') ++endp;
	ok = (endp != p) && (*endp == L'\0');
	return v;
}

}  // namespace

int StrToInt(const UnicodeString &s)
{
	bool ok = false;
	long long v = ParseIntStrict(s, ok);
	if (!ok) throw EConvertError(UnicodeString(L"'") + s + L"' is not a valid integer value");
	return static_cast<int>(v);
}

int StrToIntDef(const UnicodeString &s, int defValue)
{
	bool ok = false;
	long long v = ParseIntStrict(s, ok);
	return ok ? static_cast<int>(v) : defValue;
}

Int64 StrToInt64(const UnicodeString &s)
{
	bool ok = false;
	long long v = ParseIntStrict(s, ok);
	if (!ok) throw EConvertError(UnicodeString(L"'") + s + L"' is not a valid integer value");
	return v;
}

Int64 StrToInt64Def(const UnicodeString &s, Int64 defValue)
{
	bool ok = false;
	long long v = ParseIntStrict(s, ok);
	return ok ? v : defValue;
}

double StrToFloat(const UnicodeString &s)
{
	bool ok = false;
	double v = ParseFloatStrict(s, ok);
	if (!ok) throw EConvertError(UnicodeString(L"'") + s + L"' is not a valid floating point value");
	return v;
}

double StrToFloatDef(const UnicodeString &s, double defValue)
{
	bool ok = false;
	double v = ParseFloatStrict(s, ok);
	return ok ? v : defValue;
}

UnicodeString FloatToStr(double value)
{
	// 【簡略化】実 RTL は最短往復表現+ロケール依存の小数点記号を使うが、
	// ここでは "%.15g" (小数点は '.' 固定) で代替する。
	wchar_t buf[64];
	std::swprintf(buf, 64, L"%.15g", value);
	return UnicodeString(buf);
}

namespace {

std::wstring InsertThousandsSeparators(const std::wstring &digits)
{
	std::wstring out;
	int n = static_cast<int>(digits.size());
	for (int i = 0; i < n; ++i) {
		if (i > 0 && (n - i) % 3 == 0) out.push_back(L',');
		out.push_back(digits[i]);
	}
	return out;
}

}  // namespace

UnicodeString FormatFloat(const UnicodeString &format, double value)
{
	// 【簡略化】実測: ",0" (桁区切りの整数) のみが対象コードで使われている。
	// 正負/ゼロ別の書式 (';' 区切り) や通貨記号は未対応。
	std::wstring fmt(format.c_str(), format.Length());
	std::size_t dot = fmt.find(L'.');
	std::wstring intFmt = (dot == std::wstring::npos) ? fmt : fmt.substr(0, dot);
	std::wstring decFmt = (dot == std::wstring::npos) ? std::wstring() : fmt.substr(dot + 1);

	bool grouping = intFmt.find(L',') != std::wstring::npos;
	int minIntDigits = 0;
	for (wchar_t c : intFmt) {
		if (c == L'0') ++minIntDigits;
	}
	int decDigitsMax = 0, decDigitsMin = 0;
	for (wchar_t c : decFmt) {
		if (c == L'0') {
			++decDigitsMax;
			++decDigitsMin;
		}
		else if (c == L'#') {
			++decDigitsMax;
		}
	}

	bool neg = value < 0.0;
	value = std::fabs(value);
	double scale = std::pow(10.0, decDigitsMax);
	double rounded = std::floor(value * scale + 0.5) / scale;

	long long intPart = static_cast<long long>(rounded);
	double fracPart = rounded - static_cast<double>(intPart);

	std::wstring intStr = std::to_wstring(intPart);
	if (static_cast<int>(intStr.size()) < minIntDigits) {
		intStr = std::wstring(static_cast<std::size_t>(minIntDigits - intStr.size()), L'0') + intStr;
	}
	if (grouping) intStr = InsertThousandsSeparators(intStr);

	std::wstring result = intStr;
	if (decDigitsMax > 0) {
		long long fracDigits = static_cast<long long>(std::llround(fracPart * scale));
		wchar_t buf[32];
		std::swprintf(buf, 32, L"%0*lld", decDigitsMax, fracDigits);
		std::wstring fracStr = buf;
		while (static_cast<int>(fracStr.size()) > decDigitsMin && fracStr.back() == L'0') fracStr.pop_back();
		if (!fracStr.empty()) result += L"." + fracStr;
	}
	if (neg && (intPart != 0 || fracPart != 0.0)) result = L"-" + result;
	return UnicodeString(result);
}

UnicodeString Format(const UnicodeString &format, ...)
{
	// 【簡略化】実測: src/ 全体で Format( の直接呼び出しは 0 件だった
	// (ヘッダコメントの "9" はおそらく FormatFloat/FormatDateTime を含めた
	// 誤集計)。呼び出し実績が無いため、printf 互換の素朴な実装で代替する。
	// %s は UnicodeString/wchar_t* 引数を想定して %ls に読み替える。
	// Delphi 独自のインデックス指定 (%1:d 等) は非対応。
	std::wstring fmt;
	fmt.reserve(static_cast<std::size_t>(format.Length()) + 8);
	const wchar_t *p = format.c_str();
	const wchar_t *end = p + format.Length();
	while (p < end) {
		if (*p == L'%') {
			fmt.push_back(*p++);
			while (p < end && std::wcschr(L"-+ 0123456789.*", *p) != nullptr) fmt.push_back(*p++);
			if (p < end) {
				if (*p == L's') {
					fmt += L"ls";
					++p;
				}
				else {
					fmt.push_back(*p++);
				}
			}
			continue;
		}
		fmt.push_back(*p++);
	}

	va_list args;
	va_start(args, format);
	wchar_t buf[4096];
	int n = std::vswprintf(buf, 4096, fmt.c_str(), args);
	va_end(args);
	if (n < 0) return UnicodeString();
	return UnicodeString(buf, n);
}

//===========================================================================
// 文字列 (System.SysUtils)
//===========================================================================
UnicodeString Trim(const UnicodeString &s)
{
	int len = s.Length();
	int first = 0;
	while (first < len && std::iswspace(static_cast<wint_t>(s.c_str()[first]))) ++first;
	int last = len - 1;
	while (last >= first && std::iswspace(static_cast<wint_t>(s.c_str()[last]))) --last;
	if (first > last) return UnicodeString();
	return s.SubString(first + 1, last - first + 1);
}

UnicodeString TrimLeft(const UnicodeString &s)
{
	int len = s.Length();
	int first = 0;
	while (first < len && std::iswspace(static_cast<wint_t>(s.c_str()[first]))) ++first;
	return s.SubString(first + 1);
}

UnicodeString TrimRight(const UnicodeString &s)
{
	int len = s.Length();
	int last = len - 1;
	while (last >= 0 && std::iswspace(static_cast<wint_t>(s.c_str()[last]))) --last;
	if (last < 0) return UnicodeString();
	return s.SubString(1, last + 1);
}

UnicodeString UpperCase(const UnicodeString &s)
{
	std::wstring out(s.c_str(), static_cast<std::size_t>(s.Length()));
	for (wchar_t &c : out) c = AsciiUpperChar(c);
	return UnicodeString(out);
}

UnicodeString LowerCase(const UnicodeString &s)
{
	std::wstring out(s.c_str(), static_cast<std::size_t>(s.Length()));
	for (wchar_t &c : out) c = AsciiLowerChar(c);
	return UnicodeString(out);
}

UnicodeString AnsiUpperCase(const UnicodeString &s)
{
	std::wstring out(s.c_str(), static_cast<std::size_t>(s.Length()));
	if (!out.empty()) ::CharUpperBuffW(out.data(), static_cast<DWORD>(out.size()));
	return UnicodeString(out);
}

UnicodeString AnsiLowerCase(const UnicodeString &s)
{
	std::wstring out(s.c_str(), static_cast<std::size_t>(s.Length()));
	if (!out.empty()) ::CharLowerBuffW(out.data(), static_cast<DWORD>(out.size()));
	return UnicodeString(out);
}

int CompareStr(const UnicodeString &a, const UnicodeString &b)
{
	int la = a.Length(), lb = b.Length();
	int n = std::min(la, lb);
	for (int i = 0; i < n; ++i) {
		wchar_t ca = a.c_str()[i], cb = b.c_str()[i];
		if (ca != cb) return (ca < cb) ? -1 : 1;
	}
	if (la != lb) return (la < lb) ? -1 : 1;
	return 0;
}

int CompareText(const UnicodeString &a, const UnicodeString &b)
{
	int la = a.Length(), lb = b.Length();
	int n = std::min(la, lb);
	for (int i = 0; i < n; ++i) {
		wchar_t ca = AsciiUpperChar(a.c_str()[i]), cb = AsciiUpperChar(b.c_str()[i]);
		if (ca != cb) return (ca < cb) ? -1 : 1;
	}
	if (la != lb) return (la < lb) ? -1 : 1;
	return 0;
}

bool SameStr(const UnicodeString &a, const UnicodeString &b)
{
	return CompareStr(a, b) == 0;
}

bool SameText(const UnicodeString &a, const UnicodeString &b)
{
	return CompareText(a, b) == 0;
}

UnicodeString StringOfChar(wchar_t ch, int count)
{
	if (count <= 0) return UnicodeString();
	return UnicodeString(std::wstring(static_cast<std::size_t>(count), ch));
}

UnicodeString StringReplace(const UnicodeString &s, const UnicodeString &oldPattern,
                            const UnicodeString &newPattern, bool replaceAll, bool ignoreCase)
{
	if (oldPattern.IsEmpty()) return s;
	int lens = s.Length(), lenOld = oldPattern.Length();

	std::wstring result;
	int i = 0;
	bool replacedOnce = false;
	while (i < lens) {
		bool doReplace = false;
		if ((replaceAll || !replacedOnce) && i + lenOld <= lens) {
			bool match = true;
			for (int k = 0; k < lenOld; ++k) {
				wchar_t a = s.c_str()[i + k], b = oldPattern.c_str()[k];
				if (ignoreCase) {
					a = AsciiUpperChar(a);
					b = AsciiUpperChar(b);
				}
				if (a != b) {
					match = false;
					break;
				}
			}
			doReplace = match;
		}
		if (doReplace) {
			result.append(newPattern.c_str(), static_cast<std::size_t>(newPattern.Length()));
			i += lenOld;
			replacedOnce = true;
			if (!replaceAll) {
				result.append(s.c_str() + i, static_cast<std::size_t>(lens - i));
				return UnicodeString(result);
			}
		}
		else {
			result.push_back(s.c_str()[i]);
			++i;
		}
	}
	return UnicodeString(result);
}

UnicodeString QuotedStr(const UnicodeString &s)
{
	return AnsiQuotedStr(s, L'\'');
}

UnicodeString AnsiQuotedStr(const UnicodeString &s, wchar_t quote)
{
	std::wstring out;
	out.push_back(quote);
	for (int i = 0; i < s.Length(); ++i) {
		wchar_t c = s.c_str()[i];
		out.push_back(c);
		if (c == quote) out.push_back(quote);
	}
	out.push_back(quote);
	return UnicodeString(out);
}

UnicodeString AnsiExtractQuotedStr(const wchar_t *&src, wchar_t quote)
{
	std::wstring result;
	if (src == nullptr || *src != quote) return UnicodeString();
	++src;
	while (*src) {
		if (*src == quote) {
			if (*(src + 1) == quote) {
				result.push_back(quote);
				src += 2;
				continue;
			}
			++src;
			break;
		}
		result.push_back(*src++);
	}
	return UnicodeString(result);
}

UnicodeString SysErrorMessage(DWORD errorCode)
{
	LPWSTR buf = nullptr;
	DWORD len = ::FormatMessageW(
	    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
	    errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
	if (len == 0 || buf == nullptr) return UnicodeString();
	while (len > 0 && (buf[len - 1] == L'\r' || buf[len - 1] == L'\n')) --len;
	UnicodeString result(buf, static_cast<int>(len));
	::LocalFree(buf);
	return result;
}

//===========================================================================
// 文字列 (System.StrUtils)
//===========================================================================
bool ContainsStr(const UnicodeString &text, const UnicodeString &sub)
{
	return text.Pos(sub) != 0;
}

bool ContainsText(const UnicodeString &text, const UnicodeString &sub)
{
	int lt = text.Length(), ls = sub.Length();
	if (ls == 0) return true;
	if (ls > lt) return false;
	for (int start = 0; start <= lt - ls; ++start) {
		bool match = true;
		for (int i = 0; i < ls; ++i) {
			if (AsciiUpperChar(text.c_str()[start + i]) != AsciiUpperChar(sub.c_str()[i])) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

bool StartsStr(const UnicodeString &sub, const UnicodeString &text)
{
	int ls = sub.Length(), lt = text.Length();
	if (ls > lt) return false;
	for (int i = 0; i < ls; ++i) {
		if (sub.c_str()[i] != text.c_str()[i]) return false;
	}
	return true;
}

bool StartsText(const UnicodeString &sub, const UnicodeString &text)
{
	int ls = sub.Length(), lt = text.Length();
	if (ls > lt) return false;
	for (int i = 0; i < ls; ++i) {
		if (AsciiUpperChar(sub.c_str()[i]) != AsciiUpperChar(text.c_str()[i])) return false;
	}
	return true;
}

bool EndsStr(const UnicodeString &sub, const UnicodeString &text)
{
	int ls = sub.Length(), lt = text.Length();
	if (ls > lt) return false;
	int offset = lt - ls;
	for (int i = 0; i < ls; ++i) {
		if (sub.c_str()[i] != text.c_str()[offset + i]) return false;
	}
	return true;
}

bool EndsText(const UnicodeString &sub, const UnicodeString &text)
{
	int ls = sub.Length(), lt = text.Length();
	if (ls > lt) return false;
	int offset = lt - ls;
	for (int i = 0; i < ls; ++i) {
		if (AsciiUpperChar(sub.c_str()[i]) != AsciiUpperChar(text.c_str()[offset + i])) return false;
	}
	return true;
}

UnicodeString ReplaceStr(const UnicodeString &text, const UnicodeString &from, const UnicodeString &to)
{
	return StringReplace(text, from, to, true, false);
}

UnicodeString ReplaceText(const UnicodeString &text, const UnicodeString &from, const UnicodeString &to)
{
	return StringReplace(text, from, to, true, true);
}

UnicodeString LeftStr(const UnicodeString &s, int count)
{
	if (count <= 0) return UnicodeString();
	int n = std::min(count, s.Length());
	return s.SubString(1, n);
}

UnicodeString RightStr(const UnicodeString &s, int count)
{
	if (count <= 0) return UnicodeString();
	int len = s.Length();
	int n = std::min(count, len);
	return s.SubString(len - n + 1, n);
}

UnicodeString MidStr(const UnicodeString &s, int start, int count)
{
	int len = s.Length();
	if (start < 1) {
		count += (start - 1);
		start = 1;
	}
	if (start > len || count <= 0) return UnicodeString();
	int n = std::min(count, len - start + 1);
	return s.SubString(start, n);
}

UnicodeString DupeString(const UnicodeString &s, int count)
{
	if (count <= 0 || s.IsEmpty()) return UnicodeString();
	std::wstring out;
	out.reserve(static_cast<std::size_t>(s.Length()) * static_cast<std::size_t>(count));
	for (int i = 0; i < count; ++i) out.append(s.c_str(), static_cast<std::size_t>(s.Length()));
	return UnicodeString(out);
}

UnicodeString ReverseString(const UnicodeString &s)
{
	std::wstring out(s.c_str(), static_cast<std::size_t>(s.Length()));
	std::reverse(out.begin(), out.end());
	return UnicodeString(out);
}

int PosEx(const UnicodeString &sub, const UnicodeString &text, int offset)
{
	int lt = text.Length(), ls = sub.Length();
	if (offset < 1) offset = 1;
	if (ls == 0 || offset > lt - ls + 1) return 0;
	for (int start = offset; start <= lt - ls + 1; ++start) {
		bool match = true;
		for (int k = 0; k < ls; ++k) {
			if (text.c_str()[start - 1 + k] != sub.c_str()[k]) {
				match = false;
				break;
			}
		}
		if (match) return start;
	}
	return 0;
}

bool MatchStr(const UnicodeString &s, const TStringDynArray &values)
{
	for (int i = 0; i < values.Length; ++i) {
		if (SameStr(s, values[i])) return true;
	}
	return false;
}

bool MatchText(const UnicodeString &s, const TStringDynArray &values)
{
	for (int i = 0; i < values.Length; ++i) {
		if (SameText(s, values[i])) return true;
	}
	return false;
}

UnicodeString IfThen(bool condition, const UnicodeString &whenTrue, const UnicodeString &whenFalse)
{
	return condition ? whenTrue : whenFalse;
}

TStringDynArray SplitString(const UnicodeString &s, const UnicodeString &delimiters)
{
	if (delimiters.IsEmpty()) {
		TStringDynArray result(1);
		result[0] = s;
		return result;
	}
	int len = s.Length();
	std::vector<int> cutPositions;
	for (int i = 1; i <= len; ++i) {
		if (delimiters.Pos(s[i]) != 0) cutPositions.push_back(i);
	}
	TStringDynArray result(static_cast<int>(cutPositions.size()) + 1);
	int startIdx = 1;
	int cur = 0;
	for (int pos : cutPositions) {
		result[cur++] = s.SubString(startIdx, pos - startIdx);
		startIdx = pos + 1;
	}
	result[cur] = s.SubString(startIdx, len - startIdx + 1);
	return result;
}

//===========================================================================
// パス・ファイル (System.SysUtils / System.IOUtils 相当)
//===========================================================================
UnicodeString ExtractFileName(const UnicodeString &fileName)
{
	int i = fileName.LastDelimiter(kPathDriveDelims);
	return fileName.SubString(i + 1);
}

UnicodeString ExtractFilePath(const UnicodeString &fileName)
{
	int i = fileName.LastDelimiter(kPathDriveDelims);
	return fileName.SubString(1, i);
}

UnicodeString ExtractFileDir(const UnicodeString &fileName)
{
	int i = fileName.LastDelimiter(kPathDriveDelims);
	if (i > 1 && fileName[i] == PathDelim && !fileName.IsDelimiter(kPathDriveDelims, i - 1) &&
	    (i != 3 || fileName[1] != PathDelim)) {
		--i;
	}
	return fileName.SubString(1, i);
}

UnicodeString ExtractFileExt(const UnicodeString &fileName)
{
	int i = fileName.LastDelimiter(kExtDelims);
	if (i > 0 && fileName[i] == L'.') return fileName.SubString(i);
	return UnicodeString();
}

UnicodeString ExtractFileDrive(const UnicodeString &fileName)
{
	int len = fileName.Length();
	if (len >= 2 && fileName[2] == DriveDelim) {
		return fileName.SubString(1, 2);
	}
	if (len >= 2 && fileName.IsPathDelimiter(1) && fileName.IsPathDelimiter(2)) {
		int i = 3;
		while (i <= len && !fileName.IsPathDelimiter(i)) ++i;
		++i;
		while (i <= len && !fileName.IsPathDelimiter(i)) ++i;
		return fileName.SubString(1, i - 1);
	}
	return UnicodeString();
}

UnicodeString ChangeFileExt(const UnicodeString &fileName, const UnicodeString &extension)
{
	int i = fileName.LastDelimiter(kExtDelims);
	if (i <= 0 || fileName[i] != L'.') i = fileName.Length() + 1;
	return fileName.SubString(1, i - 1) + extension;
}

UnicodeString IncludeTrailingPathDelimiter(const UnicodeString &path)
{
	int len = path.Length();
	if (len == 0 || !path.IsPathDelimiter(len)) {
		return path + PathDelim;
	}
	return path;
}

UnicodeString ExcludeTrailingPathDelimiter(const UnicodeString &path)
{
	int len = path.Length();
	if (len > 0 && path.IsPathDelimiter(len)) {
		return path.SubString(1, len - 1);
	}
	return path;
}

UnicodeString ExtractRelativePath(const UnicodeString &baseName, const UnicodeString &destName)
{
	// 【簡略化】ドライブ (UNC 含む) が異なる場合は destName をそのまま返す点は
	// 実 RTL と同じ。共通ディレクトリの探索も実 RTL 相当だが、大文字小文字を
	// 区別しないディレクトリ名の特殊なケース (8.3 形式など) までは追わない。
	if (!SameText(ExtractFileDrive(baseName), ExtractFileDrive(destName))) {
		return destName;
	}
	UnicodeString baseDir = ExcludeTrailingPathDelimiter(ExtractFilePath(baseName));
	UnicodeString destDir = ExcludeTrailingPathDelimiter(ExtractFilePath(destName));
	UnicodeString destFile = ExtractFileName(destName);

	TStringDynArray baseParts = SplitString(baseDir, UnicodeString(PathDelim));
	TStringDynArray destParts = SplitString(destDir, UnicodeString(PathDelim));

	int common = 0;
	int maxCommon = std::min<int>(baseParts.Length, destParts.Length);
	while (common < maxCommon && SameText(baseParts[common], destParts[common])) ++common;

	std::wstring result;
	for (int i = common; i < static_cast<int>(baseParts.Length); ++i) result += L"..\\";
	for (int i = common; i < static_cast<int>(destParts.Length); ++i) {
		result.append(destParts[i].c_str(), static_cast<std::size_t>(destParts[i].Length()));
		result += L"\\";
	}
	result.append(destFile.c_str(), static_cast<std::size_t>(destFile.Length()));
	return UnicodeString(result);
}

UnicodeString ExpandFileName(const UnicodeString &fileName)
{
	wchar_t buf[32768];
	DWORD len = ::GetFullPathNameW(fileName.c_str(), 32768, buf, nullptr);
	if (len == 0 || len >= 32768) return fileName;
	return UnicodeString(buf, static_cast<int>(len));
}

bool IsPathDelimiter(const UnicodeString &s, int index)
{
	return index > 0 && index <= s.Length() && s[index] == PathDelim;
}

bool FileExists(const UnicodeString &fileName)
{
	DWORD attr = ::GetFileAttributesW(fileName.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirectoryExists(const UnicodeString &directory)
{
	DWORD attr = ::GetFileAttributesW(directory.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DeleteFile(const UnicodeString &fileName)
{
	return ::DeleteFileW(fileName.c_str()) != 0;
}

bool RenameFile(const UnicodeString &oldName, const UnicodeString &newName)
{
	return ::MoveFileW(oldName.c_str(), newName.c_str()) != 0;
}

bool CreateDir(const UnicodeString &dir)
{
	return ::CreateDirectoryW(dir.c_str(), nullptr) != 0;
}

bool RemoveDir(const UnicodeString &dir)
{
	return ::RemoveDirectoryW(dir.c_str()) != 0;
}

bool ForceDirectories(const UnicodeString &dir)
{
	UnicodeString d = ExcludeTrailingPathDelimiter(dir);
	if (d.IsEmpty()) return false;
	if (DirectoryExists(d)) return true;

	UnicodeString parent = ExcludeTrailingPathDelimiter(ExtractFilePath(d));
	if (!parent.IsEmpty() && !SameText(parent, d)) {
		if (!ForceDirectories(parent)) return false;
	}
	return CreateDir(d);
}

int FileGetAttr(const UnicodeString &fileName)
{
	DWORD attr = ::GetFileAttributesW(fileName.c_str());
	return (attr == INVALID_FILE_ATTRIBUTES) ? faInvalid : static_cast<int>(attr);
}

int FileSetAttr(const UnicodeString &fileName, int attr)
{
	return ::SetFileAttributesW(fileName.c_str(), static_cast<DWORD>(attr)) ? 0
	                                                                        : static_cast<int>(::GetLastError());
}

bool FileAge(const UnicodeString &fileName, TDateTime &fileDateTime)
{
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if (!::GetFileAttributesExW(fileName.c_str(), GetFileExInfoStandard, &data)) return false;
	FILETIME localTime;
	if (!::FileTimeToLocalFileTime(&data.ftLastWriteTime, &localTime)) return false;
	SYSTEMTIME st;
	if (!::FileTimeToSystemTime(&localTime, &st)) return false;
	fileDateTime = SystemTimeToDateTime(st);
	return true;
}

TDateTime FileAge(const UnicodeString &fileName)
{
	// 【注意】実 RTL のこの 1 引数版 (非推奨) は失敗時 Integer(-1) を返すが、
	// 本シムは契約上 TDateTime を返すため、失敗時は 0.0 (Delphi epoch) とする。
	TDateTime result;
	if (FileAge(fileName, result)) return result;
	return TDateTime(0.0);
}

UnicodeString GetCurrentDir()
{
	wchar_t buf[MAX_PATH];
	DWORD len = ::GetCurrentDirectoryW(MAX_PATH, buf);
	if (len == 0) return UnicodeString();
	if (len >= MAX_PATH) {
		std::wstring big(len, L'\0');
		DWORD len2 = ::GetCurrentDirectoryW(len, big.data());
		big.resize(len2);
		return UnicodeString(big);
	}
	return UnicodeString(buf, static_cast<int>(len));
}

bool SetCurrentDir(const UnicodeString &dir)
{
	return ::SetCurrentDirectoryW(dir.c_str()) != 0;
}

UnicodeString GetEnvironmentVariable(const UnicodeString &name)
{
	DWORD needed = ::GetEnvironmentVariableW(name.c_str(), nullptr, 0);
	if (needed == 0) return UnicodeString();
	std::wstring buf(needed, L'\0');
	DWORD written = ::GetEnvironmentVariableW(name.c_str(), buf.data(), needed);
	if (written == 0) return UnicodeString();
	buf.resize(written);
	return UnicodeString(buf);
}

//===========================================================================
// FindFirst / FindNext / FindClose
//===========================================================================
namespace {

constexpr int kFaSpecial =
    faHidden | faSysFile | faVolumeID | faDirectory | faArchive | faSymLink | faNormal;

bool AttrExcluded(const TSearchRec &rec)
{
	return (static_cast<int>(rec.FindData.dwFileAttributes) & rec.ExcludeAttrMask) != 0;
}

void FillFromFindData(TSearchRec &rec)
{
	rec.Name = UnicodeString(rec.FindData.cFileName);
	rec.Attr = static_cast<int>(rec.FindData.dwFileAttributes);
	rec.Size = (static_cast<Int64>(rec.FindData.nFileSizeHigh) << 32) |
	           static_cast<Int64>(rec.FindData.nFileSizeLow);

	FILETIME localTime;
	SYSTEMTIME st;
	if (::FileTimeToLocalFileTime(&rec.FindData.ftLastWriteTime, &localTime) &&
	    ::FileTimeToSystemTime(&localTime, &st)) {
		rec.TimeStamp = SystemTimeToDateTime(st);
		WORD dosDate = 0, dosTime = 0;
		if (::FileTimeToDosDateTime(&localTime, &dosDate, &dosTime)) {
			rec.Time = (static_cast<int>(dosDate) << 16) | dosTime;
		}
	}
}

}  // namespace

int FindFirst(const UnicodeString &path, int attr, TSearchRec &rec)
{
	// 実 RTL (System.SysUtils.FindFirst) と同じ ExcludeAttr = (not Attr) and faSpecial の
	// フィルタを再現する。faAnyFile (対象コードでの全 8 箇所の実測で唯一使われる値) では
	// ExcludeAttr = 0 になるため、"." / ".." を含む全エントリがそのまま返る
	// (呼び出し側は usr_file_ex.cpp のように ContainsStr("..", sr.Name) で自前除外している)。
	rec.ExcludeAttrMask = (~attr) & kFaSpecial;
	rec.FindHandle = ::FindFirstFileW(path.c_str(), &rec.FindData);
	if (rec.FindHandle == INVALID_HANDLE_VALUE) {
		return static_cast<int>(::GetLastError());
	}
	while (AttrExcluded(rec)) {
		if (!::FindNextFileW(rec.FindHandle, &rec.FindData)) {
			int err = static_cast<int>(::GetLastError());
			FindClose(rec);
			return err;
		}
	}
	FillFromFindData(rec);
	return 0;
}

int FindNext(TSearchRec &rec)
{
	do {
		if (!::FindNextFileW(rec.FindHandle, &rec.FindData)) {
			return static_cast<int>(::GetLastError());
		}
	} while (AttrExcluded(rec));
	FillFromFindData(rec);
	return 0;
}

void FindClose(TSearchRec &rec)
{
	if (rec.FindHandle != INVALID_HANDLE_VALUE) {
		::FindClose(rec.FindHandle);
		rec.FindHandle = INVALID_HANDLE_VALUE;
	}
}

//---------------------------------------------------------------------------
/**
 * @brief Delphi の自由関数 Pos
 * @details `Pos(SubStr, Str, Offset)` の形。1 始まりで、見つからなければ 0。
 *          実測: usr_str.cpp:953 が 1 文字ずつ順に探す用途で使っている。
 */
int Pos(const UnicodeString &sub, const UnicodeString &text, int offset)
{
	return PosEx(sub, text, offset);
}

//---------------------------------------------------------------------------
// ディスク容量 (System.SysUtils)
//---------------------------------------------------------------------------
namespace {

/// GetDiskFreeSpaceEx をまとめて呼ぶ。失敗なら false
///
/// drive は 0=カレント, 1=A, 2=B ...。**drive==0 では Delphi の実装と同じく
/// ルートパスに NULL を渡す** (カレントディレクトリのあるディスク)。
/// "カレントドライブの文字を採ってきて 'X:\\' を組み立てる" ようにすると、
/// カレントディレクトリが UNC パス (\\\\server\\share\\...) のときに
/// ドライブ文字が無くて失敗する。実際に WSL 上でテストを走らせたときに
/// カレントが \\\\wsl.localhost\\... になって -1 が返り、これで気づいた。
bool query_disk_space(Byte drive, ULARGE_INTEGER &availToCaller, ULARGE_INTEGER &total)
{
	wchar_t rootBuf[4] = {0};
	const wchar_t *root = nullptr;  // drive==0 はカレントディスク
	if (drive != 0) {
		if (drive > 26) return false;
		rootBuf[0] = static_cast<wchar_t>(L'A' + (drive - 1));
		rootBuf[1] = L':';
		rootBuf[2] = L'\\';
		rootBuf[3] = L'\0';
		root = rootBuf;
	}

	ULARGE_INTEGER freeTotal = {};
	availToCaller.QuadPart = 0;
	total.QuadPart = 0;
	// エラーダイアログ (リムーバブルの空ドライブ) を抑止する。Delphi の
	// DiskSize/DiskFree も SetErrorMode で同じことをしている
	const UINT prevMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);
	const BOOL ok = ::GetDiskFreeSpaceExW(root, &availToCaller, &total, &freeTotal);
	::SetErrorMode(prevMode);
	return ok != FALSE;
}

}  // namespace

Int64 DiskSize(Byte drive)
{
	ULARGE_INTEGER avail = {}, total = {};
	if (!query_disk_space(drive, avail, total)) return -1;
	return static_cast<Int64>(total.QuadPart);
}

Int64 DiskFree(Byte drive)
{
	ULARGE_INTEGER avail = {}, total = {};
	if (!query_disk_space(drive, avail, total)) return -1;
	return static_cast<Int64>(avail.QuadPart);
}

//---------------------------------------------------------------------------
// バージョン情報 (System.SysUtils)
//---------------------------------------------------------------------------
bool GetProductVersion(const UnicodeString &fileName, unsigned &major, unsigned &minor, unsigned &build)
{
	major = 0;
	minor = 0;
	build = 0;
	if (fileName.IsEmpty()) return false;

	DWORD handle = 0;
	const DWORD size = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
	if (size == 0) return false;

	std::vector<BYTE> buf(size);
	if (!::GetFileVersionInfoW(fileName.c_str(), handle, size, buf.data())) return false;

	VS_FIXEDFILEINFO *fi = nullptr;
	UINT len = 0;
	if (!::VerQueryValueW(buf.data(), L"\\", reinterpret_cast<LPVOID *>(&fi), &len)) return false;
	if (fi == nullptr || len < sizeof(VS_FIXEDFILEINFO)) return false;

	// ProductVersion (FileVersion ではない) の a.b.c.d のうち a/b/c を返す
	major = static_cast<unsigned>(HIWORD(fi->dwProductVersionMS));
	minor = static_cast<unsigned>(LOWORD(fi->dwProductVersionMS));
	build = static_cast<unsigned>(HIWORD(fi->dwProductVersionLS));
	return true;
}

//---------------------------------------------------------------------------
// OS バージョン (System.SysUtils の TOSVersion)
//---------------------------------------------------------------------------
namespace {

/// RtlGetVersion で真の OS バージョンを採る。互換性マニフェストの影響を
/// 受けないのが GetVersionEx との違い (ヘッダのコメント参照)
const RTL_OSVERSIONINFOW &real_os_version()
{
	static const RTL_OSVERSIONINFOW info = [] {
		RTL_OSVERSIONINFOW vi = {};
		vi.dwOSVersionInfoSize = sizeof(vi);

		using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
		if (HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll")) {
			auto fn = reinterpret_cast<RtlGetVersionFn>(
				reinterpret_cast<void *>(::GetProcAddress(ntdll, "RtlGetVersion")));
			if (fn != nullptr && fn(&vi) == 0) return vi;
		}

		// ntdll から採れない場合のフォールバック。GetVersionEx はマニフェスト
		// が無いと 6.2 で頭打ちになるが、何も返さないよりはよい
		OSVERSIONINFOW ovi = {};
		ovi.dwOSVersionInfoSize = sizeof(ovi);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		if (::GetVersionExW(&ovi)) {
			vi.dwMajorVersion = ovi.dwMajorVersion;
			vi.dwMinorVersion = ovi.dwMinorVersion;
			vi.dwBuildNumber = ovi.dwBuildNumber;
		}
#pragma GCC diagnostic pop
		return vi;
	}();
	return info;
}

}  // namespace

const int TOSVersion::Major = static_cast<int>(real_os_version().dwMajorVersion);
const int TOSVersion::Minor = static_cast<int>(real_os_version().dwMinorVersion);
const int TOSVersion::Build = static_cast<int>(real_os_version().dwBuildNumber);
