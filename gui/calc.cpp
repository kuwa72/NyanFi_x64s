/**
 * @file gui/calc.cpp
 * @brief gui/calc.h の wx 非依存式評価実装
 */
#include "gui/calc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace calc {
namespace {

constexpr wchar_t kBitXor = L'\x1f';  // ASCII ^ はべき乗。VCL の ＾ 用
constexpr wchar_t kGcdOp = L'\x1e';
constexpr wchar_t kLcmOp = L'\x1d';
constexpr long double kPi = 3.141592653589793238462643383279502884L;

std::wstring to_w(const UnicodeString &value)
{
	return value.wstr();
}

UnicodeString to_u(const std::wstring &value)
{
	return UnicodeString(value.c_str(), static_cast<int>(value.size()));
}

bool is_space(wchar_t ch)
{
	return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == L'\f';
}

bool is_digit(wchar_t ch)
{
	return ch >= L'0' && ch <= L'9';
}

bool is_alpha(wchar_t ch)
{
	return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'_';
}

bool is_hex_digit(wchar_t ch)
{
	return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') ||
	       (ch >= L'A' && ch <= L'F');
}

wchar_t lower(wchar_t ch)
{
	return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch - L'A' + L'a') : ch;
}

std::wstring lower_copy(std::wstring value)
{
	for (wchar_t &ch : value) ch = lower(ch);
	return value;
}

std::wstring trim_copy(const std::wstring &value)
{
	std::size_t first = 0;
	while (first < value.size() && is_space(value[first])) ++first;
	std::size_t last = value.size();
	while (last > first && is_space(value[last - 1])) --last;
	return value.substr(first, last - first);
}

bool is_finite(long double value)
{
	return std::isfinite(value);
}

bool parse_unsigned(const std::wstring &text, unsigned long long &value)
{
	if (text.empty()) return false;
	unsigned long long result = 0;
	for (wchar_t ch : text) {
		if (!is_digit(ch)) return false;
		const unsigned digit = static_cast<unsigned>(ch - L'0');
		if (result > (std::numeric_limits<unsigned long long>::max() - digit) / 10ULL) {
			return false;
		}
		result = result * 10ULL + digit;
	}
	value = result;
	return true;
}

bool parse_signed_integer(const std::wstring &text, long long &value)
{
	if (text.empty()) return false;
	std::size_t pos = 0;
	bool negative = false;
	if (text[0] == L'+' || text[0] == L'-') {
		negative = text[0] == L'-';
		pos = 1;
	}
	unsigned long long magnitude = 0;
	if (!parse_unsigned(text.substr(pos), magnitude)) return false;
	if (negative) {
		if (magnitude > static_cast<unsigned long long>(std::numeric_limits<long long>::max()) + 1ULL) {
			return false;
		}
		if (magnitude == static_cast<unsigned long long>(std::numeric_limits<long long>::max()) + 1ULL) {
			value = std::numeric_limits<long long>::min();
		} else {
			value = -static_cast<long long>(magnitude);
		}
	} else {
		if (magnitude > static_cast<unsigned long long>(std::numeric_limits<long long>::max())) {
			return false;
		}
		value = static_cast<long long>(magnitude);
	}
	return true;
}

bool parse_hex_value(const std::wstring &text, long double &value)
{
	if (text.size() <= 2 || text[0] != L'0' || lower(text[1]) != L'x') return false;
	long double result = 0.0L;
	for (std::size_t i = 2; i < text.size(); ++i) {
		const wchar_t ch = lower(text[i]);
		unsigned digit = 0;
		if (ch >= L'0' && ch <= L'9') digit = static_cast<unsigned>(ch - L'0');
		else if (ch >= L'a' && ch <= L'f') digit = static_cast<unsigned>(ch - L'a' + 10);
		else return false;
		result = result * 16.0L + digit;
		if (!is_finite(result)) return false;
	}
	value = result;
	return true;
}

struct TimeValue {
	bool ok = false;
	long double seconds = 0.0L;
	std::size_t end = 0;
};

