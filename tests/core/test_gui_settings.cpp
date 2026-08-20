/**
 * @file tests/core/test_gui_settings.cpp
 * @brief gui/settings.cpp (ini 永続化) と gui/key_map.cpp (ini のキー割り当て
 *        解析) の回帰テスト
 *
 * @details
 * gui/ 配下のうち wx に依存しない部分だけをここでテストする
 * (`nyanfi_gui_core`、ルート CMakeLists.txt 参照)。GUI そのもの (wxWidgets)
 * はテストできないが、ini の読み書きとキー割り当ての解析は素の関数として
 * 切り出してあるので、ここで直接呼べる。
 *
 * ファイルシステムに触れるテストは tests/temp_dir.h の TempDir が作る
 * 一時ディレクトリの中だけで行う。
 */
#include "doctest/doctest.h"

#include "gui/key_map.h"
#include "gui/settings.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

//===========================================================================
// Settings: ini への保存・復元
//===========================================================================

TEST_CASE("Settings: 既定値はどれもディレクトリ未設定・既定サイズ")
{
	TempDir dir;
	Settings s(dir.file(_T("nyanfi_wx.ini")));

	CHECK(s.Window.left == -1);
	CHECK(s.Window.top == -1);
	CHECK(s.Window.width == 1000);
	CHECK(s.Window.height == 640);
	CHECK_FALSE(s.Window.maximized);
	CHECK(s.LeftDir.IsEmpty());
	CHECK(s.RightDir.IsEmpty());
}

TEST_CASE("Settings: Save して別インスタンスで読み直すと同じ値になる")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi_wx.ini"));

	{
		Settings s(ini_path);
		s.Window.left = 123;
		s.Window.top = 45;
		s.Window.width = 800;
		s.Window.height = 600;
		s.Window.maximized = true;
		s.LeftDir = _T("C:\\Left\\");
		s.RightDir = _T("C:\\Right\\");
		CHECK(s.Save());
	}

	Settings reloaded(ini_path);
	CHECK(reloaded.Window.left == 123);
	CHECK(reloaded.Window.top == 45);
	CHECK(reloaded.Window.width == 800);
	CHECK(reloaded.Window.height == 600);
	CHECK(reloaded.Window.maximized);
	CHECK(reloaded.LeftDir == UnicodeString(_T("C:\\Left\\")));
	CHECK(reloaded.RightDir == UnicodeString(_T("C:\\Right\\")));
}

TEST_CASE("Settings: 存在しない ini を開いても例外を投げず既定値のまま")
{
	TempDir dir;
	// ファイルを作らずに、存在しないパスをそのまま渡す
	Settings s(dir.file(_T("does_not_exist.ini")));
	CHECK(s.Window.width == 1000);
	CHECK(s.LeftDir.IsEmpty());
}

TEST_CASE("Settings: Save はこのクラスのセクション以外を書き換えない")
{
	// UsrIniFile::UpdateFile はコンストラクタ時に読み込んだ全セクションを
	// まるごと書き戻す実装 (src/UIniFile.cpp) なので、他セクションの値が
	// Save 後も保たれることを確認する (既存 ini を壊さないことの回帰テスト)
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi_wx.ini"));

	{
		std::unique_ptr<TStringList> buf(new TStringList());
		buf->Text =
			_T("[General]\r\n")
			_T("SomeOtherKey=SomeOtherValue\r\n");
		buf->SaveToFile(ini_path);
	}

	{
		Settings s(ini_path);
		s.Window.left = 1;
		s.Window.top = 2;
		CHECK(s.Save());
	}

	std::unique_ptr<UsrIniFile> check(new UsrIniFile(ini_path));
	CHECK(check->ReadString(_T("General"), _T("SomeOtherKey")) == UnicodeString(_T("SomeOtherValue")));
}

TEST_CASE("Settings: DefaultIniPath は実行ファイルと同じ場所の別ファイル名になる")
{
	// VCL 版と同じ ini (<exe名>.ini) を上書きしないよう、専用の別ファイルに
	// する設計 (gui/settings.h 参照)。実行ファイル名そのものとは異なることだけ確認する
	const UnicodeString path = Settings::DefaultIniPath();
	CHECK_FALSE(path.IsEmpty());
	CHECK(EndsText(_T("_wx.ini"), path));
	CHECK_FALSE(EndsText(_T(".exe_wx.ini"), path));  // ChangeFileExt で拡張子を置き換えている
}

//===========================================================================
// KeyMap: 既定の割り当てと Assign/Lookup
//===========================================================================

TEST_CASE("KeyMap: 既定の割り当てが引ける")
{
	KeyMap km;
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("Refresh")));
	CHECK(km.Lookup(_T("Ctrl+Q")) == UnicodeString(_T("Exit")));
	CHECK(km.Lookup(_T("NoSuchKey")).IsEmpty());
	CHECK(km.Lookup(EmptyStr).IsEmpty());
}

