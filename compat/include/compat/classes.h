/**
 * @file compat/classes.h
 * @brief TObject / TStrings / TStringList 互換シム (System.Classes.hpp 相当)
 *
 * 対象コードでの実測: TStringList 243 / ->Add() 152 / ->Strings[] 84 /
 * ->Count 75 / ->Values[] 37 / ->Text 32 / ->Objects[] 23 /
 * std::unique_ptr<TStringList> 125。
 *
 * 呼び出し形を変えないことが最優先。特に:
 *   - `lst->Strings[i]` は代入もメンバ呼び出しも来るので UnicodeString& を返す
 *   - `lst->Values[k]` は `.ToIntDef()` `.IsEmpty()` が来るうえ代入もあるため、
 *     UnicodeString を継承した書き戻し可能な参照型を返す
 *   - インデックスは Strings/Objects は **0 始まり** (UnicodeString とは逆)
 */
#ifndef NYANFI_COMPAT_CLASSES_H
#define NYANFI_COMPAT_CLASSES_H

#include <memory>
#include <vector>

#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

class TStrings;

//---------------------------------------------------------------------------
/// TObject 互換 (ClassNameIs / Free のみ実装)
class TObject {
public:
	TObject() = default;
	virtual ~TObject() = default;

	void Free() { delete this; }
	virtual UnicodeString ClassName() const;
	bool ClassNameIs(const UnicodeString &name) const;
};

/// TPersistent 互換 (Assign のみ)
class TPersistent : public TObject {
public:
	virtual void Assign(TPersistent *source);
};

//---------------------------------------------------------------------------
/// TStringList::Duplicates の値
enum TDuplicates { dupIgnore, dupAccept, dupError };

/// CustomSort に渡す比較関数の型
class TStringList;
typedef int(__fastcall *TStringListSortCompare)(TStringList *List, int Index1, int Index2);

//---------------------------------------------------------------------------
/**
 * @brief TStrings 互換の基底
 * @details Phase 0 では TStringList だけが必要なので、実体の保持もここで行う。
 */
class TStrings : public TPersistent {
public:
	TStrings();
	~TStrings() override;
	TStrings(const TStrings &) = delete;
	TStrings &operator=(const TStrings &) = delete;

	//-- 要素の追加・削除 --------------------------------------------------
	virtual int Add(const UnicodeString &s);
	int AddObject(const UnicodeString &s, TObject *obj);
	void AddStrings(TStrings *strings);
	virtual void Insert(int index, const UnicodeString &s);
	void InsertObject(int index, const UnicodeString &s, TObject *obj);
	virtual void Delete(int index);
	virtual void Clear();
	void Exchange(int index1, int index2);
	void Move(int curIndex, int newIndex);
	void Assign(TPersistent *source) override;

	//-- 検索 --------------------------------------------------------------
	int IndexOf(const UnicodeString &s) const;
	int IndexOfName(const UnicodeString &name) const;
	int IndexOfObject(TObject *obj) const;

	//-- 入出力 ------------------------------------------------------------
	void LoadFromFile(const UnicodeString &fileName);
	void SaveToFile(const UnicodeString &fileName) const;
	void BeginUpdate();
	void EndUpdate();

	//-- プロパティのアクセサ (プロキシから呼ばれる) -----------------------
	int GetCount() const;
	UnicodeString GetText() const;
	void SetText(const UnicodeString &value);
	UnicodeString GetCommaText() const;
	void SetCommaText(const UnicodeString &value);
	UnicodeString GetDelimitedText() const;
	void SetDelimitedText(const UnicodeString &value);
	wchar_t GetDelimiter() const;
	void SetDelimiter(wchar_t value);
	wchar_t GetNameValueSeparator() const;
	void SetNameValueSeparator(wchar_t value);
	wchar_t GetQuoteChar() const;
	void SetQuoteChar(wchar_t value);
	UnicodeString GetLineBreak() const;
	void SetLineBreak(const UnicodeString &value);

	//-- シム独自: 実体への直接アクセス ------------------------------------
	UnicodeString &StringAt(int index);
	const UnicodeString &StringAt(int index) const;
	TObject *&ObjectAt(int index);
	UnicodeString ValueOf(const UnicodeString &name) const;
	void SetValue(const UnicodeString &name, const UnicodeString &value);
	UnicodeString NameAt(int index) const;
	UnicodeString ValueAt(int index) const;
	void SetValueAt(int index, const UnicodeString &value);

	//-- プロパティ --------------------------------------------------------
	/// 書き戻し可能な文字列プロパティ参照 (Values / ValueFromIndex 用)
	class StringRef : public UnicodeString {
	public:
		StringRef(TStrings *owner, int index, const UnicodeString &value)
			: UnicodeString(value), owner_(owner), index_(index) {}
		StringRef(TStrings *owner, const UnicodeString &name, const UnicodeString &value)
			: UnicodeString(value), owner_(owner), index_(-1), name_(name) {}
		StringRef &operator=(const UnicodeString &value);

