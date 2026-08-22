/**
 * @file tests/core/test_gui_system_ops.cpp
 * @brief gui/system_ops.cpp のテスト
 *
 * @details 実際に起動・実行はしない (ごみ箱・ロック・モニタ電源・ミュート・
 *          ドライブの取り外し・CD トレイは呼び出し元だけをテストしても安全に
 *          確認できないため対象外。gui/system_ops.h のコメント参照)。
 *
 *          代替データストリームの列挙・削除は NTFS 上でしか動かないので、
 *          実行環境で対応していなければ MESSAGE を残してスキップする
 *          (tests/core/test_usr_file_ex.cpp が delete_ADS 系を対象外にした
 *          ときと同じ考え方だが、こちらはガード付きで実際に検証する)。
 */
#include "doctest/doctest.h"

#include <string>

#include "gui/system_ops.h"
#include "tests/temp_dir.h"

using nyanfi_test::TempDir;

//===========================================================================
// UrlEncode
//===========================================================================
TEST_CASE("UrlEncode: 英数字とハイフン等はそのまま")
{
	CHECK(system_ops::UrlEncode(_T("abcXYZ019-._~")) == UnicodeString(_T("abcXYZ019-._~")));
}

TEST_CASE("UrlEncode: 空白やクエリを壊す記号はエンコードされる")
{
	CHECK(system_ops::UrlEncode(_T("a b")) == UnicodeString(_T("a%20b")));
	CHECK(system_ops::UrlEncode(_T("a&b=c")) == UnicodeString(_T("a%26b%3Dc")));
}

TEST_CASE("UrlEncode: 日本語はUTF-8のpercent-encodeになる")
{
	// "あ" = U+3042 = UTF-8で E3 81 82
	CHECK(system_ops::UrlEncode(_T("あ")) == UnicodeString(_T("%E3%81%82")));
}

//===========================================================================
// BuildSearchUrl
//===========================================================================
TEST_CASE("BuildSearchUrl: \\S をエンコードしたキーワードで置換する")
{
	// 既定値相当 (Global.cpp:1513)
	const UnicodeString tmpl = _T("https://www.google.co.jp/search?q=\\S&ie=UTF-8");
	const auto url = system_ops::BuildSearchUrl(tmpl, _T("a b"));
	CHECK(url == UnicodeString(_T("https://www.google.co.jp/search?q=a%20b&ie=UTF-8")));
}

TEST_CASE("BuildSearchUrl: キーワードが空なら空文字列を返す (VCLはShellExecuteしない)")
{
	CHECK(system_ops::BuildSearchUrl(_T("https://example.com/?q=\\S"), _T("")).IsEmpty());
}

TEST_CASE("BuildSearchUrl: & や = を含むキーワードでクエリを壊さない")
{
	const auto url = system_ops::BuildSearchUrl(_T("https://example.com/?q=\\S"), _T("a&b=c"));
	CHECK(url == UnicodeString(_T("https://example.com/?q=a%26b%3Dc")));
}

TEST_CASE("BuildSearchUrl: %s ではなく \\S だけがプレースホルダ")
{
	// %s をそのまま残すことを確認 (VCLの記法が \S であることの裏付け)
	const auto url = system_ops::BuildSearchUrl(_T("https://example.com/?q=%s&x=\\S"), _T("k"));
	CHECK(url == UnicodeString(_T("https://example.com/?q=%s&x=k")));
}

//===========================================================================
// BuildMapUrl
//===========================================================================
TEST_CASE("BuildMapUrl: 緯度経度は小数点以下8桁で埋め込む")
{
	const auto s = system_ops::BuildMapUrl(_T("[$Latitude$, $Longitude$]"), 35.6812, 139.7671);
	CHECK(s == UnicodeString(_T("[35.68120000, 139.76710000]")));
}

TEST_CASE("BuildMapUrl: ズームは既定16")
{
	const auto s = system_ops::BuildMapUrl(_T("zoom=$Zoom$"), 0.0, 0.0);
	CHECK(s == UnicodeString(_T("zoom=16")));
}

TEST_CASE("BuildMapUrl: ズームは1〜18に丸める")
{
	CHECK(system_ops::BuildMapUrl(_T("z=$Zoom$"), 0.0, 0.0, 0) == UnicodeString(_T("z=1")));
	CHECK(system_ops::BuildMapUrl(_T("z=$Zoom$"), 0.0, 0.0, -5) == UnicodeString(_T("z=1")));
	CHECK(system_ops::BuildMapUrl(_T("z=$Zoom$"), 0.0, 0.0, 30) == UnicodeString(_T("z=18")));
	CHECK(system_ops::BuildMapUrl(_T("z=$Zoom$"), 0.0, 0.0, 18) == UnicodeString(_T("z=18")));
}

TEST_CASE("BuildMapUrl: 負の緯度経度も扱える")
{
	const auto s = system_ops::BuildMapUrl(_T("$Latitude$,$Longitude$"), -33.8688, -151.2093);
	CHECK(s == UnicodeString(_T("-33.86880000,-151.20930000")));
}

TEST_CASE("BuildMapUrl: プレースホルダが無ければ何もしない")
{
	CHECK(system_ops::BuildMapUrl(_T("no placeholder here"), 1.0, 2.0) == UnicodeString(_T("no placeholder here")));
}

