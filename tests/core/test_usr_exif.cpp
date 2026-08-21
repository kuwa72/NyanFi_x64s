/**
 * @file tests/core/test_usr_exif.cpp
 * @brief src/usr_exif.cpp (Exif 情報の解析) の回帰テスト
 *
 * 目的: 現在の実装の挙動をそのまま固定すること (regression test)。
 * ヘッダ (src/usr_exif.h) の Doxygen コメントは参考にするが、コメントより
 * 実装 (src/usr_exif.cpp) の挙動を信じる。実装がおかしいと思っても直さず、
 * 報告にのみ記載する。
 *
 * 方針: 実画像 (JPEG/RAW コンテナ) 全体のパースが無いと呼べない関数
 * (EXIF_GetInf / EXIF_GetExifTime / EXIF_GetExifTimeStr / EXIF_SetExifTime /
 * EXIF_DelJpgExif / CIFF_GetInf / CIFF_parse) は対象から外す (理由は末尾
 * コメント参照)。代わりに、
 *   - CIFF_ev            : 純粋な数値変換
 *   - EXIF_format_inf    : TStringList の中身だけで完結する書式整形
 *   - Exif_GetImgSize    : TStringList の中身だけで完結するサイズ判定
 *   - EXIF_get_idf_inf   : IFD (TIFF/Exif の tag/type/count/value テーブル)
 *                          1個分のバイト列を一時ファイルに書き出し、実際の
 *                          TFileStream 経由で読ませる
 * の4つに絞って、有理数→文字列/露出値/GPS座標/日時の解析といった
 * 「実画像が無くても再現できる」変換ロジックを固定する。
 *
 * 注意 (CP932/文字コード): 非ASCIIの期待値文字列は、実行時のANSIコード
 * ページに依存する narrow リテラルの文字化けを避けるため、_T(...) で
 * wide 化して書く (プロジェクト全体の方針)。
 */
#include "doctest/doctest.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "usr_exif.h"
#include "usr_file_ex.h"
#include "usr_str.h"

//===========================================================================
// テスト用ヘルパー: 一時ディレクトリ + IFDバイト列の組み立て
//===========================================================================
namespace {

void remove_all_recursive(const UnicodeString &path_with_delim)
{
	WIN32_FIND_DATAW fd;
	UnicodeString pattern = path_with_delim + "*";
	HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;

	do {
		UnicodeString name(fd.cFileName);
		if (name == UnicodeString(".") || name == UnicodeString("..")) continue;
		UnicodeString full = path_with_delim + name;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			remove_all_recursive(IncludeTrailingPathDelimiter(full));
			::RemoveDirectoryW(full.c_str());
		}
		else {
			::DeleteFileW(full.c_str());
		}
	} while (::FindNextFileW(h, &fd));
	::FindClose(h);
}

/// SUBCASE/TEST_CASE ごとに一意な一時ディレクトリを作成し、破棄時に再帰削除する
struct TempDir {
	UnicodeString path;  //!< 末尾 "\" 付き

	TempDir()
	{
		wchar_t buf[MAX_PATH];
		::GetTempPathW(MAX_PATH, buf);
		wchar_t unique[64];
		static LONG counter = 0;
		DWORD n = ::InterlockedIncrement(&counter);
		swprintf(unique, 64, L"nyanfi_ut_exif_%08lx_%04lx", (unsigned long)::GetCurrentProcessId(), (unsigned long)n);
		path = IncludeTrailingPathDelimiter(UnicodeString(buf)) + UnicodeString(unique);
		::CreateDirectoryW(path.c_str(), NULL);
		path = IncludeTrailingPathDelimiter(path);
	}
	~TempDir()
	{
		remove_all_recursive(path);
		::RemoveDirectoryW(ExcludeTrailingPathDelimiter(path).c_str());
	}

	UnicodeString file(const UnicodeString &name) const { return path + name; }
};

//---------------------------------------------------------------------------
// 1個の IFD (tag/dtype/count/value) を組み立てるビルダー
// 値は常にリトルエンディアン (Intel "II") で書く。
//---------------------------------------------------------------------------
struct IfdEntry {
	uint16_t tag;
	uint16_t dtype;
	uint32_t count;
	uint8_t value4[4];       //!< inline の場合の値/オフセットフィールド(4byte)
	std::vector<uint8_t> ext; //!< external の場合の実データ (無ければ空)
	bool external;
};