TimeValue parse_time_at(const std::wstring &text, std::size_t pos)
{
	TimeValue result;
	if (pos >= text.size()) return result;
	std::size_t p = pos;
	int sign = 1;
	if (text[p] == L'+' || text[p] == L'-') {
		if (text[p] == L'-') sign = -1;
		++p;
	}
	const std::size_t hour_begin = p;
	while (p < text.size() && is_digit(text[p])) ++p;
	if (p == hour_begin || p >= text.size() || text[p] != L':') return result;
	const std::size_t hour_end = p++;
	const std::size_t minute_begin = p;
	while (p < text.size() && is_digit(text[p])) ++p;
	if (p == minute_begin) return result;
	const std::size_t minute_end = p;
	if (p < text.size() && text[p] == L':') {
		++p;
		const std::size_t second_begin = p;
		while (p < text.size() && is_digit(text[p])) ++p;
		if (p == second_begin) return result;
		const std::size_t second_end = p;
		unsigned long long hh = 0, mm = 0, ss = 0;
		if (!parse_unsigned(text.substr(hour_begin, hour_end - hour_begin), hh) ||
		    !parse_unsigned(text.substr(minute_begin, minute_end - minute_begin), mm) ||
		    !parse_unsigned(text.substr(second_begin, second_end - second_begin), ss) ||
		    mm > 59 || ss > 59) {
			return result;
		}
		result.seconds = static_cast<long double>(sign) *
		                 (static_cast<long double>(hh) * 3600.0L +
		                  static_cast<long double>(mm) * 60.0L + static_cast<long double>(ss));
	} else {
		unsigned long long hh = 0, mm = 0;
		if (!parse_unsigned(text.substr(hour_begin, hour_end - hour_begin), hh) ||
		    !parse_unsigned(text.substr(minute_begin, minute_end - minute_begin), mm) || mm > 59) {
			return result;
		}
		result.seconds = static_cast<long double>(sign) *
		                 (static_cast<long double>(hh) * 3600.0L + static_cast<long double>(mm) * 60.0L);
	}
	result.end = p;
	result.ok = is_finite(result.seconds);
	return result;
}

bool parse_time_string(const std::wstring &input, long double &seconds)
{
	const std::wstring text = trim_copy(input);
	if (lower_copy(text) == L"now") return false;
	const TimeValue parsed = parse_time_at(text, 0);
	if (!parsed.ok || parsed.end != text.size()) return false;
	seconds = parsed.seconds;
	return true;
}

std::wstring current_time_text()
{
	const std::time_t now = std::time(nullptr);
	std::tm local {};
	std::tm *ptr = std::localtime(&now);
	if (ptr == nullptr) return L"00:00:00";
	local = *ptr;
	wchar_t buffer[16] = {};
	std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%02d:%02d:%02d",
	              local.tm_hour, local.tm_min, local.tm_sec);
	return buffer;
}

enum class TokenKind { Number, Identifier, Operator, LParen, RParen, Comma, End };

struct Token {
	TokenKind kind = TokenKind::End;
	std::wstring text;
	long double number = 0.0L;
	bool is_hex = false;
	bool is_time = false;
};

class Lexer {
public:
	explicit Lexer(const std::wstring &text) : text_(text) {}

