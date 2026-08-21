/**
 * @file compat/ustring.h
 * @brief UnicodeString / AnsiString / UTF8String / DynamicArray 互換シム
 *
 * 対象コードでの実測: UnicodeString 1,671 / .IsEmpty() 235 / .Length() 116 /
 * .ToIntDef() 61 / .Insert() 39 / .SubString() 37 / .Pos() 31 / .Delete() 31 /
 * TStringDynArray 85。
 *
 * 重要な仕様: UnicodeString の添字と SubString / Pos / Insert / Delete の
 * index は **1 始まり** (Delphi 由来)。ここを 0 始まりにすると既存コード
 * 17,000 行が静かに壊れるため、絶対に変更しないこと。
 */
#ifndef NYANFI_COMPAT_USTRING_H
#define NYANFI_COMPAT_USTRING_H

#include <cstdarg>
#include <string>
#include <utility>
#include <vector>

#include "compat/config.h"

class UnicodeString;

//---------------------------------------------------------------------------
/**
 * @brief Delphi の動的配列 (System::DynamicArray) 互換
 * @details `ary.Length` をメンバ関数呼び出しなしで読み書きできる必要がある
 *          (C++Builder の __property 相当)。添字は 0 始まり。
 */
template <class T>
class DynamicArray {
public:
	DynamicArray();
	DynamicArray(const DynamicArray &src);
	DynamicArray(DynamicArray &&src) noexcept;
	explicit DynamicArray(int len);
	~DynamicArray();

	DynamicArray &operator=(const DynamicArray &src);
	DynamicArray &operator=(DynamicArray &&src) noexcept;

	int get_length() const;
	void set_length(int len);

	T &operator[](int index);
	const T &operator[](int index) const;

	/// `ary.Length` / `ary.Length = n` を成立させるためのプロキシ
	class LengthProxy {
	public:
		explicit LengthProxy(DynamicArray *owner) : owner_(owner) {}
		operator int() const { return owner_->get_length(); }
		LengthProxy &operator=(int len);
		void rebind(DynamicArray *owner) { owner_ = owner; }

	private:
		DynamicArray *owner_;
	};

	LengthProxy Length{this};

	std::vector<T> &vec() { return items_; }              //!< シム独自: 実体への直接アクセス
	const std::vector<T> &vec() const { return items_; }  //!< シム独自: 実体への直接アクセス

private:
	std::vector<T> items_;
};

using TStringDynArray = DynamicArray<UnicodeString>;
using TBytes = DynamicArray<Byte>;

//---------------------------------------------------------------------------
/**
 * @brief AnsiString / UTF8String / RawByteString の共通実装
 * @tparam CodePage 0 = ANSI(CP_ACP), 65001 = UTF-8, 0xFFFF = 変換なし
 */
template <unsigned short CodePage>
class AnsiStringT {
public:
	AnsiStringT();
	AnsiStringT(const AnsiStringT &src);
	AnsiStringT(const char *src);
	AnsiStringT(const char *src, int len);
	AnsiStringT(const wchar_t *src);
	AnsiStringT(const UnicodeString &src);
	~AnsiStringT();

	AnsiStringT &operator=(const AnsiStringT &src);

	int Length() const;
	bool IsEmpty() const;
	const char *c_str() const;
	char *data();
	const char *data() const;
	Byte operator[](int index) const;  //!< 1 始まり

	AnsiStringT SubString(int index, int count) const;  //!< 1 始まり
	int Pos(const AnsiStringT &sub) const;              //!< 1 始まり、無ければ 0
	AnsiStringT &SetLength(int len);

	UnicodeString ToUnicode() const;  //!< シム独自: 明示変換

	bool operator==(const AnsiStringT &rhs) const;
	bool operator!=(const AnsiStringT &rhs) const;
	AnsiStringT operator+(const AnsiStringT &rhs) const;
	AnsiStringT &operator+=(const AnsiStringT &rhs);

	std::string &str() { return bytes_; }              //!< シム独自: 実体への直接アクセス
	const std::string &str() const { return bytes_; }  //!< シム独自: 実体への直接アクセス

private:
	std::string bytes_;
};

using AnsiString = AnsiStringT<0>;
using UTF8String = AnsiStringT<65001>;
using RawByteString = AnsiStringT<0xFFFF>;

//---------------------------------------------------------------------------
/**
 * @brief C++Builder の UnicodeString 互換クラス (std::wstring ラッパ)
 * @details index はすべて 1 始まり。
 */
