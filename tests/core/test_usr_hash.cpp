/**
 * @file tests/core/test_usr_hash.cpp
 * @brief src/usr_file_inf.cpp のハッシュ関数の回帰テスト
 *
 * @details Phase 1 で移植したが**値が正しいかを確かめたテストが1件も無かった**。
 *          Phase 3 第2段で GetHash / CompareHash / ToOppSameHash の3コマンドが
 *          これに依存するようになったので足す (規約9)。
 *
 *          期待値は公開されている既知のベクタ:
 *            MD5("abc")      = 900150983cd24fb0d6963f7d28e17f72
 *            SHA1("abc")     = a9993e364706816aba3e25717850c26c9cd0d89d
 *            SHA256("abc")   = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 *            MD5("")         = d41d8cd98f00b204e9800998ecf8427e
 */
#include "doctest/doctest.h"

#include "temp_dir.h"
#include "usr_file_inf.h"

using nyanfi_test::TempDir;

namespace {

/// 中身をバイト列そのままで書く (改行変換などを挟まない)
void write_bytes(const UnicodeString &path, const char *data, std::size_t len)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	DWORD written = 0;
	if (len > 0) ::WriteFile(h, data, static_cast<DWORD>(len), &written, NULL);
	::CloseHandle(h);
}

}  // namespace

TEST_CASE("get_HashStr: MD5 が既知のベクタと一致する")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("abc.bin")), "abc", 3);

	CHECK(get_HashStr(tmp.file(_T("abc.bin")), _T("MD5"))
	      == UnicodeString(_T("900150983cd24fb0d6963f7d28e17f72")));
}

TEST_CASE("get_HashStr: SHA1 / SHA256 が既知のベクタと一致する")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("abc.bin")), "abc", 3);

	CHECK(get_HashStr(tmp.file(_T("abc.bin")), _T("SHA1"))
	      == UnicodeString(_T("a9993e364706816aba3e25717850c26c9cd0d89d")));
	CHECK(get_HashStr(tmp.file(_T("abc.bin")), _T("SHA256"))
	      == UnicodeString(_T("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
}

TEST_CASE("get_HashStr: 空のファイル")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("empty.bin")), "", 0);

	CHECK(get_HashStr(tmp.file(_T("empty.bin")), _T("MD5"))
	      == UnicodeString(_T("d41d8cd98f00b204e9800998ecf8427e")));
}

TEST_CASE("get_HashStr: 存在しないファイルは空文字列")
{
	// 呼び出し側 (gui/main_frame.cpp の CmdGetHash) は空を「取得できません」と
	// 表示する。例外を投げないことを固定しておく
	CHECK(get_HashStr(_T("C:\\nosuch\\none.bin"), _T("MD5")).IsEmpty());
}

TEST_CASE("get_HashStr: ブロック境界をまたぐ大きさでも正しい")
{
	// 読み込みバッファ (FILE_RBUF_SIZE) をまたぐと CryptHashData を複数回呼ぶ。
	// そこを間違えると小さいファイルだけ合う、という壊れ方をする。
	// 'a' を 1,000,000 個並べた MD5 は既知のベクタ
	TempDir tmp;
	std::string big(1000000, 'a');
	write_bytes(tmp.file(_T("million.bin")), big.data(), big.size());

	CHECK(get_HashStr(tmp.file(_T("million.bin")), _T("MD5"))
	      == UnicodeString(_T("7707d6ae4e027c70eea2a935c2296f21")));
}

TEST_CASE("get_HashStr: only_1blk は先頭ブロックだけを見る")
{
	// 先頭 32KB が同じで後ろが違う2つのファイルは、only_1blk なら一致する
	TempDir tmp;
	std::string head(40000, 'x');
	std::string a = head + "AAAA";
	std::string b = head + "BBBB";
	write_bytes(tmp.file(_T("a.bin")), a.data(), a.size());
	write_bytes(tmp.file(_T("b.bin")), b.data(), b.size());

	const UnicodeString full_a = get_HashStr(tmp.file(_T("a.bin")), _T("MD5"));
	const UnicodeString full_b = get_HashStr(tmp.file(_T("b.bin")), _T("MD5"));
	CHECK(full_a != full_b);

	const UnicodeString blk_a = get_HashStr(tmp.file(_T("a.bin")), _T("MD5"), true);
	const UnicodeString blk_b = get_HashStr(tmp.file(_T("b.bin")), _T("MD5"), true);
	CHECK_FALSE(blk_a.IsEmpty());
	CHECK(blk_a == blk_b);
}

TEST_CASE("get_TextHashStr: 文字列のハッシュ (UTF-8 として扱う)")
{
	// VCL 側の入力ダイアログが「マルチバイト文字はUTF-8として処理」と
	// 表示している (MainFrm.cpp:14814) ので、UTF-8 でのハッシュになるはず
	CHECK(get_TextHashStr(_T("abc"), _T("MD5"))
	      == UnicodeString(_T("900150983cd24fb0d6963f7d28e17f72")));
}

TEST_CASE("get_HashStr: 未知のアルゴリズム名")
{
	TempDir tmp;
	write_bytes(tmp.file(_T("a.bin")), "abc", 3);
	// 落ちないことだけを見る (戻り値の約束は実装に合わせて記録する)
	const UnicodeString r = get_HashStr(tmp.file(_T("a.bin")), _T("NOSUCH"));
	// 実測: 空文字列を返す (呼び出し側は「取得できません」と表示する)
	CHECK(r.IsEmpty());
}