	Token Next()
	{
		while (pos_ < text_.size() && is_space(text_[pos_])) ++pos_;
		if (pos_ >= text_.size()) return {};

		const std::size_t start = pos_;
		if (pos_ + 1 < text_.size() && text_[pos_] == L'0' &&
		    (text_[pos_ + 1] == L'x' || text_[pos_ + 1] == L'X')) {
			pos_ += 2;
			const std::size_t digit_begin = pos_;
			while (pos_ < text_.size() && is_hex_digit(text_[pos_])) ++pos_;
			if (pos_ > digit_begin) {
				Token token;
				token.kind = TokenKind::Number;
				token.text = text_.substr(start, pos_ - start);
				return token;
			}
			pos_ = start;
		}
		if (pos_ + 1 < text_.size() && is_digit(text_[pos_])) {
			const TimeValue time = parse_time_at(text_, pos_);
			if (time.ok) {
				pos_ = time.end;
				Token token;
				token.kind = TokenKind::Number;
				token.text = text_.substr(start, pos_ - start);
				token.number = time.seconds;
				token.is_time = true;
				return token;
			}
		}

		if (is_digit(text_[pos_]) || text_[pos_] == L'.') {
			return Number();
		}
		if (is_alpha(text_[pos_])) {
			while (pos_ < text_.size() && (is_alpha(text_[pos_]) || is_digit(text_[pos_]))) {
				++pos_;
			}
			Token token;
			const std::wstring word = text_.substr(start, pos_ - start);
			const std::wstring lower_word = lower_copy(word);
			if (lower_word == L"and" || lower_word == L"xor" || lower_word == L"or") {
				token.kind = TokenKind::Operator;
				token.text = lower_word == L"and" ? L"&" :
				             lower_word == L"xor" ? std::wstring(1, kBitXor) : L"|";
			} else {
				token.kind = TokenKind::Identifier;
				token.text = word;
			}
			return token;
		}

		const wchar_t ch = text_[pos_++];
		Token token;
		switch (ch) {
		case L'(': token.kind = TokenKind::LParen; token.text = L"("; break;
		case L')': token.kind = TokenKind::RParen; token.text = L")"; break;
		case L',': token.kind = TokenKind::Comma; token.text = L","; break;
		case L'^': token.kind = TokenKind::Operator; token.text = L"^"; break;
		case L'＆': token.kind = TokenKind::Operator; token.text = L"&"; break;
		case L'＾': token.kind = TokenKind::Operator; token.text = std::wstring(1, kBitXor); break;
		case L'Ｇ': token.kind = TokenKind::Operator; token.text = std::wstring(1, kGcdOp); break;
		case L'Ｌ': token.kind = TokenKind::Operator; token.text = std::wstring(1, kLcmOp); break;
		default:
			token.kind = TokenKind::Operator;
			token.text = std::wstring(1, ch);
			break;
		}
		return token;
	}

private:
	Token Number()
	{
		const std::size_t start = pos_;
		bool has_digit = false;
		bool has_dot = false;
		while (pos_ < text_.size()) {
			const wchar_t ch = text_[pos_];
			if (is_digit(ch)) {
				has_digit = true;
				++pos_;
			} else if (ch == L'.' && !has_dot) {
				has_dot = true;
				++pos_;
			} else if (ch == L',' && pos_ + 1 < text_.size() && is_digit(text_[pos_ + 1])) {
				// 3桁区切りのカンマは数値の一部。関数引数のカンマは空白を挟むため残る。
				++pos_;
			} else {
				break;
			}
		}
		if (!has_digit) {
			Token token;
			token.kind = TokenKind::Operator;
			token.text = std::wstring(1, text_[pos_++]);
			return token;
		}

		// 指数表記の e/E だけを取り込む。
		if (pos_ < text_.size() && (text_[pos_] == L'e' || text_[pos_] == L'E')) {
			std::size_t q = pos_ + 1;
			if (q < text_.size() && (text_[q] == L'+' || text_[q] == L'-')) ++q;
			const std::size_t digit_begin = q;
			while (q < text_.size() && is_digit(text_[q])) ++q;
			if (q > digit_begin) pos_ = q;
		}

		// 単位だけを数値に付加する。未知の英字は後続の識別子として残す。
		const std::size_t suffix_begin = pos_;
		while (pos_ < text_.size() && is_alpha(text_[pos_])) ++pos_;
		const std::wstring suffix = lower_copy(text_.substr(suffix_begin, pos_ - suffix_begin));
		if (!suffix.empty() && !is_known_suffix(suffix)) pos_ = suffix_begin;

		Token token;
		token.kind = TokenKind::Number;
		token.text = text_.substr(start, pos_ - start);
		return token;
	}

	static bool is_known_suffix(const std::wstring &suffix)
	{
		return suffix == L"b" || suffix == L"kb" || suffix == L"mb" || suffix == L"gb" ||
		       suffix == L"tb" || suffix == L"s" || suffix == L"ms" || suffix == L"min" ||
		       suffix == L"h" || suffix == L"deg" || suffix == L"rad" || suffix == L"grad";
	}

	const std::wstring &text_;
	std::size_t pos_ = 0;
};

struct Value {
	long double number = 0.0L;
	bool is_time = false;
	bool is_hex = false;
};

struct ParseError {
	std::wstring message;
};

