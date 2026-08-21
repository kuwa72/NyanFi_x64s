/**
 * @file gui/misc_ops.h
 * @brief 設定ファイル・クリップボード・ネットワーク関連の判断 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `EditIniFile` / `ViewIniFile` /
 *          `NameFromClip` / `ShareList` / `NetConnect` など。
 */
#ifndef NYANFI_GUI_MISC_OPS_H
#define NYANFI_GUI_MISC_OPS_H

#include <vector>

namespace misc_ops {

/**
 * @brief クリップボードの内容からファイル名を作る (NameFromClip)
 * @param clipboard_text クリップボードの文字列
 * @param error_out 使えない場合の理由
 * @return 使える名前。使えなければ空
 * @details 実測 (MainFrm.cpp の NameFromClip): クリップボードの内容を
 *          `ExtractFileName(exclude_quot(get_norm_str(...)))` に通す。
 *          つまり**引用符を外し、パスが入っていれば末尾の要素だけ**を使う。
 *
 *          そのうえで**ファイル名に使えない文字が残っていたら弾く**。
 *          VCL 側の確認は取れていないが、弾かないと rename が失敗するだけで
 *          理由が分からないため、こちらで判定して理由を返す。
 */
UnicodeString NameFromClipboard(const UnicodeString &clipboard_text, UnicodeString &error_out);

/// Windows のファイル名に使えない文字を含むか
bool HasInvalidNameChar(const UnicodeString &name);

/// ネットワーク共有の1件
struct ShareEntry {
	UnicodeString name;     //!< 共有名
	UnicodeString path;     //!< ローカルのパス (取れなければ空)
	UnicodeString remark;   //!< コメント
};

/**
 * @brief このコンピュータの共有フォルダを列挙する (ShareList)
 * @param out 結果
 * @param error_out 失敗した理由
 * @return 取れたら true
 * @details 管理共有 (末尾が `$`) は除く。VCL 版の一覧に合わせた**こちらの判断**
 */
bool EnumLocalShares(std::vector<ShareEntry> &out, UnicodeString &error_out);

}  // namespace misc_ops

#endif  // NYANFI_GUI_MISC_OPS_H
