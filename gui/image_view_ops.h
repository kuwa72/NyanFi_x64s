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

/**
 * @brief スクロール位置を1刻み進める (端では clamp して留まる)
 * @details src/MainFrm.cpp の ScrollUpI/ScrollDownI/ScrollLeft/ScrollRight
 * ActionExecute は `ScrollBar->Position ±= Increment` で、範囲外は VCL が
 * clamp する。ここではその clamp を明示化した
 * @param pos 現在位置、lo/hi 位置の範囲、step 刻み幅、dir +1/-1
 */
int ScrollStepPos(int pos, int lo, int hi, int step, int dir);

/**
 * @brief サムネイルのページ移動 (page 件ずつ進めて範囲外は clamp)
 * @details src/MainFrm.cpp の NextPage/PrevPage/PageUpI/PageDownI は
 * グリッドの表示件数分だけ進めて SetThumbnailIndex で clamp するのと同じ
 */
int PageStepIndex(int cur, int count, int page, int dir);

/**
 * @brief 見開き表示の2件ずつ移動 (端では cur のまま留まる)
 * @details src/MainFrm.cpp::NextPrevFileICore の IsDoubleStep 分岐と同じ。
 * 次: 末尾とその手前では留まる。前: 先頭では留まる、先頭付近 (1) は先頭へ
 */
int DoubleStepIndex(int count, int cur, int dir);

/**
 * @brief 表示トグルの次状態 (VCL の SetToggleAction と同じ)
 * @details src/MainFrm.cpp:12651。param が "ON" なら true、"OFF" なら
 * false、それ以外 (空の通常起動を含む) は反転する
 */
bool ToggleViewFlag(bool cur, const UnicodeString &param);

/**
 * @brief 見開きの綴じ方向の次状態 (右綴じ=true)
 * @details src/MainFrm.cpp::PageBindActionExecute と同じ。"R" なら右綴じ、
 * "L" なら左綴じ、それ以外は反転する
 */
bool NextPageBind(bool right_bind, const UnicodeString &param);

/**
 * @brief アクションパラメータに指定トークンが含まれるか
 * @details src/MainFrm.cpp::TestActionParam と同じ (";" 区切りの完全一致・
 * 大文字小文字を区別しない)
 */
bool HasActionToken(const UnicodeString &param, const UnicodeString &token);

/**
 * @brief 補間アルゴリズムの次状態 (VCL の SetInterpolation と同じ)
 * @details src/MainFrm.cpp::SetInterpolationActionExecute と同じ。
 * idstr="NLCFHX" の中を param で絞った順に進め、端では先頭へ周回する
 * (VCL の imgv_thread.cpp:245 の表示とも対応)。param が空・候補なしは
 * nullopt (VCL の UserAbort 相当)
 */
std::optional<int> NextInterpolation(int cur, const UnicodeString &param);

/**
 * @brief 壁紙に使うパス (VCL の LoadBgImage と同じ。指定があればそれ、
 * 無ければカーソル位置。どちらも無ければ空)
 */
UnicodeString ResolveBgImagePath(const UnicodeString &param, const UnicodeString &cursor);

/**
 * @brief サブビューアを隠すべきか (VCL の SubViewerActionExecute と同じ。
 * 表示中かつ OFF/空なら隠す)
 */
bool ShouldHideSubViewer(bool visible, const UnicodeString &param);

/**
 * @brief サブビューアの回転操作コード (同。RL→3/RR→1/FH→4/FV→5。
 * SubView.cpp の RotateImage 引数と同値。それ以外は 0=回転なし)
 */
int SubViewerRotateCode(const UnicodeString &param);

/**
 * @brief 別インスタンス起動時に複製すべきか (VCL の NextNyanFi の "DN" 分岐)
 */
bool ShouldDuplicateOnNext(const UnicodeString &param);

/**
 * @brief 類似画像ソートの基準サイズ (VCL の SimilarImage と同じ。既定 32、
 * 範囲外は nullopt = UserAbort 相当。CB 等のトークンは無視する)
 */
std::optional<int> ParseSimilarImageSize(const UnicodeString &param);

/** @brief クリップボード転送元 (VCL の ClipCopyActionExecute と同じ区分) */
enum class ClipCopySrc {
	None,   //!< 転送できる画像がない
	Image,  //!< 表示中の画像バッファ (ImgBuff 相当)
	Viewer, //!< ビューア表示内容 ("VI" 指定。別途描画が必要なため警告扱い)
};

/**
 * @brief クリップボード転送元を決める (画像なしは None、"VI" 指定は Viewer、
 * それ以外は Image)
 */
ClipCopySrc ResolveClipCopySource(bool has_image, const UnicodeString &param);

/**
 * @brief 編集対象パス (VCL の FileEdit と同じ。指定があればそれ、
 * 無ければカーソル位置。どちらも無ければ空)
 */
UnicodeString ResolveFileEditPath(const UnicodeString &param, const UnicodeString &cursor);

/**
 * @brief コマンドファイル一覧をフィルタ付きで開くか
 * (VCL の CmdFileList の "FF" 分岐)
 */
bool ShouldShowCmdFileFilter(const UnicodeString &param);

/**
 * @brief メインメニューポップアップの対象インデックス
 * @details src/MainFrm.cpp::PopupMainMenuActionExecute と同じ。空は全体
 * (-1)、先頭1文字を "FESVVVLTOH" で探す。見つからなければ -1
 */
int PopupMenuIndex(const UnicodeString &param);

}  // namespace image_view_ops

#endif  // NYANFI_GUI_IMAGE_VIEW_OPS_H
