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
#include "usr_key.h"  // get_KeyStr (VkFromWxKeyCode の結果をキー名まで通して確認する)

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
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("ReloadList")));
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
// KeyMap::VkFromWxKeyCode: wx のキーコード → 仮想キーコード
//
// この変換にテストが無く、KeyStrOf() が wx の GetKeyCode() をそのまま
// get_KeyStr() に渡していたため、英数字キーだけ効いて上下キー・PgUp/PgDn・
// F キーが全部無反応になっていた (報告書 §16.5)。
//
// wxKeyCode の実際の値との一致は gui/key_map_wx.cpp の static_assert が
// コンパイル時に確認する。ここでは「VK に落ちてキー名まで通ること」を見る。
//===========================================================================

TEST_CASE("VkFromWxKeyCode: 矢印キーが仮想キーコードになる")
{
	// WXK_LEFT=314 / WXK_UP=315 / WXK_RIGHT=316 / WXK_DOWN=317
	CHECK(KeyMap::VkFromWxKeyCode(314) == VK_LEFT);
	CHECK(KeyMap::VkFromWxKeyCode(315) == VK_UP);
	CHECK(KeyMap::VkFromWxKeyCode(316) == VK_RIGHT);
	CHECK(KeyMap::VkFromWxKeyCode(317) == VK_DOWN);

	// wx の値をそのまま get_KeyStr() に渡していた頃の壊れ方を記録しておく。
	// 315 は VK として意味を持たないが、get_KeyStr() の default 節の
	// `if (_istalnum(Key)) keystr = (char)Key;` に落ちるため **空にならない**。
	// (char)315 は下位1バイトの 59 = ';' になり、上下キーが ';' '=' という
	// 別のキー名として引かれていた (割り当てが無いので無反応。もし ';' に
	// 何か割り当てていたら誤動作していた)
	CHECK(get_KeyStr(315) == UnicodeString(_T(";")));
	CHECK(get_KeyStr(317) == UnicodeString(_T("=")));
}

TEST_CASE("VkFromWxKeyCode: ページ移動・行頭行末・挿入削除")
{
	CHECK(KeyMap::VkFromWxKeyCode(366) == VK_PRIOR);   // WXK_PAGEUP
	CHECK(KeyMap::VkFromWxKeyCode(367) == VK_NEXT);    // WXK_PAGEDOWN
	CHECK(KeyMap::VkFromWxKeyCode(313) == VK_HOME);    // WXK_HOME
	CHECK(KeyMap::VkFromWxKeyCode(312) == VK_END);     // WXK_END
	CHECK(KeyMap::VkFromWxKeyCode(322) == VK_INSERT);  // WXK_INSERT
	CHECK(KeyMap::VkFromWxKeyCode(127) == VK_DELETE);  // WXK_DELETE
}

TEST_CASE("VkFromWxKeyCode: F1〜F12 が連番で対応する")
{
	for (int i = 0; i < 12; i++) {
		CHECK(KeyMap::VkFromWxKeyCode(340 + i) == VK_F1 + i);  // WXK_F1 = 340
	}
}

TEST_CASE("VkFromWxKeyCode: 10キーの数字と演算子")
{
	for (int i = 0; i < 10; i++) {
		CHECK(KeyMap::VkFromWxKeyCode(324 + i) == VK_NUMPAD0 + i);  // WXK_NUMPAD0 = 324
	}
	CHECK(KeyMap::VkFromWxKeyCode(334) == VK_MULTIPLY);
	CHECK(KeyMap::VkFromWxKeyCode(335) == VK_ADD);
	CHECK(KeyMap::VkFromWxKeyCode(337) == VK_SUBTRACT);
	CHECK(KeyMap::VkFromWxKeyCode(338) == VK_DECIMAL);
	CHECK(KeyMap::VkFromWxKeyCode(339) == VK_DIVIDE);
}

TEST_CASE("VkFromWxKeyCode: VK と同値の範囲はそのまま通す")
{
	// 英数字は wx も VK も同値。ここが一致していたので G や R だけ効いていた
	CHECK(KeyMap::VkFromWxKeyCode('G') == 'G');
	CHECK(KeyMap::VkFromWxKeyCode('0') == '0');

	// 制御キーも偶然一致している組
	CHECK(KeyMap::VkFromWxKeyCode(8)  == VK_BACK);
	CHECK(KeyMap::VkFromWxKeyCode(9)  == VK_TAB);
	CHECK(KeyMap::VkFromWxKeyCode(13) == VK_RETURN);
	CHECK(KeyMap::VkFromWxKeyCode(27) == VK_ESCAPE);
	CHECK(KeyMap::VkFromWxKeyCode(32) == VK_SPACE);
}

TEST_CASE("VkFromWxKeyCode: キー名を持たないものは 0")
{
	CHECK(KeyMap::VkFromWxKeyCode(306) == 0);  // WXK_SHIFT
	CHECK(KeyMap::VkFromWxKeyCode(308) == 0);  // WXK_CONTROL
	CHECK(KeyMap::VkFromWxKeyCode(0)   == 0);  // WXK_NONE
}

TEST_CASE("VkFromWxKeyCode: 変換した値が get_KeyStr でキー名になる")
{
	// 実際の経路 (wx のキーコード → VK → キー名 → コマンド名) を通す
	KeyMap km;
	CHECK(get_KeyStr(KeyMap::VkFromWxKeyCode(315)) == UnicodeString(_T("UP")));
	CHECK(get_KeyStr(KeyMap::VkFromWxKeyCode(317)) == UnicodeString(_T("DOWN")));
	CHECK(get_KeyStr(KeyMap::VkFromWxKeyCode(366)) == UnicodeString(_T("PGUP")));
	CHECK(get_KeyStr(KeyMap::VkFromWxKeyCode(344)) == UnicodeString(_T("F5")));

	// 上下キーは get_CsrKeyCmd 経由でコマンドに解決される
	CHECK_FALSE(km.Lookup(get_KeyStr(KeyMap::VkFromWxKeyCode(315))).IsEmpty());
	CHECK_FALSE(km.Lookup(get_KeyStr(KeyMap::VkFromWxKeyCode(317))).IsEmpty());
	CHECK(km.Lookup(get_KeyStr(KeyMap::VkFromWxKeyCode(366))) == UnicodeString(_T("PageUp")));
	CHECK(km.Lookup(get_KeyStr(KeyMap::VkFromWxKeyCode(344))) == UnicodeString(_T("ReloadList")));
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
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("ReloadList")));
}

TEST_CASE("LoadFromIni: ini が存在しない場合は既定のまま (無視する)")
{
	TempDir dir;
	KeyMap km;
	km.LoadFromIni(dir.file(_T("does_not_exist.ini")));
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("ReloadList")));
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
	CHECK(km.Lookup(_T("F5")) == UnicodeString(_T("ReloadList")));
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