void put_u16(uint8_t *p, uint16_t v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
void put_u32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}

IfdEntry inline_u32(uint16_t tag, uint16_t dtype, uint32_t count, uint32_t value)
{
	IfdEntry e{}; e.tag = tag; e.dtype = dtype; e.count = count; e.external = false;
	put_u32(e.value4, value);
	return e;
}
IfdEntry inline_i32(uint16_t tag, uint16_t dtype, uint32_t count, int32_t value)
{
	return inline_u32(tag, dtype, count, (uint32_t)value);
}
IfdEntry inline_bytes(uint16_t tag, uint16_t dtype, uint32_t count, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
	IfdEntry e{}; e.tag = tag; e.dtype = dtype; e.count = count; e.external = false;
	e.value4[0] = b0; e.value4[1] = b1; e.value4[2] = b2; e.value4[3] = b3;
	return e;
}
IfdEntry inline_u16pair(uint16_t tag, uint16_t dtype, uint32_t count, uint16_t v0, uint16_t v1)
{
	IfdEntry e{}; e.tag = tag; e.dtype = dtype; e.count = count; e.external = false;
	put_u16(&e.value4[0], v0); put_u16(&e.value4[2], v1);
	return e;
}
IfdEntry external_bytes(uint16_t tag, uint16_t dtype, uint32_t count, std::vector<uint8_t> data)
{
	IfdEntry e{}; e.tag = tag; e.dtype = dtype; e.count = count; e.external = true; e.ext = std::move(data);
	return e;
}
void append_u16(std::vector<uint8_t> &v, uint16_t x) { uint8_t b[2]; put_u16(b, x); v.insert(v.end(), b, b + 2); }
void append_u32(std::vector<uint8_t> &v, uint32_t x) { uint8_t b[4]; put_u32(b, x); v.insert(v.end(), b, b + 4); }
void append_i32(std::vector<uint8_t> &v, int32_t x) { append_u32(v, (uint32_t)x); }
void append_rational(std::vector<uint8_t> &v, uint32_t n0, uint32_t n1) { append_u32(v, n0); append_u32(v, n1); }
void append_srational(std::vector<uint8_t> &v, int32_t n0, int32_t n1) { append_i32(v, n0); append_i32(v, n1); }
std::vector<uint8_t> ascii_bytes(const char *s, int len) { return std::vector<uint8_t>(s, s + len); }

/// entries をひとつの IFD バイト列に組み立てる (external データは末尾に連結)
std::vector<uint8_t> build_ifd(std::vector<IfdEntry> entries)
{
	size_t header_size = 2 + entries.size() * 12;
	std::vector<size_t> ext_pos(entries.size(), 0);
	size_t cur = header_size;
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].external) {
			ext_pos[i] = cur;
			cur += entries[i].ext.size();
		}
	}

	std::vector<uint8_t> buf(cur, 0);
	put_u16(&buf[0], (uint16_t)entries.size());
	for (size_t i = 0; i < entries.size(); i++) {
		size_t epos = 2 + i * 12;
		put_u16(&buf[epos], entries[i].tag);
		put_u16(&buf[epos + 2], entries[i].dtype);
		put_u32(&buf[epos + 4], entries[i].count);
		if (entries[i].external) {
			put_u32(&buf[epos + 8], (uint32_t)ext_pos[i]);
			std::copy(entries[i].ext.begin(), entries[i].ext.end(), buf.begin() + ext_pos[i]);
		}
		else {
			std::copy(entries[i].value4, entries[i].value4 + 4, buf.begin() + epos + 8);
		}
	}
	return buf;
}

/// バイト列を一時ファイルに書き、EXIF_get_idf_inf を呼んで結果リストを返す
/// (top=0, bsw=false = リトルエンディアン 固定)
std::unique_ptr<TStringList> parse_ifd(TempDir &td, const char *fname, std::vector<IfdEntry> entries,
	UnicodeString id = EmptyStr)
{
	std::vector<uint8_t> bytes = build_ifd(std::move(entries));
	UnicodeString fnam = td.file(fname);
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
		fs->WriteBuffer(bytes.data(), (int)bytes.size());
	}
	std::unique_ptr<TStringList> lst(new TStringList());
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmOpenRead | fmShareDenyNone));
	EXIF_get_idf_inf(fs.get(), 0, false, lst.get(), id);
	return lst;
}

} // namespace

