/**
 * @file tests/core/test_gui_text_viewer.cpp
 * @brief gui/text_viewer_core.cpp (テキストビューアの文字コード判定・行分割・
 *        折り返し計算) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。文字コード判定は自前実装せず、移植済みの
 * get_MemoryCodePage (src/usr_str.cpp) をそのまま使っていることを、
 * 実際に各種文字コードのファイルを一時ディレクトリに書いて確認する
 * (tests/temp_dir.h の TempDir)。
 */
#include "doctest/doctest.h"

#include <memory>

#include "gui/text_viewer_core.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;
using text_viewer_core::CharDisplayWidth;
using text_viewer_core::ClearMarkList;
using text_viewer_core::FindMarkNext;
using text_viewer_core::FindNextLine;
using text_viewer_core::LoadForView;
using text_viewer_core::NextCodePage;
using text_viewer_core::PageStepLines;
using text_viewer_core::ParseCodePageParam;
using text_viewer_core::ParseJumpLine;
using text_viewer_core::ParseMoveCount;
using text_viewer_core::StepLine;
using text_viewer_core::ToggleMark;
using text_viewer_core::WrapLine;

namespace {

/// 生バイト列をそのままファイルへ書く (UTF-16 等、途中に 0x00 を含む内容のため
/// strlen に頼れない)
void write_bytes(const UnicodeString &fnam, const unsigned char *data, int len)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	fs->WriteBuffer(data, len);
}

}  // namespace

//===========================================================================
// LoadForView: 文字コード判定 (get_MemoryCodePage 経由)
//===========================================================================
TEST_CASE("LoadForView: UTF-8 (BOM無し) を読める")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("utf8_nobom.txt"));
	const unsigned char bytes[] = {
		'h', 'e', 'l', 'l', 'o', '\r', '\n',
		0xe6, 0x97, 0xa5, 0xe6, 0x9c, 0xac,  //日本 (UTF-8)
	};
	write_bytes(fnam, bytes, sizeof(bytes));

	text_viewer_core::LoadResult r = LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.error.IsEmpty());
	CHECK(!r.is_binary);
	CHECK(!r.has_bom);
	CHECK(r.code_page == CP_UTF8);
	REQUIRE(r.lines.size() == 2);
	CHECK(r.lines[0] == _T("hello"));
	CHECK(r.lines[1] == _T("日本"));
}

TEST_CASE("LoadForView: UTF-8 BOM付きを読める (BOMは行内容に残らない)")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("utf8_bom.txt"));
	const unsigned char bytes[] = {
		0xEF, 0xBB, 0xBF,        //BOM
		'a', 'b', 'c', '\n',
		'd', 'e', 'f',
	};
	write_bytes(fnam, bytes, sizeof(bytes));

	text_viewer_core::LoadResult r = LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.has_bom);
	CHECK(r.code_page == CP_UTF8);
	REQUIRE(r.lines.size() == 2);
	CHECK(r.lines[0] == _T("abc"));  // BOM の3バイトが先頭行に混ざっていないこと
	CHECK(r.lines[1] == _T("def"));
}

TEST_CASE("LoadForView: CP932 (Shift_JIS、BOM無し) を読める")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("sjis.txt"));
	// "日本語" の Shift_JIS(CP932) バイト列: 日=93FA 本=967B 語=8CEA
	const unsigned char bytes[] = {0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA};
	write_bytes(fnam, bytes, sizeof(bytes));

	text_viewer_core::LoadResult r = LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(!r.has_bom);
	CHECK(r.code_page == 932);
	REQUIRE(r.lines.size() == 1);
	CHECK(r.lines[0] == _T("日本語"));
}

TEST_CASE("LoadForView: UTF-16LE (BOM付き) を読める")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("utf16le.txt"));
	// BOM(FF FE) + "AB" (UTF-16LE)
	const unsigned char bytes[] = {0xFF, 0xFE, 'A', 0x00, 'B', 0x00};
	write_bytes(fnam, bytes, sizeof(bytes));

	text_viewer_core::LoadResult r = LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.has_bom);
	CHECK(r.code_page == 1200);
	REQUIRE(r.lines.size() == 1);
	CHECK(r.lines[0] == _T("AB"));
}

TEST_CASE("LoadForView: バイナリ (0x00 の連続) は is_binary=true になり行分割しない")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("binary.dat"));
	const unsigned char bytes[] = {'A', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 'C', 'D'};
	write_bytes(fnam, bytes, sizeof(bytes));

	text_viewer_core::LoadResult r = LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.is_binary);
	CHECK(r.lines.empty());
}

TEST_CASE("LoadForView: 存在しないファイルは ok=false でエラーメッセージを持つ")
{
	TempDir dir;
	text_viewer_core::LoadResult r = LoadForView(dir.file(_T("notfound.txt")));
	CHECK(!r.ok);
	CHECK(!r.error.IsEmpty());
}

