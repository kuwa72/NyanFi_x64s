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
#include "compat/exception.h"
#include "compat/property.h"
#include "compat/ustring.h"

class TStrings;
class TStream;    //!< compat/streams.h で定義 (循環回避のため前方宣言に留める)
class TEncoding;  //!< compat/encoding.h で定義 (循環回避のため前方宣言に留める)

//---------------------------------------------------------------------------
/// System.Rtti 相当の最小限。compat/controls.h の __classid マクロと対で使う
using TClass = const void *;

/// TObject 互換 (ClassNameIs / Free のみ実装)
class TObject {
public:
	TObject() = default;
	virtual ~TObject() = default;

	void Free() { delete this; }
	virtual UnicodeString ClassName() const;
	bool ClassNameIs(const UnicodeString &name) const;

	/// @warning 宣言のみ。UserFunc.h::class_is_CustomEdit の型チェックのためだけに
	/// 存在し、実際に呼ばれる経路は無い (未使用の inline 関数)
	bool InheritsFrom(TClass cls) const;
};

/// TPersistent 互換 (Assign のみ)
class TPersistent : public TObject {
public:
	virtual void Assign(TPersistent *source);
};

//---------------------------------------------------------------------------
/**
 * @brief TComponent 互換 (最小実装)
 * @details 実測: usr_swatch.cpp の `UsrSwatchPanel(TComponent *Owner) : TPanel(Owner)`、
 *          usr_scrpanel.cpp の `new TPaintBox(ParentPanel)` 等、GUI コントロールの
 *          所有者チェーンを表すためだけに使われている。Phase 0/1 の範囲では
 *          実際の所有・自動破棄は不要 (テストではオブジェクトを明示的に
 *          new/delete しているため)。Name (コンポーネント名。UIniFile.cpp が
 *          INI のセクション名キーとして使う) だけ実体を持たせる。
 */
class TComponent : public TPersistent {
public:
	TComponent() = default;
	explicit TComponent(TComponent *owner) : Owner(owner) {}

	TComponent *Owner = nullptr;
	UnicodeString Name;
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
	// 実測: LoadFromFile(fnam) / LoadFromFile(fnam, enc.get()) / SaveToFile(fnam) /
	// SaveToFile(fnam, TEncoding::UTF8) / LoadFromStream(fs.get()) /
	// LoadFromStream(fs.get(), enc.get()) の形がいずれも使われている。
	void LoadFromFile(const UnicodeString &fileName);
	void LoadFromFile(const UnicodeString &fileName, TEncoding *encoding);
	void SaveToFile(const UnicodeString &fileName) const;
	void SaveToFile(const UnicodeString &fileName, TEncoding *encoding) const;
	void LoadFromStream(TStream *stream);
	void LoadFromStream(TStream *stream, TEncoding *encoding);
	void SaveToStream(TStream *stream) const;       //!< シム独自: src/ での実測は無いが対称性のため用意
	void SaveToStream(TStream *stream, TEncoding *encoding) const;
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
	/// true なら DelimitedText の解析・生成で引用符/空白の特別扱いをせず Delimiter だけで区切る
	/// (実測: usr_str.cpp の get_csv_array/get_csv_item が CSV を正しく分割するために true にする。
	/// 既定値は Delphi RTL と同じ false)
	bool GetStrictDelimiter() const { return strict_delimiter_; }
	void SetStrictDelimiter(bool value) { strict_delimiter_ = value; }
	/// LoadFromFile/LoadFromStream で判定 (または明示指定) された文字コード。
	/// 一度も読み込んでいなければ NULL (実測: `if (...->Encoding && ...)` で null チェックあり)
	TEncoding *GetEncoding() const { return encoding_; }
	bool GetWriteBOM() const { return write_bom_; }
	void SetWriteBOM(bool value) { write_bom_ = value; }

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

