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

namespace compat {

/// 読み取り専用プロパティ: `obj->Count`
template <class Owner, class T, T (Owner::*Getter)() const>
class ROProperty {
public:
	explicit ROProperty(Owner *owner) : owner_(owner) {}
	ROProperty(const ROProperty &) = delete;
	ROProperty &operator=(const ROProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }

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