class UnicodeString {
public:
	UnicodeString();
	UnicodeString(const UnicodeString &src);
	UnicodeString(UnicodeString &&src) noexcept;
	UnicodeString(const wchar_t *src);
	UnicodeString(const wchar_t *src, int len);
	UnicodeString(const char *src);  //!< CP_ACP からの変換
	UnicodeString(const char *src, int len);  //!< CP_ACP からの変換 (len バイト分、NUL 終端不要)
	UnicodeString(wchar_t ch);
	/// 1文字からの構築。`get_tkn_r(s, ',')` のように char リテラルをそのまま
	/// 渡す慣用句が src 全体で多用されているため、**必須**。これが無いと
	/// char は「昇格」で UnicodeString(int) に解決され、`','` が "44" になる
	UnicodeString(char ch);

	//-- 数値からの構築 --------------------------------------------------
	// C++Builder の UnicodeString は数値からの変換が **暗黙** で、
	// `val_str = v_ui;` のように整数を代入すると10進表記の文字列になる
	// (実測: usr_exif.cpp に 9箇所、usr_str.cpp に 4箇所)。
	//
	// ここを explicit にすると、暗黙変換が UnicodeString(wchar_t) 経由に
	// 落ちて「その整数値のコードポイント1文字」になり、コンパイルは通るのに
	// 静かに壊れる。実際に Exif_GetImgSize が常に 0x0 を返す不具合になった。
	//
	// 型ごとに用意してあるのは曖昧さを避けるため。int だけにすると
	// unsigned int の実引数が int と wchar_t のどちらにも変換できてしまう。
	UnicodeString(int value);
	UnicodeString(unsigned int value);
	UnicodeString(long value);
	UnicodeString(unsigned long value);
	UnicodeString(long long value);
	UnicodeString(unsigned long long value);
	UnicodeString(double value);  //!< FloatToStr 相当 (General 書式)
	UnicodeString(const AnsiString &src);
	UnicodeString(const UTF8String &src);
	explicit UnicodeString(const std::wstring &src);  //!< シム独自
	~UnicodeString();

	UnicodeString &operator=(const UnicodeString &src);
	UnicodeString &operator=(UnicodeString &&src) noexcept;
	UnicodeString &operator=(const wchar_t *src);

	//-- 参照 --------------------------------------------------------------
	int Length() const;
	bool IsEmpty() const;
	const wchar_t *c_str() const;
	wchar_t *data();
	const wchar_t *data() const;
	const wchar_t *LastChar() const;
	const wchar_t *FirstChar() const;

	wchar_t &operator[](int index);             //!< 1 始まり
	const wchar_t &operator[](int index) const; //!< 1 始まり

	//-- 部分文字列・検索 --------------------------------------------------
	UnicodeString SubString(int index, int count) const;  //!< 1 始まり
	UnicodeString SubString(int index) const;             //!< 1 始まり、末尾まで
	UnicodeString SubStr(int index, int count) const;     //!< SubString の別名
	int Pos(const UnicodeString &sub) const;              //!< 1 始まり、無ければ 0
	int Pos(wchar_t ch) const;
	int LastDelimiter(const UnicodeString &delims) const; //!< 1 始まり、無ければ 0
	bool IsDelimiter(const UnicodeString &delims, int index) const;
	bool IsPathDelimiter(int index) const;
	bool IsLeadSurrogate(int index) const;
	bool IsTrailSurrogate(int index) const;

	//-- 変更 (自身を書き換えて *this を返す) ------------------------------
	UnicodeString &Delete(int index, int count);
	UnicodeString &Insert(const UnicodeString &str, int index);
	UnicodeString &SetLength(int len);
	UnicodeString &Unique();  //!< COW 前提の API。シムでは何もしない

	//-- 生成 (自身は変更しない) ------------------------------------------
	UnicodeString Trim() const;
	UnicodeString TrimLeft() const;
	UnicodeString TrimRight() const;
	UnicodeString UpperCase() const;
	UnicodeString LowerCase() const;

	//-- 数値変換 ----------------------------------------------------------
	int ToInt() const;                //!< 失敗時 EConvertError
	int ToIntDef(int defValue) const; //!< 失敗時 defValue
	double ToDouble() const;

	//-- 書式化 (可変引数。戻り値は *this) --------------------------------
	UnicodeString &sprintf(const wchar_t *format, ...);
	UnicodeString &cat_sprintf(const wchar_t *format, ...);
	/// cat_sprintf と同じ。C++Builder は両方の綴りを持ち、src/Global.cpp:5572 が
	/// こちらを使っている (src 全体でこの1箇所だけ)
	UnicodeString &cat_printf(const wchar_t *format, ...);
	UnicodeString &vprintf(const wchar_t *format, va_list args);
	UnicodeString &cat_vprintf(const wchar_t *format, va_list args);