//===========================================================================
// CIFF_ev: CIFF形式のEV値(下位5bitが特殊な小数表現)をfloatに変換
//===========================================================================
TEST_CASE("CIFF_ev: 通常のbit値(下位5bitが0x0c/0x14以外)はそのまま32分率")
{
	CHECK(CIFF_ev(0x20) == doctest::Approx(1.0));   //0x20 = 32 → 32/32 = 1.0
	CHECK(CIFF_ev(0x40) == doctest::Approx(2.0));
	CHECK(CIFF_ev(0)    == doctest::Approx(0.0));
}

TEST_CASE("CIFF_ev: 下位5bitが0x0cは1/3段、0x14は2/3段の特殊値")
{
	//v=0x0c: fr=0x0c→0x20/3.0, v-=fr(0のまま) → sig*(0 + 0x20/3.0)/0x20 = 1.0/3.0
	CHECK(CIFF_ev(0x0c) == doctest::Approx(1.0 / 3.0));
	//v=0x14: fr=0x14→0x40/3.0 → sig*(0 + 0x40/3.0)/0x20 = 2.0/3.0
	CHECK(CIFF_ev(0x14) == doctest::Approx(2.0 / 3.0));
}

TEST_CASE("CIFF_ev: 負値は符号を反転して同じ計算")
{
	CHECK(CIFF_ev(-0x20) == doctest::Approx(-1.0));
	CHECK(CIFF_ev(-0x0c) == doctest::Approx(-1.0 / 3.0));
}

//===========================================================================
// EXIF_get_idf_inf: IFD 1個分の tag/type/count/value を読み、文字列化する
//===========================================================================
TEST_CASE("EXIF_get_idf_inf: BYTE/SHORT (inline1件・inline2件・external3件以上)")
{
	//[修正済み] 以前は compat/ustring.h の UnicodeString(int)/UnicodeString(double) が
	//explicit 指定だったため、"val_str = v_s0;" (v_s0はunsigned short)のような直接
	//代入が、唯一 non-explicit だった UnicodeString(wchar_t) 経由の暗黙変換(コード
	//ポイント1文字化)になってしまうシムのバグがあった([シムのバグ疑い]として報告
	//し、コーディネーター側の compat/ustring.h 修正で解消済み)。現在は int/unsigned
	//系の非explicitコンストラクタが復元され、"42"のような10進文字列に正しく変換
	//される。
	TempDir td;
	std::vector<IfdEntry> entries;
	entries.push_back(inline_bytes(201, 1, 3, 10, 20, 30, 0));         //BYTE count=3
	entries.push_back(inline_u16pair(202, 3, 1, 42, 0));               //SHORT count=1 (inline)
	entries.push_back(inline_u16pair(203, 3, 2, 5, 9));                //SHORT count=2 (inline)
	std::vector<uint8_t> ext;
	append_u16(ext, 1); append_u16(ext, 2); append_u16(ext, 3);
	entries.push_back(external_bytes(204, 3, 3, ext));                 //SHORT count=3 (external)

	auto lst = parse_ifd(td, "byte_short.bin", entries);
	CHECK(lst->Values["201"] == UnicodeString("10,20,30"));
	CHECK(lst->Values["202"] == UnicodeString("42"));
	CHECK(lst->Values["203"] == UnicodeString("5,9"));
	CHECK(lst->Values["204"] == UnicodeString("1,2,3"));
}

TEST_CASE("EXIF_get_idf_inf: LONG/SLONG (inline1件・external複数件)")
{
	//[修正済み] 上のBYTE/SHORTテストと同じ経緯([シムのバグ疑い]は解消済み)。
	TempDir td;
	std::vector<IfdEntry> entries;
	entries.push_back(inline_u32(205, 4, 1, 100000));                  //LONG count=1 (inline)
	std::vector<uint8_t> ext_long;
	append_u32(ext_long, 111); append_u32(ext_long, 222);
	entries.push_back(external_bytes(206, 4, 2, ext_long));            //LONG count=2 (external)
	entries.push_back(inline_i32(207, 9, 1, -5));                      //SLONG count=1 (inline)
	std::vector<uint8_t> ext_slong;
	append_i32(ext_slong, -1); append_i32(ext_slong, -2);
	entries.push_back(external_bytes(208, 9, 2, ext_slong));           //SLONG count=2 (external)

	auto lst = parse_ifd(td, "long_slong.bin", entries);
	CHECK(lst->Values["205"] == UnicodeString("100000"));
	CHECK(lst->Values["206"] == UnicodeString("111,222"));
	CHECK(lst->Values["207"] == UnicodeString("-5"));
	CHECK(lst->Values["208"] == UnicodeString("-1,-2"));
}

