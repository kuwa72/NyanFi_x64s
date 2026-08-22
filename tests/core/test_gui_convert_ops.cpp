/**
 * @file tests/core/test_gui_convert_ops.cpp
 * @brief gui/convert_ops.cpp のテスト
 *
 * 中身の処理は移植済みのもの (usr_exif / usr_id3 / usr_xd2tx / htmconv /
 * usr_wic) に任せているので、ここで見るのは
 *   - 出力名の決め方 (純関数)
 *   - 対象の選び方と件数の数え方
 *   - **元を壊さないこと** (出力先が元と同じなら断る、既存を上書きしない)
 * の3つ。
 *
 * **この .cpp が存在すること自体に意味がある。** 静的ライブラリのメンバは
 * 参照されて初めて取り出されるので、呼ぶコードが1つも無いと
 * 「ビルドは通るがリンクできない」に気づけない (報告書 §24)。
 * ここから実際に呼ぶことで、リンクできることを CI が毎回確かめる。
 */
#include "doctest/doctest.h"

#include "gui/convert_ops.h"
#include "temp_dir.h"
#include "usr_file_ex.h"

using nyanfi_test::TempDir;

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
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return std::string();
	std::string out;
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

}  // namespace

//===========================================================================
// 出力名の決め方
//===========================================================================

TEST_CASE("OutputPath: 名前主部 + 新しい拡張子を出力先に置く")
{
	CHECK(convert_ops::OutputPath(_T("C:\\src\\doc.docx"), _T("D:\\out"), _T(".txt"))
	      == UnicodeString(_T("D:\\out\\doc.txt")));
}

TEST_CASE("OutputPath: 拡張子が空なら元の名前のまま")
{
	CHECK(convert_ops::OutputPath(_T("C:\\src\\photo.jpg"), _T("D:\\out"), EmptyStr)
	      == UnicodeString(_T("D:\\out\\photo.jpg")));
}

TEST_CASE("OutputPath: 出力先の末尾区切りは有っても無くてもよい")
{
	CHECK(convert_ops::OutputPath(_T("C:\\a.htm"), _T("D:\\out\\"), _T(".md"))
	      == UnicodeString(_T("D:\\out\\a.md")));
}

TEST_CASE("OutputPath: 名前にドットが複数あっても最後だけを拡張子とみなす")
{
	CHECK(convert_ops::OutputPath(_T("C:\\src\\my.report.docx"), _T("D:\\out"), _T(".txt"))
	      == UnicodeString(_T("D:\\out\\my.report.txt")));
}

TEST_CASE("IndexedOutputPath: 連番は3桁固定 (VCL の %03u と同じ)")
{
	CHECK(convert_ops::IndexedOutputPath(_T("C:\\bin\\app.exe"), _T("D:\\out"), 0, _T(".ico"))
	      == UnicodeString(_T("D:\\out\\app_000.ico")));
	CHECK(convert_ops::IndexedOutputPath(_T("C:\\bin\\app.exe"), _T("D:\\out"), 12, _T(".ico"))
	      == UnicodeString(_T("D:\\out\\app_012.ico")));
	// 3桁を超えたら詰めずにそのまま伸びる
	CHECK(convert_ops::IndexedOutputPath(_T("C:\\bin\\app.exe"), _T("D:\\out"), 1234, _T(".ico"))
	      == UnicodeString(_T("D:\\out\\app_1234.ico")));
}

//===========================================================================
// 対象の選び方と件数
//
// 中身の変換そのものは外部の DLL や画像コーデックに依存するので、
// **対象でないものを飛ばすこと**と**元を壊さないこと**を見る
//===========================================================================

TEST_CASE("SetExifTime: Exif を持てない拡張子は飛ばす")
{
	TempDir tmp;
	const UnicodeString txt = tmp.path + _T("note.txt");
	mkfile(txt, "not a photo");

	std::vector<UnicodeString> paths;
	paths.push_back(txt);
	const file_ops::FileOpResult r = convert_ops::SetExifTime(paths);

	CHECK(r.success_count == 0);
	CHECK(r.skipped_existing == 1);
	CHECK(r.failures.empty());
}

