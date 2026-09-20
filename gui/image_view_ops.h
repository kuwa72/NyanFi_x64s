/**
 * @file gui/image_view_ops.h
 * @brief Iモード画像操作の判断ロジック層 (wx 非依存)
 *
 * @details VCL 版の対応箇所 (いずれも src を実測):
 *   - Exif Orientation→回転: src/imgv_thread.cpp:724-728、
 *     src/thumb_thread.cpp:190-194、src/SubView.cpp:132-136。3箇所とも
 *     ori==6→右90 / ori==3→180 / ori==8→左90、それ以外 (ミラー系 2/4/5/7
 *     を含む) は無視する。ここもそれに合わせる
 *   - 適用条件: Exif 対応拡張子 (test_ExifExt) かつ WIC標準形式
 *     (FEXT_WICSTD)。VCL は load_ImageFile の戻り値が LOADED_BY_WIC
 *     (≒非標準形式、src/Global.cpp::load_ImageFile) のとき Exif 回転を
 *     適用しない (SubView.cpp:136、imgv_thread.cpp:724、
 *     thumb_thread.cpp:176 の `res!=LOADED_BY_WIC` 条件と同値)
 *   - ズーム段階: src/Global.cpp の ZoomRatioList 既定値
 *     (10/25/50/75/100/150/200/300/400) + src/MainFrm.cpp の
 *     ZoomInIActionExecute/ZoomOutIActionExecute の探索 (端では動かない
 *     "z_over" 判定を含む)
 *   - 次前移動・先頭/末尾: src/MainFrm.cpp::NextPrevFileICore/TopFile/
 *     EndFile (失敗ファイルを飛ばす・端では留まる・LoopViewCursor での周回)
 *   - JumpIndex: src/MainFrm.cpp::JumpIndexActionExecute (空は中止・
 *     "+-n" は相対・それ以外は1-based絶対・範囲外は clamp)
 *   - フィット倍率の上限: src/Global.cpp 既定 ImgFitMaxZoom=100
 *     (等倍を超えて自動拡大しない)
 *   - グレースケール: WIC の 8bppGray 変換 (src/usr_wic.cpp の
 *     WIC_grayscale_image) は BT.601 輝度相当なので、ここでも
 *     Y=0.299R+0.587G+0.114B を使う (推測・要検証の近似)
 *
 * tests/core/test_gui_image_view_ops.cpp から直接テストできる
 * (nyanfi_gui_core、ルート CMakeLists.txt に追加)。
 */
#ifndef NYANFI_GUI_IMAGE_VIEW_OPS_H
#define NYANFI_GUI_IMAGE_VIEW_OPS_H

#include <optional>
#include <vector>

#include "usr_str.h"

namespace image_view_ops {

/** @brief 回転・反転の合成状態 (WIC_rotate_image の rot_opt 1〜5 に対応) */
struct Transform {
	int rot_cw = 0;       //!< 右回り90度単位 (0〜3。1=右90/2=180/3=左90)
	bool flip_h = false;  //!< 左右反転 (rot_opt=4)
	bool flip_v = false;  //!< 上下反転 (rot_opt=5)
};

/** @brief 変換後の画像サイズ */
struct ImageSize {
	unsigned int w = 0;
	unsigned int h = 0;
};

/** @brief 変換後のピクセルバッファ (上から詰めた RGB24) */
struct PixelBuf {
	unsigned int w = 0;
	unsigned int h = 0;
	std::vector<unsigned char> rgb;
};

/**
 * @brief Exif Orientation 値 (1〜8) を右回り90度単位に変換する
 * @details VCL 3箇所と同一対応。ミラー系・範囲外は 0 (無視)
 */
int ExifOrientationToRotCw(int ori);

/**
 * @brief Exif Orientation に基づく自動回転を適用すべきか
 * @details VCL の `res!=LOADED_BY_WIC` 条件と同値 (Exif 対応拡張子かつ
 * WIC標準形式のみ true)
 */
bool ShouldApplyExifRotation(const UnicodeString &fnam);

/** @brief 変換後のサイズ (rot_cw が奇数なら縦横が入れ替わる) */
ImageSize TransformedSize(unsigned int w, unsigned int h, const Transform &t);

/**
 * @brief RGB24 バッファに回転・反転を適用する (先に回転、後に反転)
 * @param src 上から詰めた RGB24 (w*h*3 バイト)
 */
PixelBuf TransformBuffer(const unsigned char *src, unsigned int w, unsigned int h, const Transform &t);

/** @brief 右回り (+1) / 左回り (-1) に90度進めた rot_cw 値 */
int RotCwStep(int rot_cw, bool right);

/** @brief RGB24 バッファをグレースケール化する (BT.601 輝度、in-place) */
void ApplyGrayscale(std::vector<unsigned char> &rgb);

/**
 * @brief ズーム段階の次/前 (ZoomRatioList 既定値の探索。端では cur を返す)
 * @param cur 現在の倍率(%)。段階の間の値でもよい (VCL と同じく次/前の段階へ)
 * @param dir +1:ズームイン / -1:ズームアウト
 */
int NextZoomStep(int cur, int dir);

/**
 * @brief フィット表示の倍率 (アスペクト比を保って収める。等倍超えは 1.0)
 * @details src/UserFunc.cpp の get_ZoomRatio は回転表示等まで考慮する複雑な式だが、
 * その定義元は GUI グローバル依存で未移植のため、ここでは標準的な式に
 * 簡略化した (推測・要検証)。等倍を超えて自動拡大しない点は
 * src/Global.cpp の既定値 ImgFitMaxZoom=100 と同じ
 */
double FitRatio(unsigned int img_w, unsigned int img_h, int avail_w, int avail_h);

/**
 * @brief 次/前の有効なインデックス (失敗項目を飛ばす)
 * @details 移動先が無ければ cur を返す (VCL はその場合 REDRAW のみで留まる)。
 * @param count 一覧の件数
 * @param cur 現在のインデックス
 * @param dir +1:次 / -1:前
 * @param failed 失敗フラグ (count 件。true=失敗=飛ばす)
 * @param loop 端で周回するか (VCL の LoopViewCursor 相当)
 */
int NextPrevIndex(int count, int cur, int dir, const std::vector<char> &failed, bool loop);

/** @brief 先頭の有効なインデックス (無ければ -1。TopFile 相当) */
int FirstValidIndex(const std::vector<char> &failed);

/** @brief 末尾の有効なインデックス (無ければ -1。EndFile 相当) */
int LastValidIndex(const std::vector<char> &failed);

/**
 * @brief JumpIndex パラメータを0-basedインデックスに変換する
 * @details 空・0・非数値は nullopt (VCL は Abort)。"+-n" は cur からの相対、
 * それ以外は1-based絶対。範囲外は clamp する
 */
std::optional<int> ParseJumpIndex(const UnicodeString &param, int count, int cur);

}  // namespace image_view_ops

#endif  // NYANFI_GUI_IMAGE_VIEW_OPS_H
