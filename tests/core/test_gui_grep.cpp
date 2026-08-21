/**
 * @file tests/core/test_gui_grep.cpp
 * @brief gui/grep.cpp (ファイル内容検索) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。文字コード判定・バイナリ判定は自前実装せず
 * gui/text_viewer_core.h の LoadForView (get_MemoryCodePage 経由) をそのまま
 * 使っていることを、実際に UTF-8/CP932/UTF-16LE/バイナリのファイルを
 * 一時ディレクトリに書いて確認する (tests/temp_dir.h の TempDir)。
 */
#include "doctest/doctest.h"

#include <algorithm>
#include <memory>

#include "gui/grep.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;
using grep_core::GrepLimits;
using grep_core::GrepMatch;
using grep_core::GrepOptions;
using grep_core::GrepResult;

namespace {

/// 生バイト列をそのままファイルへ書く (UTF-16 等、途中に 0x00 を含む内容のため
/// strlen に頼れない。tests/core/test_gui_text_viewer.cpp と同じ手法)
void write_bytes(const UnicodeString &fnam, const unsigned char *data, int len)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	fs->WriteBuffer(data, len);
}

/// マッチ結果に (file, line) が含まれるか
bool contains_match(const std::vector<GrepMatch> &matches, const UnicodeString &file, int line)
{
	for (std::size_t i = 0; i < matches.size(); ++i) {
		if (SameStr(matches[i].file, file) && matches[i].line == line) return true;
	}
	return false;
}

}  // namespace

//===========================================================================
// 文字コード: UTF-8 / CP932 / UTF-16LE / バイナリ
//===========================================================================
TEST_CASE("SearchDirectory: UTF-8 (BOM無し) の中を検索できる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("utf8.txt"));
	const unsigned char bytes[] = {
		'h', 'e', 'l', 'l', 'o', '\r', '\n',
		0xe6, 0x97, 0xa5, 0xe6, 0x9c, 0xac, '\r', '\n',  //日本 (UTF-8)
		'w', 'o', 'r', 'l', 'd',
	};
	write_bytes(fnam, bytes, sizeof(bytes));

	GrepOptions opt;
	opt.keyword = _T("日本");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	CHECK(r.error.IsEmpty());
	CHECK(r.files_scanned == 1);
	CHECK(r.files_skipped_binary == 0);
	REQUIRE(r.matches.size() == 1);
	CHECK(contains_match(r.matches, fnam, 2));
	CHECK(r.matches[0].text == _T("日本"));
}

TEST_CASE("SearchDirectory: CP932 (Shift_JIS) の中を検索できる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("sjis.txt"));
	// "日本語" の Shift_JIS(CP932) バイト列: 日=93FA 本=967B 語=8CEA
	const unsigned char bytes[] = {0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA};
	write_bytes(fnam, bytes, sizeof(bytes));

	GrepOptions opt;
	opt.keyword = _T("本語");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	REQUIRE(r.matches.size() == 1);
	CHECK(r.matches[0].line == 1);
	CHECK(r.matches[0].text == _T("日本語"));
}

TEST_CASE("SearchDirectory: UTF-16LE (BOM付き) の中を検索できる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("utf16le.txt"));
	// BOM(FF FE) + "AB\r\nCD" (UTF-16LE)
	const unsigned char bytes[] = {
		0xFF, 0xFE, 'A', 0x00, 'B', 0x00, '\r', 0x00, '\n', 0x00, 'C', 0x00, 'D', 0x00,
	};
	write_bytes(fnam, bytes, sizeof(bytes));

	GrepOptions opt;
	opt.keyword = _T("CD");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	REQUIRE(r.matches.size() == 1);
	CHECK(r.matches[0].line == 2);
}