TEST_CASE("SetExifTime: ディレクトリは飛ばす")
{
	TempDir tmp;
	const UnicodeString dir = tmp.path + _T("sub");
	::CreateDirectoryW(dir.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(dir);
	CHECK(convert_ops::SetExifTime(paths).skipped_existing == 1);
}

TEST_CASE("DeleteJpegExif: Jpeg でないものは飛ばす")
{
	TempDir tmp;
	const UnicodeString txt = tmp.path + _T("note.txt");
	mkfile(txt);
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(txt);
	const file_ops::FileOpResult r = convert_ops::DeleteJpegExif(paths, out, true);
	CHECK(r.skipped_existing == 1);
	CHECK(r.success_count == 0);
}

TEST_CASE("DeleteJpegExif: 出力先が元と同じなら断る (元を壊さない)")
{
	TempDir tmp;
	const UnicodeString jpg = tmp.path + _T("photo.jpg");
	mkfile(jpg, "not really a jpeg");

	std::vector<UnicodeString> paths;
	paths.push_back(jpg);
	// 出力先 = 元のディレクトリ
	const file_ops::FileOpResult r = convert_ops::DeleteJpegExif(paths, tmp.path, true);

	CHECK(r.success_count == 0);
	REQUIRE(r.failures.size() == 1);
	// **元のファイルが残っていること**
	CHECK(read_all(jpg) == "not really a jpeg");
}

TEST_CASE("DeleteJpegExif: 出力先に同名があれば上書きしない")
{
	TempDir tmp;
	const UnicodeString jpg = tmp.path + _T("photo.jpg");
	mkfile(jpg, "src");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);
	mkfile(out + _T("\\photo.jpg"), "KEEP");

	std::vector<UnicodeString> paths;
	paths.push_back(jpg);
	const file_ops::FileOpResult r = convert_ops::DeleteJpegExif(paths, out, true);

	CHECK(r.skipped_existing == 1);
	CHECK(read_all(out + _T("\\photo.jpg")) == "KEEP");
}

TEST_CASE("ExtractEmbeddedImages: MP3/FLAC でないものは飛ばす")
{
	TempDir tmp;
	const UnicodeString txt = tmp.path + _T("note.txt");
	mkfile(txt);
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(txt);
	const file_ops::FileOpResult r = convert_ops::ExtractEmbeddedImages(paths, out);
	CHECK(r.skipped_existing == 1);
	CHECK(r.success_count == 0);
}

TEST_CASE("ExtractEmbeddedImages: 画像を持たない MP3 は失敗ではなくスキップ")
{
	TempDir tmp;
	// ID3 タグを持たない中身。ID3_GetImage は false を返すはず
	const UnicodeString mp3 = tmp.path + _T("silent.mp3");
	mkfile(mp3, "\xFF\xFB\x90\x00 not a real frame");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(mp3);
	const file_ops::FileOpResult r = convert_ops::ExtractEmbeddedImages(paths, out);
	CHECK(r.success_count == 0);
	CHECK(r.skipped_existing == 1);
	CHECK(r.failures.empty());
}

TEST_CASE("ConvertHtmlToText: HTML でないものは飛ばす")
{
	TempDir tmp;
	const UnicodeString txt = tmp.path + _T("note.txt");
	mkfile(txt, "plain");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(txt);
	const file_ops::FileOpResult r = convert_ops::ConvertHtmlToText(paths, out, false);
	CHECK(r.skipped_existing == 1);
	CHECK(r.success_count == 0);
}

TEST_CASE("ConvertHtmlToText: HTML をテキストにして出力先に置く")
{
	TempDir tmp;
	const UnicodeString htm = tmp.path + _T("page.html");
	mkfile(htm, "<html><body><p>hello world</p></body></html>");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(htm);
	const file_ops::FileOpResult r = convert_ops::ConvertHtmlToText(paths, out, false);

	CHECK(r.success_count == 1);
	CHECK(file_exists(out + _T("\\page.txt")));
	// タグが落ちて本文が残っていること
	const std::string body = read_all(out + _T("\\page.txt"));
	CHECK(body.find("hello world") != std::string::npos);
	CHECK(body.find("<p>") == std::string::npos);
}

