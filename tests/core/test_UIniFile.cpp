/**
 * @file tests/core/test_UIniFile.cpp
 * @brief src/UIniFile.cpp (INI ファイル処理クラス) の回帰テスト
 *
 * 目的: 現在の実装の挙動をそのまま固定すること (regression test)。
 * ファイルシステムに触れるテストは tests/temp_dir.h の TempDir が作る
 * 一時ディレクトリの中だけで行う。
 *
 * 除外した経路: ドライブ/ネットワークパスの実在チェックが絡む
 * CheckMarkItems の「削除」分岐 (テストで作るマーク対象パスは実在しない
 * ため、UsrIniFile 構築直後の CheckMarkItems では削除条件に触れない。
 * FileMark 自体は UpdateMarkIdxList のみを呼ぶので実ファイル依存が無い)。
 */
#include "doctest/doctest.h"

#include <memory>

#include "UIniFile.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

/**
 * @brief Application->MainForm を一時的に差し替える RAII ガード
 * @details usr_scale.h の UnscaledInt(n)/ScaledInt(n) (cp 省略形) は
 *          `cp==NULL` のとき `Application->MainForm->CurrentPPI` を直接
 *          参照する (Screen->ActiveForm も NULL の場合)。ヘッドレスな
 *          テストでは MainForm が無い (nullptr) ため、この経路を通ると
 *          NULL 参照でクラッシュする。UsrIniFile::SaveGridColWidth 等が
 *          この cp 省略形を使うため、該当テストでは一時的に MainForm を
 *          用意する。
 */
struct MainFormGuard {
	TForm *saved;
	TForm dummy;
	explicit MainFormGuard(int ppi = 96) : saved(Application->MainForm)
	{
		dummy.CurrentPPI = ppi;
		Application->MainForm = &dummy;
	}
	~MainFormGuard() { Application->MainForm = saved; }
};

/// 指定内容の INI ファイルを作って UsrIniFile を構築する
std::unique_ptr<UsrIniFile> make_ini(const UnicodeString &fnam, const UnicodeString &content)
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Text = content;
	buf->SaveToFile(fnam);
	return std::unique_ptr<UsrIniFile>(new UsrIniFile(fnam));
}

}  // namespace

//===========================================================================
// ReadString
//===========================================================================
TEST_CASE("UsrIniFile: ReadString は既存キーの値を返し、無ければ既定値")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T("[Sect]\r\nKey=Value\r\n"));

	CHECK(ini->ReadString("Sect", "Key") == UnicodeString("Value"));
	CHECK(ini->ReadString("Sect", "NoSuchKey", "Def") == UnicodeString("Def"));
	CHECK(ini->ReadString("NoSuchSect", "Key", "Def") == UnicodeString("Def"));
}

TEST_CASE("UsrIniFile: ReadString は既定で引用符を外す")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T("[Sect]\r\nKey=\"Quoted\"\r\n"));

	CHECK(ini->ReadString("Sect", "Key") == UnicodeString("Quoted"));
	CHECK(ini->ReadString("Sect", "Key", EmptyStr, false) == UnicodeString("\"Quoted\""));
}

//===========================================================================
// ReadInteger / ReadInt64 / ReadBool / ReadColor
//===========================================================================
TEST_CASE("UsrIniFile: ReadInteger/ReadInt64/ReadBool/ReadColor")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"),
		_T("[Sect]\r\nNum=42\r\nBig=1234567890123\r\nFlagOn=1\r\nFlagOff=0\r\nCol=16711680\r\n"));

	CHECK(ini->ReadInteger("Sect", "Num") == 42);
	CHECK(ini->ReadInteger("Sect", "NoKey", 7) == 7);
	//既存の挙動: UsrIniFile::ReadInt64 の戻り値型は (__int64 ではなく) int なので、
	//64bit を要する値は下位32bitへ切り詰められる (src/UIniFile.h の既存の宣言どおり。
	//バグの可能性が高いが、修正せず現状の挙動を固定する)
	CHECK(ini->ReadInt64("Sect", "Big") == 1912276171);
	CHECK(ini->ReadBool("Sect", "FlagOn") == true);
	CHECK(ini->ReadBool("Sect", "FlagOff") == false);
	CHECK(ini->ReadBool("Sect", "NoKey", true) == true);
	CHECK(ini->ReadColor("Sect", "Col", clBlack) == (TColor)16711680);
	CHECK(ini->ReadColor("Sect", "NoKey", clBlack) == clBlack);
}

