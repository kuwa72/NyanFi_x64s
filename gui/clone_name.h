/**
 * @file gui/clone_name.h
 * @brief クローン作成・自動改名の名前の組み立て (wx 非依存)
 *
 * @details VCL 版の該当は `src/Global.cpp` の `format_CloneName` (3820行)。
 *          `Global.cpp` はビルド対象に入っていないので**書式の解釈を書き写した**
 *          (報告書 §24 と同じ扱い。元の行番号は各関数のコメントに書いてある)。
 *
 *          書式は既定が `\N_\SN(1)` (`FMT_AUTO_REN`, src/Global.h:174) で、
 *          「元の名前 + `_` + 連番」になる。同名があるかぎり連番を進める。
 */
#ifndef NYANFI_GUI_CLONE_NAME_H
#define NYANFI_GUI_CLONE_NAME_H

#include <functional>

namespace clone_name {

/// 既定の書式 (`FMT_AUTO_REN`, src/Global.h:174)
extern const wchar_t *const kDefaultFormat;

/**
 * @brief 書式を1回だけ展開する (連番は seq で与える)
 * @param fmt 書式。空なら既定 (`\N_\SN(1)`)
 * @param base 名前の主部 (ファイルなら拡張子を除いた部分、ディレクトリなら名前全体)
 * @param seq 連番の加算分 (0 が1回目)
 * @param stamp `\TS(...)` で使う元のタイムスタンプ
 * @param now `\DT(...)` で使う現在時刻
 * @return 展開した名前 (拡張子は付かない)
 * @details 対応する書式指定 (`format_CloneName` から書き写し):
 *          - `\N`       元の名前の主部
 *          - `\SN(n)`   連番。`n` の**桁数**でゼロ詰めし、`n + seq` を出す
 *          - `\DT(fmt)` 現在時刻を `fmt` (FormatDateTime の書式) で
 *          - `\TS(fmt)` 元のタイムスタンプを同様に
 *          - `\-`       **1回目だけ、ここから後ろを捨てる** (連番を付けずに試す)。
 *            2回目以降は `\-` 自体が消える
 */
UnicodeString Expand(const UnicodeString &fmt, const UnicodeString &base, int seq,
                     const TDateTime &stamp, const TDateTime &now);

/**
 * @brief 空いている名前が見つかるまで連番を進める
 * @param fmt 書式 (空なら既定)
 * @param src_path 元のフルパス
 * @param dst_dir 作る先のディレクトリ
 * @param is_dir 元がディレクトリか
 * @param taken 「その名前は使用済みか」を答えるもの。
 *        通常は実体の有無、まとめて作るときは予定分も含めて答える
 * @param limit 連番の上限 (これを超えたら空を返す。暴走防止)
 * @return 使えるフルパス。見つからなければ空
 * @details ファイルなら元の拡張子が末尾に付く。ディレクトリなら付かない
 *          (`format_CloneName` の `fext` の扱いと同じ)
 */
UnicodeString MakeUnique(const UnicodeString &fmt, const UnicodeString &src_path,
                         const UnicodeString &dst_dir, bool is_dir,
                         const std::function<bool(const UnicodeString &)> &taken,
                         int limit = 10000);

}  // namespace clone_name

#endif  // NYANFI_GUI_CLONE_NAME_H