//===========================================================================
// StreamPath
//===========================================================================
TEST_CASE("StreamPath: file:name:$DATA の形を組み立てる")
{
	CHECK(system_ops::StreamPath(_T("C:\\work\\a.txt"), _T("secret"))
	      == UnicodeString(_T("C:\\work\\a.txt:secret:$DATA")));
}

//===========================================================================
// DeleteStream: ストリーム名が空のときの安全ガード
//===========================================================================
TEST_CASE("DeleteStream: ストリーム名が空なら本体を消さず失敗を返す")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("body.txt"));

	// 本体を作る
	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, "hello", 5, &written, NULL);
	::CloseHandle(h);

	UnicodeString err;
	CHECK(system_ops::DeleteStream(fnam, _T(""), err) == false);
	CHECK(!err.IsEmpty());

	// 本体が残っていること (中身も変わっていないこと)
	CHECK(::GetFileAttributesW(fnam.c_str()) != INVALID_FILE_ATTRIBUTES);
	HANDLE h2 = ::CreateFileW(fnam.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h2 != INVALID_HANDLE_VALUE);
	char buf[16] = {};
	DWORD read = 0;
	::ReadFile(h2, buf, sizeof(buf), &read, NULL);
	::CloseHandle(h2);
	CHECK(read == 5);
	CHECK(std::string(buf, read) == "hello");
}

//===========================================================================
// ListStreams / DeleteStream: 実際の ADS を使う検証 (NTFS 以外ではスキップ)
//===========================================================================
namespace {

/// テスト用に "本体:ストリーム名" へ書き込んでみて、ADS が使える環境かを兼ねて確認する
bool try_create_stream(const UnicodeString &fnam, const UnicodeString &stream, const char *data, DWORD len)
{
	const UnicodeString path = fnam + _T(":") + stream;
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	::WriteFile(h, data, len, &written, NULL);
	::CloseHandle(h);
	return true;
}

}  // namespace

TEST_CASE("ListStreams: 作成したストリームが列挙され、既定の::$DATAは含まない")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("ads_list.txt"));

	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, "body-data", 9, &written, NULL);
	::CloseHandle(h);

	if (!try_create_stream(fnam, _T("mystream"), "12345", 5)) {
		MESSAGE("skipped: this filesystem does not support alternate data streams (not NTFS?)");
		return;
	}

	const auto streams = system_ops::ListStreams(fnam);
	bool found = false;
	for (const auto &e : streams) {
		CHECK(!e.name.IsEmpty());       //既定の ::$DATA が混ざっていないこと
		if (e.name == UnicodeString(_T("mystream"))) {
			found = true;
			CHECK(e.size == 5);
		}
	}
	CHECK(found);
}

TEST_CASE("ListStreams: ストリームが無いファイルは空の一覧")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("ads_none.txt"));
	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	::CloseHandle(h);

	const auto streams = system_ops::ListStreams(fnam);
	CHECK(streams.empty());
}

TEST_CASE("ListStreams: 存在しないファイルは空の一覧 (例外を投げない)")
{
	const auto streams = system_ops::ListStreams(_T("Z:\\this\\path\\does\\not\\exist_12345.txt"));
	CHECK(streams.empty());
}

TEST_CASE("DeleteStream: ストリームだけ消えて本体は残る")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("ads_del.txt"));

	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	::WriteFile(h, "keep-me", 7, &written, NULL);
	::CloseHandle(h);

	if (!try_create_stream(fnam, _T("todelete"), "xxxxx", 5)) {
		MESSAGE("skipped: this filesystem does not support alternate data streams (not NTFS?)");
		return;
	}

	UnicodeString err;
	CHECK(system_ops::DeleteStream(fnam, _T("todelete"), err) == true);
	CHECK(err.IsEmpty());

	// ストリームは一覧から消えている
	const auto streams = system_ops::ListStreams(fnam);
	for (const auto &e : streams) CHECK(e.name != UnicodeString(_T("todelete")));

	// 本体は無事
	HANDLE h2 = ::CreateFileW(fnam.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h2 != INVALID_HANDLE_VALUE);
	char buf[16] = {};
	DWORD read = 0;
	::ReadFile(h2, buf, sizeof(buf), &read, NULL);
	::CloseHandle(h2);
	CHECK(std::string(buf, read) == "keep-me");
}

TEST_CASE("DeleteStream: 存在しないストリームの削除は失敗し理由を返す")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("ads_missing.txt"));
	HANDLE h = ::CreateFileW(fnam.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	::CloseHandle(h);

	UnicodeString err;
	CHECK(system_ops::DeleteStream(fnam, _T("nosuch"), err) == false);
	CHECK(!err.IsEmpty());
}

//===========================================================================
// IsRemovableDrive
//===========================================================================
TEST_CASE("IsRemovableDrive: 一時ディレクトリのドライブは通常リムーバブルではない")
{
	TempDir dir;
	const UnicodeString drive = ExtractFileDrive(dir.path) + _T("\\");
	// CI/開発機のシステムドライブは固定ディスクである前提 (リムーバブルメディアで
	// 動かしている環境では成立しないが、通常の CI/開発環境ではこれで十分安全に検証できる)
	CHECK(system_ops::IsRemovableDrive(drive) == false);
}

TEST_CASE("IsRemovableDrive: 末尾に\\が無くても同じ結果になる")
{
	TempDir dir;
	const UnicodeString drive = ExtractFileDrive(dir.path);
	CHECK(system_ops::IsRemovableDrive(drive) == system_ops::IsRemovableDrive(drive + _T("\\")));
}
