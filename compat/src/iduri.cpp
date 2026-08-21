/**
 * @file compat/src/iduri.cpp
 * @brief `TIdURI::URLEncode()` の実装 (IdURI.hpp のコメントに設計の根拠がある)
 */
#include "IdURI.hpp"

#include <string>

namespace {

/// percent-encode せずそのまま残す文字。
/// RFC 3986 の unreserved (`A-Za-z0-9-._~`) に、URL の構造を壊さないための
/// 予約区切り文字を足したもの。`%` を残すのは、既に符号化済みの URL を
/// 二重符号化しないため (呼び出し側の `if (url.Pos("%")==0)` と同じ意図)
bool is_keep_char(wchar_t c)
{
	if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9')) return true;
	switch (c) {
	case L'-': case L'.': case L'_': case L'~':                       // unreserved
	case L':': case L'/': case L'?': case L'#': case L'[': case L']': // gen-delims
	case L'@':
	case L'!': case L'$': case L'&': case L'\'': case L'(': case L')': // sub-delims
	case L'*': case L'+': case L',': case L';': case L'=':
	case L'%':                                                         // 二重符号化を避ける
		return true;
	default:
		return false;
	}
}

const wchar_t kHex[] = L"0123456789ABCDEF";

}  // namespace

//---------------------------------------------------------------------------
UnicodeString TIdURI::URLEncode(const UnicodeString &url, IdTextEncoding)
{
	// UTF-8 のバイト列にしてから、残す文字以外を %XX にする
	const std::wstring src(url.c_str() ? url.c_str() : L"");
	std::wstring out;
	out.reserve(src.size() * 2);

	for (size_t i = 0; i < src.size(); i++) {
		const wchar_t c = src[i];
		if (is_keep_char(c)) {
			out += c;
			continue;
		}

		// サロゲートペアを1つのコードポイントとして扱う
		unsigned long cp = static_cast<unsigned long>(static_cast<unsigned short>(c));
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < src.size()) {
			const wchar_t lo = src[i + 1];
			if (lo >= 0xDC00 && lo <= 0xDFFF) {
				cp = 0x10000UL + ((cp - 0xD800UL) << 10) + (static_cast<unsigned long>(lo) - 0xDC00UL);
				i++;
			}
		}

		unsigned char bytes[4];
		int n = 0;
		if (cp < 0x80) {
			bytes[n++] = static_cast<unsigned char>(cp);
		}
		else if (cp < 0x800) {
			bytes[n++] = static_cast<unsigned char>(0xC0 | (cp >> 6));
			bytes[n++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000) {
			bytes[n++] = static_cast<unsigned char>(0xE0 | (cp >> 12));
			bytes[n++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
			bytes[n++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
		}
		else {
			bytes[n++] = static_cast<unsigned char>(0xF0 | (cp >> 18));
			bytes[n++] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3F));
			bytes[n++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
			bytes[n++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
		}

		for (int k = 0; k < n; k++) {
			out += L'%';
			out += kHex[bytes[k] >> 4];
			out += kHex[bytes[k] & 0x0F];
		}
	}

	return UnicodeString(out.c_str());
}
