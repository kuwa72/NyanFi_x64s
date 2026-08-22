/**
 * @file tests/core/test_gui_log_win.cpp
 * @brief gui/log_win.cpp (ログの蓄積・整形・書き出し) のテスト
 *
 * 実測の根拠 (状態文字の一覧・書式) は gui/log_win.h の冒頭コメントを参照。
 */
#include "doctest/doctest.h"

#include <string>

#include "gui/log_win.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;
using log_win::LogBuffer;
using log_win::LogLine;
using log_win::LogStatus;

namespace {

void mkfile(const UnicodeString &path, const std::string &body = std::string())
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	if (!body.empty()) {
		DWORD written = 0;
		::WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, NULL);
	}
	::CloseHandle(h);
}

std::string read_all(const UnicodeString &path)
{
	std::string out;
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

}  // namespace

//===========================================================================
// StatusChar
//===========================================================================

TEST_CASE("StatusChar: 実測した先頭1文字の対応")
{
	CHECK(log_win::StatusChar(LogStatus::Info)      == L' ');
	CHECK(log_win::StatusChar(LogStatus::Overwrite)  == L'O');
	CHECK(log_win::StatusChar(LogStatus::Newer)      == L'N');
	CHECK(log_win::StatusChar(LogStatus::Skipped)    == L'S');
	CHECK(log_win::StatusChar(LogStatus::Warning)    == L'W');
	CHECK(log_win::StatusChar(LogStatus::Error)      == L'E');
	CHECK(log_win::StatusChar(LogStatus::Canceled)   == L'C');
}

//===========================================================================
// FormatLine
//===========================================================================

TEST_CASE("FormatLine: 通常行は「 >」+ 状態文字 + 本文")
{
	LogLine line;
	line.status = LogStatus::Error;
	line.text   = _T("COPY file.txt");
	UnicodeString s = log_win::FormatLine(line);
	CHECK(s == _T(" >ECOPY file.txt"));
}

TEST_CASE("FormatLine: Info(既定) は状態文字を差し込まない")
{
	LogLine line;
	line.status = LogStatus::Info;
	line.text   = _T("COPY file.txt");
	CHECK(log_win::FormatLine(line) == _T(" >COPY file.txt"));
}

TEST_CASE("FormatLine: show_time なら時刻を差し込む")
{
	LogLine line;
	line.status    = LogStatus::Info;
	line.text      = _T("削除終了  OK:3");
	line.show_time = true;
	line.stamp     = EncodeTime(9, 5, 3, 0);
	CHECK(log_win::FormatLine(line) == _T(" >09:05:03 削除終了  OK:3"));
}

TEST_CASE("FormatLine: 空行は空文字列になる")
{
	LogLine line;
	CHECK(log_win::FormatLine(line) == _T(""));
}

TEST_CASE("FormatLine: raw は装飾なしでそのまま")
{
	LogLine line;
	line.raw  = true;
	line.text = _T("    詳細情報の1行");
	CHECK(log_win::FormatLine(line) == _T("    詳細情報の1行"));
}

TEST_CASE("FormatLine: 開始行はタスク番号無しなら >>")
{
	LogLine line;
	line.is_start  = true;
	line.text      = _T("コピー準備\tD:\\dst\\");
	line.show_time = true;
	line.stamp     = EncodeTime(12, 0, 0, 0);
	CHECK(log_win::FormatLine(line) == _T(">>12:00:00 コピー準備 ---> D:\\dst\\"));
}

TEST_CASE("FormatLine: 開始行はタスク番号があれば 番号+1 > (0起点)")
{
	LogLine line;
	line.is_start  = true;
	line.task_no   = 0;
	line.text      = _T("コピー準備");
	line.show_time = true;
	line.stamp     = EncodeTime(12, 0, 0, 0);
	CHECK(log_win::FormatLine(line) == _T("1>12:00:00 コピー準備"));
}

//===========================================================================
// MakeLogHeader
//===========================================================================

TEST_CASE("MakeLogHeader: コマンド名を6文字幅で右詰めし、ファイル名を続ける")
{
	UnicodeString s = log_win::MakeLogHeader(_T("COPY"), _T("file.txt"));
	CHECK(s == _T("   COPY file.txt"));
}

TEST_CASE("MakeLogHeader: is_dir なら角括弧で囲む")
{
	UnicodeString s = log_win::MakeLogHeader(_T("CREATE"), _T("D:\\work\\sub"), /*is_dir=*/true, /*full_path=*/true);
	CHECK(s == _T(" CREATE [D:\\work\\sub]"));
}

TEST_CASE("MakeLogHeader: full_path=false ならファイル名部分だけになる")
{
	UnicodeString s = log_win::MakeLogHeader(_T("DELETE"), _T("D:\\work\\file.txt"), false, false);
	CHECK(s == _T(" DELETE file.txt"));
}