//===========================================================================
// WriteXxx / Modified / UpdateFile / Reload (永続化の往復)
//===========================================================================
TEST_CASE("UsrIniFile: WriteString は新規キーを追加し Modified を立てる")
{
	TempDir dir;
	UnicodeString fnam = dir.file("t.ini");
	auto ini = make_ini(fnam, _T("[Sect]\r\nKey=Old\r\n"));

	CHECK(ini->Modified == false);
	ini->WriteString("Sect", "Key", "New");
	CHECK(ini->Modified == true);
	CHECK(ini->ReadString("Sect", "Key") == UnicodeString("New"));

	//同じ値を再度書いても Modified は変化しない (既に true だが、
	//別キーで確認: 一度 UpdateFile して false に戻してから調べる)
	ini->UpdateFile();
	CHECK(ini->Modified == false);
	ini->WriteString("Sect", "Key", "New");	//同値
	CHECK(ini->Modified == false);
}

TEST_CASE("UsrIniFile: UpdateFile で保存し、別インスタンスで読み直せる")
{
	TempDir dir;
	UnicodeString fnam = dir.file("t.ini");
	{
		auto ini = make_ini(fnam, _T("[Sect]\r\n"));
		ini->WriteString("Sect", "Key", "Persisted");
		ini->WriteInteger("Sect", "Num", 123);
		CHECK(ini->UpdateFile() == true);
	}
	{
		//別インスタンスとして読み直す
		std::unique_ptr<UsrIniFile> ini2(new UsrIniFile(fnam));
		CHECK(ini2->ReadString("Sect", "Key") == UnicodeString("Persisted"));
		CHECK(ini2->ReadInteger("Sect", "Num") == 123);
	}
}

TEST_CASE("UsrIniFile: Reload で外部からの変更を再読込する")
{
	TempDir dir;
	UnicodeString fnam = dir.file("t.ini");
	auto ini = make_ini(fnam, _T("[Sect]\r\nKey=A\r\n"));
	CHECK(ini->ReadString("Sect", "Key") == UnicodeString("A"));

	//ファイルを直接書き換える (別の UsrIniFile を経由)
	{
		std::unique_ptr<UsrIniFile> writer(new UsrIniFile(fnam));
		writer->WriteString("Sect", "Key", "B");
		writer->UpdateFile();
	}

	ini->Reload();
	CHECK(ini->ReadString("Sect", "Key") == UnicodeString("B"));
}

//===========================================================================
// セクション/キーの CRUD
//===========================================================================
TEST_CASE("UsrIniFile: SectionExists/KeyExists/DeleteKey/EraseSection")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T("[Sect]\r\nA=1\r\nB=2\r\n"));

	CHECK(ini->SectionExists("Sect") == true);
	CHECK(ini->SectionExists("NoSect") == false);
	CHECK(ini->KeyExists("Sect", "A") == true);
	CHECK(ini->KeyExists("Sect", "C") == false);

	CHECK(ini->DeleteKey("Sect", "A") == true);
	CHECK(ini->KeyExists("Sect", "A") == false);
	CHECK(ini->DeleteKey("Sect", "A") == false);	//既に無い

	ini->EraseSection("Sect");
	CHECK(ini->SectionExists("Sect") == false);
}

TEST_CASE("UsrIniFile: RenameKey/ReplaceKey")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T("[Sect]\r\nOldName=Val\r\nFooBar=1\r\nFooBaz=2\r\n"));

	ini->RenameKey("Sect", "OldName", "NewName");
	CHECK(ini->KeyExists("Sect", "OldName") == false);
	CHECK(ini->ReadString("Sect", "NewName") == UnicodeString("Val"));

	//"Foo" を含むキー名を "Bar" に置換 (FooBar -> BarBar / FooBaz -> BarBaz)
	ini->ReplaceKey("Sect", "Foo", "Bar");
	CHECK(ini->ReadString("Sect", "BarBar") == UnicodeString("1"));
	CHECK(ini->ReadString("Sect", "BarBaz") == UnicodeString("2"));
}

//===========================================================================
// ReadSection / AssignSection
//===========================================================================
TEST_CASE("UsrIniFile: ReadSection/AssignSection")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T("[Sect]\r\nA=1\r\nB=2\r\n"));

	std::unique_ptr<TStringList> lst(new TStringList());
	ini->ReadSection("Sect", lst.get());
	CHECK(lst->Count == 2);
	CHECK(lst->Values["A"] == UnicodeString("1"));

	lst->Clear();
	lst->Add("X=9");
	ini->AssignSection("NewSect", lst.get());
	CHECK(ini->ReadString("NewSect", "X") == UnicodeString("9"));
}

//===========================================================================
// LoadListItems / SaveListItems
//===========================================================================
TEST_CASE("UsrIniFile: LoadListItems/SaveListItems の往復")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	std::unique_ptr<TStringList> src(new TStringList());
	src->Add("One");
	src->Add("Two");
	src->Add("Three");
	ini->SaveListItems("Items", src.get());

	std::unique_ptr<TStringList> dst(new TStringList());
	ini->LoadListItems("Items", dst.get());
	REQUIRE(dst->Count == 3);
	CHECK(dst->Strings[0] == UnicodeString("One"));
	CHECK(dst->Strings[1] == UnicodeString("Two"));
	CHECK(dst->Strings[2] == UnicodeString("Three"));
}