TEST_CASE("EXIF_get_idf_inf: ASCII (inline・external)、撮影日時(tag=36867)は : を / に置換")
{
	TempDir td;
	std::vector<IfdEntry> entries;
	entries.push_back(inline_bytes(209, 2, 3, 'A', 'B', 0, 0));        //ASCII count<=4 (inline)
	entries.push_back(external_bytes(210, 2, 6, ascii_bytes("Hello\0", 6)));  //ASCII count>4 (external)
	entries.push_back(external_bytes(36867, 2, 20, ascii_bytes("2024:01:02 03:04:05\0", 20)));  //撮影日時

	auto lst = parse_ifd(td, "ascii.bin", entries);
	CHECK(lst->Values["209"] == UnicodeString("AB"));
	CHECK(lst->Values["210"] == UnicodeString("Hello"));
	//19文字ちょうどのときだけ 5,8文字目(1始まり)の : を / に置換する
	CHECK(lst->Values["36867"] == UnicodeString("2024/01/02 03:04:05"));
}

TEST_CASE("EXIF_get_idf_inf: RATIONAL (F値/焦点距離/一般値の整数化・分数化・ゼロ)")
{
	TempDir td;
	std::vector<IfdEntry> entries;
	std::vector<uint8_t> ext;

	//tag=33437 (FNumber): %.1f
	append_rational(ext, 28, 10);
	entries.push_back(external_bytes(33437, 5, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=37386 (焦点距離): 四捨五入して %umm
	append_rational(ext, 500, 10);
	entries.push_back(external_bytes(37386, 5, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9991 (一般、素数分解で約分できて整数比になる場合)
	append_rational(ext, 10, 5);
	entries.push_back(external_bytes(9991, 5, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9992 (一般、約分できず 0.25 未満なので 1/x 表記)
	append_rational(ext, 1, 8);
	entries.push_back(external_bytes(9992, 5, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9996 (一般、分子または分母が0なら"0")
	append_rational(ext, 0, 5);
	entries.push_back(external_bytes(9996, 5, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	auto lst = parse_ifd(td, "rational.bin", entries);
	CHECK(lst->Values["33437"] == UnicodeString("2.8"));
	CHECK(lst->Values["37386"] == UnicodeString("50mm"));
	CHECK(lst->Values["9991"] == UnicodeString("2"));
	CHECK(lst->Values["9992"] == UnicodeString("1/8"));
	CHECK(lst->Values["9996"] == UnicodeString("0"));
}

TEST_CASE("EXIF_get_idf_inf: RATIONAL, id=GPS: のときは各値を%.8fで連結")
{
	TempDir td;
	std::vector<uint8_t> ext;
	append_rational(ext, 35, 1);
	append_rational(ext, 15, 1);
	append_rational(ext, 2000, 100);
	std::vector<IfdEntry> entries;
	entries.push_back(external_bytes(2, 5, 3, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));

	auto lst = parse_ifd(td, "gps.bin", entries, "GPS:");
	CHECK(lst->Values["GPS:2"] == UnicodeString("35.00000000,15.00000000,20.00000000"));
}

TEST_CASE("EXIF_get_idf_inf: SRATIONAL (露出補正tag=37380は符号付き1桁、一般値は整数化/分数化/ゼロ)")
{
	TempDir td;
	std::vector<uint8_t> ext;
	std::vector<IfdEntry> entries;

	//tag=37380 (露出補正、正): %+.1f
	append_srational(ext, 1, 2);
	entries.push_back(external_bytes(37380, 10, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9993 (一般、0/0は"0")
	append_srational(ext, 0, 5);
	entries.push_back(external_bytes(9993, 10, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9994 (一般、負値どうし整除できる場合)
	append_srational(ext, -4, 2);
	entries.push_back(external_bytes(9994, 10, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	//tag=9995 (一般、負値で整除できず分数表記)
	append_srational(ext, -1, 8);
	entries.push_back(external_bytes(9995, 10, 1, ascii_bytes(reinterpret_cast<char*>(ext.data()), (int)ext.size())));
	ext.clear();

	auto lst = parse_ifd(td, "srational.bin", entries);
	CHECK(lst->Values["37380"] == UnicodeString("+0.5"));
	CHECK(lst->Values["9993"] == UnicodeString("0"));
	CHECK(lst->Values["9994"] == UnicodeString("-2"));
	CHECK(lst->Values["9995"] == UnicodeString("-1/8"));
}

TEST_CASE("EXIF_get_idf_inf: UNDEFINED (ExifVersion/ComponentsConfiguration/既定は数値化)")
{
	//[修正済み] 上のBYTE/SHORTテストと同じ経緯([シムのバグ疑い]は解消済み)。
	//既定分岐(tag=36864/40960/37121以外)は "val_str = v_ui;" を通り、10進数値
	//文字列になる。
	TempDir td;
	std::vector<IfdEntry> entries;
	entries.push_back(inline_bytes(36864, 7, 4, '0', '2', '3', '1'));   //ExifVersion
	entries.push_back(inline_bytes(40960, 7, 4, '0', '1', '0', '0'));   //FlashPixVersion (同じ書式)
	entries.push_back(inline_bytes(37121, 7, 4, 1, 2, 3, 0));           //ComponentsConfiguration
	entries.push_back(inline_u32(9997, 7, 1, 12345));                   //既定 (数値のまま)

	auto lst = parse_ifd(td, "undefined.bin", entries);
	CHECK(lst->Values["36864"] == UnicodeString("0231"));
	CHECK(lst->Values["40960"] == UnicodeString("0100"));
	CHECK(lst->Values["37121"] == UnicodeString("1,2,3,0"));
	CHECK(lst->Values["9997"] == UnicodeString("12345"));
}

TEST_CASE("EXIF_get_idf_inf: tag=37500(メーカーノート)はdtypeに関わらず値をそのまま数値化")
{
	//[修正済み] 上のBYTE/SHORTテストと同じ経緯([シムのバグ疑い]は解消済み)。
	TempDir td;
	std::vector<IfdEntry> entries;
	entries.push_back(inline_u32(37500, 7, 1, 555));

	auto lst = parse_ifd(td, "makernote.bin", entries);
	CHECK(lst->Values["37500"] == UnicodeString("555"));
}

//===========================================================================
// EXIF_format_inf: タグ値の書式整形 (TStringList を直接組み立てて検証)
//===========================================================================
TEST_CASE("EXIF_format_inf: 方向(274)は横/縦の説明文字列を274Lとして追加")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("274=6");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["274L"] == UnicodeString(_T("6 縦(90度回転)")));
}

TEST_CASE("EXIF_format_inf: 方向(274)が無ければ274Lは追加されない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("100=dummy");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->IndexOfName("274L") == -1);
}

TEST_CASE("EXIF_format_inf: ISO(34855) 通常のjpegはそのまま維持")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("34855=200");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["34855"] == UnicodeString("200"));
}

TEST_CASE("EXIF_format_inf: ISO(34855) rw2はtag23の値を優先し、無ければ34855として追加")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("23=400");
	EXIF_format_inf(".rw2", lst.get());
	CHECK(lst->Values["34855"] == UnicodeString("400"));
}

TEST_CASE("EXIF_format_inf: ISO(34855) が\"0\"なら NK:2 にフォールバックする")
{
	// src/usr_exif.cpp:453 は
	//   if (vstr.IsEmpty() || SameStr(vstr, "0")) vstr = get_tkn_r(lst->Values["NK:2"], ',');
	// で、get_tkn_r は「最初の区切りより後ろ全部」を返す (usr_str.cpp:181)。
	// つまり "100,200,320" からは最右トークンではなく "200,320" が得られる。
	// ISO 値としては不自然だが、これが既存実装の挙動なのでそのまま固定する。
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("34855=0");
	lst->Add("NK:2=100,200,320");
	EXIF_format_inf(".nef", lst.get());
	CHECK(lst->Values["34855"] == UnicodeString("200,320"));
}

TEST_CASE("EXIF_format_inf: 露出時間(33434)は33434Uとして「秒」付きの表示を追加")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("33434=1/125");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["33434U"] == UnicodeString(_T("1/125秒")));
}

TEST_CASE("EXIF_format_inf: 露出プログラム(34850)は名称を34850Lに追加、範囲外は不明")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("34850=3");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["34850L"] == UnicodeString(_T("絞り優先")));

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->Add("34850=99");
	EXIF_format_inf(".jpg", lst2.get());
	CHECK(lst2->Values["34850L"] == UnicodeString(_T("不明")));
}

TEST_CASE("EXIF_format_inf: 測光方式(37383)は既存の値自体を名称に置き換える")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("37383=2");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["37383"] == UnicodeString(_T("中央重視")));
	CHECK(lst->IndexOfName("37383L") == -1);  //新規キーは作らない

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->Add("37383=255");
	EXIF_format_inf(".jpg", lst2.get());
	CHECK(lst2->Values["37383"] == UnicodeString(_T("その他")));

	std::unique_ptr<TStringList> lst3(new TStringList());
	lst3->Add("37383=50");
	EXIF_format_inf(".jpg", lst3.get());
	CHECK(lst3->Values["37383"] == UnicodeString(_T("不明")));
}

TEST_CASE("EXIF_format_inf: フラッシュ(37385)は奇数ならON/偶数ならOFFに既存の値を置き換える")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("37385=1");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["37385"] == UnicodeString("ON"));

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->Add("37385=4");
	EXIF_format_inf(".jpg", lst2.get());
	CHECK(lst2->Values["37385"] == UnicodeString("OFF"));

	std::unique_ptr<TStringList> lst3(new TStringList());
	lst3->Add("37385=5");
	EXIF_format_inf(".jpg", lst3.get());
	CHECK(lst3->Values["37385"] == UnicodeString("ON"));
}

TEST_CASE("EXIF_format_inf: WB(CN:4.7)は名称に置き換え、範囲外は空文字になる")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("CN:4.7=0");
	EXIF_format_inf(".crw", lst.get());
	CHECK(lst->Values["CN:4.7"] == UnicodeString("Auto"));

	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->Add("CN:4.7=9");
	EXIF_format_inf(".crw", lst2.get());
	CHECK(lst2->Values["CN:4.7"] == UnicodeString("Manual"));

	//範囲外はget_word_i_idxが空文字を返し、そのまま空文字で上書きされる(不明へのフォールバックは無い)
	std::unique_ptr<TStringList> lst3(new TStringList());
	lst3->Add("CN:4.7=99");
	EXIF_format_inf(".crw", lst3.get());
	CHECK(lst3->Values["CN:4.7"] == UnicodeString(""));
}

TEST_CASE("EXIF_format_inf: LensModel(42036) 既に値があれば何もしない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("42036=50mm");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["42036"] == UnicodeString("50mm"));
}

TEST_CASE("EXIF_format_inf: LensModel(42036) 無ければNK:132(Nikon)をそのまま採用")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("NK:132=24-70mm F/2.8");
	EXIF_format_inf(".nef", lst.get());
	CHECK(lst->Values["42036"] == UnicodeString("24-70mm F/2.8"));
}

TEST_CASE("EXIF_format_inf: LensModel(42036) Canonは焦点距離の換算値から組み立てる(ズーム/単焦点)")
{
	//[修正済み] 以前は "vstr = w;" (wはint)の直接代入がシムのバグで文字化けして
	//いた([シムのバグ疑い]として報告し、コーディネーター側のcompat/ustring.h
	//修正で解消済み)。現在は "20" のような10進文字列に正しく変換される。

	//ズーム: w!=t なら "w-t mm"
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("CN:1.23=600");  //t(換算前, 望遠端)
	lst->Add("CN:1.24=200");  //w(換算前, 広角端)
	lst->Add("CN:1.25=10");   //換算単位 → t=60, w=20
	EXIF_format_inf(".crw", lst.get());
	CHECK(lst->Values["42036"] == UnicodeString("20-60mm"));

	//単焦点: w==t なら "w mm" のみ
	std::unique_ptr<TStringList> lst2(new TStringList());
	lst2->Add("CN:1.23=200");
	lst2->Add("CN:1.24=200");
	lst2->Add("CN:1.25=10");   //→ t=20, w=20
	EXIF_format_inf(".crw", lst2.get());
	CHECK(lst2->Values["42036"] == UnicodeString("20mm"));
}

TEST_CASE("EXIF_format_inf: GPS座標(GPS:2/GPS:4)を10進度+DMS表記に変換、S/Wは負符号")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("GPS:1=N");
	lst->Add("GPS:2=35,0,0");
	lst->Add("GPS:3=W");
	lst->Add("GPS:4=139,0,0");
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["GPS:2"] == UnicodeString(_T("35.00000000 (N35°0′0.00″)")));
	CHECK(lst->Values["GPS:4"] == UnicodeString(_T("-139.00000000 (W139°0′0.00″)")));
}

