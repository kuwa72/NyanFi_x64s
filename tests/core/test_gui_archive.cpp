/**
 * @file tests/core/test_gui_archive.cpp
 * @brief gui/archive.cpp のテスト
 *
 * @details 書庫の読み書きそのものは**外部の書庫 DLL** (7-zip32.dll など) に
 *          依存するので、CI にも手元にも無いのが普通。ここでは
 *          **DLL が無くても確かめられること**を固定する:
 *            - 名前の決め方 (純粋な判断)
 *            - DLL が無いときに**黙って成功したことにしない**こと
 */
#include "doctest/doctest.h"

#include "gui/archive.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

TEST_CASE("DefaultArchiveBaseName: カーソル位置の主部を使う")
{
	// MainFrm.cpp:23331 と同じ順序
	CHECK(archive::DefaultArchiveBaseName(_T("doc.txt"), {_T("other.dat")})
	      == UnicodeString(_T("doc")));
}

TEST_CASE("DefaultArchiveBaseName: カーソルが空なら最初の選択項目")
{
	CHECK(archive::DefaultArchiveBaseName(EmptyStr, {_T("first.zip"), _T("second.txt")})
	      == UnicodeString(_T("first")));
}

TEST_CASE("DefaultArchiveBaseName: どちらも無ければ空")
{
	CHECK(archive::DefaultArchiveBaseName(EmptyStr, {}).IsEmpty());
	CHECK(archive::DefaultArchiveBaseName(EmptyStr, {EmptyStr}).IsEmpty());
}

TEST_CASE("DefaultArchiveBaseName: 拡張子を落とす")
{
	CHECK(archive::DefaultArchiveBaseName(_T("a.tar.gz"), {}) == UnicodeString(_T("a.tar")));
	CHECK(archive::DefaultArchiveBaseName(_T("noext"), {}) == UnicodeString(_T("noext")));
}

TEST_CASE("ListEntries: 存在しない書庫は理由つきで失敗する")
{
	// 黙って空の一覧を返さないこと
	std::vector<archive::Entry> out;
	UnicodeString error;
	CHECK_FALSE(archive::ListEntries(_T("C:\\nosuch\\none.zip"), out, error));
	CHECK_FALSE(error.IsEmpty());
	CHECK(out.empty());
}

TEST_CASE("Extract: 展開先が無ければ失敗する")
{
	UnicodeString error;
	CHECK_FALSE(archive::Extract(_T("C:\\nosuch.zip"), _T("C:\\nosuch_dir"), error));
	CHECK_FALSE(error.IsEmpty());
}

TEST_CASE("Create: 詰めるものが無ければ失敗する")
{
	TempDir tmp;
	UnicodeString error;
	CHECK_FALSE(archive::Create(tmp.file(_T("out.zip")), tmp.path, {}, error));
	CHECK(ContainsText(error, _T("詰めるもの")));
}

TEST_CASE("Create: 同名の書庫があれば上書きしない")
{
	TempDir tmp;
	HANDLE h = ::CreateFileW(tmp.file(_T("exists.zip")).c_str(), GENERIC_WRITE, 0, NULL,
	                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	::CloseHandle(h);

	UnicodeString error;
	CHECK_FALSE(archive::Create(tmp.file(_T("exists.zip")), tmp.path, {_T("a.txt")}, error));
	CHECK(ContainsText(error, _T("既にあります")));
}

TEST_CASE("Create: 拡張子から形式を判別できなければ失敗する")
{
	TempDir tmp;
	UnicodeString error;
	CHECK_FALSE(archive::Create(tmp.file(_T("out.unknownext")), tmp.path, {_T("a.txt")}, error));
	CHECK_FALSE(error.IsEmpty());
}
