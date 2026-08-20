/**
 * @file tests/core/test_usr_highlight.cpp
 * @brief src/usr_highlight.cpp (構文強調表示の設定解析) の回帰テスト
 *
 * 目的: 現在の実装の挙動をそのまま固定すること (regression test)。
 *
 * GetDefaultHighlight は内部で `UserHighlight` (グローバルの HighlightFile*)
 * を経由するため、テストの前後で自前のインスタンスを差し込み/退避する
 * (他の TU のテストに影響しないよう、TEST_CASE ごとに RAII で退避する)。
 */
#include "doctest/doctest.h"

#include <memory>

#include "usr_highlight.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

/// UserHighlight を一時的に差し替え、破棄時に元へ戻す
struct UserHighlightGuard {
	HighlightFile *saved;
	explicit UserHighlightGuard(HighlightFile *replacement) : saved(UserHighlight) { UserHighlight = replacement; }
	~UserHighlightGuard() { UserHighlight = saved; }
};

std::unique_ptr<TStringList> make_buf(const UnicodeString &content)
{
	std::unique_ptr<TStringList> buf(new TStringList());
	buf->Text = content;
	return buf;
}

}  // namespace

//===========================================================================
// GetDefNumericPtn (拡張子ごとの数値パターン)
//===========================================================================
TEST_CASE("GetDefNumericPtn: 拡張子ごとに既定パターンを返す")
{
	CHECK(GetDefNumericPtn(".css") == UnicodeString("([: ]+#[0-9a-f]+)|(\\b[0-9][0-9.]*)"));
	CHECK(GetDefNumericPtn(".json") == UnicodeString("\\b-?[0-9][0-9.]*\\b"));
	CHECK(GetDefNumericPtn(".unknownext") == UnicodeString(""));
}

//===========================================================================
// GetDefSymbolChars (拡張子ごとのシンボル文字)
//===========================================================================
TEST_CASE("GetDefSymbolChars: is_xml=true は共通パターンを優先")
{
	CHECK(GetDefSymbolChars(".css", true, false) == UnicodeString("{}/=<>:;?"));
}

TEST_CASE("GetDefSymbolChars: 拡張子ごとの既定")
{
	CHECK(GetDefSymbolChars(".json", false, false) == UnicodeString("{}[],:"));
	CHECK(GetDefSymbolChars(".cpp", false, false) == UnicodeString("{}()[]+-*/%&|^!~=<>,:;?"));
	CHECK(GetDefSymbolChars(".unknownext", false, false) == UnicodeString(""));
}

//===========================================================================
// GetDefQuotChars (拡張子ごとの文字列引用符)
//===========================================================================
TEST_CASE("GetDefQuotChars: 拡張子ごとの引用符と use_esc")
{
	bool use_esc = false;
	CHECK(GetDefQuotChars(".cpp", use_esc, false, false, false) == UnicodeString("\""));
	CHECK(use_esc == true);

	CHECK(GetDefQuotChars(".pas", use_esc, false, false, false) == UnicodeString("\'"));

	//vbs 系はエスケープシーケンス無し
	GetDefQuotChars(".vbs", use_esc, false, false, false);
	CHECK(use_esc == false);

	CHECK(GetDefQuotChars(".sh", use_esc, false, false, false) == UnicodeString("\"\'`"));
}

//===========================================================================
// GetDefFunctionPtn (拡張子ごとの関数パターン)
//===========================================================================
TEST_CASE("GetDefFunctionPtn: Python/Go/未対応拡張子")
{
	UnicodeString name_ptn;
	CHECK(GetDefFunctionPtn(".py", name_ptn, false) == UnicodeString("^\\s*def\\s+[_a-zA-Z]\\w*"));

	CHECK(GetDefFunctionPtn(".go", name_ptn, false) == UnicodeString("^func(\\s\\(.+\\))?\\s+[_a-zA-Z]\\w*"));
	CHECK(name_ptn == UnicodeString("\\b[_a-zA-Z]\\w*\\("));

	CHECK(GetDefFunctionPtn(".unknownext", name_ptn, false) == UnicodeString(""));
}

//===========================================================================
// GetSearchPairPtn (SearchPair 用パターン)
//===========================================================================
TEST_CASE("GetSearchPairPtn: Pascal は begin/end 系を1件返す")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	CHECK(GetSearchPairPtn(".pas", lst.get()) == true);
	CHECK(lst->Count == 1);
}

TEST_CASE("GetSearchPairPtn: 未対応拡張子は false で空のまま")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	CHECK(GetSearchPairPtn(".unknownext", lst.get()) == false);
	CHECK(lst->Count == 0);
}

