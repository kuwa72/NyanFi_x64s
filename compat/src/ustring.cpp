/**
 * @file compat/src/ustring.cpp
 * @brief UnicodeString および関連する非メンバ演算子の実装
 *
 * DynamicArray<T> / AnsiStringT<CodePage> はテンプレートのため
 * compat/include/compat/ustring.h に本体を置いている。ここでは
 * UnicodeString (非テンプレート) の実装のみを行う。
 */
#include "compat/ustring.h"

#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <vector>

#include "compat/exception.h"

namespace {

//---------------------------------------------------------------------------
/**
 * @brief Delphi の StrToInt 互換パーサ
 * @details 前後の空白は呼び出し側で除去済みであることを前提とする。
 *          対応する形式: 任意の符号 + (10進 | "$"+16進 | "0x"/"0X"+16進)。
 *          Integer (32bit 符号あり) の範囲を超える場合も失敗として扱う。
 * @param trimmed 前後の空白を除去済みの文字列
 * @param out     変換結果の格納先
 * @return 変換に成功したか
 */
bool TryParseDelphiInt(const std::wstring &trimmed, int &out)
{
	if (trimmed.empty()) return false;

	std::size_t pos = 0;
	bool neg = false;
	if (trimmed[pos] == L'+' || trimmed[pos] == L'-') {
		neg = (trimmed[pos] == L'-');
		++pos;
	}
	if (pos >= trimmed.size()) return false;
	if (trimmed[pos] == L'+' || trimmed[pos] == L'-') return false;  // 二重符号は不正

	int base = 10;
	if (trimmed[pos] == L'$') {
		base = 16;
		++pos;
	}
	else if (pos + 1 < trimmed.size() && trimmed[pos] == L'0' &&
			 (trimmed[pos + 1] == L'x' || trimmed[pos + 1] == L'X')) {
		base = 16;
		pos += 2;
	}
	if (pos >= trimmed.size()) return false;  // 符号・接頭辞だけで数字がない

	const wchar_t *start = trimmed.c_str() + pos;
	wchar_t *endptr = nullptr;
	errno = 0;
	const long long value_raw = std::wcstoll(start, &endptr, base);
	if (endptr == start) return false;                             // 数字が一つもない
	if (endptr != trimmed.c_str() + trimmed.size()) return false;  // 末尾に余分な文字
	if (errno == ERANGE) return false;

	const long long value = neg ? -value_raw : value_raw;
	if (value < (std::numeric_limits<Int32>::min)() || value > (std::numeric_limits<Int32>::max)()) return false;

	out = static_cast<int>(value);
	return true;
}

//---------------------------------------------------------------------------
/**
 * @brief Windows の wide printf 系 (vswprintf 等) 向けに書式文字列を補正する
 * @details 重要な発見: mingw-w64 / MSVCRT の wide printf 系は ISO C と異なり
 *          既定で %s を **マルチバイト文字列 (char*)**、%c を **マルチバイト
 *          文字 (下位バイトのみ)** として解釈する (%S / %C がその逆で wide)。
 *          既存コードは %s / %c に wchar_t* / wchar_t を渡す前提 (実測: 書式
 *          に %s(wchar_t*) %c を多用) のため、長さ修飾子のない %s / %c を
 *          %ls / %lc に書き換えてから vswprintf に渡す。
 * @param format 元の書式文字列
 * @return 補正済みの書式文字列
 */
std::wstring RewriteFormatForWidePrintf(const wchar_t *format)
{
	std::wstring out;
	if (format == nullptr) return out;

	for (const wchar_t *p = format; *p != L'\0';) {
		if (*p != L'%') {
			out += *p++;
			continue;
		}
		out += *p++;  // '%' をコピー

		if (*p == L'%') {  // "%%" はリテラル % (書式指定ではない)
			out += *p++;
			continue;
		}

		// フラグ
		while (*p == L'-' || *p == L'+' || *p == L' ' || *p == L'0' || *p == L'#') out += *p++;
		// 幅
		if (*p == L'*') out += *p++;
		else
			while (std::iswdigit(*p)) out += *p++;
		// 精度
		if (*p == L'.') {
			out += *p++;
			if (*p == L'*') out += *p++;
			else
				while (std::iswdigit(*p)) out += *p++;
		}
		// 長さ修飾子 (h/hh/l/ll/L/j/z/t/w)
		bool has_length_modifier = false;
		while (*p == L'h' || *p == L'l' || *p == L'L' || *p == L'j' || *p == L'z' || *p == L't' || *p == L'w') {
			has_length_modifier = true;
			out += *p++;
		}

		if (*p == L'\0') break;  // 不正な書式 (末尾が % で終わる)
		if (!has_length_modifier && (*p == L's' || *p == L'c')) out += L'l';
		out += *p++;  // 変換指定子
	}

	return out;
}

}  // namespace

