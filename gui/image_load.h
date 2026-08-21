/**
 * @file gui/image_load.h
 * @brief 画像ビューアのロジック層 (wx 非依存)
 *
 * @details 画像のデコードは自前で書かず、移植済みの WIC ラッパー
 * (src/usr_wic.cpp、cmake/phase0_sources.cmake でビルド対象) をそのまま使う。
 * ここでは
 *   - 対応拡張子の判定 (test_FileExt + FEXT_WICSTD/FEXT_RAW ベース)
 *   - ファイルの読み込み (WIC_get_img_size でサイズ確認 → WIC_load_image で
 *     24bpp BGR にデコード → 表示用に上から詰めた RGB24 バッファへ変換)
 * だけを切り出してあり、gui/file_pane.h に対する gui/file_item.h と同じ関係で
 * tests/core/test_gui_image_load.cpp から直接テストできる
 * (nyanfi_gui_core、ルート CMakeLists.txt に追加)。
 *
 * 大きな画像で固まらないための方針 (要件6):
 * WIC_load_image は元のピクセルサイズのまま全展開してから RGB バッファを
 * 確保するため、巨大な画像 (数億ピクセル級) を無条件に読むとメモリ確保と
 * デコードだけで長時間固まりうる。VCL 版 (src/imgv_thread.cpp) は専用スレッド
 * (TImgViewThread) でこれを吸収しているが、Phase 2 骨格の画像ビューアは
 * スレッド化していない (推測・要検証の設計判断。詳細は報告を参照)。
 * 代わりに WIC_get_img_size (デコード前にヘッダ相当の情報だけを読む、軽い
 * 呼び出し) で事前に総ピクセル数を確認し、kMaxDecodePixels を超えるものは
 * デコードせずにエラーとして扱う (推測・要検証。VCL 版に相当する明確な上限値は
 * 見当たらなかったため、フリーズを避けられる範囲で新規に決めた値)。
 */
#ifndef NYANFI_GUI_IMAGE_LOAD_H
#define NYANFI_GUI_IMAGE_LOAD_H

#include <vector>

#include "usr_str.h"

namespace image_load {

/**
 * @brief これを超えるピクセル数 (幅×高さ) の画像はデコードせずエラーにする
 * @details 推測・要検証の新規値。200,000,000 (200メガピクセル) は 24bpp で
 * 約600MBのデコードバッファに相当し、これより大きいと確保・デコードだけで
 * 数秒〜、環境によってはそれ以上かかりうる。VCL 版に相当する明確な上限値は
 * 見当たらなかった (imgv_thread.cpp はスレッド化で対応しており、上限値という
 * 形の対策を持たない)。
 */
constexpr Int64 kMaxDecodePixels = 200'000'000LL;

/** @brief LoadForView() の結果 */
struct LoadResult {
	bool ok = false;         //!< 読み込みに成功したか
	UnicodeString error;      //!< 失敗時のメッセージ (ok=false のときのみ)

	unsigned int width = 0;   //!< 幅(px)
	unsigned int height = 0;  //!< 高さ(px)

	/// 上から詰めた RGB24 (パディング無し、width*height*3 バイト、行の並びは
	/// 画像の上から下)。gui/image_viewer.cpp で wxImage の生バッファとして使う
	std::vector<unsigned char> rgb;
};

/**
 * @brief 拡張子から WIC 経由で表示できる可能性がある画像かどうかを判定する
 * @details src/usr_file_inf.h の FEXT_IMAGE (FEXT_WICSTD + FEXT_RAW + META +
 * heic/webp) からメタファイル (.wmf/.emf、FEXT_META) を除いたもの。
 * メタファイルは TMetafile::LoadFromFile が未移植 (呼ばれたら落とす扱い。
 * gui/usr_file_inf_link_shim.cpp) のため対象外にしている (報告に明記)。
 * RAW 形式 (FEXT_RAW) は対応コーデックが無い環境では WIC_load_image が
 * 失敗するだけなので、ここでは楽観的に true を返す (実際に開けるかは
 * LoadForView の戻り値で判断する)。
 * @param fnam ファイル名 (拡張子だけ見る。パスの有無は問わない)
 */
bool IsSupportedExt(const UnicodeString &fnam);

/**
 * @brief 画像ファイルを読み込む
 * @details WIC_get_img_size → (サイズ確認) → WIC_load_image (usr_wic.cpp、
 * WICIMG_PREVIEW) の順に呼ぶ。ファイルが存在しない、画像として認識できない、
 * kMaxDecodePixels を超える、デコードに失敗する (壊れたファイル・未対応
 * コーデック) のいずれでも例外を投げず ok=false を返す (要件7)。
 * @param path ファイルパス
 * @return LoadResult
 */
LoadResult LoadForView(const UnicodeString &path);

}  // namespace image_load

#endif  // NYANFI_GUI_IMAGE_LOAD_H
