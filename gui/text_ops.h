/**
 * @file gui/text_ops.h
 * @brief テキストファイルに対する操作 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `CountLines` / `JoinText` /
 *          `ConvertTextEnc`。読み込みと文字コード判定は
 *          `gui/text_viewer_core.h` の `LoadForView()` を使う
 *          (テキストビューアと同じ経路を通すので、判定が食い違わない)。
 */
#ifndef NYANFI_GUI_TEXT_OPS_H
#define NYANFI_GUI_TEXT_OPS_H

#include <vector>

namespace text_ops {

/// 行数の内訳
struct LineStats {
	int total = 0;      //!< 全行数
	int blank = 0;      //!< 空白だけの行
	int non_blank = 0;  //!< それ以外
};

/**
 * @brief 行数を数える
 * @details **コメント行は数えない。** VCL の `CountListLines`
 *          (Global.cpp:9887) は `UserHighlight` が持つ**ユーザ設定の
 *          コメント定義**を使って「ソース行 / コメント行」を分けるが、
 *          その設定は `Global.cpp` 側にあり、まだリンク対象に入っていない。
 *          ここでは全行数と空白行だけを数え、**コメントの内訳は出さない**
 *          (中途半端な独自定義で数えると VCL と静かに食い違うため)。
 */
LineStats CountLines(const std::vector<UnicodeString> &lines);

/// 1ファイル分の結合結果
struct JoinResult {
	int joined = 0;                        //!< 結合できたファイル数
	std::vector<UnicodeString> failures;   //!< 失敗の内訳
};

/**
 * @brief テキストファイルを1つに結合する (JoinText)
 * @param paths 元のファイル (この順に並べる)
 * @param out_path 出力先
 * @return 結合できた件数と失敗の内訳
 * @details 出力は **UTF-8 (BOM 無し)**。元の文字コードはファイルごとに
 *          判定して読み、出力側で揃える (元がまちまちでも混ざらない)。
 *          **出力先が既にあれば何もしない** (規約: 上書きを既定にしない)。
 *          改行は CRLF で揃える
 */
JoinResult JoinTextFiles(const std::vector<UnicodeString> &paths, const UnicodeString &out_path);

/**
 * @brief テキストファイルの文字コードを変換する (ConvertTextEnc)
 * @param path 対象のファイル (その場で書き換える)
 * @param target_code_page 変換先のコードページ (65001 = UTF-8, 932 = Shift_JIS)
 * @param with_bom UTF-8/UTF-16 のとき BOM を付けるか
 * @param error_out 失敗した理由
 * @return 変換できたら true
 * @details **バイナリと判定されたファイルは変換しない** (壊すため)。
 *          変換前と同じコードページなら何もせず true を返す
 */
bool ConvertEncoding(const UnicodeString &path, int target_code_page, bool with_bom,
                     UnicodeString &error_out);

}  // namespace text_ops

#endif  // NYANFI_GUI_TEXT_OPS_H