[[noreturn]] void fail(const std::wstring &message)
{
	throw ParseError{message};
}

int precedence(wchar_t op)
{
	switch (op) {
	case L'^': return 70;
	case L'*':
	case L'/':
	case L'%': return 60;
	case L'+':
	case L'-': return 50;
	case L'&': return 40;
	case kBitXor: return 30;
	case L'|': return 20;
	case kGcdOp:
	case kLcmOp: return 10;
	default: return -1;
	}
}

bool integer_value(long double value, long long &result)
{
	if (!is_finite(value) || std::trunc(value) != value) return false;
	if (value < static_cast<long double>(std::numeric_limits<long long>::min()) ||
	    value > static_cast<long double>(std::numeric_limits<long long>::max())) {
		return false;
	}
	result = static_cast<long long>(value);
	return true;
}

long long gcd_value(long long a, long long b)
{
	if (a < 0) a = -a;
	if (b < 0) b = -b;
	while (b != 0) {
		const long long r = a % b;
		a = b;
		b = r;
	}
	return a;
}

class Parser {
public:
	Parser(const std::wstring &text, const EvalOptions &options) : options_(options)
	{
		Lexer lexer(text);
		Token token;
		while (true) {
			token = lexer.Next();
			tokens_.push_back(token);
			if (token.kind == TokenKind::End) break;
		}
	}

	Value Parse()
	{
		Value value = Expression(0);
		if (Current().kind != TokenKind::End) {
			if (Current().kind == TokenKind::RParen) fail(L"不正な括弧");
			fail(L"不正な文字");
		}
		return value;
	}

private:
	Token &Current() { return tokens_[index_]; }
	const Token &Current() const { return tokens_[index_]; }
	void Take() { if (Current().kind != TokenKind::End) ++index_; }

	Value Expression(int minimum_precedence)
	{
		Value left = Prefix();
		while (true) {
			wchar_t op = L'\0';
			if (Current().kind == TokenKind::Operator && !Current().text.empty()) {
				op = Current().text[0];
			} else if (Current().kind == TokenKind::Identifier) {
				const std::wstring name = lower_copy(Current().text);
				if (name == L"gcd") op = kGcdOp;
				else if (name == L"lcm") op = kLcmOp;
			}
			const int p = precedence(op);
			if (p < minimum_precedence) break;
			Take();
			Value right = Expression(p + 1);
			left = Apply(left, op, right);
		}
		return left;
	}

	Value Prefix()
	{
		if (Current().kind == TokenKind::Operator &&
		    (Current().text == L"+" || Current().text == L"-")) {
			const bool negative = Current().text == L"-";
			Take();
			Value value = Prefix();
			if (negative) value.number = -value.number;
			return value;
		}
		return Primary();
	}

	Value Primary()
	{
		Value value;
		if (Current().kind == TokenKind::Number) {
			value = NumberValue(Current());
			Take();
		} else if (Current().kind == TokenKind::Identifier) {
			const std::wstring name = lower_copy(Current().text);
			Take();
			if (Current().kind == TokenKind::LParen) {
				std::vector<Value> args;
				Take();
				if (Current().kind != TokenKind::RParen) {
					while (true) {
						args.push_back(Expression(0));
						if (Current().kind != TokenKind::Comma) break;
						Take();
					}
				}
				if (Current().kind != TokenKind::RParen) fail(L"不正な括弧");
				Take();
				return Function(name, args);
			}
			return Constant(name);
		} else if (Current().kind == TokenKind::LParen) {
			Take();
			value = Expression(0);
			if (Current().kind != TokenKind::RParen) fail(L"不正な括弧");
			Take();
		} else {
			fail(Current().kind == TokenKind::End ? L"入力内容の誤り" : L"不正な文字");
		}

		while (Current().kind == TokenKind::Operator && Current().text == L"!") {
			Take();
			value = Factorial(value);
		}
		return value;
	}