//---------------------------------------------------------------------------
// 構築・代入
//---------------------------------------------------------------------------
UnicodeString::UnicodeString() : text_()
{
}

UnicodeString::UnicodeString(const UnicodeString &src) : text_(src.text_)
{
}

UnicodeString::UnicodeString(UnicodeString &&src) noexcept : text_(std::move(src.text_))
{
}

UnicodeString::UnicodeString(const wchar_t *src) : text_(src != nullptr ? src : L"")
{
}

UnicodeString::UnicodeString(const wchar_t *src, int len)
{
	if (src != nullptr && len > 0) text_.assign(src, static_cast<std::size_t>(len));
}

UnicodeString::UnicodeString(const char *src)
{
	// RTL と同じ意味論: CP_ACP からの変換。書庫 DLL が返す実行時 CP932
	// データもこの経路を通るため、ここを変更してはならない。
	if (src == nullptr || *src == '\0') return;

	const int wlen = ::MultiByteToWideChar(CP_ACP, 0, src, -1, nullptr, 0);
	if (wlen <= 1) return;  // 1 = 終端 NUL のみ (空文字列)

	text_.resize(static_cast<std::size_t>(wlen - 1));
	::MultiByteToWideChar(CP_ACP, 0, src, -1, text_.data(), wlen - 1);
}

UnicodeString::UnicodeString(const char *src, int len)
{
	// RTL と同じ意味論: CP_ACP からの変換。NUL 終端に依存せず、指定された
	// len バイトぶんだけを変換する (embedded NUL を含む固定長バッファ読み
	// 取り用途。実測: src/usr_file_inf.cpp の get_id_str4/get_chunk_hdr が
	// BYTE buf[4]/[8] から UnicodeString((char*)&buf, 4) の形で使用)。
	// -1 を渡さず len を明示するため MultiByteToWideChar は途中の NUL でも
	// 打ち切らず、CP932 のマルチバイト境界も len バイト全体を見て変換する。
	if (src == nullptr || len <= 0) return;

	const int wlen = ::MultiByteToWideChar(CP_ACP, 0, src, len, nullptr, 0);
	if (wlen <= 0) return;

	text_.resize(static_cast<std::size_t>(wlen));
	::MultiByteToWideChar(CP_ACP, 0, src, len, text_.data(), wlen);
}

UnicodeString::UnicodeString(wchar_t ch) : text_(1, ch)
{
}

UnicodeString::UnicodeString(int value)
{
	wchar_t buf[16];  // INT32_MIN は "-2147483648" (11 文字 + NUL) で足りる
	const int n = std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%d", value);
	if (n > 0) text_.assign(buf, static_cast<std::size_t>(n));
}

UnicodeString::UnicodeString(double value)
{
	// C++Builder の FloatToStr (General 書式、有効桁 15 桁) を模す
	wchar_t buf[64];
	const int n = std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%.15g", value);
	if (n > 0) text_.assign(buf, static_cast<std::size_t>(n));
}

UnicodeString::UnicodeString(const AnsiString &src) : text_(src.ToUnicode().wstr())
{
}

