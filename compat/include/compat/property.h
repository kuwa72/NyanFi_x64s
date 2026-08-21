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
	auto LastDelimiter(A &&...a) const { return get().LastDelimiter(std::forward<A>(a)...); } \
	/* Insert は「一時オブジェクトを書き換えて捨てる」= 何もしない。            */   \
	/* C++Builder でも __property の読みは値返しなので挙動は同じで、意図的に     */   \
	/* そちらに合わせてある (規約6: 既存のバグは直さず記録する)。               */   \
	/* 実際に src/Global.cpp:15077 の `cb_buf->Text.Insert(...)` は何もしておらず、*/  \
	/* 「AD (クリップボードに追加)」が効いていない (報告書 §12)。               */   \
	/* 戻り値を void にしてあるのは、UnicodeString::Insert が参照を返すため      */   \
	/* そのまま転送すると破棄済みの一時オブジェクトへの参照になるから。          */   \
	template <class... A>                                                              \
	void Insert(A &&...a) const { auto tmp = get(); tmp.Insert(std::forward<A>(a)...); } \
	/* c_str は「その式の中でだけ使う」ことが前提。プロパティの読みは値返しなので */  \
	/* 一時オブジェクトを指し、式を抜けると無効になる。                          */  \
	/*   OK : ::PathIsDirectory(PathName.c_str())   (check_thread.cpp:35)        */  \
	/*   NG : const wchar_t *p = obj->Prop.c_str(); (式を抜けた時点でダングリング) */  \
	/* **C++Builder の __property でもまったく同じ**なので、危険度は移植で         */  \
	/* 増えていない。src は C++Builder 向けに書かれているため、ここで通る形は      */  \
	/* 向こうでも通っていたことになる。                                           */  \
	auto c_str() const { return get().c_str(); }

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

	/// 値がポインタのときに `obj->Prop->Member` と書けるようにする
	/// (`Screen->Fonts->IndexOf(...)` = src/Global.cpp:5979)。
	/// T がポインタでない場合はこのメンバが実体化されないので害は無い
	auto operator->() const { return (owner_->*Getter)(); }

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

/// 値型の読み書きプロパティ (getter が非 const、setter が値渡しの場合)
///
/// `src/task_thread.h` などスレッド系のクラスは
/// `bool __fastcall GetTaskReady()` / `void __fastcall SetTaskReady(bool Value)`
/// のように **getter が非 const、setter が値渡し** で書かれている
/// (排他ロックを取るため const にできない)。RWValueProperty はこの形に
/// 束縛できないので、専用の版を用意する。src 側の宣言は変えない (規約3)。
template <class Owner, class T, T (Owner::*Getter)(), void (Owner::*Setter)(T)>
class RWMutableProperty {
public:
	explicit RWMutableProperty(Owner *owner) : owner_(owner) {}
	RWMutableProperty(const RWMutableProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }
	T get() const { return (owner_->*Getter)(); }

	NYANFI_PROPERTY_FORWARD_CONST_METHODS

	RWMutableProperty &operator=(const T &value)
	{
		(owner_->*Setter)(value);
		return *this;
	}

	RWMutableProperty &operator=(const RWMutableProperty &rhs)
	{
		(owner_->*Setter)(static_cast<T>(rhs));
		return *this;
	}

private:
	Owner *owner_;
};

/// 読み取り専用プロパティ (getter が非 const の場合)
template <class Owner, class T, T (Owner::*Getter)()>
class ROMutableProperty {
public:
	explicit ROMutableProperty(Owner *owner) : owner_(owner) {}
	ROMutableProperty(const ROMutableProperty &) = delete;

	operator T() const { return (owner_->*Getter)(); }
	T operator()() const { return (owner_->*Getter)(); }
	T get() const { return (owner_->*Getter)(); }

	NYANFI_PROPERTY_FORWARD_CONST_METHODS

private:
	Owner *owner_;
};

/// ポインタ要素の添字プロパティ: `lst->Items[i]` / `lst->Items[i] = p`
///
/// `__property T * Items[int Index] = {read=Get, write=Put};` の置き換え。
/// `src/usr_shell.h` の `TItemsProperty` を手で書いた実例があるが、同じ形が
/// `MarkList.h` と `task_thread.h` にも出てきたのでテンプレートにまとめた。
///
/// `operator->` を持たせてあるのは、既存コードに `Items[i]->Member` の書き方が
/// あるため。`operator T *` だけだと `->` が通らない。
template <class Owner, class T, T *(Owner::*Getter)(int), void (Owner::*Setter)(int, T *)>
class IndexedPtrProperty {
public:
	explicit IndexedPtrProperty(Owner *owner) : owner_(owner) {}
	IndexedPtrProperty(const IndexedPtrProperty &) = delete;

	/// 1要素への参照。読みと書きの両方に使う
	class Ref {
	public:
		Ref(Owner *owner, int index) : owner_(owner), index_(index) {}

		operator T *() const { return (owner_->*Getter)(index_); }
		T *operator->() const { return (owner_->*Getter)(index_); }

		Ref &operator=(T *item)
		{
			(owner_->*Setter)(index_, item);
			return *this;
		}

	private:
		Owner *owner_;
		int index_;
	};

	Ref operator[](int index) const { return Ref(owner_, index); }

private:
	Owner *owner_;
};

/// 読みはデータメンバ直読み、書きはセッター経由のプロパティ
///
/// `__property bool TopIsHeader = {read = FTopIsHeader, write = SetTopIsHeader};`
/// (src/TxtViewer.h:224) の形。C++Builder の __property は read にメンバ変数を
/// 直接書けるが、他のプロキシは getter 関数を要求するので専用の版を用意する。
/// src 側に getter を足さずに済ませるため (規約3)。
template <class Owner, class T, T Owner::*Field, void (Owner::*Setter)(T)>
class FieldRWProperty {
public:
	explicit FieldRWProperty(Owner *owner) : owner_(owner) {}
	FieldRWProperty(const FieldRWProperty &) = delete;

	operator T() const { return owner_->*Field; }
	T operator()() const { return owner_->*Field; }
	T get() const { return owner_->*Field; }

	NYANFI_PROPERTY_FORWARD_CONST_METHODS

	FieldRWProperty &operator=(const T &value)
	{
		(owner_->*Setter)(value);
		return *this;
	}

	FieldRWProperty &operator=(const FieldRWProperty &rhs)
	{
		(owner_->*Setter)(static_cast<T>(rhs));
		return *this;
	}

private:
	Owner *owner_;
};

}  // namespace compat

#endif  // NYANFI_COMPAT_PROPERTY_H