TEST_CASE("EXIF_format_inf: GPS Refが無い(長さ1でない)場合は対象のGPS:2/4を変更しない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("GPS:2=35,0,0");  //GPS:1(Ref)が無い
	EXIF_format_inf(".jpg", lst.get());
	CHECK(lst->Values["GPS:2"] == UnicodeString("35,0,0"));  //未加工のまま
}

//===========================================================================
// Exif_GetImgSize: Exifリストから画像サイズを取得
//===========================================================================
TEST_CASE("Exif_GetImgSize: RW2はtag2/3を基本にtag5-7の差分があれば上書き")
{
	//[修正済み] 以前は "w_str = s_w;" (s_wはint)の直接代入がシムのバグで
	//数値化に失敗し、*w/*hが常に0になっていた([シムのバグ疑い]として報告し、
	//コーディネーター側のcompat/ustring.h修正で解消済み)。
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("4=1");     //分岐条件(存在すること)のダミー
	lst->Add("2=800"); lst->Add("3=600");
	lst->Add("5=10");  lst->Add("7=410");   //w差分: 410-10=400
	lst->Add("6=601");                       //h差分: 601-4(=tag4)=600 ※tag4はさらに使われる
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".rw2", &w, &h) == true);
	CHECK(w == 400);
	CHECK(h == 600);
}