UnicodeString::UnicodeString(const UTF8String &src) : text_(src.ToUnicode().wstr())
{
}

UnicodeString::UnicodeString(const std::wstring &src) : text_(src)
{
}

UnicodeString::~UnicodeString() = default;

UnicodeString &UnicodeString::operator=(const UnicodeString &src)
{
	if (this != &src) text_ = src.text_;
	return *this;
}

UnicodeString &UnicodeString::operator=(UnicodeString &&src) noexcept
{
	if (this != &src) text_ = std::move(src.text_);
	return *this;
}

UnicodeString &UnicodeString::operator=(const wchar_t *src)
{
	text_ = (src != nullptr ? src : L"");
	return *this;
}

//---------------------------------------------------------------------------
// 参照
//---------------------------------------------------------------------------
int UnicodeString::Length() const
{
	return static_cast<int>(text_.size());
}

bool UnicodeString::IsEmpty() const
{
	return text_.empty();
}

const wchar_t *UnicodeString::c_str() const
{
	return text_.c_str();
}

wchar_t *UnicodeString::data()
{
	return text_.data();
}

const wchar_t *UnicodeString::data() const
{
	return text_.data();
}

const wchar_t *UnicodeString::LastChar() const
{
	return IsEmpty() ? text_.c_str() : (text_.c_str() + text_.size() - 1);
}

const wchar_t *UnicodeString::FirstChar() const
{
	return text_.c_str();
}

wchar_t &UnicodeString::operator[](int index)
{
	return text_[static_cast<std::size_t>(index - 1)];
}

const wchar_t &UnicodeString::operator[](int index) const
{
	return text_[static_cast<std::size_t>(index - 1)];
}

//---------------------------------------------------------------------------
// 部分文字列・検索
//---------------------------------------------------------------------------
UnicodeString UnicodeString::SubString(int index, int count) const
{
	const int len = Length();
	if (index < 1) index = 1;
	if (index > len || count <= 0) return UnicodeString();

	const int avail = len - index + 1;
	if (count > avail) count = avail;

	return UnicodeString(text_.substr(static_cast<std::size_t>(index - 1), static_cast<std::size_t>(count)));
}

UnicodeString UnicodeString::SubString(int index) const
{
	return SubString(index, Length() - index + 1);
}

UnicodeString UnicodeString::SubStr(int index, int count) const
{
	return SubString(index, count);
}

int UnicodeString::Pos(const UnicodeString &sub) const
{
	if (sub.IsEmpty()) return 0;
	const std::size_t p = text_.find(sub.text_);
	return (p == std::wstring::npos) ? 0 : static_cast<int>(p) + 1;
}

int UnicodeString::Pos(wchar_t ch) const
{
	const std::size_t p = text_.find(ch);
	return (p == std::wstring::npos) ? 0 : static_cast<int>(p) + 1;
}

int UnicodeString::LastDelimiter(const UnicodeString &delims) const
{
	// ASCII の区切り文字を想定する限り、サロゲート単位 (0xD800-0xDFFF) と
	// 衝突しないため、単純な後方一致で安全にサロゲートペアを壊さず判定できる。
	for (int i = Length(); i >= 1; --i) {
		const wchar_t ch = text_[static_cast<std::size_t>(i - 1)];
		if (delims.text_.find(ch) != std::wstring::npos) return i;
	}
	return 0;
}

bool UnicodeString::IsDelimiter(const UnicodeString &delims, int index) const
{
	if (index < 1 || index > Length()) return false;
	const wchar_t ch = text_[static_cast<std::size_t>(index - 1)];
	return delims.text_.find(ch) != std::wstring::npos;
}

bool UnicodeString::IsPathDelimiter(int index) const
{
	if (index < 1 || index > Length()) return false;
	return text_[static_cast<std::size_t>(index - 1)] == L'\\';
}