	Value NumberValue(const Token &token)
	{
		Value value;
		value.is_time = token.is_time;
		value.is_hex = token.is_hex;
		if (token.is_time) {
			value.number = token.number;
			return value;
		}
		std::wstring text = token.text;
		const bool hex = text.size() > 2 && text[0] == L'0' && lower(text[1]) == L'x';
		std::wstring suffix;
		if (!hex) {
			const std::size_t alpha = text.find_first_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
			if (alpha != std::wstring::npos) {
				suffix = lower_copy(text.substr(alpha));
				text.erase(alpha);
			}
		}
		text.erase(std::remove(text.begin(), text.end(), L','), text.end());
		if (hex) {
			if (!parse_hex_value(text, value.number)) fail(L"異常値またはオーバーフロー");
			value.is_hex = true;
		} else {
			wchar_t *end = nullptr;
			value.number = std::wcstold(text.c_str(), &end);
			if (end == text.c_str() || *end != L'\0' || !is_finite(value.number)) {
				fail(L"異常値またはオーバーフロー");
			}
		}
		if (!suffix.empty()) {
			long double multiplier = 1.0L;
			if (suffix == L"b") multiplier = 1.0L;
			else if (suffix == L"kb") multiplier = 1024.0L;
			else if (suffix == L"mb") multiplier = 1024.0L * 1024.0L;
			else if (suffix == L"gb") multiplier = 1024.0L * 1024.0L * 1024.0L;
			else if (suffix == L"tb") multiplier = 1024.0L * 1024.0L * 1024.0L * 1024.0L;
			else if (suffix == L"s") multiplier = 1.0L;
			else if (suffix == L"ms") multiplier = 0.001L;
			else if (suffix == L"min") multiplier = 60.0L;
			else if (suffix == L"h") multiplier = 3600.0L;
			else if (suffix == L"deg") multiplier = 1.0L;
			else if (suffix == L"rad") multiplier = kPi / 180.0L;
			else if (suffix == L"grad") multiplier = 0.9L;
			value.number *= multiplier;
			if (!is_finite(value.number)) fail(L"異常値またはオーバーフロー");
		}
		return value;
	}

	Value Constant(const std::wstring &name)
	{
		Value value;
		if (name == L"pi") value.number = kPi;
		else if (name == L"e") value.number = 2.71828182845904523536L;
		else if (name == L"c") value.number = 299792458.0L;
		else if (name == L"g") value.number = 9.80665L;
		else if (name == L"h") value.number = 6.6260755e-34L;
		else if (name == L"ec") value.number = 1.60217733e-19L;
		else if (name == L"u0") value.number = 1.25663706e-6L;
		else if (name == L"e0") value.number = 8.85418782e-12L;
		else if (name == L"k") value.number = 1.380658e-23L;
		else if (name == L"na") value.number = 6.0221367e23L;
		else if (name == L"now") {
			const std::wstring now = options_.now.IsEmpty() ? current_time_text() : to_w(options_.now);
			if (!parse_time_string(now, value.number)) fail(L"Now の時刻が不正です");
			value.is_time = true;
		} else {
			fail(L"不明な定数");
		}
		return value;
	}

	Value Factorial(Value value)
	{
		long long integer = 0;
		if (!integer_value(value.number, integer) || integer < 0) fail(L"定義域エラー");
		long double result = 1.0L;
		for (long long i = 2; i <= integer; ++i) {
			result *= static_cast<long double>(i);
			if (!is_finite(result)) fail(L"異常値またはオーバーフロー");
		}
		value.number = result;
		return value;
	}