TEST_CASE("MakeLogHeader: width>0 なら名前部分を左詰めで揃える")
{
	UnicodeString s = log_win::MakeLogHeader(_T("LOAD"), _T("a.txt"), false, false, 10);
	CHECK(s == _T("   LOAD a.txt     "));
}

//===========================================================================
// FormatResultCount
//===========================================================================

TEST_CASE("FormatResultCount: 0件の項目は出さない")
{
	CHECK(log_win::FormatResultCount(0, 0, 0, 0) == _T(""));
	CHECK(log_win::FormatResultCount(3) == _T("  OK:3"));
}

TEST_CASE("FormatResultCount: 出力順は OK, NG, ERR, SKIP (引数順とは異なる)")
{
	// 引数は (ok, er, sk, ng) だが、出力は OK -> NG -> ERR -> SKIP (実測どおり)
	CHECK(log_win::FormatResultCount(1, 2, 3, 4) == _T("  OK:1  NG:4  ERR:2  SKIP:3"));
}

//===========================================================================
// LogBuffer::Add / CountOf
//===========================================================================

TEST_CASE("LogBuffer::Add: 追加した行が Lines() に積まれる")
{
	LogBuffer buf;
	buf.Add(LogStatus::Info, _T("hello"));
	REQUIRE(buf.Count() == 1);
	CHECK(buf.Lines()[0].text == _T("hello"));
}

TEST_CASE("LogBuffer::CountOf: 状態ごとに数えられる")
{
	LogBuffer buf;
	buf.Add(LogStatus::Error, _T("a"));
	buf.Add(LogStatus::Error, _T("b"));
	buf.Add(LogStatus::Skipped, _T("c"));
	buf.Add(LogStatus::Info, _T("d"));
	CHECK(buf.CountOf(LogStatus::Error) == 2);
	CHECK(buf.CountOf(LogStatus::Skipped) == 1);
	CHECK(buf.CountOf(LogStatus::Info) == 1);
	CHECK(buf.CountOf(LogStatus::Canceled) == 0);
}

TEST_CASE("LogBuffer::Clear: 全て消える")
{
	LogBuffer buf;
	buf.Add(LogStatus::Info, _T("x"));
	buf.Clear();
	CHECK(buf.Count() == 0);
}

//===========================================================================
// LogBuffer: 上限を超えたら古いものから捨てる (規約9 / 完了条件で明示要求)
//===========================================================================

TEST_CASE("LogBuffer: 上限を超えたら先頭 (古い方) から捨てる")
{
	LogBuffer buf(3);
	buf.Add(LogStatus::Info, _T("1"));
	buf.Add(LogStatus::Info, _T("2"));
	buf.Add(LogStatus::Info, _T("3"));
	buf.Add(LogStatus::Info, _T("4"));
	REQUIRE(buf.Count() == 3);
	CHECK(buf.Lines()[0].text == _T("2"));
	CHECK(buf.Lines()[1].text == _T("3"));
	CHECK(buf.Lines()[2].text == _T("4"));
}

TEST_CASE("LogBuffer: 上限0以下は無制限")
{
	LogBuffer buf(0);
	for (int i = 0; i < 50; i++) buf.Add(LogStatus::Info, _T("x"));
	CHECK(buf.Count() == 50);
}

TEST_CASE("LogBuffer: 既定の上限は VCL の MaxLogLines=1000 に合わせてある")
{
	LogBuffer buf;
	CHECK(buf.MaxLines() == 1000);
}

//===========================================================================
// LogBuffer::AddRaw / AddBlankIfNeeded
//===========================================================================

TEST_CASE("LogBuffer::AddRaw: 改行区切りで複数行に分かれ、装飾されない")
{
	LogBuffer buf;
	buf.AddRaw(_T("1行目\r\n2行目"));
	REQUIRE(buf.Count() == 2);
	CHECK(log_win::FormatLine(buf.Lines()[0]) == _T("1行目"));
	CHECK(log_win::FormatLine(buf.Lines()[1]) == _T("2行目"));
}

TEST_CASE("LogBuffer::AddBlankIfNeeded: 最後が空行でなければ空行を足す")
{
	LogBuffer buf;
	buf.Add(LogStatus::Info, _T("x"));
	buf.AddBlankIfNeeded();
	REQUIRE(buf.Count() == 2);
	CHECK(buf.Lines()[1].text.IsEmpty());
}

TEST_CASE("LogBuffer::AddBlankIfNeeded: 最後が既に空行なら足さない")
{
	LogBuffer buf;
	buf.Add(LogStatus::Info, _T("x"));
	buf.AddBlankIfNeeded();
	buf.AddBlankIfNeeded();
	CHECK(buf.Count() == 2);
}

TEST_CASE("LogBuffer::AddBlankIfNeeded: 空のバッファには何もしない")
{
	LogBuffer buf;
	buf.AddBlankIfNeeded();
	CHECK(buf.Count() == 0);
}