TEST_CASE("ConvertHtmlToText: Markdown 指定なら .md になる")
{
	TempDir tmp;
	const UnicodeString htm = tmp.path + _T("page.html");
	mkfile(htm, "<html><body><h1>title</h1></body></html>");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(htm);
	const file_ops::FileOpResult r = convert_ops::ConvertHtmlToText(paths, out, true);

	CHECK(r.success_count == 1);
	CHECK(file_exists(out + _T("\\page.md")));
}

TEST_CASE("ConvertHtmlToText: 出力先に同名があれば上書きしない")
{
	TempDir tmp;
	const UnicodeString htm = tmp.path + _T("page.html");
	mkfile(htm, "<html><body>x</body></html>");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);
	mkfile(out + _T("\\page.txt"), "KEEP");

	std::vector<UnicodeString> paths;
	paths.push_back(htm);
	const file_ops::FileOpResult r = convert_ops::ConvertHtmlToText(paths, out, false);

	CHECK(r.skipped_existing == 1);
	CHECK(read_all(out + _T("\\page.txt")) == "KEEP");
}

TEST_CASE("ConvertDocToText: xdoc2txt が無ければ1件も触らず理由を返す")
{
	TempDir tmp;
	const UnicodeString doc = tmp.path + _T("a.docx");
	mkfile(doc);
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(doc);
	UnicodeString error;
	const file_ops::FileOpResult r = convert_ops::ConvertDocToText(paths, out, 932, error);

	// DLL がある環境では変換を試み、無ければ error が入る。
	// **どちらでも「黙って0件」にはならない**ことを見る
	if (r.success_count == 0 && r.skipped_existing == 0 && r.failures.empty()) {
		CHECK(!error.IsEmpty());
	}
	// 出力先の中身を壊していないこと
	CHECK(dir_exists(out));
}

TEST_CASE("ConvertImages: 変換先が元と同じなら断る (元を壊さない)")
{
	TempDir tmp;
	const UnicodeString png = tmp.path + _T("a.png");
	mkfile(png, "not really a png");

	std::vector<UnicodeString> paths;
	paths.push_back(png);
	// 同じディレクトリに同じ拡張子で出そうとする
	const file_ops::FileOpResult r = convert_ops::ConvertImages(paths, tmp.path, _T(".png"), 100);

	CHECK(r.success_count == 0);
	REQUIRE(r.failures.size() == 1);
	CHECK(read_all(png) == "not really a png");
}

TEST_CASE("ConvertImages: 画像として読めなければ失敗として報告する")
{
	TempDir tmp;
	const UnicodeString bad = tmp.path + _T("broken.png");
	mkfile(bad, "definitely not an image");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(bad);
	const file_ops::FileOpResult r = convert_ops::ConvertImages(paths, out, _T(".bmp"), 100);

	CHECK(r.success_count == 0);
	CHECK(r.failures.size() == 1);
	// **書きかけを残さない**
	CHECK(file_exists(out + _T("\\broken.bmp")) == false);
}

TEST_CASE("ConvertImages: ディレクトリは飛ばす")
{
	TempDir tmp;
	const UnicodeString dir = tmp.path + _T("sub");
	::CreateDirectoryW(dir.c_str(), NULL);
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(dir);
	CHECK(convert_ops::ConvertImages(paths, out, _T(".png"), 100).skipped_existing == 1);
}

TEST_CASE("ExtractIcons: アイコンを持たないファイルは飛ばす")
{
	TempDir tmp;
	const UnicodeString txt = tmp.path + _T("note.txt");
	mkfile(txt, "no icons here");
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(txt);
	const file_ops::FileOpResult r = convert_ops::ExtractIcons(paths, out, -1);
	CHECK(r.success_count == 0);
	CHECK(r.failures.empty());
}

