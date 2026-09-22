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

// 画像スクロール (VCL は ImgScrollBox の ScrollBar Position ±= Increment。
// src/MainFrm.cpp::ScrollUpI/ScrollDownI/ScrollLeft/ScrollRightActionExecute)
TEST_CASE("ScrollStepPos moves by step and clamps")
{
	CHECK(ScrollStepPos(50, 0, 200, 20, 1) == 70);
	CHECK(ScrollStepPos(50, 0, 200, 20, -1) == 30);
	CHECK(ScrollStepPos(190, 0, 200, 20, 1) == 200);  // 上端で clamp
	CHECK(ScrollStepPos(10, 0, 200, 20, -1) == 0);    // 下端で clamp
	CHECK(ScrollStepPos(0, 0, 0, 20, 1) == 0);        // 動けない範囲
	CHECK(ScrollStepPos(50, 0, 200, 0, 1) == 50);     // 刻み0は動かない
}

// サムネイルのページ移動 (VCL の NextPage/PrevPage/PageUpI/PageDownI は
// グリッドの表示件数分だけ進めて SetThumbnailIndex で clamp する)
TEST_CASE("PageStepIndex moves by page and clamps")
{
	CHECK(PageStepIndex(5, 20, 10, 1) == 15);
	CHECK(PageStepIndex(15, 20, 10, -1) == 5);
	CHECK(PageStepIndex(15, 20, 10, 1) == 19);  // 末尾で clamp
	CHECK(PageStepIndex(5, 20, 10, -1) == 0);   // 先頭で clamp
	CHECK(PageStepIndex(0, 1, 10, 1) == 0);
	CHECK(PageStepIndex(3, 0, 10, 1) == 3);  // 空は動かない
	CHECK(PageStepIndex(5, 20, 0, 1) == 5);  // ページ0は動かない
}

// 見開き時の2件ずつ移動 (src/MainFrm.cpp::NextPrevFileICore の IsDoubleStep 分岐。
// 端では留まる = cur を返す)
TEST_CASE("DoubleStepIndex moves by 2 and stays at ends")
{
	CHECK(DoubleStepIndex(7, 1, 1) == 3);
	CHECK(DoubleStepIndex(7, 3, -1) == 1);
	CHECK(DoubleStepIndex(7, 5, 1) == 5);   // max(=count-2) では留まる
	CHECK(DoubleStepIndex(7, 6, 1) == 6);   // 末尾では留まる
	CHECK(DoubleStepIndex(7, 1, -1) == 0);  // 先頭付近 (1) では先頭へ
	CHECK(DoubleStepIndex(7, 0, -1) == 0);  // 先頭では留まる
	CHECK(DoubleStepIndex(1, 0, 1) == 0);
	CHECK(DoubleStepIndex(0, 0, 1) == 0);
}

// 表示トグル (VCL の SetToggleAction: src/MainFrm.cpp:12651。ON で true、
// OFF で false、それ以外は反転。Histogram/Loupe/Thumbnail/ThumbnailEx/
// DoublePage/WarnHighlight/ShowSeekBar/Sidebar の共通動作)
TEST_CASE("ToggleViewFlag follows SetToggleAction")
{
	CHECK(ToggleViewFlag(false, _T("")) == true);
	CHECK(ToggleViewFlag(true, _T("")) == false);
	CHECK(ToggleViewFlag(false, _T("ON")) == true);
	CHECK(ToggleViewFlag(true, _T("ON")) == true);
	CHECK(ToggleViewFlag(true, _T("OFF")) == false);
	CHECK(ToggleViewFlag(false, _T("OFF")) == false);
}

// 見開きの綴じ方向 (src/MainFrm.cpp::PageBindActionExecute。"R" で右綴じ、
// "L" で左綴じ、それ以外は反転)
TEST_CASE("NextPageBind follows PageBindActionExecute")
{
	CHECK(NextPageBind(false, _T("")) == true);
	CHECK(NextPageBind(true, _T("")) == false);
	CHECK(NextPageBind(false, _T("R")) == true);
	CHECK(NextPageBind(true, _T("R")) == true);
	CHECK(NextPageBind(true, _T("L")) == false);
	CHECK(NextPageBind(false, _T("L")) == false);
}

// アクションパラメータのトークン有無 (VCL の TestActionParam と同じ。
// ";" 区切りの完全一致・大文字小文字を区別しない)
TEST_CASE("HasActionToken matches TestActionParam")
{
	CHECK(HasActionToken(_T("DN"), _T("DN")));
	CHECK(HasActionToken(_T("dn"), _T("DN")));
	CHECK(HasActionToken(_T("CB;RL"), _T("RL")));
	CHECK_FALSE(HasActionToken(_T(""), _T("DN")));
	CHECK_FALSE(HasActionToken(_T("D"), _T("DN")));
	CHECK_FALSE(HasActionToken(_T("DNA"), _T("DN")));
}

// 補間アルゴリズムの切替 (src/MainFrm.cpp::SetInterpolationActionExecute と同じ。
// idstr="NLCFHX" の中を param で絞った順に進め、端では先頭へ周回。
// param 空・不正は nullopt = VCL の UserAbort 相当)
TEST_CASE("NextInterpolation cycles within param filter")
{
	CHECK(NextInterpolation(3, _T("NLCFHX")) == 4);
	CHECK(NextInterpolation(5, _T("NLCFHX")) == 0);  // 端では先頭へ
	CHECK(NextInterpolation(0, _T("NC")) == 2);      // "N" の次は "C"
	CHECK(NextInterpolation(2, _T("NC")) == 0);      // "C" は末尾なので先頭へ
	CHECK(NextInterpolation(3, _T("NC")) == 0);      // 現在値が候補外なら先頭へ
	CHECK_FALSE(NextInterpolation(3, _T("")).has_value());
	CHECK_FALSE(NextInterpolation(3, _T("Z")).has_value());
}

