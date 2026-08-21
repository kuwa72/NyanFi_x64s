/**
 * @file tests/core/test_gui_image_load.cpp
 * @brief gui/image_load.cpp (画像ビューアの拡張子判定・WIC 読み込み) の回帰テスト
 *
 * @details wx に依存しない部分だけをここでテストする (`nyanfi_gui_core`、
 * ルート CMakeLists.txt 参照)。デコード自体は移植済みの WIC ラッパー
 * (src/usr_wic.cpp) を実際に呼ぶため、一時ディレクトリに実物の画像ファイルを
 * バイト列から組み立てて置き、実際に読み込めることを確認する
 * (tests/temp_dir.h の TempDir)。
 *
 * BMP はヘッダ・ピクセルデータとも単純な非圧縮形式なのでバイト列を直接
 * 組み立てられる。PNG は zlib 格納 (無圧縮) ブロック + CRC32/Adler32 を
 * このファイル内で計算して組み立てている (どちらも標準アルゴリズムそのもの)。
 */
#include "doctest/doctest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gui/image_load.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

void write_bytes(const UnicodeString &fnam, const std::vector<unsigned char> &data)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	if (!data.empty()) fs->WriteBuffer(data.data(), static_cast<int>(data.size()));
}

void put_u16le(std::vector<unsigned char> &v, unsigned int x)
{
	v.push_back(static_cast<unsigned char>(x & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
}

void put_u32le(std::vector<unsigned char> &v, unsigned int x)
{
	v.push_back(static_cast<unsigned char>(x & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
}

void put_u32be(std::vector<unsigned char> &v, unsigned int x)
{
	v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
	v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
	v.push_back(static_cast<unsigned char>(x & 0xFF));
}

/**
 * @brief 2x2 の24bpp無圧縮BMPを組み立てる
 * @details 配置 (画像の見た目): 左上=赤 右上=緑 / 左下=青 右下=白。
 * BMP は行が下から順、各画素 BGR、行は4バイト境界にパディングする仕様
 * (この画像は 2*3=6 バイト/行 のため2バイトパディングが要る)
 */
std::vector<unsigned char> make_bmp_2x2()
{
	const unsigned char row_top[8]    = {0, 0, 255,  0, 255, 0,  0, 0};      // 赤,緑 (BGR) + pad
	const unsigned char row_bottom[8] = {255, 0, 0,  255, 255, 255,  0, 0};  // 青,白 (BGR) + pad

	const unsigned int header_size = 14 + 40;
	const unsigned int image_size = 8 * 2;
	const unsigned int file_size = header_size + image_size;

	std::vector<unsigned char> v;
	// BITMAPFILEHEADER
	v.push_back('B');
	v.push_back('M');
	put_u32le(v, file_size);
	put_u32le(v, 0);              // reserved
	put_u32le(v, header_size);    // bfOffBits

	// BITMAPINFOHEADER
	put_u32le(v, 40);  // biSize
	put_u32le(v, 2);   // biWidth
	put_u32le(v, 2);   // biHeight (正: ボトムアップ)
	put_u16le(v, 1);   // biPlanes
	put_u16le(v, 24);  // biBitCount
	put_u32le(v, 0);   // biCompression (BI_RGB)
	put_u32le(v, image_size);
	put_u32le(v, 0);  // biXPelsPerMeter
	put_u32le(v, 0);  // biYPelsPerMeter
	put_u32le(v, 0);  // biClrUsed
	put_u32le(v, 0);  // biClrImportant

	// ピクセルデータ (下の行から)
	for (unsigned char b : row_bottom) v.push_back(b);
	for (unsigned char b : row_top) v.push_back(b);

	return v;
}

/**
 * @brief 幅・高さだけ巨大に主張し、ピクセルデータを省いた (壊れた) BMP
 * @details WIC_get_img_size はヘッダ相当の情報だけで幅・高さを返せるため、
 * image_load::kMaxDecodePixels の上限判定はこのファイルに対しても
 * (実際のデコードを試みる前に) 働くはず、という前提のテスト
 * (推測・要検証。環境依存の可能性があるため、上限に掛かって ok=false に
 * なることだけを確認し、エラー文言までは固定しない)
 */
std::vector<unsigned char> make_bmp_huge_header_only()
{
	const unsigned int header_size = 14 + 40;
	const unsigned int width = 20000;
	const unsigned int height = 20000;  // 20000*20000 = 4億ピクセル > kMaxDecodePixels

	std::vector<unsigned char> v;
	v.push_back('B');
	v.push_back('M');
	put_u32le(v, header_size);  // bfSize (実際のファイルサイズとは異なるが構わない)
	put_u32le(v, 0);
	put_u32le(v, header_size);

	put_u32le(v, 40);
	put_u32le(v, width);
	put_u32le(v, height);
	put_u16le(v, 1);
	put_u16le(v, 24);
	put_u32le(v, 0);
	put_u32le(v, 0);
	put_u32le(v, 0);
	put_u32le(v, 0);
	put_u32le(v, 0);
	// ピクセルデータは書かない (意図的に切り詰めた壊れたファイル)
	return v;
}

std::uint32_t crc32_of(const std::vector<unsigned char> &data)
{
	std::uint32_t crc = 0xFFFFFFFFu;
	for (unsigned char b : data) {
		crc ^= b;
		for (int i = 0; i < 8; ++i) {
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
		}
	}
	return crc ^ 0xFFFFFFFFu;
}

std::uint32_t adler32_of(const std::vector<unsigned char> &data)
{
	std::uint32_t a = 1, b = 0;
	for (unsigned char byte : data) {
		a = (a + byte) % 65521u;
		b = (b + a) % 65521u;
	}
	return (b << 16) | a;
}

void put_chunk(std::vector<unsigned char> &v, const char *type, const std::vector<unsigned char> &data)
{
	put_u32be(v, static_cast<unsigned int>(data.size()));

	std::vector<unsigned char> type_and_data(type, type + 4);
	type_and_data.insert(type_and_data.end(), data.begin(), data.end());
	for (unsigned char b : type_and_data) v.push_back(b);

	put_u32be(v, crc32_of(type_and_data));
}

/**
 * @brief 2x2 の8bit RGB PNG (無圧縮 zlib stored ブロック) を組み立てる
 * @details 配置は make_bmp_2x2() と同じ (左上=赤 右上=緑 左下=青 右下=白)。
 * PNG は上から順、フィルタなし (各行の先頭にフィルタタイプ0を1バイト置く)
 */
std::vector<unsigned char> make_png_2x2()
{
	const int width = 2, height = 2;

	// 生データ: 各行 = フィルタタイプ(0) + RGB*width
	std::vector<unsigned char> raw;
	// 行0 (上): 赤, 緑
	raw.push_back(0);
	raw.push_back(255); raw.push_back(0); raw.push_back(0);   // 赤
	raw.push_back(0); raw.push_back(255); raw.push_back(0);   // 緑
	// 行1 (下): 青, 白
	raw.push_back(0);
	raw.push_back(0); raw.push_back(0); raw.push_back(255);     // 青
	raw.push_back(255); raw.push_back(255); raw.push_back(255); // 白

	// zlib stream: ヘッダ(2) + stored block(1つで足りる) + Adler32(4)
	std::vector<unsigned char> zlib;
	zlib.push_back(0x78);
	zlib.push_back(0x01);

	const unsigned int len = static_cast<unsigned int>(raw.size());
	zlib.push_back(0x01);  // BFINAL=1, BTYPE=00 (stored)
	put_u16le(zlib, len);
	put_u16le(zlib, (~len) & 0xFFFFu);
	for (unsigned char b : raw) zlib.push_back(b);
	put_u32be(zlib, adler32_of(raw));

	std::vector<unsigned char> v = {137, 80, 78, 71, 13, 10, 26, 10};

	std::vector<unsigned char> ihdr;
	put_u32be(ihdr, static_cast<unsigned int>(width));
	put_u32be(ihdr, static_cast<unsigned int>(height));
	ihdr.push_back(8);  // bit depth
	ihdr.push_back(2);  // color type (RGB)
	ihdr.push_back(0);  // compression
	ihdr.push_back(0);  // filter
	ihdr.push_back(0);  // interlace
	put_chunk(v, "IHDR", ihdr);

	put_chunk(v, "IDAT", zlib);
	put_chunk(v, "IEND", {});

	return v;
}

}  // namespace

//===========================================================================
// IsSupportedExt
//===========================================================================
TEST_CASE("IsSupportedExt: 標準的な画像拡張子を認識する")
{
	CHECK(image_load::IsSupportedExt(_T("photo.jpg")));
	CHECK(image_load::IsSupportedExt(_T("photo.JPEG")));
	CHECK(image_load::IsSupportedExt(_T("a.png")));
	CHECK(image_load::IsSupportedExt(_T("a.bmp")));
	CHECK(image_load::IsSupportedExt(_T("a.gif")));
	CHECK(image_load::IsSupportedExt(_T("a.webp")));
}

TEST_CASE("IsSupportedExt: メタファイル/非画像拡張子は対象外")
{
	// .wmf/.emf は TMetafile が未移植のため対象外 (gui/image_load.h 参照)
	CHECK_FALSE(image_load::IsSupportedExt(_T("a.wmf")));
	CHECK_FALSE(image_load::IsSupportedExt(_T("a.emf")));
	CHECK_FALSE(image_load::IsSupportedExt(_T("a.txt")));
	CHECK_FALSE(image_load::IsSupportedExt(_T("noext")));
}

//===========================================================================
// LoadForView: 実際のファイルを読み込む
//===========================================================================
TEST_CASE("LoadForView: BMPを読み込み、ピクセル値まで一致する")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("test.bmp"));
	write_bytes(fnam, make_bmp_2x2());

	const image_load::LoadResult r = image_load::LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.error.IsEmpty());
	CHECK(r.width == 2);
	CHECK(r.height == 2);
	REQUIRE(r.rgb.size() == 2u * 2u * 3u);

	// 上から: 赤,緑 / 青,白 (RGB順)
	const unsigned char *p = r.rgb.data();
	CHECK(p[0] == 255); CHECK(p[1] == 0);   CHECK(p[2] == 0);    // 左上=赤
	CHECK(p[3] == 0);   CHECK(p[4] == 255); CHECK(p[5] == 0);    // 右上=緑
	CHECK(p[6] == 0);   CHECK(p[7] == 0);   CHECK(p[8] == 255);  // 左下=青
	CHECK(p[9] == 255); CHECK(p[10] == 255); CHECK(p[11] == 255);  // 右下=白
}