//===========================================================================
// LoadForView: サイズ上限 (大きなファイルで固まらないための切り詰め)
//===========================================================================
TEST_CASE("LoadForView: max_bytes を超えるファイルは先頭だけ読み truncated=true になる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("big.txt"));

	UnicodeString content;
	for (int i = 0; i < 100; ++i) content += _T("0123456789\n");  // 1100 バイト
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi(content);  // ASCII のみなので narrow 化して安全にバイト数を制御する
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	const Int64 limit = 50;
	text_viewer_core::LoadResult r = LoadForView(fnam, limit);
	REQUIRE(r.ok);
	CHECK(r.truncated);
	CHECK(r.read_size == limit);
	CHECK(r.file_size == ansi.Length());
	// 切り詰めても、先頭部分の行は正しく読めていること
	REQUIRE(!r.lines.empty());
	CHECK(r.lines[0] == _T("0123456789"));
}

TEST_CASE("LoadForView: max_bytes 以下のファイルは truncated=false")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("small.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("short");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	text_viewer_core::LoadResult r = LoadForView(fnam, 4096);
	REQUIRE(r.ok);
	CHECK(!r.truncated);
	CHECK(r.read_size == r.file_size);
}

//===========================================================================
// CharDisplayWidth / WrapLine (折り返し計算。wx 非依存の簡易実装。
// gui/text_viewer_core.h のコメント参照: フォント実測ではなく
// Unicode ブロック範囲によるおおまかな判定)
//===========================================================================
TEST_CASE("CharDisplayWidth: ASCIIは幅1、全角相当は幅2")
{
	CHECK(CharDisplayWidth(L'A') == 1);
	CHECK(CharDisplayWidth(L'0') == 1);
	CHECK(CharDisplayWidth(0x3042) == 2);  // ひらがな「あ」
	CHECK(CharDisplayWidth(0x65E5) == 2);  // 漢字「日」
	CHECK(CharDisplayWidth(0xFF21) == 2);  // 全角「Ａ」
	CHECK(CharDisplayWidth(0xFF71) == 1);  // 半角カナ「ｱ」
}

TEST_CASE("WrapLine: 幅以下ならそのまま1行")
{
	std::vector<UnicodeString> r = WrapLine(_T("hello"), 10);
	REQUIRE(r.size() == 1);
	CHECK(r[0] == _T("hello"));
}

TEST_CASE("WrapLine: ASCIIの半角換算で幅ちょうどに折り返す")
{
	std::vector<UnicodeString> r = WrapLine(_T("0123456789"), 4);
	REQUIRE(r.size() == 3);
	CHECK(r[0] == _T("0123"));
	CHECK(r[1] == _T("4567"));
	CHECK(r[2] == _T("89"));
}

TEST_CASE("WrapLine: 全角文字は幅2として数える")
{
	// 全角4文字 * 幅2 = 8。幅6では2文字目までしか入らない (2*2=4 <= 6, 3文字目で 6 <= 6 も入る境界を避け幅5で確認)
	std::vector<UnicodeString> r = WrapLine(_T("あいうえ"), 5);
	REQUIRE(r.size() == 2);
	CHECK(r[0] == _T("あい"));  // 2*2=4 <= 5, 3文字目を足すと6>5 で折り返す
	CHECK(r[1] == _T("うえ"));
}

TEST_CASE("WrapLine: 幅0以下なら折り返さない")
{
	std::vector<UnicodeString> r = WrapLine(_T("0123456789"), 0);
	REQUIRE(r.size() == 1);
	CHECK(r[0] == _T("0123456789"));
}

TEST_CASE("WrapLine: 1文字だけで幅を超える場合でも無限ループせず必ず入る")
{
	std::vector<UnicodeString> r = WrapLine(_T("あああ"), 1);
	REQUIRE(r.size() == 3);
	CHECK(r[0] == _T("あ"));
	CHECK(r[1] == _T("あ"));
	CHECK(r[2] == _T("あ"));
}

//===========================================================================
// Vモード操作の純関数 (TxtViewer.cpp 由来。行単位に単純化)
//===========================================================================
TEST_CASE("StepLine: カーソル上下は範囲内に収まる (CursorDown/CursorUp相当)")
{
	// TxtViewer::CursorDown/CursorUp は CurPos.y を 0..MaxDispLine-1 に収める。
	// 行単位ビューアでは current_line_ を 0..count-1 に収めるのと同値
	CHECK(StepLine(0, 1, 5) == 1);
	CHECK(StepLine(4, 1, 5) == 4);   // 末尾では留まる
	CHECK(StepLine(0, -1, 5) == 0);  // 先頭では留まる
	CHECK(StepLine(1, 3, 5) == 4);   // 飛び越えは末尾で止まる
	CHECK(StepLine(0, 1, 0) == 0);   // 空ファイルでは動かない
}

TEST_CASE("PageStepLines: 1ページは表示行-1 (MovePage相当)")
{
	// TxtViewer::MovePage は LineCount+1 行進む。表示行-1と同値
	CHECK(PageStepLines(20) == 19);
	CHECK(PageStepLines(1) == 1);
	CHECK(PageStepLines(0) == 1);
}