	//-- 比較 --------------------------------------------------------------
	int CompareIC(const UnicodeString &rhs) const;  //!< 大小文字無視
	int Compare(const UnicodeString &rhs) const;

	//-- シム独自 ----------------------------------------------------------
	std::wstring &wstr() { return text_; }
	const std::wstring &wstr() const { return text_; }

private:
	std::wstring text_;
};

//===========================================================================
// テンプレート本体
//
// DynamicArray<T> / AnsiStringT<CodePage> はテンプレートであるため、
// 各翻訳単位から実体化できるようヘッダに本体を置く (契約ヘッダの追記可)。
// UnicodeString を値で返す関数があるため、UnicodeString の完全な定義より
// 後ろに置くこと。
//===========================================================================

//---------------------------------------------------------------------------
// DynamicArray<T>
//---------------------------------------------------------------------------
template <class T>
DynamicArray<T>::DynamicArray() : items_()
{
}

template <class T>
DynamicArray<T>::DynamicArray(const DynamicArray &src) : items_(src.items_)
{
}

template <class T>
DynamicArray<T>::DynamicArray(DynamicArray &&src) noexcept : items_(std::move(src.items_))
{
}

template <class T>
DynamicArray<T>::DynamicArray(int len) : items_(static_cast<std::size_t>(len > 0 ? len : 0))
{
}

template <class T>
DynamicArray<T>::~DynamicArray() = default;

template <class T>
DynamicArray<T> &DynamicArray<T>::operator=(const DynamicArray &src)
{
	if (this != &src) items_ = src.items_;
	return *this;
}

template <class T>
DynamicArray<T> &DynamicArray<T>::operator=(DynamicArray &&src) noexcept
{
	if (this != &src) items_ = std::move(src.items_);
	return *this;
}

template <class T>
int DynamicArray<T>::get_length() const
{
	return static_cast<int>(items_.size());
}

template <class T>
void DynamicArray<T>::set_length(int len)
{
	// vector::resize は既存要素を保持したまま伸縮する (Delphi SetLength と同じ)
	items_.resize(static_cast<std::size_t>(len > 0 ? len : 0));
}

template <class T>
T &DynamicArray<T>::operator[](int index)
{
	return items_[static_cast<std::size_t>(index)];
}

template <class T>
const T &DynamicArray<T>::operator[](int index) const
{
	return items_[static_cast<std::size_t>(index)];
}

template <class T>
typename DynamicArray<T>::LengthProxy &DynamicArray<T>::LengthProxy::operator=(int len)
{
	owner_->set_length(len);
	return *this;
}

