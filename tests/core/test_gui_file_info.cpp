/**
 * @file tests/core/test_gui_file_info.cpp
 * @brief gui/file_info.cpp (ファイル情報の組み立て) の回帰テスト
 *
 * @details gui/file_info.h/.cpp は wx に依存しない (nyanfi_gui_core、
 * ルート CMakeLists.txt 参照) ため、GUI (wxWidgets) 無しでもここで
 * テストできる。BuildFileInfoLines が呼ぶ src/usr_file_inf.cpp の解析関数
 * (get_PdfVer 等) はテストファイルを一時ディレクトリに作って動作確認する。
 *
 * ファイルは TFileStream で生バイトを書く (tests/core/test_usr_file_ex.cpp
 * と同じ考え方)。TStringList::SaveToFile は BOM や行末の付与でバイト列が
 * 変わるため、ハッシュ値や "%PDF-" ヘッダのようにバイト単位で意味を持つ
 * テストには使わない。
 *
 * ファイルシステムに触れるテストは tests/temp_dir.h の TempDir が作る
 * 一時ディレクトリの中だけで行う。
 *
 * 実際に ShellExecuteExW / SHOpenWithDialog を呼ぶ gui/file_open.cpp は
 * ここではテストしない (テスト実行環境で実際にプログラムが起動してしまう
 * ため。gui/file_open.h のコメントと報告を参照)。
 */
#include "doctest/doctest.h"

#include <cstring>
#include <memory>

#include "gui/file_info.h"
#include "usr_file_ex.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

/// 指定バイト列でファイルを作る
UnicodeString write_bytes_file(const TempDir &dir, const UnicodeString &name, const char *data, int len)
{
	const UnicodeString path = dir.file(name);
	std::unique_ptr<TFileStream> fs(new TFileStream(path, fmCreate));
	if (len > 0) fs->WriteBuffer(data, len);
	return path;
}

FileItem make_item(const UnicodeString &name, Int64 size, bool is_dir = false)
{
	FileItem itm;
	itm.name = name;
	itm.size = size;
	itm.is_dir = is_dir;
	return itm;
}

}  // namespace

//===========================================================================
// BuildFileInfoLines: 基本情報
//===========================================================================
TEST_CASE("BuildFileInfoLines: ファイルの基本情報 (名前・パス・種類・サイズ) を含む")
{
	TempDir dir;
	const UnicodeString path = write_bytes_file(dir, "hello.txt", "hello", 5);

	FileItem itm = make_item("hello.txt", 5);
	std::unique_ptr<TStringList> lst(new TStringList());
	BuildFileInfoLines(path, itm, lst.get());

	const UnicodeString text = lst->Text;
	CHECK(ContainsStr(text, "名前: hello.txt"));
	CHECK(ContainsStr(text, "パス: " + path));
	CHECK(ContainsStr(text, "種類: ファイル"));
	CHECK(ContainsStr(text, "サイズ:"));
}

TEST_CASE("BuildFileInfoLines: ディレクトリはサイズ行と種別ごとの詳細を含まない")
{
	TempDir dir;
	FileItem itm = make_item("some_dir", -1, true);
	std::unique_ptr<TStringList> lst(new TStringList());
	// ディレクトリ自体は存在しなくてもよい (種別ごとの詳細を呼ばないため、
	// 存在確認が絡む解析関数を通らない)
	BuildFileInfoLines(dir.file("some_dir"), itm, lst.get());

	const UnicodeString text = lst->Text;
	CHECK(ContainsStr(text, "種類: ディレクトリ"));
	CHECK(!ContainsStr(text, "サイズ:"));
}

//===========================================================================
// BuildFileInfoLines: 拡張子別の詳細情報 (簡易ケースのみ)
//===========================================================================
TEST_CASE("BuildFileInfoLines: PDFのバージョンをヘッダから読み取る (get_PdfVer)")
{
	TempDir dir;
	const char bytes[] = "%PDF-1.4\n";
	const UnicodeString path = write_bytes_file(dir, "sample.pdf", bytes, static_cast<int>(strlen(bytes)));

	FileItem itm = make_item("sample.pdf", static_cast<Int64>(strlen(bytes)));
	std::unique_ptr<TStringList> lst(new TStringList());
	BuildFileInfoLines(path, itm, lst.get());

	CHECK(ContainsStr(lst->Text, "PDFバージョン: 1.4"));
}

TEST_CASE("BuildFileInfoLines: 実行可能ファイルは確認用の1行だけ追加する (get_AppInf は GUI 依存で未使用)")
{
	TempDir dir;
	// get_AppInf は usr_SH (未移植) に依存するため呼ばない。test_ExeExt だけで
	// 判定し、内容の妥当性 (PEヘッダ等) は問わないので中身は空でよい
	const UnicodeString path = write_bytes_file(dir, "tool.exe", "", 0);

	FileItem itm = make_item("tool.exe", 0);
	std::unique_ptr<TStringList> lst(new TStringList());
	BuildFileInfoLines(path, itm, lst.get());

	CHECK(ContainsStr(lst->Text, "実行可能ファイルです"));
}

TEST_CASE("BuildFileInfoLines: 未知の拡張子は基本情報のみで、解析関数を呼ばずに例外も起きない")
{
	TempDir dir;
	const UnicodeString path = write_bytes_file(dir, "data.unknown_ext", "abc", 3);

	FileItem itm = make_item("data.unknown_ext", 3);
	std::unique_ptr<TStringList> lst(new TStringList());
	BuildFileInfoLines(path, itm, lst.get());

	CHECK(ContainsStr(lst->Text, "名前: data.unknown_ext"));
}

//===========================================================================
// AppendHashLines: CRC32 / SHA256 (既知の内容と照合)
//===========================================================================
TEST_CASE("AppendHashLines: \"abc\" のCRC32/SHA256が既知の値と一致する")
{
	TempDir dir;
	const UnicodeString path = write_bytes_file(dir, "abc.bin", "abc", 3);

	std::unique_ptr<TStringList> lst(new TStringList());
	AppendHashLines(path, lst.get());

	const UnicodeString text = lst->Text;
	// get_CRC32_str / get_HashStr はどちらも小文字16進 ("%02x"/"%08x") で返す
	CHECK(ContainsStr(text, "CRC32: 352441c2"));
	CHECK(ContainsStr(text, "SHA256: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}