// 壁紙パス (src/MainFrm.cpp::LoadBgImageActionExecute と同じ。指定があれば
// それ、無ければカーソル位置。どちらも無ければ空)
TEST_CASE("ResolveBgImagePath prefers param over cursor")
{
	CHECK(ResolveBgImagePath(_T("wall.jpg"), _T("cur.jpg")) == _T("wall.jpg"));
	CHECK(ResolveBgImagePath(_T(""), _T("cur.jpg")) == _T("cur.jpg"));
	CHECK(ResolveBgImagePath(_T(""), _T("")).IsEmpty());
}

// サブビューアの表示切替 (src/MainFrm.cpp::SubViewerActionExecute と同じ。
// 表示中かつ OFF/空なら隠す)
TEST_CASE("ShouldHideSubViewer hides only when visible")
{
	CHECK(ShouldHideSubViewer(true, _T("")) == true);
	CHECK(ShouldHideSubViewer(true, _T("OFF")) == true);
	CHECK(ShouldHideSubViewer(true, _T("RL")) == false);
	CHECK(ShouldHideSubViewer(false, _T("")) == false);
	CHECK(ShouldHideSubViewer(false, _T("OFF")) == false);
}

// サブビューアの回転操作 (同・RL→3/RR→1/FH→4/FV→5、それ以外は 0)
TEST_CASE("SubViewerRotateCode follows rotate params")
{
	CHECK(SubViewerRotateCode(_T("RL")) == 3);
	CHECK(SubViewerRotateCode(_T("RR")) == 1);
	CHECK(SubViewerRotateCode(_T("FH")) == 4);
	CHECK(SubViewerRotateCode(_T("FV")) == 5);
	CHECK(SubViewerRotateCode(_T("")) == 0);
	CHECK(SubViewerRotateCode(_T("CB")) == 0);
}

// 別インスタンス起動の複製要求 (src/MainFrm.cpp::NextNyanFiActionExecute の
// "DN" 分岐と同じ)
TEST_CASE("ShouldDuplicateOnNext detects DN token")
{
	CHECK(ShouldDuplicateOnNext(_T("DN")) == true);
	CHECK(ShouldDuplicateOnNext(_T("")) == false);
	CHECK(ShouldDuplicateOnNext(_T("XX")) == false);
}

// 類似画像ソートのサイズ (src/MainFrm.cpp::SimilarImageActionExecute と同じ。
// 既定 32、範囲外 4..120 は nullopt = VCL の UserAbort 相当。CB 等の
// トークンは無視する)
TEST_CASE("ParseSimilarImageSize validates 4..120")
{
	CHECK(ParseSimilarImageSize(_T("")) == 32);
	CHECK(ParseSimilarImageSize(_T("64")) == 64);
	CHECK(ParseSimilarImageSize(_T("64;CC")) == 64);
	CHECK(ParseSimilarImageSize(_T("CB")) == 32);
	CHECK_FALSE(ParseSimilarImageSize(_T("3")).has_value());
	CHECK_FALSE(ParseSimilarImageSize(_T("121")).has_value());
	CHECK_FALSE(ParseSimilarImageSize(_T("abc")).has_value());
}

// クリップボード転送元 (src/MainFrm.cpp::ClipCopyActionExecute と同じ。
// 画像なしは None、"VI" 指定は Viewer、それ以外は Image)
TEST_CASE("ResolveClipCopySource selects image or viewer")
{
	CHECK(ResolveClipCopySource(false, _T("")) == ClipCopySrc::None);
	CHECK(ResolveClipCopySource(true, _T("")) == ClipCopySrc::Image);
	CHECK(ResolveClipCopySource(true, _T("VI")) == ClipCopySrc::Viewer);
	CHECK(ResolveClipCopySource(false, _T("VI")) == ClipCopySrc::None);
}

// 編集対象パス (VCL の FileEdit は指定があればそれ、無ければカーソル位置)
TEST_CASE("ResolveFileEditPath prefers param over cursor")
{
	CHECK(ResolveFileEditPath(_T("a.txt"), _T("b.txt")) == _T("a.txt"));
	CHECK(ResolveFileEditPath(_T(""), _T("b.txt")) == _T("b.txt"));
	CHECK(ResolveFileEditPath(_T(""), _T("")).IsEmpty());
}

// コマンドファイル一覧のフィルタ付き表示
// (src/MainFrm.cpp::CmdFileListActionExecute の "FF" 分岐と同じ)
TEST_CASE("ShouldShowCmdFileFilter detects FF token")
{
	CHECK(ShouldShowCmdFileFilter(_T("FF")) == true);
	CHECK(ShouldShowCmdFileFilter(_T("")) == false);
}

// メインメニューポップアップの対象 (src/MainFrm.cpp::
// PopupMainMenuActionExecute と同じ。空は全体(-1)、先頭1文字を
// "FESVVVLTOH" で探す。見つからなければ -1)
TEST_CASE("PopupMenuIndex follows FESVVVLTOH lookup")
{
	CHECK(PopupMenuIndex(_T("")) == -1);
	CHECK(PopupMenuIndex(_T("F")) == 0);
	CHECK(PopupMenuIndex(_T("V")) == 3);
	CHECK(PopupMenuIndex(_T("H")) == 9);
	CHECK(PopupMenuIndex(_T("Z")) == -1);
}