//---------------------------------------------------------------------------
// AnsiStringT<CodePage>
//---------------------------------------------------------------------------
namespace compat_detail {

/// @brief AnsiStringT<CodePage> の変換に使う実際の Win32 コードページを返す
/// @details CodePage==0(AnsiString) と CodePage==0xFFFF(RawByteString) は
///          埋め込みコードページ情報を持たないため、実行時 CP_ACP 変換で
///          代用する (推測: RawByteString の「変換なし」は本シムでは対応しない)。
template <unsigned short CodePage>
constexpr unsigned int AnsiCodePageOf()
{
	if constexpr (CodePage == 65001)
		return CP_UTF8;
	else
		return CP_ACP;
}

}  // namespace compat_detail

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT() : bytes_()
{
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT(const AnsiStringT &src) : bytes_(src.bytes_)
{
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT(const char *src) : bytes_(src != nullptr ? src : "")
{
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT(const char *src, int len)
{
	if (src != nullptr && len > 0) bytes_.assign(src, static_cast<std::size_t>(len));
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT(const wchar_t *src)
{
	if (src == nullptr || *src == L'\0') return;

	const unsigned int cp = compat_detail::AnsiCodePageOf<CodePage>();
	const int wlen = static_cast<int>(std::char_traits<wchar_t>::length(src));
	const int len = ::WideCharToMultiByte(cp, 0, src, wlen, nullptr, 0, nullptr, nullptr);
	if (len > 0) {
		bytes_.resize(static_cast<std::size_t>(len));
		::WideCharToMultiByte(cp, 0, src, wlen, bytes_.data(), len, nullptr, nullptr);
	}
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::AnsiStringT(const UnicodeString &src) : AnsiStringT(src.c_str())
{
}

template <unsigned short CodePage>
AnsiStringT<CodePage>::~AnsiStringT() = default;

template <unsigned short CodePage>
AnsiStringT<CodePage> &AnsiStringT<CodePage>::operator=(const AnsiStringT &src)
{
	bytes_ = src.bytes_;
	return *this;
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::Length() const
{
	return static_cast<int>(bytes_.size());
}

template <unsigned short CodePage>
bool AnsiStringT<CodePage>::IsEmpty() const
{
	return bytes_.empty();
}

template <unsigned short CodePage>
const char *AnsiStringT<CodePage>::c_str() const
{
	return bytes_.c_str();
}

template <unsigned short CodePage>
char *AnsiStringT<CodePage>::data()
{
	return bytes_.data();
}

template <unsigned short CodePage>
const char *AnsiStringT<CodePage>::data() const
{
	return bytes_.data();
}

template <unsigned short CodePage>
Byte AnsiStringT<CodePage>::operator[](int index) const
{
	return static_cast<Byte>(bytes_[static_cast<std::size_t>(index - 1)]);
}

template <unsigned short CodePage>
AnsiStringT<CodePage> AnsiStringT<CodePage>::SubString(int index, int count) const
{
	const int len = Length();
	if (index < 1) index = 1;
	if (index > len || count <= 0) return AnsiStringT();

	const int avail = len - index + 1;
	if (count > avail) count = avail;

	AnsiStringT result;
	result.bytes_ = bytes_.substr(static_cast<std::size_t>(index - 1), static_cast<std::size_t>(count));
	return result;
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::Pos(const AnsiStringT &sub) const
{
	if (sub.IsEmpty()) return 0;
	const std::size_t p = bytes_.find(sub.bytes_);
	return (p == std::string::npos) ? 0 : static_cast<int>(p) + 1;
}

template <unsigned short CodePage>
AnsiStringT<CodePage> &AnsiStringT<CodePage>::SetLength(int len)
{
	bytes_.resize(static_cast<std::size_t>(len > 0 ? len : 0));
	return *this;
}

template <unsigned short CodePage>
UnicodeString AnsiStringT<CodePage>::ToUnicode() const
{
	if (bytes_.empty()) return UnicodeString();

	const unsigned int cp = compat_detail::AnsiCodePageOf<CodePage>();
	const int wlen = ::MultiByteToWideChar(cp, 0, bytes_.data(), static_cast<int>(bytes_.size()), nullptr, 0);
	if (wlen <= 0) return UnicodeString();

	std::wstring w(static_cast<std::size_t>(wlen), L'\0');
	::MultiByteToWideChar(cp, 0, bytes_.data(), static_cast<int>(bytes_.size()), w.data(), wlen);
	return UnicodeString(w);
}

template <unsigned short CodePage>
bool AnsiStringT<CodePage>::operator==(const AnsiStringT &rhs) const
{
	return bytes_ == rhs.bytes_;
}

template <unsigned short CodePage>
bool AnsiStringT<CodePage>::operator!=(const AnsiStringT &rhs) const
{
	return !(*this == rhs);
}

template <unsigned short CodePage>
AnsiStringT<CodePage> AnsiStringT<CodePage>::operator+(const AnsiStringT &rhs) const
{
	AnsiStringT result(*this);
	result.bytes_ += rhs.bytes_;
	return result;
}

template <unsigned short CodePage>
AnsiStringT<CodePage> &AnsiStringT<CodePage>::operator+=(const AnsiStringT &rhs)
{
	bytes_ += rhs.bytes_;
	return *this;
}

//---------------------------------------------------------------------------
// 演算子。const char* / const wchar_t* との混在を許すため非メンバで定義する。
//---------------------------------------------------------------------------
UnicodeString operator+(const UnicodeString &lhs, const UnicodeString &rhs);
UnicodeString operator+(const UnicodeString &lhs, const wchar_t *rhs);
UnicodeString operator+(const wchar_t *lhs, const UnicodeString &rhs);
UnicodeString operator+(const UnicodeString &lhs, wchar_t rhs);
UnicodeString operator+(wchar_t lhs, const UnicodeString &rhs);
UnicodeString &operator+=(UnicodeString &lhs, const UnicodeString &rhs);

bool operator==(const UnicodeString &lhs, const UnicodeString &rhs);
bool operator!=(const UnicodeString &lhs, const UnicodeString &rhs);
bool operator<(const UnicodeString &lhs, const UnicodeString &rhs);
bool operator<=(const UnicodeString &lhs, const UnicodeString &rhs);
bool operator>(const UnicodeString &lhs, const UnicodeString &rhs);
bool operator>=(const UnicodeString &lhs, const UnicodeString &rhs);

namespace System {
using ::AnsiString;
using ::AnsiStringT;
using ::DynamicArray;
using ::RawByteString;
using ::TBytes;
using ::TStringDynArray;
using ::UnicodeString;
using ::UTF8String;
}  // namespace System

#endif  // NYANFI_COMPAT_USTRING_H