	/**
	 * @brief Encoding プロパティ用の専用プロキシ
	 * @details 実測 `TxtBufList->Encoding->CodePage` のようにポインタとしての
	 *          `->` 連鎖と `if (...->Encoding && ...)` の真偽値チェックの両方が
	 *          来る。compat::ROProperty は operator-> を持たないため、ここだけ
	 *          専用の軽量プロキシを用意する。
	 */
	class EncodingProperty {
	public:
		explicit EncodingProperty(TStrings *owner) : owner_(owner) {}
		operator TEncoding *() const { return owner_->GetEncoding(); }
		TEncoding *operator->() const { return owner_->GetEncoding(); }

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
	compat::RWValueProperty<TStrings, bool, &TStrings::GetWriteBOM, &TStrings::SetWriteBOM> WriteBOM{this};
	compat::RWValueProperty<TStrings, bool, &TStrings::GetStrictDelimiter, &TStrings::SetStrictDelimiter>
		StrictDelimiter{this};

	StringsProperty Strings{this};
	ObjectsProperty Objects{this};
	ValuesProperty Values{this};
	ValueFromIndexProperty ValueFromIndex{this};
	NamesProperty Names{this};
	EncodingProperty Encoding{this};

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
	bool strict_delimiter_ = false;  // Delphi RTL の既定と同じ false

	//-- Encoding プロパティの実体 -------------------------------------------
	// encoding_ が owned_encoding_.get() と一致しない場合、encoding_ は
	// TEncoding::UTF8 などの静的インスタンス (delete しない) を指している。
	TEncoding *encoding_ = nullptr;
	std::unique_ptr<TEncoding> owned_encoding_;
	bool write_bom_ = true;

	/// CaseSensitive を考慮した比較 (TStringList が上書きする)
	virtual int CompareStrings(const UnicodeString &a, const UnicodeString &b) const;
	/// Load 系が確定したエンコードを Encoding プロパティへ安全に取り込む
	/// (静的インスタンスならそのまま保持、そうでなければ複製して所有権を持つ。
	///  呼び出し元が `std::unique_ptr<TEncoding>` で即座に delete する形が
	///  多いため、生ポインタをそのまま保持すると解放後アクセスになる対策)
	void AdoptEncoding(TEncoding *encoding);
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
	int CompareStrings(const UnicodeString &a, const UnicodeString &b) const override;

	bool sorted_ = false;
	bool case_sensitive_ = false;
	TDuplicates duplicates_ = dupIgnore;
};

/**
 * @brief Sorted=true かつ Duplicates=dupError のとき Add が投げる例外
 * @details src/ での実測は無い (Duplicates を dupError にする箇所が無い) が、
 *          Delphi RTL の契約として実装しておく。
 */
class EStringListError : public Exception {
public:
	using Exception::Exception;
};

//---------------------------------------------------------------------------
/// TList::Notify の Action 値
enum TListNotification { lnAdded, lnExtracted, lnDeleted };

/**
 * @brief TList 互換 (Delphi の `TList = class(TObject)`、void* の動的配列)
 * @details 実測: src/usr_shell.h の TDropTargetList がこれを継承し、
 *          `Get(int)` / `Put(int,void*)` と同名だが異なる引数型のメンバを
 *          private に再定義して (C++ 的にはオーバーライドではなく隠蔽)、
 *          型付きの `drop_target_rec*` を返す独自の __property Items[] から
 *          `TList::Get` / `TList::Put` を明示呼び出しする形で使っている。
 *          `Notify` だけは実際に仮想関数としてオーバーライドされ、
 *          `Action==lnDeleted` のときに要素 (drop_target_rec*) を delete する
 *          ためのフックとして使われている。
 *
 *          注意 (Delphi と異なる挙動): 本シムの `~TList()` は Clear() を呼んで
 *          残った要素を Notify(lnDeleted) 経由で解放しようとするが、C++ の
 *          仮想関数はデストラクタ連鎖中に「一番派生したクラスの vtable」へは
 *          もう戻らない (基底クラスのデストラクタ本体を実行している間は
 *          vtable がそのクラスのものに巻き戻る) ため、派生クラス
 *          (TDropTargetList など) が独自デストラクタで明示的に Clear() を
 *          呼ばない限り、`delete` 時点で残っている要素の Notify(lnDeleted)
 *          は基底の (何もしない) 実装にしかディスパッチされない。Object
 *          Pascal の VMT はデストラクタ実行中も巻き戻らないため実機の
 *          C++Builder ではこの問題は起きないが、素の C++ ではオブジェクトの
 *          寿命中 (Delete/Remove/Clear を明示的に呼ぶ経路) は正しく動作する
 *          ものの、破棄時の自動クリーンアップだけは効かない場合がある
 *          (usr_shell.cpp の TDropTargetList はこの経路に该当し、
 *          プロセス終了直前のリークにしかならない実害の小さいケース)。
 */
class TList : public TObject {
public:
	TList() = default;
	~TList() override;
	TList(const TList &) = delete;
	TList &operator=(const TList &) = delete;