bool UnicodeString::IsLeadSurrogate(int index) const
{
	if (index < 1 || index > Length()) return false;
	const wchar_t ch = text_[static_cast<std::size_t>(index - 1)];
	return ch >= 0xD800 && ch <= 0xDBFF;
}

bool UnicodeString::IsTrailSurrogate(int index) const
{
	if (index < 1 || index > Length()) return false;
	const wchar_t ch = text_[static_cast<std::size_t>(index - 1)];
	return ch >= 0xDC00 && ch <= 0xDFFF;
}

//---------------------------------------------------------------------------
// 変更
//---------------------------------------------------------------------------
UnicodeString &UnicodeString::Delete(int index, int count)
{
	const int len = Length();
	if (index < 1) index = 1;
	if (index > len || count <= 0) return *this;  // 範囲外は何もしない (例外にしない)

	int avail = len - index + 1;
	if (count > avail) count = avail;

	text_.erase(static_cast<std::size_t>(index - 1), static_cast<std::size_t>(count));
	return *this;
}

UnicodeString &UnicodeString::Insert(const UnicodeString &str, int index)
{
	const int len = Length();
	if (index < 1) index = 1;
	if (index > len + 1) index = len + 1;  // 範囲外は末尾にクランプ

	text_.insert(static_cast<std::size_t>(index - 1), str.text_);
	return *this;
}

UnicodeString &UnicodeString::SetLength(int len)
{
	text_.resize(static_cast<std::size_t>(len > 0 ? len : 0));
	return *this;
}

UnicodeString &UnicodeString::Unique()
{
	// シムに COW は存在しないため何もしない
	return *this;
}

//---------------------------------------------------------------------------
// 生成
//---------------------------------------------------------------------------
UnicodeString UnicodeString::Trim() const
{
	return TrimLeft().TrimRight();
}

UnicodeString UnicodeString::TrimLeft() const
{
	std::size_t i = 0;
	while (i < text_.size() && text_[i] <= L' ') ++i;
	return UnicodeString(text_.substr(i));
}

UnicodeString UnicodeString::TrimRight() const
{
	std::size_t n = text_.size();
	while (n > 0 && text_[n - 1] <= L' ') --n;
	return UnicodeString(text_.substr(0, n));
}

UnicodeString UnicodeString::UpperCase() const
{
	// Delphi と同じく ASCII のみ変換する (ロケール依存の変換は sysutils 側の担当)
	UnicodeString result(*this);
	for (wchar_t &ch : result.text_) {
		if (ch >= L'a' && ch <= L'z') ch = static_cast<wchar_t>(ch - (L'a' - L'A'));
	}
	return result;
}

UnicodeString UnicodeString::LowerCase() const
{
	UnicodeString result(*this);
	for (wchar_t &ch : result.text_) {
		if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch + (L'a' - L'A'));
	}
	return result;
}

//---------------------------------------------------------------------------
// 数値変換
//---------------------------------------------------------------------------
int UnicodeString::ToInt() const
{
	int result = 0;
	const UnicodeString trimmed = Trim();
	if (!TryParseDelphiInt(trimmed.text_, result)) {
		UnicodeString msg;
		msg.sprintf(L"'%s' is not a valid integer value", text_.c_str());
		throw EConvertError(msg);
	}
	return result;
}

int UnicodeString::ToIntDef(int defValue) const
{
	int result = 0;
	const UnicodeString trimmed = Trim();
	return TryParseDelphiInt(trimmed.text_, result) ? result : defValue;
}

double UnicodeString::ToDouble() const
{
	const UnicodeString trimmed = Trim();
	const std::wstring &t = trimmed.text_;
	if (t.empty()) throw EConvertError(UnicodeString(L"'' is not a valid floating point value"));

	const wchar_t *start = t.c_str();
	wchar_t *endptr = nullptr;
	errno = 0;
	const double value = std::wcstod(start, &endptr);
	if (endptr == start || endptr != start + t.size() || errno == ERANGE) {
		UnicodeString msg;
		msg.sprintf(L"'%s' is not a valid floating point value", text_.c_str());
		throw EConvertError(msg);
	}
	return value;
}