	private:
		TStrings *owner_;
		int index_;
		UnicodeString name_;
	};

	class StringsProperty {
	public:
		explicit StringsProperty(TStrings *owner) : owner_(owner) {}
		UnicodeString &operator[](int index) { return owner_->StringAt(index); }
		const UnicodeString &operator[](int index) const { return owner_->StringAt(index); }

	private:
		TStrings *owner_;
	};

	class ObjectsProperty {
	public:
		explicit ObjectsProperty(TStrings *owner) : owner_(owner) {}
		TObject *&operator[](int index) { return owner_->ObjectAt(index); }

	private:
		TStrings *owner_;
	};

	class ValuesProperty {
	public:
		explicit ValuesProperty(TStrings *owner) : owner_(owner) {}
		StringRef operator[](const UnicodeString &name) const
		{
			return StringRef(owner_, name, owner_->ValueOf(name));
		}

	private:
		TStrings *owner_;
	};

	class ValueFromIndexProperty {
	public:
		explicit ValueFromIndexProperty(TStrings *owner) : owner_(owner) {}
		StringRef operator[](int index) const { return StringRef(owner_, index, owner_->ValueAt(index)); }

	private:
		TStrings *owner_;
	};

	class NamesProperty {
	public:
		explicit NamesProperty(TStrings *owner) : owner_(owner) {}
		UnicodeString operator[](int index) const { return owner_->NameAt(index); }

	private:
		TStrings *owner_;
	};

	compat::ROProperty<TStrings, int, &TStrings::GetCount> Count{this};
	compat::RWProperty<TStrings, UnicodeString, &TStrings::GetText, &TStrings::SetText> Text{this};
	compat::RWProperty<TStrings, UnicodeString, &TStrings::GetCommaText, &TStrings::SetCommaText> CommaText{this};
	compat::RWProperty<TStrings, UnicodeString, &TStrings::GetDelimitedText, &TStrings::SetDelimitedText>
		DelimitedText{this};
	compat::RWValueProperty<TStrings, wchar_t, &TStrings::GetDelimiter, &TStrings::SetDelimiter> Delimiter{this};
	compat::RWValueProperty<TStrings, wchar_t, &TStrings::GetNameValueSeparator, &TStrings::SetNameValueSeparator>
		NameValueSeparator{this};
	compat::RWValueProperty<TStrings, wchar_t, &TStrings::GetQuoteChar, &TStrings::SetQuoteChar> QuoteChar{this};
	compat::RWProperty<TStrings, UnicodeString, &TStrings::GetLineBreak, &TStrings::SetLineBreak> LineBreak{this};

	StringsProperty Strings{this};
	ObjectsProperty Objects{this};
	ValuesProperty Values{this};
	ValueFromIndexProperty ValueFromIndex{this};
	NamesProperty Names{this};

protected:
	struct Item {
		UnicodeString text;
		TObject *object = nullptr;
	};
	std::vector<Item> items_;
	wchar_t delimiter_ = L',';
	wchar_t name_value_separator_ = L'=';
	wchar_t quote_char_ = L'"';
	UnicodeString line_break_;
	int update_count_ = 0;
};

//---------------------------------------------------------------------------
/// TStringList 互換
class TStringList : public TStrings {
public:
	TStringList();
	~TStringList() override;

	int Add(const UnicodeString &s) override;
	void Insert(int index, const UnicodeString &s) override;

	void Sort();
	void CustomSort(TStringListSortCompare compare);

	bool GetSorted() const;
	void SetSorted(bool value);
	bool GetCaseSensitive() const;
	void SetCaseSensitive(bool value);
	TDuplicates GetDuplicates() const;
	void SetDuplicates(TDuplicates value);

	compat::RWValueProperty<TStringList, bool, &TStringList::GetSorted, &TStringList::SetSorted> Sorted{this};
	compat::RWValueProperty<TStringList, bool, &TStringList::GetCaseSensitive, &TStringList::SetCaseSensitive>
		CaseSensitive{this};
	compat::RWValueProperty<TStringList, TDuplicates, &TStringList::GetDuplicates, &TStringList::SetDuplicates>
		Duplicates{this};

private:
	bool sorted_ = false;
	bool case_sensitive_ = false;
	TDuplicates duplicates_ = dupIgnore;
};

namespace System {
namespace Classes {
using ::TDuplicates;
using ::TObject;
using ::TPersistent;
using ::TStringList;
using ::TStrings;
}  // namespace Classes
using namespace Classes;
}  // namespace System

namespace Classes = System::Classes;

#endif  // NYANFI_COMPAT_CLASSES_H