//===========================================================================
// HighlightFile: セクション選択 (拡張子/CLIPBOARD/TASKLOG によるマッチ)
//===========================================================================
TEST_CASE("HighlightFile::GetSection は拡張子でセクションを選ぶ")
{
	//既存の挙動: GetSection は TargetPath/TargetName が「未設定」でも
	//split_strings_semicolon(EmptyStr) が長さ1(空文字列1件)の配列を返す
	//ため、「フィルタあり」の分岐に入ってしまい、対象パス/対象名が一切
	//マッチせず CurSection が空にリセットされる (TargetPath/TargetName の
	//両方を明示的に指定した場合のみマッチする)。バグの可能性が高いが、
	//修正せず現状の挙動を固定する
	TempDir dir;
	UnicodeString fnam = dir.file("highlight.ini");
	make_buf(_T("[.cpp.h:C/C++]\r\n")
	         _T("TargetPath=C:\\\r\n")
	         _T("TargetName=test\r\n")
	         _T("ReservedPtn=\\b(if|else|for|while)\\b\r\n")
	         _T("NumericPtn=\\b[0-9]+\\b\r\n")
	         _T("FuncCol=0000ff\r\n"))
		->SaveToFile(fnam);

	std::unique_ptr<HighlightFile> hl(new HighlightFile(fnam));

	//設定内容に構文エラーが無いこと (ReservedPtn/NumericPtn は妥当な正規表現、
	//FuncCol は6桁16進数)
	CHECK(hl->ErrorList->Count == 0);

	CHECK(hl->GetSection("C:\\test.cpp") == true);
	CHECK(hl->CurSection == UnicodeString(".cpp.h:C/C++"));
	CHECK(hl->ReadKeyStr(_T("ReservedPtn")) == UnicodeString("\\b(if|else|for|while)\\b"));
	CHECK(hl->ReadRegExPtn(_T("ReservedPtn")) == UnicodeString("\\b(if|else|for|while)\\b"));
	//"RRGGBB" (0000ff = 青) は xRRGGBB_to_col で TColor の BGR 順 (0x00BBGGRR) に
	//変換されるため 0xFF0000 になる
	CHECK(hl->ReadColorRGB6H(_T("FuncCol"), clBlack) == (TColor)0xff0000);

	//マッチしない拡張子
	CHECK(hl->GetSection("C:\\test.py") == false);
	CHECK(hl->CurSection == UnicodeString(""));

	//TargetPath に一致しないパス
	CHECK(hl->GetSection("D:\\test.cpp") == false);
}

TEST_CASE("HighlightFile::GetSection は TargetPath/TargetName が無いと決してマッチしない")
{
	//既存の挙動の固定 (上のテストのコメント参照)。TargetPath/TargetName の
	//どちらも指定しない「素の」セクションは、拡張子が一致していても
	//CurSection が空にリセットされてマッチしない
	TempDir dir;
	UnicodeString fnam = dir.file("highlight_bare.ini");
	make_buf(_T("[.cpp.h:C/C++]\r\nReservedPtn=\\b(if|else)\\b\r\n"))->SaveToFile(fnam);

	std::unique_ptr<HighlightFile> hl(new HighlightFile(fnam));
	CHECK(hl->GetSection("C:\\test.cpp") == false);
}

TEST_CASE("HighlightFile: 不正な設定値は ErrorList に記録される")
{
	TempDir dir;
	UnicodeString fnam = dir.file("bad_highlight.ini");
	//FuncCol が6桁16進数でない (不正値)
	make_buf(_T("[.xx:Bad]\r\nFuncCol=not_a_color\r\n"))->SaveToFile(fnam);

	std::unique_ptr<HighlightFile> hl(new HighlightFile(fnam));
	CHECK(hl->ErrorList->Count > 0);
}

//===========================================================================
// GetDefaultHighlight (UserHighlight 経由でデフォルト定義一式を生成)
//===========================================================================
TEST_CASE("GetDefaultHighlight: ユーザ定義が無い拡張子は既定パターンで埋まる")
{
	TempDir dir;
	UnicodeString fnam = dir.file("empty_highlight.ini");
	make_buf(_T(""))->SaveToFile(fnam);

	std::unique_ptr<HighlightFile> hl(new HighlightFile(fnam));
	UserHighlightGuard guard(hl.get());

	std::unique_ptr<TStringList> lst(new TStringList());
	CHECK(GetDefaultHighlight(".py", lst.get()) == true);

	UnicodeString text = lst->Text;
	CHECK(text.Pos("[.py]") > 0);
	CHECK(text.Pos("NumericPtn=") > 0);
	CHECK(text.Pos("FunctionPtn=^\\s*def\\s+[_a-zA-Z]\\w*") > 0);
}