TEST_CASE("FindNextLine: 次の行から探して折り返す (SearchDown/SearchUp相当)")
{
	const std::vector<UnicodeString> lines = {_T("aaa"), _T("bbb"), _T("ccc bbb")};
	// 大小文字を区別しない (TextViewer::SearchForward と同じ簡略化)
	CHECK(FindNextLine(lines, _T("BBB"), 0, true) == 1);
	CHECK(FindNextLine(lines, _T("bbb"), 1, true) == 2);  // 自行を飛ばして次へ
	CHECK(FindNextLine(lines, _T("bbb"), 2, true) == 1);  // 末尾から先頭へ折り返す
	CHECK(FindNextLine(lines, _T("bbb"), 2, false) == 1);  // 上方向
	CHECK(FindNextLine(lines, _T("bbb"), 0, false) == 2);  // 上方向も折り返す
	CHECK(FindNextLine(lines, _T("zzz"), 0, true) == -1);
	CHECK(FindNextLine(lines, _T(""), 0, true) == -1);
}

TEST_CASE("ToggleMark/ClearMarkList/FindMarkNext: 栞マーク (Mark/MarkLine相当)")
{
	// TxtViewer::MarkLine は "lno;" 形式の文字列でトグルする。ここでは
	// 0ベース行番号のvectorで同じトグル・検索を行う
	std::vector<int> marks;
	marks = ToggleMark(marks, 3);
	REQUIRE(marks.size() == 1);
	marks = ToggleMark(marks, 1);
	CHECK(marks.size() == 2);
	// 同じ行でもう一度呼ぶと解除される
	marks = ToggleMark(marks, 3);
	REQUIRE(marks.size() == 1);
	CHECK(marks[0] == 1);

	marks = ToggleMark(marks, 5);
	// 下方向: 現在行より大きい最小のマークへ (FindMarkDown相当)
	CHECK(FindMarkNext(marks, 0, true) == 1);
	CHECK(FindMarkNext(marks, 1, true) == 5);
	CHECK(FindMarkNext(marks, 5, true) == -1);  // これより下には無い
	// 上方向: 現在行より小さい最大のマークへ (FindMarkUp相当)
	CHECK(FindMarkNext(marks, 6, false) == 5);
	CHECK(FindMarkNext(marks, 5, false) == 1);
	CHECK(FindMarkNext(marks, 0, false) == -1);

	CHECK(ClearMarkList(marks).empty());
}

TEST_CASE("ParseJumpLine: 行番号ジャンプ (JumpLine相当)")
{
	// テキスト表示の JumpLine は1ベース絶対指定のみ ("+|=" を含むとAbort)。
	// ここでは "+n/-n" の相対指定も受け付ける (バイナリ表示の ToAddrA と同じ考え方)
	CHECK(ParseJumpLine(_T("5"), 0, 10) == 4);
	CHECK(ParseJumpLine(_T("1"), 0, 10) == 0);
	CHECK(ParseJumpLine(_T("10"), 0, 10) == 9);
	CHECK(ParseJumpLine(_T("+3"), 1, 10) == 4);
	CHECK(ParseJumpLine(_T("-2"), 5, 10) == 3);
	CHECK(ParseJumpLine(_T("0"), 0, 10) == -1);   // 範囲外は無効
	CHECK(ParseJumpLine(_T("11"), 0, 10) == -1);
	CHECK(ParseJumpLine(_T(""), 0, 10) == -1);
	CHECK(ParseJumpLine(_T("abc"), 0, 10) == -1);
	CHECK(ParseJumpLine(_T("+99"), 0, 10) == -1);  // 相対の行き過ぎも無効
}

TEST_CASE("NextCodePage/ParseCodePageParam: 文字コード切替 (change_CodePage相当)")
{
	// TxtViewer::change_CodePage の循環表 (932→50220→20932→1252→65001→1200→932)
	CHECK(NextCodePage(932) == 50220);
	CHECK(NextCodePage(50220) == 20932);
	CHECK(NextCodePage(20932) == 1252);
	CHECK(NextCodePage(1252) == 65001);
	CHECK(NextCodePage(65001) == 1200);
	CHECK(NextCodePage(1200) == 932);
	CHECK(NextCodePage(9999) == 932);  // 不明値は932へ
	// パラメータ指定は表にある値だけ有効。空なら次へ進む
	CHECK(ParseCodePageParam(_T("65001"), 932) == 65001);
	CHECK(ParseCodePageParam(_T("9999"), 932) == 0);
	CHECK(ParseCodePageParam(_T(""), 932) == 50220);
}

TEST_CASE("ParseMoveCount: 移動行数 (get_MovePrmの数値部分相当)")
{
	CHECK(ParseMoveCount(_T(""), 7) == 1);
	CHECK(ParseMoveCount(_T("3"), 7) == 3);
	CHECK(ParseMoveCount(_T("0"), 7) == 1);
	CHECK(ParseMoveCount(_T("abc"), 7) == 1);
}