//===========================================================================
// LogBuffer::StartGroup / EndGroup
//===========================================================================

TEST_CASE("LogBuffer::StartGroup: 直前に何もなければ空行を挟まずに開始行を積む")
{
	LogBuffer buf;
	buf.StartGroup(_T("コピー準備"));
	REQUIRE(buf.Count() == 1);
	CHECK(buf.Lines()[0].is_start);
}

TEST_CASE("LogBuffer::StartGroup: 直前が通常行なら空行を1つ挟む")
{
	LogBuffer buf;
	buf.Add(LogStatus::Info, _T("前回の終了行"));
	buf.StartGroup(_T("コピー準備"));
	REQUIRE(buf.Count() == 3);
	CHECK(buf.Lines()[1].text.IsEmpty());
	CHECK(buf.Lines()[2].is_start);
}

TEST_CASE("LogBuffer::StartGroup: 直前が開始行なら空行を挟まない (連続する準備行)")
{
	LogBuffer buf;
	buf.StartGroup(_T("1つ目の準備"));
	buf.StartGroup(_T("2つ目の準備"));
	REQUIRE(buf.Count() == 2);
	CHECK(buf.Lines()[0].is_start);
	CHECK(buf.Lines()[1].is_start);
}

TEST_CASE("LogBuffer::StartGroup: text が空なら空行だけを積む")
{
	LogBuffer buf;
	buf.StartGroup(_T(""));
	REQUIRE(buf.Count() == 1);
	CHECK(buf.Lines()[0].text.IsEmpty());
	CHECK_FALSE(buf.Lines()[0].is_start);
}

TEST_CASE("LogBuffer::EndGroup: 本文+終了+結果件数を時刻付きで積む")
{
	LogBuffer buf;
	buf.EndGroup(_T("コピー"), log_win::FormatResultCount(5, 1));
	REQUIRE(buf.Count() == 1);
	UnicodeString s = log_win::FormatLine(buf.Lines()[0]);
	CHECK(ContainsStr(s, _T("コピー終了  OK:5  ERR:1")));
}

//===========================================================================
// SaveTo
//===========================================================================

TEST_CASE("SaveTo: UTF-8 + BOM で書き出す")
{
	TempDir dir;
	LogBuffer buf;
	buf.Add(LogStatus::Error, _T("日本語のテスト"));

	UnicodeString err;
	UnicodeString path = dir.file(_T("log.txt"));
	REQUIRE(log_win::SaveTo(path, buf.Lines(), err));

	std::string bytes = read_all(path);
	REQUIRE(bytes.size() >= 3);
	CHECK(static_cast<unsigned char>(bytes[0]) == 0xEF);
	CHECK(static_cast<unsigned char>(bytes[1]) == 0xBB);
	CHECK(static_cast<unsigned char>(bytes[2]) == 0xBF);
	// UTF-8 で "E" (状態文字) がそのまま出ていること
	CHECK(bytes.find("E\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e") != std::string::npos);
}

TEST_CASE("SaveTo: 保存先が空なら失敗する")
{
	LogBuffer buf;
	UnicodeString err;
	CHECK_FALSE(log_win::SaveTo(_T(""), buf.Lines(), err));
	CHECK_FALSE(err.IsEmpty());
}

//===========================================================================
// FormatFileInfo (LogFileInfo)
//===========================================================================

TEST_CASE("FormatFileInfo: 存在するファイルはヘッダ+詳細行を返す")
{
	TempDir dir;
	UnicodeString path = dir.file(_T("sample.txt"));
	mkfile(path, "hello");

	std::vector<UnicodeString> lines = log_win::FormatFileInfo(path);
	REQUIRE(lines.size() >= 2);
	CHECK(StartsStr(_T("  FLINFO "), lines[0]));
	CHECK(ContainsStr(lines[0], _T("sample.txt")));

	bool found_size = false;
	for (std::size_t i = 1; i < lines.size(); i++) if (ContainsStr(lines[i], _T("サイズ"))) found_size = true;
	CHECK(found_size);
}

TEST_CASE("FormatFileInfo: 存在しないファイルはエラー文1行だけを返す")
{
	TempDir dir;
	std::vector<UnicodeString> lines = log_win::FormatFileInfo(dir.file(_T("nothing.txt")));
	REQUIRE(lines.size() == 1);
	CHECK(ContainsStr(lines[0], _T("見つかりません")));
}

//===========================================================================
// FormatAboutLines (ListNyanFi 簡略版)
//===========================================================================

TEST_CASE("FormatAboutLines: 実行パス等、何行か返る")
{
	std::vector<UnicodeString> lines = log_win::FormatAboutLines();
	CHECK(lines.size() >= 4);

	bool found_path = false;
	for (std::size_t i = 0; i < lines.size(); i++) if (StartsStr(_T("実行パス: "), lines[i])) found_path = true;
	CHECK(found_path);
}
