/**
 * @file gui/clipboard_files.h
 * @brief クリップボード経由のファイル操作の判断部分 (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `CopyToClip` / `CutToClip` /
 *          `Paste`。クリップボードの読み書きそのものは Win32/wx の仕事なので、
 *          **「コピーか移動か」の判定と「貼り付けてよい相手か」の判定**だけを
 *          ここに切り出す (規約8)。
 *
 *          貼り付けは破壊的な操作なので、**弾いた相手を黙って捨てず、
 *          理由つきで呼び出し側へ返す** (規約: 破壊的操作の前に必ず確認)。
 */
#ifndef NYANFI_GUI_CLIPBOARD_FILES_H
#define NYANFI_GUI_CLIPBOARD_FILES_H

#include <vector>

namespace clipboard_files {

/**
 * @brief クリップボードの "Preferred DropEffect" が移動を意味するか
 * @details 実測: VCL は `is_move = (*ep & DROPEFFECT_MOVE)` と**ビットで見る**
 *          (MainFrm.cpp:28702)。エクスプローラは切り取り時に
 *          `DROPEFFECT_MOVE | DROPEFFECT_LINK` のように複数立てることがあるので、
 *          値の一致で判定してはいけない
 */
bool IsMoveEffect(unsigned int effect);

/// 貼り付け対象の検査結果
struct PasteCheck {
	std::vector<UnicodeString> accepted;  //!< 貼り付けてよいもの
	std::vector<UnicodeString> rejected;  //!< 弾いたもの ("パス: 理由")
};

/**
 * @brief 貼り付け対象を検査する
 * @param paths クリップボードから取り出したパス
 * @param dst_dir 貼り付け先のディレクトリ
 * @return 受け付けたものと弾いたもの
 * @details 弾く条件:
 *          - 自分自身または自分の配下へ貼ろうとしている (無限再帰になる)
 *          - 貼り付け先が元と同じディレクトリ (何も起きないので弾く)
 *
 *          **弾いた分は理由つきで返す。** 黙って減らすと
 *          「選んだのに一部しか処理されない」ことに気づけない
 */
PasteCheck ValidatePasteTargets(const std::vector<UnicodeString> &paths,
                                const UnicodeString &dst_dir);

}  // namespace clipboard_files

#endif  // NYANFI_GUI_CLIPBOARD_FILES_H