TEST_CASE("Exif_GetImgSize: X3Fはtag X3F:256/257をそのまま使う")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("X3F:256=1920"); lst->Add("X3F:257=1080");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".x3f", &w, &h) == true);
	CHECK(w == 1920);
	CHECK(h == 1080);
}

TEST_CASE("Exif_GetImgSize: CFA:256/257があれば拡張子に関わらず優先")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("CFA:256=4000"); lst->Add("CFA:257=3000");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".raf", &w, &h) == true);
	CHECK(w == 4000);
	CHECK(h == 3000);
}

TEST_CASE("Exif_GetImgSize: JPEGは40962/40963があればそれ以外を見ない")
{
	//[修正済み] 既定(else)分岐末尾の "w_str = wd; h_str = hi;" (wd/hiはint)も
	//上記RW2と同じ原因([シムのバグ疑い])で影響を受けていたが解消済み。
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("40962=1024"); lst->Add("40963=768");
	lst->Add("256=9999"); lst->Add("257=9999");  //使われないはずの値
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".jpg", &w, &h) == true);
	CHECK(w == 1024);
	CHECK(h == 768);
}

TEST_CASE("Exif_GetImgSize: 非JPEGは256/257にフォールバック")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("256=640"); lst->Add("257=480");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".tif", &w, &h) == true);
	CHECK(w == 640);
	CHECK(h == 480);
}