	Value Function(const std::wstring &name, const std::vector<Value> &args)
	{
		if (name == L"gcd" || name == L"lcm") {
			if (args.size() != 2) fail(L"引数が不一致");
			long long a = 0, b = 0;
			if (!integer_value(args[0].number, a) || !integer_value(args[1].number, b)) {
				fail(L"値が整数でないかオーバーフロー");
			}
			const long long g = gcd_value(a, b);
			if (g == 0) fail(L"定義域エラー");
			Value result;
			const long double aa = std::fabs(static_cast<long double>(a));
			const long double bb = std::fabs(static_cast<long double>(b));
			result.number = name == L"gcd" ? static_cast<long double>(g) : (aa / g) * bb;
			result.is_hex = args[0].is_hex && args[1].is_hex;
			return result;
		}
		if (name == L"min" || name == L"max") {
			if (args.size() != 2) fail(L"引数が不一致");
			Value result = name == L"min" && args[0].number <= args[1].number ? args[0] : args[1];
			return result;
		}
		if (args.size() != 1) fail(L"引数が不一致");
		const long double arg = args[0].number;
		Value result;
		result.is_time = args[0].is_time;
		if (name == L"sin") result.number = std::sin(ToRadians(arg));
		else if (name == L"cos") result.number = std::cos(ToRadians(arg));
		else if (name == L"tan") result.number = std::tan(ToRadians(arg));
		else if (name == L"asin") result.number = FromRadians(std::asin(arg));
		else if (name == L"acos") result.number = FromRadians(std::acos(arg));
		else if (name == L"atan") result.number = FromRadians(std::atan(arg));
		else if (name == L"sinh") result.number = std::sinh(arg);
		else if (name == L"cosh") result.number = std::cosh(arg);
		else if (name == L"tanh") result.number = std::tanh(arg);
		else if (name == L"ln") result.number = std::log(arg);
		else if (name == L"log") result.number = std::log10(arg);
		else if (name == L"abs") result.number = std::fabs(arg);
		else if (name == L"ceil") result.number = std::ceil(arg);
		else if (name == L"floor") result.number = std::floor(arg);
		else if (name == L"round") result.number = std::round(arg);
		else if (name == L"sqrt") {
			if (arg < 0.0L) fail(L"定義域エラー");
			result.number = std::sqrt(arg);
		} else {
			fail(L"不明な関数");
		}
		if (!is_finite(result.number)) fail(L"異常値またはオーバーフロー");
		return result;
	}

	long double ToRadians(long double value) const
	{
		if (options_.angle_mode == AngleMode::Deg) return value * kPi / 180.0L;
		if (options_.angle_mode == AngleMode::Grad) return value * kPi / 200.0L;
		return value;
	}

	long double FromRadians(long double value) const
	{
		if (options_.angle_mode == AngleMode::Deg) return value * 180.0L / kPi;
		if (options_.angle_mode == AngleMode::Grad) return value * 200.0L / kPi;
		return value;
	}

	Value Apply(Value left, wchar_t op, const Value &right)
	{
		Value result;
		result.is_hex = left.is_hex && right.is_hex;
		if (op == L'+') {
			result.number = left.number + right.number;
			result.is_time = left.is_time || right.is_time;
		} else if (op == L'-') {
			result.number = left.number - right.number;
			result.is_time = left.is_time || right.is_time;
		} else if (op == L'*') {
			result.number = left.number * right.number;
			result.is_time = left.is_time != right.is_time;
		} else if (op == L'/') {
			if (right.number == 0.0L) fail(L"0による割り算");
			result.number = left.number / right.number;
			result.is_time = left.is_time && !right.is_time;
		} else if (op == L'%') {
			if (right.number == 0.0L) fail(L"0による割り算");
			result.number = std::fmod(left.number, right.number);
		} else if (op == L'^') {
			if ((left.number < 0.0L && std::trunc(right.number) != right.number) ||
			    (left.number == 0.0L && right.number <= 0.0L)) {
				fail(L"定義域エラー");
			}
			result.number = std::pow(left.number, right.number);
		} else if (op == L'&' || op == kBitXor || op == L'|') {
			long long a = 0, b = 0;
			if (!integer_value(left.number, a) || !integer_value(right.number, b)) {
				fail(L"値が整数でないかオーバーフロー");
			}
			if (op == L'&') result.number = static_cast<long double>(a & b);
			else if (op == kBitXor) result.number = static_cast<long double>(a ^ b);
			else result.number = static_cast<long double>(a | b);
		} else if (op == kGcdOp || op == kLcmOp) {
			long long a = 0, b = 0;
			if (!integer_value(left.number, a) || !integer_value(right.number, b)) {
				fail(L"値が整数でないかオーバーフロー");
			}
			const long long g = gcd_value(a, b);
			if (g == 0) fail(L"定義域エラー");
			const long double aa = std::fabs(static_cast<long double>(a));
			const long double bb = std::fabs(static_cast<long double>(b));
			result.number = op == kGcdOp ? static_cast<long double>(g) : (aa / g) * bb;
		} else {
			fail(L"不正な文字");
		}
		if (!is_finite(result.number)) fail(L"異常値またはオーバーフロー");
		return result;
	}

