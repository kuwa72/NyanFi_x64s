/**
 * @file tests/core/test_gui_pack_settings.cpp
 * @brief gui/pack_settings の書庫設定解決のテスト
 */
#include "doctest/doctest.h"

#include "gui/pack_settings.h"

TEST_CASE("pack_settings: 形式と拡張子")
{
	CHECK(pack_settings::ToIndex(pack_settings::Format::SevenZip) == 1);
	CHECK(pack_settings::Extension(pack_settings::Format::Lha) == UnicodeString(_T(".lzh")));
	pack_settings::Format parsed = pack_settings::Format::Zip;
	CHECK_FALSE(pack_settings::TryFormatFromName(_T("foo.txt"), parsed));
	CHECK(parsed == pack_settings::Format::Zip);
	CHECK(pack_settings::TryFormatFromName(_T("foo.7Z"), parsed));
	CHECK(parsed == pack_settings::Format::SevenZip);
	CHECK(pack_settings::TryFormatFromName(_T("foo.tar.gz"), parsed));
	CHECK(parsed == pack_settings::Format::Tar);
}

TEST_CASE("pack_settings: VCL の圧縮レベルコンボ変換")
{
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Zip, 0) == 0);
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Zip, 1) == 1);
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Zip, 3) == 5);
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Zip, 5) == 9);
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Cab, 1) == 15);
	CHECK(pack_settings::CompressionFromUiIndex(pack_settings::Format::Tar, 6) == 6);
	CHECK(pack_settings::UiIndexFromCompression(pack_settings::Format::Zip, 5) == 3);
	CHECK(pack_settings::UiIndexFromCompression(pack_settings::Format::Cab, 15) == 1);
}

TEST_CASE("pack_settings: 利用可能な形式へ正規化する")
{
	pack_settings::Availability av;
	av.zip = false;
	av.seven_zip = true;
	av.lha = false;
	av.cab = false;
	av.tar = false;

	pack_settings::Options o;
	o.name = _T("backup");
	o.format = pack_settings::Format::Zip;
	o.compression = 99;
	const pack_settings::Options n = pack_settings::Normalize(o, av);
	CHECK(n.format == pack_settings::Format::SevenZip);
	CHECK(n.name == UnicodeString(_T("backup")));
	CHECK(n.compression == 5);
	CHECK(pack_settings::IsAvailable(av, n.format));
}

TEST_CASE("pack_settings: 名前と書庫パスを解決する")
{
	pack_settings::Availability av;
	pack_settings::Options o;
	o.name = _T("backup.zip");
	o.format = pack_settings::Format::Zip;
	const pack_settings::Resolved r = pack_settings::Resolve(o, av, _T("D:\\out"));
	CHECK(r.available);
	CHECK(r.extension == UnicodeString(_T(".zip")));
	CHECK(r.file_name == UnicodeString(_T("backup.zip")));
	CHECK(r.archive_path == UnicodeString(_T("D:\\out\\backup.zip")));
	CHECK(r.core_compatible);
}

TEST_CASE("pack_settings: パスワード・追加スイッチは core では未実装扱い")
{
	pack_settings::Availability av;
	pack_settings::Options o;
	o.name = _T("backup");
	o.password = _T("secret");
	const pack_settings::Resolved r = pack_settings::Resolve(o, av);
	CHECK_FALSE(r.core_compatible);
	CHECK(r.unsupported_reason.Pos(_T("パスワード")) > 0);

	o.password = EmptyStr;
	o.extra_switches = _T("-mx=9");
	const pack_settings::Resolved sw = pack_settings::Resolve(o, av);
	CHECK_FALSE(sw.core_compatible);
	CHECK(sw.unsupported_reason.Pos(_T("追加スイッチ")) > 0);
}