TEST_CASE("Exif_GetImgSize: S0/S1/S2/256は大きい方を採用する(単純な優先順位ではない)")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("S0:256=100"); lst->Add("S0:257=100");
	lst->Add("S1:256=300"); lst->Add("S1:257=300");
	lst->Add("S2:256=200"); lst->Add("S2:257=200");
	lst->Add("256=50");     lst->Add("257=50");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".cr2", &w, &h) == true);
	CHECK(w == 300);
	CHECK(h == 300);
}

TEST_CASE("Exif_GetImgSize: NEF/NRWはSOF:256/257と比が近ければ実サイズとして採用")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("256=4000"); lst->Add("257=3000");
	lst->Add("SOF:256=4016"); lst->Add("SOF:257=3016");  //比が近い(5%未満)
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".nef", &w, &h) == true);
	CHECK(w == 4016);
	CHECK(h == 3016);
}

TEST_CASE("Exif_GetImgSize: NEF/NRWでもSOF比が離れていれば採用しない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("256=4000"); lst->Add("257=3000");
	lst->Add("SOF:256=1000"); lst->Add("SOF:257=1000");  //比が離れている
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".nef", &w, &h) == true);
	CHECK(w == 4000);
	CHECK(h == 3000);
}

TEST_CASE("Exif_GetImgSize: 方向(274)が6または8ならw/hを入れ替える")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("40962=100"); lst->Add("40963=200");
	lst->Add("274=6");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".jpg", &w, &h) == true);
	CHECK(w == 200);
	CHECK(h == 100);
}

