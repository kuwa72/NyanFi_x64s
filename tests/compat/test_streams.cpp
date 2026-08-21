/**
 * @file tests/compat/test_streams.cpp
 * @brief TStream / THandleStream / TFileStream / TMemoryStream 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <cstring>
#include <memory>
#include <vector>

#include "compat/streams.h"

//===========================================================================
// TMemoryStream: 基本の Read/Write/Seek/Position/Size
//===========================================================================
TEST_CASE("TMemoryStream: Write して Read で読み戻せる")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	const char data[] = "0123456789";
	CHECK(ms->Write(data, 10) == 10);
	CHECK(ms->Size == 10);
	CHECK(ms->Position == 10);

	ms->Seek(0, soFromBeginning);
	char buf[10] = {};
	CHECK(ms->Read(buf, 10) == 10);
	CHECK(std::memcmp(buf, data, 10) == 0);
}

TEST_CASE("TMemoryStream: Seek の soFromBeginning/soFromCurrent/soFromEnd (旧形式)")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	const char data[] = "abcdefghij";
	ms->Write(data, 10);

	CHECK(ms->Seek(0, soFromBeginning) == 0);
	CHECK(ms->Seek(3, soFromCurrent) == 3);
	CHECK(ms->Seek(-2, soFromCurrent) == 1);
	CHECK(ms->Seek(-1, soFromEnd) == 9);
}

TEST_CASE("TMemoryStream: Seek の soBeginning/soCurrent/soEnd (新形式 TSeekOrigin)")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	const char data[] = "abcdefghij";
	ms->Write(data, 10);

	CHECK(ms->Seek((__int64)0, soBeginning) == 0);
	CHECK(ms->Seek((__int64)3, soCurrent) == 3);
	CHECK(ms->Seek((__int64)-2, soCurrent) == 1);
	CHECK(ms->Seek((__int64)-1, soEnd) == 9);
}

TEST_CASE("TMemoryStream: 初期容量を超えて成長できる (数万バイト)")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	std::vector<Byte> src(100000);
	for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<Byte>(i & 0xFF);

	CHECK(ms->Write(src.data(), static_cast<int>(src.size())) == static_cast<int>(src.size()));
	CHECK(ms->Size == static_cast<Int64>(src.size()));

	ms->Seek(0, soFromBeginning);
	std::vector<Byte> dst(src.size());
	CHECK(ms->Read(dst.data(), static_cast<int>(dst.size())) == static_cast<int>(dst.size()));
	CHECK(dst == src);
}

TEST_CASE("TMemoryStream: SetSize で拡張・縮小できる")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("hello", 5);
	ms->SetSize(3);
	CHECK(ms->Size == 3);
	CHECK(ms->Position == 3);  // Size を超えた Position は詰められる

	ms->SetSize(10);
	CHECK(ms->Size == 10);
}

TEST_CASE("TMemoryStream: Memory は (BYTE*) キャストできる (実測どおりの呼び出し形)")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("XYZ", 3);
	BYTE *bp = (BYTE *)ms->Memory;
	CHECK(bp[0] == 'X');
	CHECK(bp[1] == 'Y');
	CHECK(bp[2] == 'Z');

	const BYTE *bp2 = reinterpret_cast<const BYTE *>(ms->Memory);
	CHECK(bp2[0] == 'X');
}

TEST_CASE("TMemoryStream: Read(TBytes&, count) / Write(TBytes&, count) オーバーロード")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("hello", 5);
	ms->Seek(0, soFromBeginning);

	TBytes bytes;
	bytes.Length = 5;
	CHECK(ms->Read(bytes, 5) == 5);
	CHECK(bytes[0] == 'h');
	CHECK(bytes[4] == 'o');
}

TEST_CASE("TMemoryStream: CopyFrom (count>0 と count<=0 の両方)")
{
	std::unique_ptr<TMemoryStream> src(new TMemoryStream());
	src->Write("0123456789", 10);

	std::unique_ptr<TMemoryStream> dst(new TMemoryStream());
	src->Seek(0, soFromBeginning);
	CHECK(dst->CopyFrom(src.get(), 4) == 4);
	CHECK(dst->Size == 4);

	std::unique_ptr<TMemoryStream> dst2(new TMemoryStream());
	CHECK(dst2->CopyFrom(src.get(), 0) == 10);  // count<=0 は Source 全体
	CHECK(dst2->Size == 10);
}

TEST_CASE("TMemoryStream: LoadFromFile/SaveToFile の往復")
{
	const UnicodeString path = _T("nyanfi_test_streams_mem.tmp");
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("save-load-roundtrip", 19);
	ms->SaveToFile(path);

	std::unique_ptr<TMemoryStream> ms2(new TMemoryStream());
	ms2->LoadFromFile(path);
	CHECK(ms2->Size == 19);
	CHECK(std::memcmp(ms2->Memory, "save-load-roundtrip", 19) == 0);

	::DeleteFileW(path.c_str());
}

//===========================================================================
// TFileStream
//===========================================================================
TEST_CASE("TFileStream: fmCreate で作成して書き込み、fmOpenRead で読み直す")
{
	const UnicodeString path = _T("nyanfi_test_streams_file.tmp");
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(path, fmCreate));
		fs->WriteBuffer("hello-file", 10);
		CHECK(fs->Size == 10);
	}
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(path, fmOpenRead | fmShareDenyNone));
		CHECK(fs->Size == 10);
		char buf[10] = {};
		fs->ReadBuffer(buf, 10);
		CHECK(std::memcmp(buf, "hello-file", 10) == 0);

		// 旧形式 Seek (Word Origin)
		CHECK(fs->Seek(0, soFromCurrent) == 10);
		CHECK(fs->Seek(-4, soFromEnd) == 6);
	}
	::DeleteFileW(path.c_str());
}

TEST_CASE("TFileStream: 存在しないファイルを fmOpenRead で開くと例外を投げる")
{
	const UnicodeString path = _T("nyanfi_test_streams_missing.tmp");
	::DeleteFileW(path.c_str());  // 万一残っていた場合に備える
	CHECK_THROWS(std::unique_ptr<TFileStream>(new TFileStream(path, fmOpenRead | fmShareDenyNone)));
}

//===========================================================================
// ReadBuffer / WriteBuffer: 不足時は例外
//===========================================================================
TEST_CASE("TStream: ReadBuffer は要求バイト数に満たないと例外を投げる")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("ab", 2);
	ms->Seek(0, soFromBeginning);
	char buf[10] = {};
	CHECK_THROWS_AS(ms->ReadBuffer(buf, 10), EReadError);
}

TEST_CASE("TStream: Read はバイト数が足りなくても例外を投げず短く返す")
{
	std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
	ms->Write("ab", 2);
	ms->Seek(0, soFromBeginning);
	char buf[10] = {};
	CHECK(ms->Read(buf, 10) == 2);
}
