/**
 * @file compat/src/encoding.cpp
 * @brief compat/encoding.h の実装
 */
#include "compat/encoding.h"

#include <cstring>
#include <vector>

namespace {

/// UTF-16 LE のコードページ (Delphi の Encoding.pas と同じ値)
constexpr unsigned int kCpUtf16LE = 1200;
/// UTF-16 BE のコードページ
constexpr unsigned int kCpUtf16BE = 1201;
/// US-ASCII のコードページ
constexpr unsigned int kCpAscii = 20127;

/// wchar_t (UTF-16 LE) の配列をバイト単位で反転させる (BE 変換用)
void SwapWords(const wchar_t *src, int charCount, std::vector<Byte> &out)
{
	out.resize(static_cast<std::size_t>(charCount) * 2);
	for (int i = 0; i < charCount; ++i) {
		const wchar_t c = src[i];
		out[static_cast<std::size_t>(i) * 2 + 0] = static_cast<Byte>((c >> 8) & 0xFF);
		out[static_cast<std::size_t>(i) * 2 + 1] = static_cast<Byte>(c & 0xFF);
	}
}

}  // namespace

TEncoding::TEncoding(unsigned int codePage) : code_page_(codePage) {}

//---------------------------------------------------------------------------
UnicodeString TEncoding::GetString(const TBytes &bytes, int index, int count) const
{
	if (count <= 0) return UnicodeString();

	const auto &vec = bytes.vec();
	if (index < 0 || static_cast<std::size_t>(index) >= vec.size()) return UnicodeString();
	const int avail = static_cast<int>(vec.size()) - index;
	if (count > avail) count = avail;
	if (count <= 0) return UnicodeString();

	const Byte *src = vec.data() + index;

	if (code_page_ == kCpUtf16LE) {
		// ネイティブの wchar_t (UTF-16 LE) と同じ並びなのでそのまま解釈する
		const int charCount = count / 2;
		return UnicodeString(reinterpret_cast<const wchar_t *>(src), charCount);
	}
	if (code_page_ == kCpUtf16BE) {
		const int charCount = count / 2;
		std::wstring tmp;
		tmp.resize(static_cast<std::size_t>(charCount));
		for (int i = 0; i < charCount; ++i) {
			const Byte hi = src[static_cast<std::size_t>(i) * 2 + 0];
			const Byte lo = src[static_cast<std::size_t>(i) * 2 + 1];
			tmp[static_cast<std::size_t>(i)] = static_cast<wchar_t>((hi << 8) | lo);
		}
		return UnicodeString(tmp);
	}

	// それ以外は Win32 の MultiByteToWideChar に丸投げする
	const int wlen = ::MultiByteToWideChar(code_page_, 0, reinterpret_cast<const char *>(src), count, nullptr, 0);
	if (wlen <= 0) return UnicodeString();
	std::wstring tmp;
	tmp.resize(static_cast<std::size_t>(wlen));
	::MultiByteToWideChar(code_page_, 0, reinterpret_cast<const char *>(src), count, tmp.data(), wlen);
	return UnicodeString(tmp);
}

//---------------------------------------------------------------------------
TBytes TEncoding::GetBytes(const UnicodeString &s) const
{
	TBytes result;
	const int len = s.Length();
	if (len <= 0) return result;

	if (code_page_ == kCpUtf16LE) {
		result.Length = len * 2;
		std::memcpy(result.vec().data(), s.c_str(), static_cast<std::size_t>(len) * sizeof(wchar_t));
		return result;
	}
	if (code_page_ == kCpUtf16BE) {
		std::vector<Byte> tmp;
		SwapWords(s.c_str(), len, tmp);
		result.Length = static_cast<int>(tmp.size());
		std::memcpy(result.vec().data(), tmp.data(), tmp.size());
		return result;
	}

	const int blen =
		::WideCharToMultiByte(code_page_, 0, s.c_str(), len, nullptr, 0, nullptr, nullptr);
	if (blen <= 0) return result;
	result.Length = blen;
	::WideCharToMultiByte(code_page_, 0, s.c_str(), len, reinterpret_cast<char *>(result.vec().data()), blen, nullptr,
	                      nullptr);
	return result;
}

//---------------------------------------------------------------------------
TBytes TEncoding::GetPreamble() const
{
	TBytes result;
	switch (code_page_) {
	case CP_UTF8:
		result.Length = 3;
		result.vec()[0] = 0xEF;
		result.vec()[1] = 0xBB;
		result.vec()[2] = 0xBF;
		break;
	case kCpUtf16LE:
		result.Length = 2;
		result.vec()[0] = 0xFF;
		result.vec()[1] = 0xFE;
		break;
	case kCpUtf16BE:
		result.Length = 2;
		result.vec()[0] = 0xFE;
		result.vec()[1] = 0xFF;
		break;
	default:
		break;  // BOM 無し (ANSI 系コードページ)
	}
	return result;
}

//---------------------------------------------------------------------------
// 静的インスタンス。プログラム終了まで生存させ、意図的に解放しない
// (実測: TEncoding::UTF8 などは unique_ptr 等で一度も包まれておらず、
//  呼び出し側が delete しない前提で書かれている)。
//---------------------------------------------------------------------------
TEncoding *TEncoding::UTF8 = new TEncoding(CP_UTF8);
TEncoding *TEncoding::Unicode = new TEncoding(kCpUtf16LE);
TEncoding *TEncoding::BigEndianUnicode = new TEncoding(kCpUtf16BE);
TEncoding *TEncoding::ANSI = new TEncoding(::GetACP());
TEncoding *TEncoding::Default = TEncoding::ANSI;
TEncoding *TEncoding::ASCII = new TEncoding(kCpAscii);

//---------------------------------------------------------------------------
TEncoding *TEncoding::GetEncoding(int codePage)
{
	// 実測どおり、呼び出す度に新規インスタンスを返す (呼び出し側が delete する)
	return new TEncoding(static_cast<unsigned int>(codePage));
}

//---------------------------------------------------------------------------
int TEncoding::GetBufferEncoding(const TBytes &buffer, TEncoding *&encoding, TEncoding *defaultEncoding)
{
	const auto &vec = buffer.vec();
	const std::size_t len = vec.size();

	if (len >= 3 && vec[0] == 0xEF && vec[1] == 0xBB && vec[2] == 0xBF) {
		encoding = TEncoding::UTF8;
		return 3;
	}
	if (len >= 2 && vec[0] == 0xFF && vec[1] == 0xFE) {
		encoding = TEncoding::Unicode;
		return 2;
	}
	if (len >= 2 && vec[0] == 0xFE && vec[1] == 0xFF) {
		encoding = TEncoding::BigEndianUnicode;
		return 2;
	}

	if (!encoding) encoding = defaultEncoding ? defaultEncoding : TEncoding::Default;
	return 0;
}
