/**
 * @file compat/set.h
 * @brief Delphi の集合型 (System::Set) 互換テンプレート
 *
 * 既存コードは file_filter.h の `Set<FilterOpt, foIsGrep, foExcludeTag>` や
 * usr_key.cpp の TShiftState で集合型を使っている (`s << ssShift`,
 * `s.Contains(ssShift)`)。issue #1 の「Delphi の Set<> 0件」は実測誤りで、
 * 少数だが実在するためシムが必要。
 */
#ifndef NYANFI_COMPAT_SET_H
#define NYANFI_COMPAT_SET_H

#include <bitset>
#include <initializer_list>

#include "compat/config.h"

/**
 * @brief Delphi の `set of T` 互換クラス
 * @tparam T    要素の列挙型
 * @tparam minE 下限の列挙子
 * @tparam maxE 上限の列挙子
 */
template <class T, unsigned char minE, unsigned char maxE>
class Set {
public:
	static constexpr unsigned char MinElem = minE;
	static constexpr unsigned char MaxElem = maxE;
	static constexpr std::size_t Size = static_cast<std::size_t>(maxE) - static_cast<std::size_t>(minE) + 1;

	Set() = default;

	Set(std::initializer_list<T> items)
	{
		for (T v : items) *this << v;
	}

	Set &operator<<(const T value)
	{
		const std::size_t i = index_of(value);
		if (i < Size) bits_.set(i);
		return *this;
	}

	Set &operator>>(const T value)
	{
		const std::size_t i = index_of(value);
		if (i < Size) bits_.reset(i);
		return *this;
	}

	bool Contains(const T value) const
	{
		const std::size_t i = index_of(value);
		return i < Size && bits_.test(i);
	}

	Set &Clear()
	{
		bits_.reset();
		return *this;
	}

	bool Empty() const { return bits_.none(); }

	Set operator+(const Set &rhs) const { return from_bits(bits_ | rhs.bits_); }   //!< 和集合
	Set operator-(const Set &rhs) const { return from_bits(bits_ & ~rhs.bits_); }  //!< 差集合
	Set operator*(const Set &rhs) const { return from_bits(bits_ & rhs.bits_); }   //!< 積集合

	Set &operator+=(const Set &rhs)
	{
		bits_ |= rhs.bits_;
		return *this;
	}
	Set &operator-=(const Set &rhs)
	{
		bits_ &= ~rhs.bits_;
		return *this;
	}

	bool operator==(const Set &rhs) const { return bits_ == rhs.bits_; }
	bool operator!=(const Set &rhs) const { return bits_ != rhs.bits_; }

private:
	static std::size_t index_of(const T value)
	{
		const long long v = static_cast<long long>(value) - static_cast<long long>(minE);
		return (v < 0) ? Size : static_cast<std::size_t>(v);
	}

	static Set from_bits(const std::bitset<Size> &b)
	{
		Set s;
		s.bits_ = b;
		return s;
	}

	std::bitset<Size> bits_;
};

namespace System {
using ::Set;
}  // namespace System

#endif  // NYANFI_COMPAT_SET_H
