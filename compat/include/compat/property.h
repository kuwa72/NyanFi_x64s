/**
 * @file compat/property.h
 * @brief C++Builder の __property を素の C++ で再現するためのプロキシ
 *
 * 既存コードは `lst->Count` / `lst->Text = s` / `lst->Strings[i]` のように
 * 括弧なしでプロパティを読み書きする (実測: ->Count 75, ->Text 32 など)。
 * 呼び出し側を書き換えずに通すため、メンバとして持てるプロキシを用意する。
 *
 * 制約: プロキシは所有者ポインタを保持するため、これを含むクラスは
 * コピー/ムーブ不可にすること (VCL のクラスも TPersistent 以下はコピー不可)。
 */
#ifndef NYANFI_COMPAT_PROPERTY_H
#define NYANFI_COMPAT_PROPERTY_H

#include <utility>

namespace compat {

/**
 * @brief プロパティ値のメンバを直接呼ぶ書き方への転送
 * @details 既存コードには `fbuf->Text.SubString(1, p + 6)` のように、算出
 *          プロパティの戻り値に対してそのままメンバを呼ぶ箇所がある
 *          (src 全体で SubString 9 / ToIntDef 8 / Pos 3 など)。
 *          メンバテンプレートなので実際に呼ばれた時だけ実体化され、値型が
 *          そのメンバを持たない場合は何も起きない。
 *
 *          c_str() は **意図的に転送していない**。一時オブジェクトのポインタを
 *          返すことになり、その場でダングリングするため。GUI コントロールの
 *          Text のように c_str() を呼ぶ側は、プロキシではなく実体のメンバとして
 *          持たせること。
 */
#define NYANFI_PROPERTY_FORWARD_CONST_METHODS                                          \
	template <class... A>                                                              \
	auto Length(A &&...a) const { return get().Length(std::forward<A>(a)...); }         \
	template <class... A>                                                              \
	auto IsEmpty(A &&...a) const { return get().IsEmpty(std::forward<A>(a)...); }       \
	template <class... A>                                                              \
	auto SubString(A &&...a) const { return get().SubString(std::forward<A>(a)...); }   \
	template <class... A>                                                              \
	auto SubStr(A &&...a) const { return get().SubStr(std::forward<A>(a)...); }         \
	template <class... A>                                                              \
	auto Pos(A &&...a) const { return get().Pos(std::forward<A>(a)...); }               \
	template <class... A>                                                              \
	auto Trim(A &&...a) const { return get().Trim(std::forward<A>(a)...); }             \
	template <class... A>                                                              \
	auto UpperCase(A &&...a) const { return get().UpperCase(std::forward<A>(a)...); }   \
	template <class... A>                                                              \
	auto LowerCase(A &&...a) const { return get().LowerCase(std::forward<A>(a)...); }   \
	template <class... A>                                                              \
	auto ToInt(A &&...a) const { return get().ToInt(std::forward<A>(a)...); }           \
	template <class... A>                                                              \
	auto ToIntDef(A &&...a) const { return get().ToIntDef(std::forward<A>(a)...); }     \
	template <class... A>                                                              \
	auto ToDouble(A &&...a) const { return get().ToDouble(std::forward<A>(a)...); }     \
	template <class... A>                                                              \
	auto LastDelimiter(A &&...a) const { return get().LastDelimiter(std::forward<A>(a)...); }

/// 読み取り専用プロパティ: `obj->Count`
template <class Owner, class T, T (Owner::*Getter)() const>
class ROProperty {
public:
	explicit ROProperty(Owner *owner) : owner_(owner) {}
	ROProperty(const ROProperty &) = delete;
	ROProperty &operator=(const ROProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }
	T get() const { return (owner_->*Getter)(); }

	NYANFI_PROPERTY_FORWARD_CONST_METHODS

private:
	Owner *owner_;
};

/// 読み書きプロパティ: `obj->Text` / `obj->Text = s` / `obj->Text += s`
template <class Owner, class T, T (Owner::*Getter)() const, void (Owner::*Setter)(const T &)>
class RWProperty {
public:
	explicit RWProperty(Owner *owner) : owner_(owner) {}
	RWProperty(const RWProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }
	T get() const { return (owner_->*Getter)(); }

	NYANFI_PROPERTY_FORWARD_CONST_METHODS

	RWProperty &operator=(const T &value)
	{
		(owner_->*Setter)(value);
		return *this;
	}

	RWProperty &operator=(const RWProperty &rhs)
	{
		(owner_->*Setter)(static_cast<T>(rhs));
		return *this;
	}

	RWProperty &operator+=(const T &value)
	{
		(owner_->*Setter)((owner_->*Getter)() + value);
		return *this;
	}

private:
	Owner *owner_;
};

/// 値型の読み書きプロパティ (bool / int / enum など)
template <class Owner, class T, T (Owner::*Getter)() const, void (Owner::*Setter)(T)>
class RWValueProperty {
public:
	explicit RWValueProperty(Owner *owner) : owner_(owner) {}
	RWValueProperty(const RWValueProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }

	RWValueProperty &operator=(T value)
	{
		(owner_->*Setter)(value);
		return *this;
	}

	RWValueProperty &operator=(const RWValueProperty &rhs)
	{
		(owner_->*Setter)(static_cast<T>(rhs));
		return *this;
	}

private:
	Owner *owner_;
};

}  // namespace compat

#endif  // NYANFI_COMPAT_PROPERTY_H