namespace {

/// アイコンを確実に持っているファイル。無ければテストを飛ばす
/// (テスト自身の実行ファイルはアイコンリソースを持たない)
UnicodeString icon_source()
{
	wchar_t sysdir[MAX_PATH] = {};
	if (::GetSystemDirectoryW(sysdir, MAX_PATH) == 0) return EmptyStr;
	const UnicodeString path = IncludeTrailingPathDelimiter(UnicodeString(sysdir)) + _T("shell32.dll");
	return file_exists(path)? path : EmptyStr;
}

}  // namespace

TEST_CASE("ExtractIcons: 実ファイルからアイコンを .ico として取り出せる")
{
	const UnicodeString src = icon_source();
	if (src.IsEmpty()) {
		MESSAGE("shell32.dll が見つからないため未検証");
		return;
	}

	TempDir tmp;
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(src);
	const file_ops::FileOpResult r = convert_ops::ExtractIcons(paths, out, 0);

	REQUIRE(r.success_count == 1);

	const UnicodeString ico = convert_ops::IndexedOutputPath(src, out, 0, _T(".ico"));
	REQUIRE(file_exists(ico));
	const std::string bytes = read_all(ico);
	REQUIRE(bytes.size() > 22);

	// ICONDIR: reserved=0, type=1 (icon), count=1
	CHECK(static_cast<unsigned char>(bytes[0]) == 0);
	CHECK(static_cast<unsigned char>(bytes[1]) == 0);
	CHECK(static_cast<unsigned char>(bytes[2]) == 1);
	CHECK(static_cast<unsigned char>(bytes[3]) == 0);
	CHECK(static_cast<unsigned char>(bytes[4]) == 1);
	CHECK(static_cast<unsigned char>(bytes[5]) == 0);

	// ICONDIRENTRY の bitCount は 32、データ位置は 22
	CHECK(static_cast<unsigned char>(bytes[12]) == 32);
	CHECK(static_cast<unsigned char>(bytes[18]) == 22);

	// 続く BITMAPINFOHEADER の biHeight は「カラー面 + マスク面」で高さの2倍
	const unsigned char w = static_cast<unsigned char>(bytes[6]);
	const unsigned char h = static_cast<unsigned char>(bytes[7]);
	if (w != 0 && h != 0) {
		const int dib_height = static_cast<unsigned char>(bytes[22 + 8])
		                     | (static_cast<unsigned char>(bytes[22 + 9]) << 8);
		CHECK(dib_height == h * 2);
	}
}

TEST_CASE("ExtractIcons: 既にあるファイルは上書きしない")
{
	const UnicodeString src = icon_source();
	if (src.IsEmpty()) { MESSAGE("shell32.dll が見つからないため未検証"); return; }

	TempDir tmp;
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);
	mkfile(convert_ops::IndexedOutputPath(src, out, 0, _T(".ico")), "KEEP");

	std::vector<UnicodeString> paths;
	paths.push_back(src);
	const file_ops::FileOpResult r = convert_ops::ExtractIcons(paths, out, 0);

	CHECK(r.skipped_existing == 1);
	CHECK(r.success_count == 0);
	CHECK(read_all(convert_ops::IndexedOutputPath(src, out, 0, _T(".ico"))) == "KEEP");
}

TEST_CASE("ExtractIcons: 範囲外の番号を指定したら理由を返す")
{
	const UnicodeString src = icon_source();
	if (src.IsEmpty()) { MESSAGE("shell32.dll が見つからないため未検証"); return; }

	TempDir tmp;
	const UnicodeString out = tmp.path + _T("out");
	::CreateDirectoryW(out.c_str(), NULL);

	std::vector<UnicodeString> paths;
	paths.push_back(src);
	const file_ops::FileOpResult r = convert_ops::ExtractIcons(paths, out, 9999);

	CHECK(r.success_count == 0);
	REQUIRE(r.failures.size() == 1);
}