	virtual int Add(void *item);
	virtual void Insert(int index, void *item);
	virtual void Delete(int index);
	int Remove(void *item);      //!< IndexOf で見つけて Delete する。見つからなければ -1
	void *Extract(void *item);   //!< Notify(lnExtracted) を呼び、削除して所有権を返す (見つからなければ nullptr)
	virtual void Clear();

	int IndexOf(void *item) const;

	int GetCount() const { return static_cast<int>(items_.size()); }
	void SetCount(int value);
	int GetCapacity() const { return static_cast<int>(items_.capacity()); }
	void SetCapacity(int value) { items_.reserve(static_cast<std::size_t>(value)); }

	virtual void *Get(int index) const { return items_[static_cast<std::size_t>(index)]; }
	virtual void Put(int index, void *item) { items_[static_cast<std::size_t>(index)] = item; }

	/// `Items[i]` / `Items[i] = p` の両方を Get/Put 経由で成立させる書き戻し可能プロキシ
	class ItemRef {
	public:
		ItemRef(TList *owner, int index) : owner_(owner), index_(index) {}
		operator void *() const { return owner_->Get(index_); }
		ItemRef &operator=(void *value)
		{
			owner_->Put(index_, value);
			return *this;
		}

	private:
		TList *owner_;
		int index_;
	};
	class ItemsProperty {
	public:
		explicit ItemsProperty(TList *owner) : owner_(owner) {}
		ItemRef operator[](int index) const { return ItemRef(owner_, index); }

	private:
		TList *owner_;
	};

	compat::ROProperty<TList, int, &TList::GetCount> Count{this};
	compat::RWValueProperty<TList, int, &TList::GetCapacity, &TList::SetCapacity> Capacity{this};
	ItemsProperty Items{this};

protected:
	/// 既定では何もしない (TDropTargetList などがオーバーライドして所有権解放に使う)
	virtual void Notify(void *ptr, TListNotification action);

	std::vector<void *> items_;
};

//---------------------------------------------------------------------------
/**
 * @brief TMultiReadExclusiveWriteSynchronizer 互換 (多重読み / 排他書き)
 * @details 実測: BeginWrite/EndWrite 各40箇所、BeginRead/EndRead 各17箇所。
 *          usr_tag.h と Global.h が TagDataList / アイコンキャッシュ / ログの
 *          保護に使っている。SRWLOCK で実装する。
 *
 *          相違点: Delphi の実装は同一スレッドからの再入を許すが、SRWLOCK は
 *          再入不可 (同じスレッドで二重取得するとデッドロックする)。呼び出し側が
 *          再入していないかは、該当ファイル (usr_tag.cpp / Global.cpp) を
 *          Phase 1 で移植する際に確認が必要。
 */
class TMultiReadExclusiveWriteSynchronizer : public TObject {
public:
	TMultiReadExclusiveWriteSynchronizer();
	~TMultiReadExclusiveWriteSynchronizer() override;

	void BeginRead();
	void EndRead();
	bool BeginWrite();
	void EndWrite();

private:
	SRWLOCK lock_;
};

using TMREWSync = TMultiReadExclusiveWriteSynchronizer;

namespace System {
namespace Classes {
using ::EStringListError;
using ::TDuplicates;
using ::TList;
using ::TListNotification;
using ::TObject;
using ::TPersistent;
using ::TStringList;
using ::TStrings;
}  // namespace Classes
using namespace Classes;
}  // namespace System

namespace Classes = System::Classes;

#endif  // NYANFI_COMPAT_CLASSES_H