TEST_CASE("UsrIniFile: LoadListItems は max_items で打ち切る")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	std::unique_ptr<TStringList> src(new TStringList());
	for (int i = 0; i < 5; i++) src->Add(UnicodeString(i));
	ini->SaveListItems("Items", src.get(), 0);	//無制限保存

	std::unique_ptr<TStringList> dst(new TStringList());
	ini->LoadListItems("Items", dst.get(), 3);	//3件で打ち切り
	CHECK(dst->Count == 3);
}

//===========================================================================
// LoadComboBoxItems / SaveComboBoxItems
//===========================================================================
TEST_CASE("UsrIniFile: LoadComboBoxItems/SaveComboBoxItems の往復")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	//TComboBox は Items (TStrings) をコンストラクタで自前に生成して所有するので、
	//既存のリストへ差し替えず、そのまま書き込む
	TComboBox cb;
	cb.Items->Add("cmd1");
	cb.Items->Add("cmd2");
	ini->SaveComboBoxItems(&cb, _T("History"));

	TComboBox cb2;
	ini->LoadComboBoxItems(&cb2, _T("History"));

	REQUIRE(cb2.Items->Count == 2);
	CHECK(cb2.Items->Strings[0] == UnicodeString("cmd1"));
	CHECK(cb2.Items->Strings[1] == UnicodeString("cmd2"));
}

//===========================================================================
// ReadFontInf / WriteFontInf
//===========================================================================
TEST_CASE("UsrIniFile: WriteFontInf/ReadFontInf の往復")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	TFont src;
	src.Name = "Consolas";
	src.Size = 14;
	src.Style << fsBold;

	ini->WriteFontInf("Font", &src);

	std::unique_ptr<TFont> loaded(ini->ReadFontInf(_T("Font")));
	CHECK(loaded->Name == UnicodeString("Consolas"));
	CHECK(loaded->Size == 14);
	CHECK(loaded->Style.Contains(fsBold) == true);
	CHECK(loaded->Style.Contains(fsItalic) == false);
}

//===========================================================================
// LoadGridColWidth / SaveGridColWidth
//===========================================================================
TEST_CASE("UsrIniFile: SaveGridColWidth/LoadGridColWidth の往復")
{
	TempDir dir;
	MainFormGuard mf;	//SaveGridColWidth 内の UnscaledInt(n) (cp 省略形) 用
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	TStringGrid grid;
	grid.Name = "Grid1";
	grid.FixedCols = 1;
	grid.ColCount = 3;
	grid.ColWidths[0] = 20;
	grid.ColWidths[1] = 100;
	grid.ColWidths[2] = 200;
	ini->SaveGridColWidth(&grid);

	TStringGrid grid2;
	grid2.Name = "Grid1";
	grid2.FixedCols = 1;
	grid2.ColCount = 3;
	ini->LoadGridColWidth(&grid2, 3, 20, 80, 150);

	CHECK((int)grid2.ColWidths[0] == 20);	//FixedCols 分は常に引数の値
	CHECK((int)grid2.ColWidths[1] == 100);
	CHECK((int)grid2.ColWidths[2] == 200);
}

//===========================================================================
// FileMark / IsMarked / GetMarkMemo / ClearAllMark
//===========================================================================
TEST_CASE("UsrIniFile: FileMark/IsMarked/GetMarkMemo (通常ファイル)")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	UnicodeString target = dir.path + "no_such_file.txt";	//実在しなくてよい (FileMark は存在チェックしない)
	CHECK(ini->IsMarked(target) == false);

	CHECK(ini->FileMark(target, 1, "memo1") == true);
	CHECK(ini->IsMarked(target) == true);
	CHECK(ini->GetMarkMemo(target) == UnicodeString("memo1"));

	//flag=-1 (既定) はトグルなので、マーク済みなら解除される
	CHECK(ini->FileMark(target) == false);
	CHECK(ini->IsMarked(target) == false);

	ini->FileMark(target, 1);
	CHECK(ini->IsMarked(target) == true);
	ini->ClearAllMark();
	CHECK(ini->IsMarked(target) == false);
}

//===========================================================================
// ReadScaledInteger / WriteScaledInteger (DPI スケーリング)
//===========================================================================
TEST_CASE("UsrIniFile: WriteScaledInteger/ReadScaledInteger は等倍PPIで値を保つ")
{
	TempDir dir;
	auto ini = make_ini(dir.file("t.ini"), _T(""));

	TForm frm;
	frm.CurrentPPI = 96;	//DEFAULT_PPI と同じ (等倍)

	ini->WriteScaledInteger("Sect", "Size", 200, &frm);
	CHECK(ini->ReadScaledInteger("Sect", "Size", 0, &frm) == 200);
}