TEST_CASE("Exif_GetImgSize: 拡張子.3frは方向による入れ替えをしない")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("CFA:256=500"); lst->Add("CFA:257=300");
	lst->Add("274=6");
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".3fr", &w, &h) == true);
	CHECK(w == 500);  //入れ替わらない
	CHECK(h == 300);
}

TEST_CASE("Exif_GetImgSize: サイズ情報が全く無ければfalse")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	unsigned int w = 0, h = 0;
	CHECK(Exif_GetImgSize(lst.get(), ".jpg", &w, &h) == false);
}

//===========================================================================
// 対象から外した関数と理由 (無言のスキップ禁止):
//
// - EXIF_GetInf:
//     JPEG(APP0/APP1マーカ)・HEIC(ftyp/meta/mdat box)・各RAW形式固有の
//     マジックナンバー/オフセット計算まで含む、実際のファイルコンテナ
//     全体のパースが必要。IFD 1個分の tag/value 変換ロジックそのものは
//     EXIF_get_idf_inf の直接呼び出しで既にカバーしているため、コンテナ
//     形式ごとの分岐(isJpeg/isHeic/isRaf/isX3f/isNikon/isCR2等)は対象外
//     とした。合成バイト列で全パターンを正確に再現するのは実装の細部
//     (マジックナンバーやオフセット定数)への依存が大きく、確度の低い
//     テストになると判断した。
// - EXIF_GetExifTime / EXIF_GetExifTimeStr / EXIF_SetExifTime:
//     内部で EXIF_GetInf を呼ぶため、上記と同じ理由で対象外。日時文字列
//     の ":" → "/" 置換ロジック自体は EXIF_get_idf_inf のテストの tag=36867
//     ケースで検証済み。
// - EXIF_DelJpgExif:
//     JPEGのAPP1(Exif)セグメントを読んでAPP0に差し替える処理。実JPEGの
//     SOI/APP1マーカ、TIFFヘッダ、0th IFD、画像データ本体まで含む合成
//     バイト列が必要で、実画像相当の構築コストに対してEXIF_get_idf_inf
//     で検証済みの内容と重複が大きいため対象外とした。
// - CIFF_GetInf / CIFF_parse (static):
//     Canon CIFF(.crw)形式のヘッダ("HEAPCCDR"マーカ)とディレクトリ木を
//     読む必要があり、実CRWファイル相当の構築が必要なため対象外とした。
//     数値変換部分の CIFF_ev は個別にテスト済み。
//
//===========================================================================
// [修正済み] シムのバグ疑いについて (経緯の記録):
//
// 当初、compat/include/compat/ustring.h の UnicodeString(int)/(double) が
// explicit 指定だったため、src/usr_exif.cpp 内の "val_str = intExpr;" という
// 形の直接代入(SHORT/LONG/SLONG(inline,count=1)・UNDEFINED既定分岐・
// メーカーノート・Canon LensModel組み立て・Exif_GetImgSizeの各サイズ確定
// 処理)が、10進文字列化ではなく「その整数値をコードポイントとする1文字」に
// 化けるバグを引き起こしていた(char が int/wchar_t のどちらにも変換できて
// しまい、explicit でない唯一の経路である UnicodeString(wchar_t) が選ばれて
// いたため)。特に Exif_GetImgSize は RW2 の差分上書き分岐と、それ以外を
// 処理する既定(else)分岐(JPEGの40962判定・非JPEGの256/257フォールバック・
// S0/S1/S2/256の最大値合成・NEF/NRWのSOF比較・方向による入れ替えを含む)の
// 双方が影響を受け、常に *w=0, *h=0 を返す状態だった。
//
// この報告を受けてコーディネーター側で compat/ustring.h・compat/ustring.cpp
// を修正(数値からのコンストラクタを型ごとに用意した非explicit版に変更)し、
// 解消を確認した。上記の各テストは、修正後の正しい期待値に更新済みである。
//===========================================================================