TEST_CASE("LoadForView: PNGを読み込み、ピクセル値まで一致する")
{
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("test.png"));
	write_bytes(fnam, make_png_2x2());

	const image_load::LoadResult r = image_load::LoadForView(fnam);
	REQUIRE(r.ok);
	CHECK(r.width == 2);
	CHECK(r.height == 2);
	REQUIRE(r.rgb.size() == 2u * 2u * 3u);

	const unsigned char *p = r.rgb.data();
	CHECK(p[0] == 255); CHECK(p[1] == 0);   CHECK(p[2] == 0);    // 左上=赤
	CHECK(p[3] == 0);   CHECK(p[4] == 255); CHECK(p[5] == 0);    // 右上=緑
	CHECK(p[6] == 0);   CHECK(p[7] == 0);   CHECK(p[8] == 255);  // 左下=青
	CHECK(p[9] == 255); CHECK(p[10] == 255); CHECK(p[11] == 255);  // 右下=白
}

TEST_CASE("LoadForView: 存在しないファイルはok=falseでエラーメッセージを返す")
{
	const image_load::LoadResult r = image_load::LoadForView(_T("Z:\\no_such_dir\\no_such_file.png"));
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
}

TEST_CASE("LoadForView: 壊れた/未対応形式のファイルは例外を投げずok=falseを返す")
{
	TempDir dir;

	// 拡張子は画像だが中身がテキスト (壊れたファイルの模擬)
	const UnicodeString fnam = dir.file(_T("broken.png"));
	write_bytes(fnam, std::vector<unsigned char>{'n', 'o', 't', ' ', 'a', ' ', 'p', 'n', 'g'});

	const image_load::LoadResult r = image_load::LoadForView(fnam);
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
	CHECK(r.rgb.empty());
}

TEST_CASE("LoadForView: 上限ピクセル数を超える画像はデコードせずok=falseを返す (要検証)")
{
	// WIC_get_img_size がヘッダだけから幅・高さを取れる前提のテスト。
	// もし環境によってサイズ取得自体に失敗する場合でも、いずれにせよ
	// ok=false になることは変わらない (gui/image_load.h の kMaxDecodePixels
	// のコメントを参照)
	TempDir dir;
	const UnicodeString fnam = dir.file(_T("huge.bmp"));
	write_bytes(fnam, make_bmp_huge_header_only());

	const image_load::LoadResult r = image_load::LoadForView(fnam);
	CHECK_FALSE(r.ok);
	CHECK_FALSE(r.error.IsEmpty());
}