	EvalOptions options_;
	std::vector<Token> tokens_;
	std::size_t index_ = 0;
};

std::wstring integer_text(long long value)
{
	std::wostringstream out;
	out.imbue(std::locale::classic());
	out << value;
	return out.str();
}

std::wstring add_thousands_separators(const std::wstring &text)
{
	const std::size_t dot = text.find(L'.');
	const std::wstring whole = dot == std::wstring::npos ? text : text.substr(0, dot);
	const std::wstring tail = dot == std::wstring::npos ? L"" : text.substr(dot);
	std::wstring result;
	const int count = static_cast<int>(whole.size());
	for (int i = 0; i < count; ++i) {
		if (i != 0 && (count - i) % 3 == 0) result.push_back(L',');
		result.push_back(whole[static_cast<std::size_t>(i)]);
	}
	return result + tail;
}

UnicodeString format_value_internal(long double value, bool is_hex, bool is_time,
                                    int output_digits, bool comma_grouping, bool answer)
{
	if (!is_finite(value)) return _T("異常値");
	long long integer = 0;
	const bool is_integer = integer_value(value, integer);
	if (is_time && is_integer) {
		const unsigned long long absolute = integer < 0
			? 0ULL - static_cast<unsigned long long>(integer)
			: static_cast<unsigned long long>(integer);
		const unsigned long long hours = absolute / 3600ULL;
		const unsigned long long minutes = (absolute % 3600ULL) / 60ULL;
		const unsigned long long seconds = absolute % 60ULL;
		const long long signed_hours = integer < 0 ? -static_cast<long long>(hours)
		                                           : static_cast<long long>(hours);
		std::wostringstream out;
		out.imbue(std::locale::classic());
		out << signed_hours << L":" << std::setw(2) << std::setfill(L'0') << minutes
		    << L":" << std::setw(2) << std::setfill(L'0') << seconds;
		return to_u(out.str());
	}
	if (is_integer) {
		if (is_hex) {
			std::wostringstream out;
			out.imbue(std::locale::classic());
			out << L"0x" << std::hex << static_cast<unsigned long long>(integer);
			return to_u(out.str());
		}
		std::wstring text = integer_text(integer);
		if (comma_grouping) text = add_thousands_separators(text);
		return to_u(text);
	}
	const int precision = std::max(1, output_digits - (answer ? 2 : 0));
	std::wostringstream out;
	out.imbue(std::locale::classic());
	out << std::setprecision(precision) << value;
	return to_u(out.str());
}

struct UnitInfo {
	int dimension = -1;
	long double factor = 0.0L;
};

bool unit_info(Unit unit, UnitInfo &info)
{
	switch (unit) {
	case Unit::Byte: info = {0, 1.0L}; return true;
	case Unit::Kibibyte: info = {0, 1024.0L}; return true;
	case Unit::Mebibyte: info = {0, 1024.0L * 1024.0L}; return true;
	case Unit::Gibibyte: info = {0, 1024.0L * 1024.0L * 1024.0L}; return true;
	case Unit::Tebibyte: info = {0, 1024.0L * 1024.0L * 1024.0L * 1024.0L}; return true;
	case Unit::Second: info = {1, 1.0L}; return true;
	case Unit::Minute: info = {1, 60.0L}; return true;
	case Unit::Hour: info = {1, 3600.0L}; return true;
	case Unit::Degree: info = {2, kPi / 180.0L}; return true;
	case Unit::Radian: info = {2, 1.0L}; return true;
	case Unit::Gradian: info = {2, kPi / 200.0L}; return true;
	case Unit::None: return false;
	}
	return false;
}

}  // namespace

bool IsTimeString(const UnicodeString &value)
{
	const std::wstring text = trim_copy(to_w(value));
	if (lower_copy(text) == L"now") return true;
	long double ignored = 0.0L;
	return parse_time_string(text, ignored);
}

