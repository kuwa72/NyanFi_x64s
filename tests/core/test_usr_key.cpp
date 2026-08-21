/**
 * @file tests/core/test_usr_key.cpp
 * @brief src/usr_key.cpp (キー名⇄キーコード変換) の回帰テスト
 *
 * 注意: is_JpKeybd() は KeyboardMode==0 の場合 ::GetKeyboardType(0) という
 * 実際の Windows API 呼び出しに依存し、実行環境に依存して結果が変わりうる。
 * そのためテストでは KeyboardMode を明示的に 1(JP)/2(US) に固定してから
 * 呼び出し、環境非依存にしている。
 */
#include "doctest/doctest.h"

#include <memory>

#include "usr_key.h"

//===========================================================================
// get_KeyStr: キーコード→文字列
//===========================================================================
TEST_CASE("get_KeyStr: 特殊キーの文字列化")
{
	CHECK(get_KeyStr(VK_RETURN) == UnicodeString("ENTER"));
	CHECK(get_KeyStr(VK_ESCAPE) == UnicodeString("ESC"));
	CHECK(get_KeyStr(VK_TAB) == UnicodeString("TAB"));
	CHECK(get_KeyStr(VK_LEFT) == UnicodeString("LEFT"));
	CHECK(get_KeyStr(VK_F5) == UnicodeString("F5"));
	CHECK(get_KeyStr(VK_NUMPAD5) == UnicodeString("10Key_5"));
}

TEST_CASE("get_KeyStr: 英数字キーはそのまま文字化、非対応キーは空")
{
	CHECK(get_KeyStr((WORD)'A') == UnicodeString("A"));
	CHECK(get_KeyStr((WORD)'5') == UnicodeString("5"));
	CHECK(get_KeyStr((WORD)0) == UnicodeString(""));
}

TEST_CASE("get_KeyStr: JP/USキーボードによる記号キーの違い")
{
	int save_mode = KeyboardMode;

	KeyboardMode = 1;  //JP固定
	CHECK(get_KeyStr(VK_OEM_1) == UnicodeString(":"));
	CHECK(get_KeyStr(VK_OEM_3) == UnicodeString("@"));
	CHECK(get_KeyStr(VK_OEM_7) == UnicodeString("^"));

	KeyboardMode = 2;  //US固定
	CHECK(get_KeyStr(VK_OEM_1) == UnicodeString(";"));
	CHECK(get_KeyStr(VK_OEM_3) == UnicodeString("`"));
	CHECK(get_KeyStr(VK_OEM_7) == UnicodeString("'"));

	KeyboardMode = save_mode;  //グローバル状態を復元
}

TEST_CASE("get_KeyStr(Key, Shift): シフト文字列を前置")
{
	TShiftState shift;
	shift << ssCtrl << ssShift;
	UnicodeString r = get_KeyStr(VK_RETURN, shift);
	//実装順は Shift+ -> Ctrl+ -> Alt+ の固定順
	CHECK(r == UnicodeString("Shift+Ctrl+ENTER"));

	//キーが空文字列を返す場合は全体が空になる
	TShiftState shift2;
	shift2 << ssShift;
	CHECK(get_KeyStr((WORD)0, shift2) == UnicodeString(""));
}

//===========================================================================
// get_ShiftStr: シフト状態の文字列化
//===========================================================================
TEST_CASE("get_ShiftStr: 複数修飾キーの組み合わせ")
{
	TShiftState shift;
	CHECK(get_ShiftStr(shift) == UnicodeString(""));

	shift << ssShift;
	CHECK(get_ShiftStr(shift) == UnicodeString("Shift+"));

	shift << ssCtrl;
	CHECK(get_ShiftStr(shift) == UnicodeString("Shift+Ctrl+"));

	shift << ssAlt;
	CHECK(get_ShiftStr(shift) == UnicodeString("Shift+Ctrl+Alt+"));
}

//===========================================================================
// is_Num0to9
//===========================================================================
TEST_CASE("is_Num0to9: 単一の数字キー文字列のみ true")
{
	CHECK(is_Num0to9("0") == true);
	CHECK(is_Num0to9("9") == true);
	CHECK(is_Num0to9("5") == true);
	CHECK(is_Num0to9("10") == false);  //2文字はfalse
	CHECK(is_Num0to9("a") == false);
	CHECK(is_Num0to9("") == false);
}

//===========================================================================
// is_DialogKey
//===========================================================================
TEST_CASE("is_DialogKey: カーソル/PageUp等のキーを判定")
{
	CHECK(is_DialogKey(VK_LEFT) == true);
	CHECK(is_DialogKey(VK_TAB) == true);
	CHECK(is_DialogKey(VK_HOME) == true);
	CHECK(is_DialogKey((WORD)'A') == false);
}

//===========================================================================
// get_shift_from_wparam
//===========================================================================
TEST_CASE("get_shift_from_wparam: MK_CONTROL/MK_SHIFT ビットの取得")
{
	CHECK(get_shift_from_wparam(0) == 0);
	CHECK(get_shift_from_wparam(MK_CONTROL) == 1);
	CHECK(get_shift_from_wparam(MK_SHIFT) == 2);
	CHECK(get_shift_from_wparam(MK_CONTROL | MK_SHIFT) == 3);
}

//===========================================================================
// get_AlNumChar
//===========================================================================
TEST_CASE("get_AlNumChar: テンキー/英数字キーの文字化")
{
	CHECK(get_AlNumChar(VK_NUMPAD3) == UnicodeString("3"));
	CHECK(get_AlNumChar((WORD)'Z') == UnicodeString("Z"));
	//非対応キー(英数字コードと衝突しない仮想キー)は空文字列
	CHECK(get_AlNumChar(VK_ESCAPE) == UnicodeString(""));
}

TEST_CASE("get_AlNumChar: 仮想キーコードが英数字のASCIIコードと衝突するケース"
          "(既存実装の疑わしい挙動をそのまま固定)")
{
	//VK_F1 (0x70) はASCIIの小文字 'p' (0x70) と同じ値であり、
	//get_AlNumChar は Key の種別(仮想キーかASCII文字か)を区別せず
	//_istalnum(Key) だけで判定しているため、VK_F1 を渡すと
	//"非対応キーのはず"が実際には "p" を返してしまう。
	//F2(0x71)='q', F3(0x72)='r' ... も同様に衝突する。
	CHECK(get_AlNumChar(VK_F1) == UnicodeString("p"));
}

//===========================================================================
// make_KeyList: キーリスト作成 (KeyboardMode 固定で環境非依存に)
//===========================================================================
TEST_CASE("make_KeyList: 英字/数字/Fキー等を含むリストを作成")
{
	int save_mode = KeyboardMode;
	KeyboardMode = 2;  //US固定

	std::unique_ptr<TStringList> lst(new TStringList());
	make_KeyList(lst.get());

	CHECK(lst->IndexOf("A") != -1);
	CHECK(lst->IndexOf("Z") != -1);
	CHECK(lst->IndexOf("0") != -1);
	CHECK(lst->IndexOf("F1") != -1);
	CHECK(lst->IndexOf("F12") != -1);
	CHECK(lst->IndexOf("Enter") != -1);
	CHECK(lst->IndexOf("10Key_5") != -1);
	CHECK(lst->IndexOf("`") != -1);  //US配列固有

	KeyboardMode = save_mode;
}