TEST_CASE("SearchDirectory: バイナリファイル (0x00 の連続) はスキップし件数に数える")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("binary.dat"));
	const unsigned char bytes[] = {'A', 'B', 'C', 0x00, 0x00, 0x00, 0x00, 0x00, 'D', 'E', 'F'};
	write_bytes(fnam, bytes, sizeof(bytes));

	GrepOptions opt;
	opt.keyword = _T("ABC");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	CHECK(r.matches.empty());
	CHECK(r.files_skipped_binary == 1);
	CHECK(r.files_scanned == 1);  // スキップした分もスキャン対象数には数える
}

//===========================================================================
// 大小文字・正規表現
//===========================================================================
TEST_CASE("SearchDirectory: 既定 (大小文字無視) では大文字小文字を区別しない")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("case.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("Hello World\n");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("hello");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);
	CHECK(r.matches.size() == 1);
}

TEST_CASE("SearchDirectory: 大小文字を区別すると一致しない")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("case2.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("Hello World\n");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("hello");
	opt.case_sensitive = true;
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);
	CHECK(r.matches.empty());
}

TEST_CASE("SearchDirectory: 正規表現で検索できる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("regex.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("foo123\nbar\nfoo456\n");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("foo\\d+");
	opt.use_regex = true;
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	REQUIRE(r.matches.size() == 2);
	CHECK(r.matches[0].line == 1);
	CHECK(r.matches[1].line == 3);
}

