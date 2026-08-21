/**
 * @file tests/compat/test_property.cpp
 * @brief compat/property.h (C++Builder の __property を再現するプロキシ) のテスト
 *
 * @details このヘッダにはテストが1件も無かった (規約9)。`__property` の置き換えは
 *          **コンパイルは通るのに静かに壊れる**種類の変更で、実際に規約2 の
 *          UnicodeString のオーバーロードでは2回壊している。
 *
 *          特に `operator T()` による暗黙変換は、意図しない変換に落ちても
 *          コンパイルが通ってしまう。値が正しく往復することを固定しておく。
 */
#include "doctest/doctest.h"

#include "compat/property.h"
#include "compat/ustring.h"

namespace {

/// getter が const・setter が const 参照 (ROProperty / RWProperty / RWValueProperty 用)
class ConstStyle {
public:
	int GetCount() const { return count_; }

	UnicodeString GetText() const { return text_; }
	void SetText(const UnicodeString &v) { text_ = v; }

	bool GetFlag() const { return flag_; }
	void SetFlag(bool v) { flag_ = v; }

	compat::ROProperty<ConstStyle, int, &ConstStyle::GetCount> Count{this};
	compat::RWProperty<ConstStyle, UnicodeString, &ConstStyle::GetText, &ConstStyle::SetText> Text{this};
	compat::RWValueProperty<ConstStyle, bool, &ConstStyle::GetFlag, &ConstStyle::SetFlag> Flag{this};

	int count_ = 3;
	UnicodeString text_ = _T("あいう");
	bool flag_ = false;
};

/// getter が非 const・setter が値渡し (スレッド系の実際の形。RWMutableProperty 用)
class MutableStyle {
public:
	// 排他ロックを取る想定なので const にできない、という src の形を再現する
	bool GetBusy() { read_count_++; return busy_; }
	void SetBusy(bool v) { write_count_++; busy_ = v; }

	int GetSize() { read_count_++; return size_; }

	compat::RWMutableProperty<MutableStyle, bool, &MutableStyle::GetBusy, &MutableStyle::SetBusy> Busy{this};
	compat::ROMutableProperty<MutableStyle, int, &MutableStyle::GetSize> Size{this};

	bool busy_ = false;
	int size_ = 5;
	int read_count_ = 0;
	int write_count_ = 0;
};

/// 添字プロパティ (MarkList / TaskConfigList の形)
class IndexedStyle {
public:
	struct Item {
		int value = 0;
	};

	Item *Get(int index) { return slots_[index]; }
	void Put(int index, Item *item) { slots_[index] = item; }

	compat::IndexedPtrProperty<IndexedStyle, Item, &IndexedStyle::Get, &IndexedStyle::Put> Items{this};

	Item *slots_[4] = {};
};

}  // namespace

//===========================================================================
// 読み取り専用 / 読み書き (getter が const)
//===========================================================================

TEST_CASE("ROProperty: 括弧なしで読める")
{
	ConstStyle o;
	CHECK(o.Count == 3);
	CHECK(o.Count() == 3);

	o.count_ = 10;
	CHECK(o.Count == 10);  // 値をコピーせず毎回 getter を呼んでいる
}

TEST_CASE("RWProperty: 括弧なしで読み書きできる")
{
	ConstStyle o;
	CHECK(o.Text == UnicodeString(_T("あいう")));

	o.Text = _T("かきく");
	CHECK(o.text_ == UnicodeString(_T("かきく")));

	o.Text += _T("けこ");
	CHECK(o.text_ == UnicodeString(_T("かきくけこ")));
}

TEST_CASE("RWProperty: 戻り値のメンバをそのまま呼べる")
{
	// 既存コードに `fbuf->Text.SubString(1, p)` の書き方があるため
	ConstStyle o;
	o.Text = _T("abcdef");
	CHECK(o.Text.Length() == 6);
	CHECK(o.Text.SubString(2, 3) == UnicodeString(_T("bcd")));
	CHECK_FALSE(o.Text.IsEmpty());
}

TEST_CASE("RWValueProperty: bool の読み書き")
{
	ConstStyle o;
	CHECK_FALSE(o.Flag);
	o.Flag = true;
	CHECK(o.flag_);
	CHECK(o.Flag);
}

//===========================================================================
// getter が非 const・setter が値渡し (スレッド系)
//===========================================================================

TEST_CASE("RWMutableProperty: 非 const の getter に束縛できる")
{
	MutableStyle o;
	CHECK_FALSE(o.Busy);
	o.Busy = true;
	CHECK(o.busy_);
	CHECK(o.Busy);

	// 読み書きが毎回アクセサを通っていること。
	// 値をキャッシュしてしまうと排他ロックの意味が無くなる
	CHECK(o.write_count_ == 1);
	CHECK(o.read_count_ >= 2);
}

TEST_CASE("RWMutableProperty: プロパティ同士の代入")
{
	MutableStyle a, b;
	a.Busy = true;
	b.Busy = a.Busy;
	CHECK(b.busy_);
}

TEST_CASE("ROMutableProperty: 非 const の getter で読み取り専用")
{
	MutableStyle o;
	CHECK(o.Size == 5);
	o.size_ = 9;
	CHECK(o.Size == 9);
}

//===========================================================================
// 添字プロパティ
//===========================================================================

TEST_CASE("IndexedPtrProperty: 添字での読み書き")
{
	IndexedStyle o;
	IndexedStyle::Item a{11};
	IndexedStyle::Item b{22};

	o.Items[0] = &a;
	o.Items[2] = &b;

	CHECK(o.slots_[0] == &a);
	CHECK(o.slots_[2] == &b);
	CHECK(o.slots_[1] == nullptr);

	// ポインタとして取り出せる
	IndexedStyle::Item *p = o.Items[0];
	CHECK(p == &a);
}

TEST_CASE("IndexedPtrProperty: 添字の結果に -> でメンバを呼べる")
{
	// 既存コードに `Items[i]->Member` の書き方があるため。
	// operator T* だけだと -> が通らない
	IndexedStyle o;
	IndexedStyle::Item a{33};
	o.Items[1] = &a;

	CHECK(o.Items[1]->value == 33);

	o.Items[1]->value = 44;
	CHECK(a.value == 44);
}

TEST_CASE("IndexedPtrProperty: 未設定の要素は nullptr")
{
	IndexedStyle o;
	IndexedStyle::Item *p = o.Items[3];
	CHECK(p == nullptr);
}