bool IsHexString(const UnicodeString &value)
{
	const std::wstring text = trim_copy(to_w(value));
	if (text.size() <= 2 || text[0] != L'0' || lower(text[1]) != L'x') return false;
	for (std::size_t i = 2; i < text.size(); ++i) {
		const wchar_t ch = lower(text[i]);
		if (!((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f'))) return false;
	}
	return true;
}

bool IsIntegerLiteral(const UnicodeString &value)
{
	std::wstring text = trim_copy(to_w(value));
	if (text.empty()) return false;
	if (text[0] == L'+' || text[0] == L'-') {
		if (text.size() == 1) return false;
		text.erase(text.begin());
	}
	if (IsHexString(UnicodeString(text.c_str(), static_cast<int>(text.size())))) return true;
	if (text.empty() || !is_digit(text[0])) return false;
	bool previous_comma = false;
	for (wchar_t ch : text) {
		if (is_digit(ch)) {
			previous_comma = false;
		} else if (ch == L',' && !previous_comma) {
			previous_comma = true;
		} else {
			return false;
		}
	}
	return true;
}

UnicodeString JoinInput(const UnicodeString &value)
{
	const std::wstring source = to_w(value);
	std::vector<std::wstring> lines;
	std::wstring current;
	for (std::size_t i = 0; i <= source.size(); ++i) {
		if (i == source.size() || source[i] == L'\r' || source[i] == L'\n') {
			if (i < source.size() && source[i] == L'\r' && i + 1 < source.size() && source[i + 1] == L'\n') {
				++i;
			}
			lines.push_back(trim_copy(current));
			current.clear();
		} else {
			current.push_back(source[i]);
		}
	}
	std::wstring result;
	for (std::wstring line : lines) {
		line = trim_copy(line);
		if (line.size() >= 2 && ((line.front() == L'"' && line.back() == L'"') ||
		                         (line.front() == L'\'' && line.back() == L'\''))) {
			line = line.substr(1, line.size() - 2);
			line = trim_copy(line);
		}
		if (line.empty()) continue;
		if (!result.empty()) result += L"+";
		result += line;
	}
	return to_u(result);
}

UnicodeString FormatValue(long double value, bool is_hex, bool is_time, int output_digits,
                          bool comma_grouping)
{
	return format_value_internal(value, is_hex, is_time, output_digits, comma_grouping, false);
}

EvalResult Evaluate(const UnicodeString &expression, const EvalOptions &options)
{
	EvalResult result;
	std::wstring text = trim_copy(to_w(JoinInput(expression)));
	const std::size_t equal = text.find(L'=');
	if (equal != std::wstring::npos) {
		if (text.find(L'=', equal + 1) != std::wstring::npos) {
			result.error = _T("不正な文字");
			return result;
		}
		text = trim_copy(text.substr(equal + 1));
	}
	if (text.empty()) {
		result.error = _T("入力内容の誤り");
		return result;
	}
	try {
		Parser parser(text, options);
		const Value value = parser.Parse();
		result.ok = true;
		result.number = value.number;
		result.is_time = value.is_time;
		result.is_hex = value.is_hex;
		const bool grouping = options.comma_grouping || text.find(L',') != std::wstring::npos;
		result.value = format_value_internal(value.number, value.is_hex, value.is_time,
		                                     options.output_digits, grouping, true);
		return result;
	} catch (const ParseError &error) {
		result.error = to_u(error.message);
		return result;
	} catch (...) {
		result.error = _T("入力内容の誤り");
		return result;
	}
}

UnicodeString ToggleHex(const UnicodeString &value)
{
	const std::wstring text = trim_copy(to_w(value));
	EvalOptions options;
	EvalResult result;
	if (IsHexString(to_u(text))) {
		result = Evaluate(to_u(text), options);
		return result.ok ? FormatValue(result.number) : value;
	}
	if (IsIntegerLiteral(to_u(text))) {
		result = Evaluate(to_u(text), options);
		return result.ok ? FormatValue(result.number, true) : value;
	}
	return value;
}

UnicodeString BitwiseNot(long long value)
{
	return FormatValue(static_cast<long double>(~value));
}

bool ConvertUnit(long double value, Unit from, Unit to, long double &result)
{
	UnitInfo from_info;
	UnitInfo to_info;
	if (!is_finite(value) || !unit_info(from, from_info) || !unit_info(to, to_info) ||
	    from_info.dimension != to_info.dimension) {
		return false;
	}
	result = value * from_info.factor / to_info.factor;
	return is_finite(result);
}

}  // namespace calc