TEST_CASE("KeyMap: Assign は同じキーなら上書きする")
{
	KeyMap km;
	km.Assign(_T("F5"), _T("CustomRefresh"));
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("CustomRefresh")));

	km.Assign(_T("Ctrl+Z"), _T("Undo"));
	CHECK(km.Lookup(_T("Ctrl+Z")) == UnicodeString(_T("Undo")));
}

//===========================================================================
// KeyMap::ParseKeyFuncListEntry: ini の KeyFuncList 1行の解析
//===========================================================================

TEST_CASE("ParseKeyFuncListEntry: F: プレフィックスのエントリは受理する")
{
	UnicodeString key_str, command;
	CHECK(KeyMap::ParseKeyFuncListEntry(_T("F:Ctrl+Q"), _T("Exit"), key_str, command));
	CHECK(key_str == UnicodeString(_T("Ctrl+Q")));
	CHECK(command == UnicodeString(_T("Exit")));
}

TEST_CASE("ParseKeyFuncListEntry: F 以外のモードは読み飛ばす")
{
	UnicodeString key_str, command;
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("S:F5"), _T("SomeCmd"), key_str, command));
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("V:F5"), _T("SomeCmd"), key_str, command));
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("I:F5"), _T("SomeCmd"), key_str, command));
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("L:F5"), _T("SomeCmd"), key_str, command));
}

TEST_CASE("ParseKeyFuncListEntry: SELECT+ 付きは非対応として読み飛ばす")
{
	UnicodeString key_str, command;
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("F:SELECT+DOWN"), _T("ExtendSelDown"), key_str, command));
}

TEST_CASE("ParseKeyFuncListEntry: 2ストローク ('~' を含む) は非対応として読み飛ばす")
{
	UnicodeString key_str, command;
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("F:Ctrl+K~D"), _T("TwoStroke"), key_str, command));
}

TEST_CASE("ParseKeyFuncListEntry: プレフィックス以外が空/値が空なら読み飛ばす")
{
	UnicodeString key_str, command;
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("F:"), _T("Exit"), key_str, command));
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("F:Ctrl+Q"), EmptyStr, key_str, command));
	CHECK_FALSE(KeyMap::ParseKeyFuncListEntry(_T("Ctrl+Q"), _T("Exit"), key_str, command));  // "F:" 無し
}

//===========================================================================
// KeyMap::LoadFromIni: 実際の ini ファイルからの読み込み
//===========================================================================

TEST_CASE("LoadFromIni: KeyFuncList の F: エントリで既定の割り当てを上書きする")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi.ini"));

	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Text =
		_T("[KeyFuncList]\r\n")
		_T("F:Ctrl+Q=CustomExit\r\n")     // 既定 (Exit) を上書き
		_T("F:F9=NewCommand\r\n")         // 既定に無い新規キー
		_T("F:SELECT+DOWN=ExtendSelDown\r\n")  // 非対応。読み飛ばされる
		_T("F:Ctrl+K~D=TwoStroke\r\n")         // 非対応。読み飛ばされる
		_T("S:F5=OtherModeIgnored\r\n");       // モード違い。読み飛ばされる
	buf->SaveToFile(ini_path);

	KeyMap km;
	km.LoadFromIni(ini_path);

	CHECK(km.Lookup(_T("Ctrl+Q")) == UnicodeString(_T("CustomExit")));
	CHECK(km.Lookup(_T("F9")) == UnicodeString(_T("NewCommand")));
	CHECK(km.Lookup(_T("SELECT+DOWN")).IsEmpty());
	CHECK(km.Lookup(_T("Ctrl+K~D")).IsEmpty());
	// 既定のまま残っている割り当てにも影響が無いこと
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("Refresh")));
}

TEST_CASE("LoadFromIni: ini が存在しない場合は既定のまま (無視する)")
{
	TempDir dir;
	KeyMap km;
	km.LoadFromIni(dir.file(_T("does_not_exist.ini")));
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("Refresh")));
	CHECK(km.Lookup(_T("Ctrl+Q")) == UnicodeString(_T("Exit")));
}

TEST_CASE("LoadFromIni: KeyFuncList セクションが無い場合は既定のまま (無視する)")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi.ini"));

	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Text = _T("[General]\r\nFoo=Bar\r\n");
	buf->SaveToFile(ini_path);

	KeyMap km;
	km.LoadFromIni(ini_path);
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("Refresh")));
}

TEST_CASE("LoadFromIni: 呼び出し元の ini を書き換えない (読み込み専用)")
{
	// KeyMap::LoadFromIni は UsrIniFile::UpdateFile を一切呼ばないので、
	// VCL 版の既存 ini をキー割り当ての読み込みだけに使っても壊れないことの回帰テスト
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi.ini"));

	const UnicodeString original =
		_T("[KeyFuncList]\r\n")
		_T("F:Ctrl+Q=CustomExit\r\n");
	{
		std::unique_ptr<TStringList> buf(new TStringList());
		buf->Text = original;
		buf->SaveToFile(ini_path);
	}

	KeyMap km;
	km.LoadFromIni(ini_path);

	std::unique_ptr<TStringList> after(new TStringList());
	after->LoadFromFile(ini_path);
	CHECK(after->Text == original);
}
