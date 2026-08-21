/**
 * @file compat/src/netencoding.cpp
 * @brief TURLEncoding の実装 (設計の根拠と未確認事項は compat/netencoding.h に記載)
 */
#include "compat/netencoding.h"

#include <string>

namespace {

TURLEncoding g_url_encoding;

/// RFC 3986 の未予約文字。これ以外はすべて percent-encode する
bool is_unreserved(wchar_t c)
{
	if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9')) return true;
	return c == L'-' || c == L'.' || c == L'_' || c == L'~';
}

const wchar_t kHex[] = L"0123456789ABCDEF";

int hex_value(wchar_t c)
{
	if (c >= L'0' && c <= L'9') return c - L'0';
	if (c >= L'A' && c <= L'F') return c - L'A' + 10;
	if (c >= L'a' && c <= L'f') return c - L'a' + 10;
	return -1;
}

/// UTF-8 のバイト列を UnicodeString にする
UnicodeString from_utf8(const std::string &bytes)
{
	if (bytes.empty()) return UnicodeString();
	const int n = ::MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), static_cast<int>(bytes.size()), NULL, 0);
	if (n <= 0) return UnicodeString();
	std::wstring out(static_cast<size_t>(n), L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), static_cast<int>(bytes.size()), &out[0], n);
	return UnicodeString(out.c_str());
}

}  // namespace

TURLEncoding *const TURLEncoding::URL = &g_url_encoding;

//---------------------------------------------------------------------------
UnicodeString TURLEncoding::Encode(const UnicodeString &s) const
{
	const std::wstring src(s.c_str() ? s.c_str() : L"");
	if (src.empty()) return UnicodeString();

	// いったん UTF-8 にしてから、未予約文字以外を %XX にする
	const int n = ::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), static_cast<int>(src.size()),
	                                    NULL, 0, NULL, NULL);
	if (n <= 0) return UnicodeString();
	std::string bytes(static_cast<size_t>(n), '\0');
	::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), static_cast<int>(src.size()),
	                      &bytes[0], n, NULL, NULL);

	std::wstring out;
	out.reserve(bytes.size() * 3);
	for (unsigned char b : bytes) {
		if (b < 0x80 && is_unreserved(static_cast<wchar_t>(b))) {
			out += static_cast<wchar_t>(b);
		}
		else {
			out += L'%';
			out += kHex[b >> 4];
			out += kHex[b & 0x0F];
		}
	}
	return UnicodeString(out.c_str());
}

//---------------------------------------------------------------------------
UnicodeString TURLEncoding::Decode(const UnicodeString &s) const
{
	const std::wstring src(s.c_str() ? s.c_str() : L"");
	if (src.empty()) return UnicodeString();

	std::string bytes;
	bytes.reserve(src.size());
	for (size_t i = 0; i < src.size(); i++) {
		const wchar_t c = src[i];
		if (c == L'%' && i + 2 < src.size()) {
			const int hi = hex_value(src[i + 1]);
			const int lo = hex_value(src[i + 2]);
			if (hi >= 0 && lo >= 0) {
				bytes += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
			// 不正な %XX はそのまま通す (Delphi も例外にはしない)
		}
		if (c < 0x80) {
			bytes += static_cast<char>(c);
		}
		else {
			// 既に非 ASCII の文字は UTF-8 に直して混ぜる
			const wchar_t one[2] = {c, L'\0'};
			char buf[8] = {};
			const int m = ::WideCharToMultiByte(CP_UTF8, 0, one, 1, buf, sizeof(buf), NULL, NULL);
			bytes.append(buf, static_cast<size_t>(m > 0 ? m : 0));
		}
	}
	return from_utf8(bytes);
}