//---------------------------------------------------------------------------
// 書式化
//---------------------------------------------------------------------------
UnicodeString &UnicodeString::vprintf(const wchar_t *format, va_list args)
{
	text_.clear();
	return cat_vprintf(format, args);
}

UnicodeString &UnicodeString::cat_vprintf(const wchar_t *format, va_list args)
{
	if (format == nullptr) return *this;

	// Windows の wide printf は %s/%c を素のまま渡すとマルチバイト解釈される
	// ため、事前に %ls/%lc へ書き換えてから vswprintf に渡す (詳細は
	// RewriteFormatForWidePrintf のコメント参照)。
	const std::wstring fixed_format = RewriteFormatForWidePrintf(format);

	std::size_t bufsize = 256;
	for (;;) {
		std::vector<wchar_t> buf(bufsize);

		va_list args_copy;
		va_copy(args_copy, args);
		const int n = std::vswprintf(buf.data(), bufsize, fixed_format.c_str(), args_copy);
		va_end(args_copy);

		if (n >= 0) {
			text_.append(buf.data(), static_cast<std::size_t>(n));
			return *this;
		}

		// vswprintf はバッファ不足時に -1 を返す (必要長を教えてくれない実装が
		// あるため) 単純にバッファを倍増してリトライする。
		bufsize *= 2;
		if (bufsize > (1u << 24)) return *this;  // 異常な書式に対する安全弁
	}
}

UnicodeString &UnicodeString::sprintf(const wchar_t *format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	return *this;
}

UnicodeString &UnicodeString::cat_sprintf(const wchar_t *format, ...)
{
	va_list args;
	va_start(args, format);
	cat_vprintf(format, args);
	va_end(args);
	return *this;
}

//---------------------------------------------------------------------------
// 比較
//---------------------------------------------------------------------------
int UnicodeString::CompareIC(const UnicodeString &rhs) const
{
	return UpperCase().text_.compare(rhs.UpperCase().text_);
}

int UnicodeString::Compare(const UnicodeString &rhs) const
{
	return text_.compare(rhs.text_);
}

//---------------------------------------------------------------------------
// 演算子 (非メンバ)
//---------------------------------------------------------------------------
UnicodeString operator+(const UnicodeString &lhs, const UnicodeString &rhs)
{
	UnicodeString result(lhs);
	result.wstr() += rhs.wstr();
	return result;
}

UnicodeString operator+(const UnicodeString &lhs, const wchar_t *rhs)
{
	UnicodeString result(lhs);
	result.wstr() += (rhs != nullptr ? rhs : L"");
	return result;
}

UnicodeString operator+(const wchar_t *lhs, const UnicodeString &rhs)
{
	UnicodeString result(lhs != nullptr ? lhs : L"");
	result.wstr() += rhs.wstr();
	return result;
}

UnicodeString operator+(const UnicodeString &lhs, wchar_t rhs)
{
	UnicodeString result(lhs);
	result.wstr() += rhs;
	return result;
}

UnicodeString operator+(wchar_t lhs, const UnicodeString &rhs)
{
	UnicodeString result(lhs);
	result.wstr() += rhs.wstr();
	return result;
}

UnicodeString &operator+=(UnicodeString &lhs, const UnicodeString &rhs)
{
	lhs.wstr() += rhs.wstr();
	return lhs;
}

bool operator==(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return lhs.wstr() == rhs.wstr();
}

bool operator!=(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return !(lhs == rhs);
}

bool operator<(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return lhs.wstr() < rhs.wstr();
}

bool operator<=(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return lhs.wstr() <= rhs.wstr();
}

bool operator>(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return lhs.wstr() > rhs.wstr();
}

bool operator>=(const UnicodeString &lhs, const UnicodeString &rhs)
{
	return lhs.wstr() >= rhs.wstr();
}