TEST_CASE("SearchDirectory: 不正な正規表現は error を設定して即座に終わる")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("dummy.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("dummy\n");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("(unclosed");
	opt.use_regex = true;
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	CHECK(!r.error.IsEmpty());
	CHECK(r.matches.empty());
	CHECK(r.files_scanned == 0);  // 走査を始める前に打ち切る
}

//===========================================================================
// ファイル名マスク・再帰
//===========================================================================
TEST_CASE("SearchDirectory: ファイル名マスクで対象を絞れる")
{
	TempDir dir;
	std::unique_ptr<TFileStream> fs1(new TFileStream(dir.file(_T("a.cpp")), fmCreate));
	const AnsiString a("keyword\n");
	fs1->WriteBuffer(a.c_str(), a.Length());
	fs1.reset();

	std::unique_ptr<TFileStream> fs2(new TFileStream(dir.file(_T("b.txt")), fmCreate));
	fs2->WriteBuffer(a.c_str(), a.Length());
	fs2.reset();

	GrepOptions opt;
	opt.keyword = _T("keyword");
	opt.mask = _T("*.cpp");
	GrepResult r = grep_core::SearchDirectory(dir.path, opt);

	REQUIRE(r.matches.size() == 1);
	CHECK(r.matches[0].file == dir.file(_T("a.cpp")));
	CHECK(r.files_scanned == 1);  // b.txt はマスクで対象外 (スキャンにも数えない)
}

TEST_CASE("SearchDirectory: 既定 (非再帰) はサブディレクトリを見ない")
{
	TempDir dir;
	::CreateDirectoryW((dir.path + _T("sub")).c_str(), NULL);

	std::unique_ptr<TFileStream> fs1(new TFileStream(dir.file(_T("top.txt")), fmCreate));
	const AnsiString a("keyword\n");
	fs1->WriteBuffer(a.c_str(), a.Length());
	fs1.reset();

	std::unique_ptr<TFileStream> fs2(new TFileStream(dir.path + _T("sub\\") + _T("nested.txt"), fmCreate));
	fs2->WriteBuffer(a.c_str(), a.Length());
	fs2.reset();

	GrepOptions opt;
	opt.keyword = _T("keyword");

	GrepResult r_flat = grep_core::SearchDirectory(dir.path, opt);
	CHECK(r_flat.matches.size() == 1);
	CHECK(r_flat.matches[0].file == dir.file(_T("top.txt")));

	opt.recursive = true;
	GrepResult r_rec = grep_core::SearchDirectory(dir.path, opt);
	CHECK(r_rec.matches.size() == 2);
}

//===========================================================================
// 上限 (max_files / max_matches / max_file_bytes) と中断
//===========================================================================
TEST_CASE("SearchDirectory: max_files を超えると打ち切り、以降のファイルは対象にしない")
{
	TempDir dir;
	const AnsiString a("keyword\n");
	for (int i = 0; i < 5; ++i) {
		UnicodeString fnam;
		fnam.sprintf(_T("f%d.txt"), i);
		std::unique_ptr<TFileStream> fs(new TFileStream(dir.file(fnam), fmCreate));
		fs->WriteBuffer(a.c_str(), a.Length());
	}

	GrepOptions opt;
	opt.keyword = _T("keyword");
	GrepLimits limits;
	limits.max_files = 2;

	GrepResult r = grep_core::SearchDirectory(dir.path, opt, limits);
	CHECK(r.stopped_by_file_limit);
	CHECK(r.files_scanned == 2);
	CHECK(r.matches.size() == 2);
}

TEST_CASE("SearchDirectory: max_matches を超えると打ち切る")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("many.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	AnsiString content;
	for (int i = 0; i < 10; ++i) content = content + AnsiString("keyword\n");
	fs->WriteBuffer(content.c_str(), content.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("keyword");
	GrepLimits limits;
	limits.max_matches = 3;

	GrepResult r = grep_core::SearchDirectory(dir.path, opt, limits);
	CHECK(r.stopped_by_match_limit);
	CHECK(r.matches.size() == 3);
}

TEST_CASE("SearchDirectory: max_file_bytes を超えるファイルは先頭だけ検索し truncated を数える")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("big.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	AnsiString line1("keyword_at_top\n");
	AnsiString filler;
	for (int i = 0; i < 200; ++i) filler = filler + AnsiString("0123456789");
	AnsiString content = line1 + filler + AnsiString("\nkeyword_at_bottom\n");
	fs->WriteBuffer(content.c_str(), content.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("keyword");
	GrepLimits limits;
	limits.max_file_bytes = 20;  // 先頭行だけが対象になる程度の小さい上限

	GrepResult r = grep_core::SearchDirectory(dir.path, opt, limits);
	CHECK(r.files_truncated == 1);
	REQUIRE(r.matches.size() == 1);
	CHECK(r.matches[0].text == _T("keyword_at_top"));
}

TEST_CASE("SearchDirectory: cancel_cb が true を返すと中断する")
{
	TempDir dir;
	const AnsiString a("keyword\n");
	for (int i = 0; i < 5; ++i) {
		UnicodeString fnam;
		fnam.sprintf(_T("c%d.txt"), i);
		std::unique_ptr<TFileStream> fs(new TFileStream(dir.file(fnam), fmCreate));
		fs->WriteBuffer(a.c_str(), a.Length());
	}

	GrepOptions opt;
	opt.keyword = _T("keyword");

	int calls = 0;
	grep_core::GrepCancelCallback cancel_cb = [&calls]() {
		++calls;
		return calls > 1;  // 2回目の呼び出しで中断
	};

	GrepResult r = grep_core::SearchDirectory(dir.path, opt, GrepLimits(), cancel_cb);
	CHECK(r.cancelled);
	CHECK(r.files_scanned < 5);
}

//===========================================================================
// SearchFile (単体呼び出し)
//===========================================================================
TEST_CASE("SearchFile: is_binary/truncated を正しく報告する")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("plain.txt"));
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	const AnsiString ansi("abc\n");
	fs->WriteBuffer(ansi.c_str(), ansi.Length());
	fs.reset();

	GrepOptions opt;
	opt.keyword = _T("abc");
	bool is_binary = true;
	bool truncated = true;
	std::vector<GrepMatch> matches =
		grep_core::SearchFile(fnam, opt, text_viewer_core::kMaxViewBytes, is_binary, truncated);

	CHECK(!is_binary);
	CHECK(!truncated);
	REQUIRE(matches.size() == 1);
}
