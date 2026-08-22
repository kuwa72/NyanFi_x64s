/**
 * @file gui/convert_ops.h
 * @brief 抽出と変換 (アイコン・埋め込み画像・文書→テキスト・画像形式・Exif)
 *
 * @details **中身の処理は自前で書かない。**すべて移植済みのものを呼ぶだけ:
 *
 *          | 機能 | 使う移植済みコード |
 *          |---|---|
 *          | Exif 撮影日時をタイムスタンプへ | `EXIF_SetExifTime` (src/usr_exif.cpp) |
 *          | Jpeg の Exif 削除 | `EXIF_DelJpgExif` (同上) |
 *          | MP3/FLAC の埋め込み画像 | `ID3_GetImage` / `get_FlacImage` |
 *          | バイナリ文書→テキスト | `xd2tx_Extract` (src/usr_xd2tx.cpp) |
 *          | HTML→テキスト/Markdown | `HtmConv` (src/htmconv.cpp) |
 *          | 画像形式の変換 | `WIC_load_image` / `WIC_save_image` (src/usr_wic.cpp) |
 *
 *          ここが持つのは「対象を選ぶ・出力名を決める・結果を数える」だけ。
 *          アイコンの抽出だけは例外で、`Graphics::TIcon::SaveToFile` が
 *          宣言のみのシム (報告書 §20) なので **.ico の書き出しを自分で書いた**。
 *
 *          VCL 版の該当は `src/MainFrm.cpp` の `ExtractIconAction` (17365行) /
 *          `ExtractMp3ImgAction` (17450行) / `ConvertDoc2TxtAction` (29144行) /
 *          `ConvertHtm2TxtAction` (29214行) / `DelJpgExifAction` (29508行) /
 *          `SetExifTimeAction` (25546行) / `SetArcTimeAction` (25431行)。
 */
#ifndef NYANFI_GUI_CONVERT_OPS_H
#define NYANFI_GUI_CONVERT_OPS_H

#include <vector>

#include "gui/file_ops.h"

namespace convert_ops {

//---------------------------------------------------------------------------
// 出力名の決め方 (純関数。テストで固定する)
//---------------------------------------------------------------------------
/**
 * @brief 変換結果の出力パスを決める
 * @param src 元のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param new_ext 新しい拡張子 (先頭のドットを含む。空なら元の名前のまま)
 * @return 出力パス
 * @details VCL は一律に `出力先 + 名前主部 + 新しい拡張子` (MainFrm.cpp:29271 ほか)
 */
UnicodeString OutputPath(const UnicodeString &src, const UnicodeString &dst_dir,
                         const UnicodeString &new_ext);

/**
 * @brief 連番付きの抽出先パスを決める (アイコン・GIF フレームなど)
 * @param src 元のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param index 0 始まりの番号
 * @param ext 拡張子 (先頭のドットを含む)
 * @return 出力パス
 * @details VCL の書式は `%s%s_%03u.ico` (MainFrm.cpp:17417)。**3桁固定**
 */
UnicodeString IndexedOutputPath(const UnicodeString &src, const UnicodeString &dst_dir,
                                int index, const UnicodeString &ext);

//---------------------------------------------------------------------------
// Exif
//---------------------------------------------------------------------------
/**
 * @brief タイムスタンプを Exif の撮影日時に合わせる (SetExifTime)
 * @param paths 対象のフルパス
 * @return 成功・スキップ (Exif 非対応の拡張子) ・失敗の件数
 * @details **その場で書き換える**。VCL も同じ (MainFrm.cpp:25570)
 */
file_ops::FileOpResult SetExifTime(const std::vector<UnicodeString> &paths);

/**
 * @brief Jpeg から Exif を取り除いたものを出力先に作る (DelJpgExif)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param keep_time 元のタイムスタンプを引き継ぐ
 * @return 件数。Exif が無いものは skipped_existing に数える
 * @details **元は書き換えず、出力先に別ファイルを作る** (VCL のタスクと同じ)。
 *          出力先に同名があれば上書きせずスキップする
 */
file_ops::FileOpResult DeleteJpegExif(const std::vector<UnicodeString> &paths,
                                      const UnicodeString &dst_dir, bool keep_time);

//---------------------------------------------------------------------------
// 抽出
//---------------------------------------------------------------------------
/**
 * @brief MP3 / FLAC の埋め込み画像を出力先に取り出す (ExtractMp3Img / ExtractImage)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @return 件数。画像を持たないものは skipped_existing に数える
 * @details 出力の拡張子は `ID3_GetImage` / `get_FlacImage` が中身を見て決める
 *          (渡すのは拡張子なしの名前)
 */
file_ops::FileOpResult ExtractEmbeddedImages(const std::vector<UnicodeString> &paths,
                                             const UnicodeString &dst_dir);

/**
 * @brief 実行ファイル等からアイコンを取り出す (ExtractIcon)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param index 取り出す番号。-1 なら全部
 * @return 件数。アイコンを持たないものは skipped_existing
 * @details `Graphics::TIcon::SaveToFile` が使えないので **.ico を自分で書く**
 *          (`ICONDIR` + `ICONDIRENTRY` + DIB)。出力名は VCL と同じ
 *          `名前_000.ico` 形式。既にあれば上書きしない
 */
file_ops::FileOpResult ExtractIcons(const std::vector<UnicodeString> &paths,
                                    const UnicodeString &dst_dir, int index);

//---------------------------------------------------------------------------
// 変換
//---------------------------------------------------------------------------
/**
 * @brief バイナリ文書をテキストにする (ConvertDoc2Txt)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param code_page 出力の文字コード (VCL の既定は 932)
 * @param error_out xdoc2txt が使えないときの理由
 * @return 件数。対応していない拡張子は skipped_existing
 * @details 変換は `xd2tx_Extract` (xdoc2txt.dll のラッパー)。
 *          **DLL が無い環境では1件も処理せず、理由を返す**
 */
file_ops::FileOpResult ConvertDocToText(const std::vector<UnicodeString> &paths,
                                        const UnicodeString &dst_dir, int code_page,
                                        UnicodeString &error_out);

/**
 * @brief HTML をテキスト / Markdown にする (ConvertHtm2Txt)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param to_markdown true なら `.md`、false なら `.txt`
 * @return 件数。HTML でないものは skipped_existing
 * @details 変換は `HtmConv` (src/htmconv.cpp)。**出力の文字コードは
 *          入力を判定した結果に合わせる** (VCL も `load_text_ex` の戻り値を使う)
 */
file_ops::FileOpResult ConvertHtmlToText(const std::vector<UnicodeString> &paths,
                                         const UnicodeString &dst_dir, bool to_markdown);

/**
 * @brief 画像の形式を変える (ConvertImage)
 * @param paths 対象のフルパス
 * @param dst_dir 出力先ディレクトリ
 * @param ext 出力の拡張子 (`.png` `.jpg` `.bmp` `.tif` `.gif`)
 * @param jpeg_quality JPEG の画質 (0〜100)
 * @return 件数。読めなかったものは失敗に数える
 * @details 読み書きとも WIC (`WIC_load_image` / `WIC_save_image`)。
 *          形式は**出力の拡張子で決まる** (usr_wic.cpp の実装がそうなっている)。
 *          出力先に同名があれば上書きせずスキップ
 */
file_ops::FileOpResult ConvertImages(const std::vector<UnicodeString> &paths,
                                     const UnicodeString &dst_dir, const UnicodeString &ext,
                                     int jpeg_quality);

}  // namespace convert_ops

#endif  // NYANFI_GUI_CONVERT_OPS_H
