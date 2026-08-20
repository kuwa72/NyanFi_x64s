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

#include <string>
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
	UnicodeString(wchar_t ch);
	explicit UnicodeString(int value);     //!< 数値の文字列化 (C++Builder 互換)
	explicit UnicodeString(double value);  //!< 数値の文字列化 (C++Builder 互換)
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
