/**
 * @file tests/core/test_gui_image_view_ops.cpp
 * @brief gui/image_view_ops.cpp (Iモード画像操作のwx非依存ロジック) の回帰テスト
 *
 * @details VCL 版の対応箇所:
 *   - Exif Orientation→回転 (src/imgv_thread.cpp:724-728、
 *     src/thumb_thread.cpp:190-194、src/SubView.cpp:132-136 の3箇所とも
 *     6→右90/3→180/8→左90、それ以外は無視)
 *   - ズーム段階 (src/Global.cpp の ZoomRatioList 既定値 + src/MainFrm.cpp の
 *     ZoomInIActionExecute/ZoomOutIActionExecute の探索)
 *   - 次前移動・Top/End (src/MainFrm.cpp::NextPrevFileICore/TopFile/EndFile)
 *   - JumpIndex (src/MainFrm.cpp::JumpIndexActionExecute)
 *   - フィット倍率上限 (src/Global.cpp 既定 ImgFitMaxZoom=100)
 *   - グレースケール (WIC の 8bppGray 変換 ≒ BT.601 輝度)
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/image_view_ops.h"
#include "usr_str.h"

using namespace image_view_ops;

// Exif Orientation → 右回り90度単位 (VCL 3箇所と同一対応)
TEST_CASE("ExifOrientationToRotCw maps 6/3/8 only")
{
	CHECK(ExifOrientationToRotCw(1) == 0);
	CHECK(ExifOrientationToRotCw(6) == 1);  // 右90
	CHECK(ExifOrientationToRotCw(3) == 2);  // 180
	CHECK(ExifOrientationToRotCw(8) == 3);  // 左90
	// VCL はミラー系 (2/4/5/7)・範囲外を無視する
	CHECK(ExifOrientationToRotCw(2) == 0);
	CHECK(ExifOrientationToRotCw(4) == 0);
	CHECK(ExifOrientationToRotCw(5) == 0);
	CHECK(ExifOrientationToRotCw(7) == 0);
	CHECK(ExifOrientationToRotCw(0) == 0);
	CHECK(ExifOrientationToRotCw(99) == 0);
}

// Exif 回転の適用可否 (VCL は Exif 対応拡張子かつ WIC標準形式のみ適用)。
// FEXT_EXIF(FEXT_JPEG+RAW+heic) ∩ FEXT_WICSTD(JPEG+bmp/png/gif/tif/…) =
// JPEG 系のみ true (bmp/png/tif は Exif 対象外、RAW/heic/webp は WIC任せ)
TEST_CASE("ShouldApplyExifRotation follows VCL conditions")
{
	CHECK(ShouldApplyExifRotation(_T("photo.jpg")));
	CHECK(ShouldApplyExifRotation(_T("photo.JPEG")));
	CHECK(ShouldApplyExifRotation(_T("photo.jfif")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.bmp")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.png")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.tif")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.txt")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.wmf")));  // メタファイル
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.cr2")));  // RAW は WIC任せ
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.heic")));
	CHECK_FALSE(ShouldApplyExifRotation(_T("photo.webp")));
}

namespace {

// 2x2 RGB (上から): 赤(10,0,0) 緑(0,20,0) / 青(0,0,30) 白(40,50,60)
std::vector<unsigned char> make_2x2()
{
	return {
		10, 0, 0, 0, 20, 0,
		0, 0, 30, 40, 50, 60,
	};
}

}  // namespace

TEST_CASE("TransformedSize swaps on odd quarter turns")
{
	Transform t;
	CHECK(TransformedSize(3, 2, t).w == 3);
	CHECK(TransformedSize(3, 2, t).h == 2);
	t.rot_cw = 1;
	CHECK(TransformedSize(3, 2, t).w == 2);
	CHECK(TransformedSize(3, 2, t).h == 3);
	t.rot_cw = 2;
	CHECK(TransformedSize(3, 2, t).w == 3);
	CHECK(TransformedSize(3, 2, t).h == 2);
	t.rot_cw = 3;
	CHECK(TransformedSize(3, 2, t).w == 2);
	CHECK(TransformedSize(3, 2, t).h == 3);
}

TEST_CASE("TransformBuffer identity keeps pixels")
{
	const auto src = make_2x2();
	Transform t;
	const PixelBuf dst = TransformBuffer(src.data(), 2, 2, t);
	CHECK(dst.w == 2);
	CHECK(dst.h == 2);
	CHECK(dst.rgb == src);
}

TEST_CASE("TransformBuffer rot_cw=1 (right 90)")
{
	const auto src = make_2x2();
	Transform t;
	t.rot_cw = 1;
	const PixelBuf dst = TransformBuffer(src.data(), 2, 2, t);
	// 上段: 青 赤 / 下段: 白 緑
	const std::vector<unsigned char> expected = {
		0, 0, 30, 10, 0, 0,
		40, 50, 60, 0, 20, 0,
	};
	CHECK(dst.rgb == expected);
}

TEST_CASE("TransformBuffer rot_cw=2 (180)")
{
	const auto src = make_2x2();
	Transform t;
	t.rot_cw = 2;
	const PixelBuf dst = TransformBuffer(src.data(), 2, 2, t);
	// 上段: 白 青 / 下段: 緑 赤
	const std::vector<unsigned char> expected = {
		40, 50, 60, 0, 0, 30,
		0, 20, 0, 10, 0, 0,
	};
	CHECK(dst.rgb == expected);
}

TEST_CASE("TransformBuffer flip_h mirrors horizontally")
{
	const auto src = make_2x2();
	Transform t;
	t.flip_h = true;
	const PixelBuf dst = TransformBuffer(src.data(), 2, 2, t);
	// 上段: 緑 赤 / 下段: 白 青
	const std::vector<unsigned char> expected = {
		0, 20, 0, 10, 0, 0,
		40, 50, 60, 0, 0, 30,
	};
	CHECK(dst.rgb == expected);
}

TEST_CASE("TransformBuffer flip_v mirrors vertically")
{
	const auto src = make_2x2();
	Transform t;
	t.flip_v = true;
	const PixelBuf dst = TransformBuffer(src.data(), 2, 2, t);
	// 上段: 青 白 / 下段: 赤 緑
	const std::vector<unsigned char> expected = {
		0, 0, 30, 40, 50, 60,
		10, 0, 0, 0, 20, 0,
	};
	CHECK(dst.rgb == expected);
}

TEST_CASE("RotCwStep composes quarter turns")
{
	CHECK(RotCwStep(0, true) == 1);
	CHECK(RotCwStep(3, true) == 0);
	CHECK(RotCwStep(0, false) == 3);
	CHECK(RotCwStep(1, false) == 0);
}

TEST_CASE("ApplyGrayscale uses BT.601 luma")
{
	// 純赤 (255,0,0) → 0.299*255 ≒ 76
	std::vector<unsigned char> rgb = {255, 0, 0, 0, 0, 0};
	ApplyGrayscale(rgb);
	CHECK(rgb[0] == 76);
	CHECK(rgb[1] == 76);
	CHECK(rgb[2] == 76);
	// 黒は黒のまま
	CHECK(rgb[3] == 0);
	CHECK(rgb[4] == 0);
	CHECK(rgb[5] == 0);
}

// ズーム段階 (ZoomRatioList 既定値 + VCL の z_over 挙動=端では動かない)
TEST_CASE("NextZoomStep follows ZoomRatioList steps")
{
	CHECK(NextZoomStep(100, 1) == 150);
	CHECK(NextZoomStep(100, -1) == 75);
	CHECK(NextZoomStep(110, 1) == 150);
	CHECK(NextZoomStep(110, -1) == 100);
	CHECK(NextZoomStep(400, 1) == 400);  // 上端 (z_over: 何もしない)
	CHECK(NextZoomStep(10, -1) == 10);   // 下端 (z_over: 何もしない)
	CHECK(NextZoomStep(5, -1) == 5);     // 段階外でも端では動かない
	CHECK(NextZoomStep(500, 1) == 500);
}

// フィット倍率 (等倍を超えて拡大しない = ImgFitMaxZoom 既定100)
TEST_CASE("FitRatio fits aspect and caps at 1.0")
{
	CHECK(FitRatio(800, 600, 400, 300) == doctest::Approx(0.5));
	CHECK(FitRatio(800, 600, 400, 200) == doctest::Approx(1.0 / 3.0));
	CHECK(FitRatio(100, 100, 400, 400) == doctest::Approx(1.0));  // 拡大しない
	CHECK(FitRatio(0, 100, 400, 400) == doctest::Approx(1.0));
	CHECK(FitRatio(100, 100, 0, 0) == doctest::Approx(1.0));
}

// 次前移動 (失敗ファイルを飛ばす・端では留まる・loop で周回)
TEST_CASE("NextPrevIndex skips failed and stops at ends")
{
	const std::vector<char> failed = {0, 0, 0, 1, 0};
	CHECK(NextPrevIndex(5, 2, 1, failed, false) == 4);  // idx3 を飛ばす
	CHECK(NextPrevIndex(5, 2, -1, failed, false) == 1);
	CHECK(NextPrevIndex(5, 4, 1, failed, false) == 4);  // 末端で留まる
	CHECK(NextPrevIndex(5, 0, -1, failed, false) == 0);  // 先端で留まる
	CHECK(NextPrevIndex(5, 4, 1, failed, true) == 0);   // loop で周回
	CHECK(NextPrevIndex(5, 0, -1, failed, true) == 4);
	const std::vector<char> all_failed = {1, 1};
	CHECK(NextPrevIndex(2, 0, 1, all_failed, true) == 0);
}

TEST_CASE("FirstValidIndex and LastValidIndex")
{
	const std::vector<char> failed = {1, 0, 0, 1};
	CHECK(FirstValidIndex(failed) == 1);
	CHECK(LastValidIndex(failed) == 2);
	const std::vector<char> none = {1, 1};
	CHECK(FirstValidIndex(none) == -1);
	CHECK(LastValidIndex(none) == -1);
	CHECK(FirstValidIndex({}) == -1);
}

// JumpIndex (空・0は中止、+-は相対、1-based 絶対、範囲外は clamp)
TEST_CASE("ParseJumpIndex handles absolute and relative")
{
	CHECK(ParseJumpIndex(_T("3"), 5, 0) == 2);
	CHECK(ParseJumpIndex(_T("+2"), 5, 1) == 3);
	CHECK(ParseJumpIndex(_T("-1"), 5, 4) == 3);
	CHECK(ParseJumpIndex(_T("99"), 5, 0) == 4);
	CHECK(ParseJumpIndex(_T("+99"), 5, 0) == 4);
	CHECK(ParseJumpIndex(_T("-99"), 5, 4) == 0);
	CHECK_FALSE(ParseJumpIndex(_T(""), 5, 0).has_value());
	CHECK_FALSE(ParseJumpIndex(_T("0"), 5, 0).has_value());
	CHECK_FALSE(ParseJumpIndex(_T("abc"), 5, 0).has_value());
}
