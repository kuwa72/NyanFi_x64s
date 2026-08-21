/**
 * @file tests/compat/test_classes.cpp
 * @brief TObject / TStrings / TStringList 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "compat/classes.h"
#include "compat/encoding.h"
#include "compat/streams.h"

//===========================================================================
// TObject
//===========================================================================
TEST_CASE("TObject: ClassName / ClassNameIs")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	CHECK(lst->ClassNameIs(_T("TStringList")));
	CHECK_FALSE(lst->ClassNameIs(_T("TStrings")));
}

//===========================================================================
// TStrings/TStringList: 追加・削除・並べ替え
//===========================================================================
TEST_CASE("TStringList: Add/Count/Strings[]/Clear は 0 始まり")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	CHECK(lst->Count == 0);

	int i0 = lst->Add(_T("foo"));
	int i1 = lst->Add(_T("bar"));
	CHECK(i0 == 0);
	CHECK(i1 == 1);
	CHECK(lst->Count == 2);
	CHECK(lst->Strings[0] == UnicodeString(_T("foo")));
	CHECK(lst->Strings[1] == UnicodeString(_T("bar")));

	lst->Strings[0] = _T("baz");
	CHECK(lst->Strings[0] == UnicodeString(_T("baz")));

	lst->Clear();
	CHECK(lst->Count == 0);
}

TEST_CASE("TStringList: Objects[] はポインタをそのまま保持し、所有権は持たない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	int dummy = 0;
	TObject *marker = reinterpret_cast<TObject *>(&dummy);
	lst->AddObject(_T("x"), marker);
	CHECK(lst->Objects[0] == marker);
	lst->Clear();  // marker を Free してはいけない (解放していないことを暗黙に検証)
}

TEST_CASE("TStringList: Insert/Delete/Exchange/Move")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("a"));
	lst->Add(_T("c"));
	lst->Insert(1, _T("b"));
	CHECK(lst->Text == UnicodeString(_T("a\r\nb\r\nc\r\n")));

	lst->Delete(1);
	CHECK(lst->Text == UnicodeString(_T("a\r\nc\r\n")));

	lst->Add(_T("b"));
	lst->Exchange(1, 2);
	CHECK(lst->Strings[1] == UnicodeString(_T("b")));
	CHECK(lst->Strings[2] == UnicodeString(_T("c")));

	lst->Move(0, 2);
	CHECK(lst->Strings[0] == UnicodeString(_T("b")));
	CHECK(lst->Strings[2] == UnicodeString(_T("a")));
}

TEST_CASE("TStringList: IndexOf/IndexOfObject")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("foo"));
	lst->Add(_T("bar"));
	CHECK(lst->IndexOf(_T("bar")) == 1);
	CHECK(lst->IndexOf(_T("nope")) == -1);
	CHECK(lst->IndexOf(_T("BAR")) == 1);  // 既定 (TStrings) は大文字小文字を無視
}

//===========================================================================
// Text (行区切り)
//===========================================================================
TEST_CASE("TStrings: Text は各行の末尾にも改行を付け、SetText は \\r\\n と \\n の両方を認識する")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("1"));
	lst->Add(_T("2"));
	CHECK(lst->Text == UnicodeString(_T("1\r\n2\r\n")));

	lst->Text = _T("a\nb\r\nc");
	CHECK(lst->Count == 3);
	CHECK(lst->Strings[0] == UnicodeString(_T("a")));
	CHECK(lst->Strings[1] == UnicodeString(_T("b")));
	CHECK(lst->Strings[2] == UnicodeString(_T("c")));

	lst->Text += _T("tail\r\n");
	CHECK(lst->Count == 4);
	CHECK(lst->Strings[3] == UnicodeString(_T("tail")));
}

TEST_CASE("TStrings: Text が空なら Count は 0")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Text = _T("");
	CHECK(lst->Count == 0);
}

//===========================================================================
// Values / ValueFromIndex / Names
//===========================================================================
TEST_CASE("TStrings: Values[] の読み書きと空文字列代入による削除")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("foo=1"));
	lst->Add(_T("bar=2"));

	CHECK(lst->Values[_T("foo")] == UnicodeString(_T("1")));
	CHECK(lst->Values[_T("nope")].IsEmpty());
	CHECK(lst->Values[_T("foo")].ToIntDef(0) == 1);

	lst->Values[_T("foo")] = _T("99");
	CHECK(lst->Strings[0] == UnicodeString(_T("foo=99")));
	CHECK(lst->Count == 2);

	lst->Values[_T("baz")] = _T("3");  // 無ければ追加
	CHECK(lst->Count == 3);
	CHECK(lst->Values[_T("baz")] == UnicodeString(_T("3")));

	// 実測 RTL 挙動の確認結果: 空文字列を代入すると該当行を削除する
	lst->Values[_T("bar")] = _T("");
	CHECK(lst->Count == 2);
	CHECK(lst->IndexOfName(_T("bar")) == -1);
}

TEST_CASE("TStrings: ValueFromIndex[] の読み書きと Names[]")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("a=1"));
	lst->Add(_T("noeq"));

	CHECK(lst->Names[0] == UnicodeString(_T("a")));
	CHECK(lst->ValueFromIndex[0] == UnicodeString(_T("1")));

	// セパレータが無い行は Name も Value も空文字列になる (実測 RTL 挙動)
	CHECK(lst->Names[1].IsEmpty());
	CHECK(lst->ValueFromIndex[1].IsEmpty());

	lst->ValueFromIndex[0] = _T("42");
	CHECK(lst->Strings[0] == UnicodeString(_T("a=42")));

	lst->ValueFromIndex[0] = _T("");
	CHECK(lst->Count == 1);  // 0 番目が削除され、旧 1 番目が繰り上がる
	CHECK(lst->Strings[0] == UnicodeString(_T("noeq")));
}

//===========================================================================
// CommaText / DelimitedText
//===========================================================================
TEST_CASE("TStrings: CommaText の引用処理")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("a"));
	lst->Add(_T("b,c"));
	lst->Add(_T("d\"e"));

	const UnicodeString ct = lst->CommaText;
	CHECK(ct == UnicodeString(_T("a,\"b,c\",\"d\"\"e\"")));

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->CommaText = ct;
	CHECK(lst2->Count == 3);
	CHECK(lst2->Strings[0] == UnicodeString(_T("a")));
	CHECK(lst2->Strings[1] == UnicodeString(_T("b,c")));
	CHECK(lst2->Strings[2] == UnicodeString(_T("d\"e")));
}

TEST_CASE("TStrings: QuoteChar=0 なら引用処理をしない (実測: FileInfDlg.cpp の使い方)")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Delimiter = '|';
	lst->QuoteChar = 0;
	lst->DelimitedText = _T("a|b|c");
	CHECK(lst->Count == 3);
	CHECK(lst->Strings[1] == UnicodeString(_T("b")));
	CHECK(lst->DelimitedText == UnicodeString(_T("a|b|c")));
}

//===========================================================================
// Sorted / Duplicates / CaseSensitive / CustomSort
//===========================================================================
TEST_CASE("TStringList: Sorted=true の Add は挿入位置を自動決定する")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Sorted = true;
	lst->Add(_T("banana"));
	lst->Add(_T("apple"));
	lst->Add(_T("cherry"));
	CHECK(lst->Strings[0] == UnicodeString(_T("apple")));
	CHECK(lst->Strings[1] == UnicodeString(_T("banana")));
	CHECK(lst->Strings[2] == UnicodeString(_T("cherry")));
}

TEST_CASE("TStringList: Duplicates (dupIgnore/dupAccept/dupError)")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Sorted = true;
	lst->Duplicates = dupIgnore;
	lst->Add(_T("a"));
	lst->Add(_T("a"));
	CHECK(lst->Count == 1);

	lst->Duplicates = dupAccept;
	lst->Add(_T("a"));
	CHECK(lst->Count == 2);

	lst->Duplicates = dupError;
	CHECK_THROWS_AS(lst->Add(_T("a")), EStringListError);
}

TEST_CASE("TStringList: CaseSensitive が比較に影響する")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Sorted = true;
	lst->CaseSensitive = false;
	lst->Duplicates = dupIgnore;
	lst->Add(_T("Foo"));
	lst->Add(_T("foo"));
	CHECK(lst->Count == 1);  // 大文字小文字を無視すれば重複

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->CaseSensitive = true;
	lst2->Sorted = true;
	lst2->Duplicates = dupIgnore;
	lst2->Add(_T("Foo"));
	lst2->Add(_T("foo"));
	CHECK(lst2->Count == 2);  // 大文字小文字を区別すれば別物
}

TEST_CASE("TStringList: CustomSort に __fastcall 関数ポインタを渡せる")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add(_T("3"));
	lst->Add(_T("1"));
	lst->Add(_T("2"));

	lst->CustomSort([](TStringList *list, int i1, int i2) -> int {
		return list->Strings[i1].ToIntDef(0) - list->Strings[i2].ToIntDef(0);
	});

	CHECK(lst->Strings[0] == UnicodeString(_T("1")));
	CHECK(lst->Strings[1] == UnicodeString(_T("2")));
	CHECK(lst->Strings[2] == UnicodeString(_T("3")));
}

//===========================================================================
// LoadFromFile / SaveToFile / LoadFromStream / Encoding / WriteBOM
//===========================================================================
TEST_CASE("TStrings: SaveToFile(UTF8)/LoadFromFile の往復と Encoding/WriteBOM")
{
	const UnicodeString path = _T("nyanfi_test_classes_utf8.tmp");

	std::unique_ptr<TStringList> out_lst(new TStringList());
	out_lst->Add(_T("あいうえお"));
	out_lst->Add(_T("second line"));
	out_lst->SaveToFile(path, TEncoding::UTF8);

	std::unique_ptr<TStringList> in_lst(new TStringList());
	in_lst->LoadFromFile(path);
	CHECK(in_lst->Count == 2);
	CHECK(in_lst->Strings[0] == UnicodeString(_T("あいうえお")));
	REQUIRE(in_lst->Encoding != nullptr);
	CHECK(in_lst->Encoding->CodePage == static_cast<unsigned int>(CP_UTF8));

	::DeleteFileW(path.c_str());
}

TEST_CASE("TStrings: WriteBOM=false なら BOM を書かない")
{
	const UnicodeString path = _T("nyanfi_test_classes_nobom.tmp");

	std::unique_ptr<TStringList> out_lst(new TStringList());
	out_lst->Add(_T("abc"));
	out_lst->WriteBOM = false;
	out_lst->SaveToFile(path, TEncoding::UTF8);

	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->LoadFromFile(path);
	REQUIRE(ms->Size >= 3);
	const BYTE *bp = (const BYTE *)ms->Memory;
	CHECK_FALSE(bp[0] == 0xEF);  // BOM (EF BB BF) が無いこと

	::DeleteFileW(path.c_str());
}

TEST_CASE("TStrings: LoadFromStream で BOM 無し ANSI を判定する")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	AnsiString ansi(UnicodeString(_T("hello")));
	ms->Write(ansi.c_str(), ansi.Length());
	ms->Seek(0, soFromBeginning);

	std::unique_ptr<TStringList> lst(new TStringList());
	lst->LoadFromStream(ms.get());
	CHECK(lst->Text == UnicodeString(_T("hello\r\n")));
}

//===========================================================================
// StrictDelimiter (usr_str.cpp の get_csv_array/get_csv_item が使う)
//===========================================================================
TEST_CASE("TStrings: StrictDelimiter=false (既定) は埋め込みの空白も区切りとして読み飛ばす")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Delimiter = ',';
	lst->QuoteChar = '"';
	CHECK_FALSE(lst->StrictDelimiter);  // 既定値は false (Delphi RTL と同じ)

	lst->DelimitedText = _T("a, b, c");
	REQUIRE(lst->Count == 3);
	CHECK(lst->Strings[0] == UnicodeString(_T("a")));
	CHECK(lst->Strings[1] == UnicodeString(_T("b")));  // 前後の空白は読み飛ばされる
	CHECK(lst->Strings[2] == UnicodeString(_T("c")));
}

TEST_CASE("TStrings: StrictDelimiter=true は Delimiter だけで区切り、埋め込みの空白を保持する")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Delimiter = ',';
	lst->QuoteChar = '"';
	lst->StrictDelimiter = true;

	lst->DelimitedText = _T("a, b, c");
	REQUIRE(lst->Count == 3);
	CHECK(lst->Strings[0] == UnicodeString(_T("a")));
	CHECK(lst->Strings[1] == UnicodeString(_T(" b")));  // 前の空白がそのまま残る
	CHECK(lst->Strings[2] == UnicodeString(_T(" c")));
}

TEST_CASE("TStrings: StrictDelimiter=true は引用符付きフィールドも正しく解析する (実測: get_csv_array 相当)")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Delimiter = ',';
	lst->QuoteChar = '"';
	lst->StrictDelimiter = true;
	lst->DelimitedText = _T("foo,\"bar, baz\",qux");

	REQUIRE(lst->Count == 3);
	CHECK(lst->Strings[0] == UnicodeString(_T("foo")));
	CHECK(lst->Strings[1] == UnicodeString(_T("bar, baz")));
	CHECK(lst->Strings[2] == UnicodeString(_T("qux")));
}

TEST_CASE("TStrings: CommaText は StrictDelimiter の値に関わらず常に非 strict")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->StrictDelimiter = true;  // DelimitedText には効くが CommaText には影響しない
	lst->CommaText = _T("a, b, c");
	REQUIRE(lst->Count == 3);
	CHECK(lst->Strings[1] == UnicodeString(_T("b")));
}

//===========================================================================
// TList / TListNotification (usr_shell.h の TDropTargetList が継承する基底コンテナ)
//===========================================================================
namespace {
/// TDropTargetList と同じパターン: Notify(lnDeleted) で要素の所有権を解放する TList 派生
struct TestOwningList : public TList {
	int deletedCount = 0;
	int addedCount = 0;

protected:
	void Notify(void *ptr, TListNotification action) override
	{
		if (action == lnDeleted) {
			delete static_cast<int *>(ptr);
			++deletedCount;
		}
		else if (action == lnAdded) {
			++addedCount;
		}
	}
};
}  // namespace

TEST_CASE("TList: Add/Count/Items/IndexOf")
{
	std::unique_ptr<TList> lst(new TList());
	int a = 1, b = 2, c = 3;

	CHECK(lst->Add(&a) == 0);
	CHECK(lst->Add(&b) == 1);
	CHECK(lst->Add(&c) == 2);
	CHECK(lst->Count == 3);

	CHECK(lst->Items[0] == static_cast<void *>(&a));
	CHECK(lst->Items[1] == static_cast<void *>(&b));
	CHECK(lst->IndexOf(&c) == 2);
	CHECK(lst->IndexOf(&a) == 0);
	int other = 99;
	CHECK(lst->IndexOf(&other) == -1);

	lst->Items[1] = &c;  // Put 経由の書き戻し
	CHECK(lst->Items[1] == static_cast<void *>(&c));
}

TEST_CASE("TList: Insert/Delete/Remove")
{
	std::unique_ptr<TList> lst(new TList());
	int a = 1, b = 2, c = 3;
	lst->Add(&a);
	lst->Add(&c);
	lst->Insert(1, &b);
	CHECK(lst->Count == 3);
	CHECK(lst->Items[1] == static_cast<void *>(&b));

	lst->Delete(1);
	CHECK(lst->Count == 2);
	CHECK(lst->Items[1] == static_cast<void *>(&c));

	lst->Add(&b);
	CHECK(lst->Remove(&b) == 2);
	CHECK(lst->Count == 2);
	CHECK(lst->Remove(&b) == -1);  // 既に無いので見つからない
}

TEST_CASE("TList: Capacity")
{
	std::unique_ptr<TList> lst(new TList());
	lst->Capacity = 16;
	CHECK(lst->Capacity >= 16);
}

TEST_CASE("TList: Notify は Add で lnAdded、Delete/Remove/Clear で lnDeleted を呼ぶ (実測どおりの拡張パターン)")
{
	std::unique_ptr<TestOwningList> lst(new TestOwningList());
	lst->Add(new int(1));
	lst->Add(new int(2));
	lst->Add(new int(3));
	CHECK(lst->addedCount == 3);

	lst->Delete(0);
	CHECK(lst->deletedCount == 1);
	CHECK(lst->Count == 2);

	lst->Clear();
	CHECK(lst->deletedCount == 3);  // 残り 2 件 + 前段の 1 件
	CHECK(lst->Count == 0);
}

//===========================================================================
// TThread
//
// 実装のあるシム (宣言のみのスタブではない) なのでテストで固定する (規約9)。
// src の 6クラス (TCheckPathThread / TTaskThread / TGrepThread /
// TImgViewThread / TGetIconThread / TThumbnailThread) がこれを継承する。
//
// 実際に OS スレッドを起こすため、待ち合わせは「回数が増えるまで」ではなく
// 「フラグが立つまで上限つきで待つ」形にして、遅いマシンでも落ちないようにする。
//===========================================================================

namespace {

/// 中断状態で作り、Start() されたら Terminate() されるまで回るスレッド
class CountingThread : public TThread {
public:
	CountingThread() : TThread(true) {}

	volatile bool entered = false;
	volatile int loops = 0;

private:
	void __fastcall Execute() override
	{
		entered = true;
		while (!Terminated) {
			loops++;
			::Sleep(1);
		}
	}
};

/// 指定のフラグが立つまで最大 ms ミリ秒待つ。立ったら true
bool wait_until(const volatile bool &flag, int ms = 3000)
{
	for (int i = 0; i < ms; i++) {
		if (flag) return true;
		::Sleep(1);
	}
	return static_cast<bool>(flag);
}

}  // namespace

TEST_CASE("TThread: 中断状態で作ると Start() まで走らない")
{
	CountingThread th;
	::Sleep(30);
	CHECK_FALSE(th.entered);  // src の 6クラスはすべて CreateSuspended=true で作る

	th.Start();
	CHECK(wait_until(th.entered));

	th.Terminate();
	CHECK(th.Terminated);
}

TEST_CASE("TThread: Terminate() でループが止まる")
{
	CountingThread th;
	th.Start();
	REQUIRE(wait_until(th.entered));

	th.Terminate();
	::Sleep(50);
	const int a = th.loops;
	::Sleep(50);
	CHECK(th.loops == a);  // 止まっている
}

TEST_CASE("TThread: Start() は 2回目以降なにもしない")
{
	// Delphi は起動済みへの Start で例外を投げるが、シムでは何もしない
	// (この差はシムのコメントに明記してある)
	CountingThread th;
	th.Start();
	REQUIRE(wait_until(th.entered));
	th.Start();  // ここで落ちないこと
	th.Terminate();
}

TEST_CASE("TThread: Start しないまま破棄してもハングしない")
{
	// FreeOnTerminate の自己解放経路と二重解放にならないこと
	{
		CountingThread th;
	}
	CHECK(true);  // ここに到達すれば合格 (デストラクタでデッドロックしない)
}

TEST_CASE("TThread: Priority の読み書き")
{
	CountingThread th;
	th.Priority = tpLowest;   // src の check/imgv/icon/thumb が使う
	CHECK(th.Priority == tpLowest);
	th.Priority = tpNormal;   // grep/task が使う
	CHECK(th.Priority == tpNormal);
}

TEST_CASE("TThread: FreeOnTerminate の読み書き")
{
	CountingThread th;
	CHECK_FALSE(th.FreeOnTerminate);
	th.FreeOnTerminate = true;
	CHECK(th.FreeOnTerminate);
	th.FreeOnTerminate = false;  // ここでは自己解放させない
}

//===========================================================================
// TMultiReadExclusiveWriteSynchronizer
//===========================================================================

TEST_CASE("TMREWSync: 読みは多重に取れる")
{
	// SRWLOCK の共有ロック。同一スレッドでも複数回 BeginRead できる
	TMultiReadExclusiveWriteSynchronizer sync;
	sync.BeginRead();
	sync.BeginRead();
	sync.EndRead();
	sync.EndRead();
	CHECK(true);
}

TEST_CASE("TMREWSync: 書きの取得と解放")
{
	TMultiReadExclusiveWriteSynchronizer sync;
	CHECK(sync.BeginWrite());
	sync.EndWrite();
}

TEST_CASE("TMREWSync: 別スレッドからの書きは待たされる")
{
	// src のスレッド系 (task_thread.h ほか) はこれでプロパティを守っている
	struct WriterThread : public TThread {
		WriterThread(TMultiReadExclusiveWriteSynchronizer *s) : TThread(true), sync(s) {}
		TMultiReadExclusiveWriteSynchronizer *sync;
		volatile bool got = false;
		void __fastcall Execute() override
		{
			sync->BeginWrite();
			got = true;
			sync->EndWrite();
		}
	};

	TMultiReadExclusiveWriteSynchronizer sync;
	WriterThread th(&sync);

	sync.BeginWrite();     // メインスレッドが書きロックを持つ
	th.Start();
	::Sleep(50);
	CHECK_FALSE(th.got);   // ワーカーは取れずに待っている

	sync.EndWrite();
	CHECK(wait_until(th.got));
}

//===========================================================================
// ショートカットキーの変換 (TShortCut)
//
// 実測: src/Global.cpp:11121,11124 の 2行だけが使う。
//   TShortCut shortcut = TextToShortCut(kstr);
//   ShortCutToKey(shortcut, vkcode, ss);
// キー名の綴りは src/usr_key.cpp の make_KeyList が作るものと同じ。
//===========================================================================

TEST_CASE("TextToShortCut / ShortCutToKey: 修飾なしのファンクションキー")
{
	const TShortCut sc = TextToShortCut(_T("F5"));
	CHECK(sc != scNone);

	Word key = 0;
	TShiftState shift;
	ShortCutToKey(sc, key, shift);
	CHECK(key == VK_F5);
	CHECK_FALSE(shift.Contains(ssShift));
	CHECK_FALSE(shift.Contains(ssCtrl));
	CHECK_FALSE(shift.Contains(ssAlt));
}

TEST_CASE("TextToShortCut / ShortCutToKey: 修飾つき")
{
	Word key = 0;
	TShiftState shift;

	ShortCutToKey(TextToShortCut(_T("Ctrl+Shift+F5")), key, shift);
	CHECK(key == VK_F5);
	CHECK(shift.Contains(ssCtrl));
	CHECK(shift.Contains(ssShift));
	CHECK_FALSE(shift.Contains(ssAlt));

	key = 0; shift = TShiftState();
	ShortCutToKey(TextToShortCut(_T("Alt+A")), key, shift);
	CHECK(key == 'A');
	CHECK(shift.Contains(ssAlt));
	CHECK_FALSE(shift.Contains(ssCtrl));
}

TEST_CASE("TextToShortCut: 特殊キーの綴りは usr_key.cpp と揃っている")
{
	Word key = 0;
	TShiftState shift;

	ShortCutToKey(TextToShortCut(_T("Ctrl+Del")), key, shift);
	CHECK(key == VK_DELETE);
	CHECK(shift.Contains(ssCtrl));

	key = 0; shift = TShiftState();
	ShortCutToKey(TextToShortCut(_T("Enter")), key, shift);
	CHECK(key == VK_RETURN);
}

TEST_CASE("TextToShortCut: 表に無いキー名は scNone")
{
	// 10Key_* / App / 記号キーは Delphi の MenuKeyCaps に無いので 0 になる。
	// **本物の VCL と同じ挙動**なので直さない (規約6)。
	// つまり NyanFi の ini でこれらにホットキーを割り当てても VCL 版でも
	// 登録されない
	CHECK(TextToShortCut(_T("10Key_5")) == scNone);
	CHECK(TextToShortCut(_T("NoSuchKey")) == scNone);
	CHECK(TextToShortCut(UnicodeString()) == scNone);
}

TEST_CASE("ShortCutToText: TextToShortCut と往復する")
{
	CHECK(ShortCutToText(TextToShortCut(_T("F5"))) == UnicodeString(_T("F5")));
	CHECK(ShortCutToText(TextToShortCut(_T("Ctrl+F5"))) == UnicodeString(_T("Ctrl+F5")));
	CHECK(ShortCutToText(scNone).IsEmpty());
}
