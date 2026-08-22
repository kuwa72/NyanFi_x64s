/**
 * @file gui/rename.h
 * @brief 一括リネームのロジック層 (wx 非依存)
 *
 * @details `src/RenDlg.cpp` (未移植、TRenameDlg。VCL の一括リネームダイアログ)
 * の実測に基づく最低限の実装。VCL 版は「名前」「連番」「置換」「リストファイル」
 * 「MP3/FLAC タグ」「属性」「タイムスタンプ」「関連改名」の8種類のモードを
 * 持つ巨大なダイアログだが、ここでは要件で指定された最低限の3方式だけを
 * 実装する:
 *
 * - **正規表現置換** (`RegexOptions`/`BuildRegexPlan`)。実測は
 *   `TRenameDlg::UpdateNewNameList()` の「置換」シート (L.649-748)。
 *   `TRegEx::IsMatch`/`TRegEx::Escape`/`replace_regex_2` (すべて移植済み、
 *   `compat/include/compat/regex.h` / `src/usr_str.cpp`) をそのまま使う。
 *   `\C` (親ディレクトリ名の置換トークン) は実装しない (最低限の要件外)。
 * - **連番の付与** (`SerialOptions`/`BuildSerialPlan`)。実測は同関数の
 *   「連番」シート (L.700-724)。開始番号・増分・桁数 (0埋め) と、前後の
 *   固定文字列、拡張子の変更に対応する。`PreNameEdit`/`PostNameEdit` の
 *   `format_FileName` によるトークン置換 (日付・親フォルダ名等) は実装しない
 *   (最低限の要件外)。
 * - **大文字/小文字の変換**。VCL 版では `TRenameDlg` 内の機能ではなく
 *   `MainFrm.cpp` の独立コマンド `NameToUpperActionExecute`/
 *   `NameToLowerActionExecute` (`fp->f_name.UpperCase()`/`LowerCase()`、
 *   確認ダイアログ無しで即実行) だが、既定のキー割り当てが無いこと
 *   (`src/Global.cpp` に `NameToUpper`/`NameToLower` の既定キーは無い) と、
 *   本移植の破壊的操作の方針 (プレビュー必須・確認必須) を優先し、
 *   このダイアログの1モードとして統合した。
 *
 * VCL 版にあってここで実装しなかったモード (理由は上記の通り「最低限」の
 * 要件外、または未移植の依存機能が必要なため):
 * - リストファイルからの置換 (`RenListSheet`)
 * - MP3/FLAC タグを使った改名 (ID3/FLAC タグ解析は移植済みだが本要件外)
 * - 属性・タイムスタンプの変更 (`AtrGroupBox`/`TimeGroupBox`)
 * - 関連改名 (同じベース名を持つ別拡張子のファイルを追従させる、
 *   `RenOkActionExecute` の「関連改名チェック」)
 *
 * # 衝突判定とリネーム同士の衝突の解決
 *
 * `ResolveConflicts()` が各行の状態 (`RowStatus`) を確定する。「元と同じ」
 * 「不正な名前」「他の対象/既存ファイルと衝突」「変更可能」を区別する。
 * 大小文字だけの変更 (`SameText` は真だが `SameStr` は偽) は Windows の
 * 仕様上リネーム可能なので、自分自身の現在の名前との衝突とは扱わない。
 *
 * `ExecutePlan()` の実行順序問題 (`a→b, b→c` のような連鎖) は、実測した
 * `src/RenDlg.cpp` の `RenOkActionExecute` の「中間処理」(`$~NFnnnn.~TMP`
 * という一時名を経由する2段階リネーム) と同じ考え方で解決する。対象が
 * 2件以上のときは常に「全件を一時名へ→一時名から最終名へ」の2段階で行う
 * (VCL 版は重複を検出したときだけ2段階にするが、ここでは実装を単純にする
 * ため常に2段階にする。ファイル名の変更はメタデータ操作でコストが小さいため、
 * 不要な場合の追加コストは実用上問題にならないと判断した)。
 */
#ifndef NYANFI_GUI_RENAME_H
#define NYANFI_GUI_RENAME_H

#include <vector>

#include "usr_str.h"

namespace rename_core {

/// リネーム対象1件 (ディレクトリ内の名前のみ。パスは持たない)
struct RenameTarget {
	UnicodeString name;    //!< 現在の名前 (パスを含まない)
	bool is_dir = false;   //!< ディレクトリか (true なら拡張子の概念を持たない)
};

//---------------------------------------------------------------------------
/// 正規表現置換の設定 (`TRenameDlg` の「置換」シートに相当)
struct RegexOptions {
	UnicodeString pattern;       //!< 検索文字列 (正規表現、または use_regex=false ならリテラル)
	UnicodeString replacement;   //!< 置換文字列 (`\1` 等のグループ参照可、`TRegEx::Replace` の仕様のまま)
	bool use_regex = true;       //!< false ならリテラル一致として扱う (`TRegEx::Escape` する)
	bool case_sensitive = true;  //!< false なら大小文字を無視して照合する (`roIgnoreCase`)
	bool only_base = false;      //!< true なら拡張子を除いた部分だけに適用する (拡張子は変化しない)
};

/// 連番付与の設定 (`TRenameDlg` の「連番」シートの最低限。トークン置換は無い)
struct SerialOptions {
	UnicodeString prefix;      //!< 連番の前に付ける固定文字列
	UnicodeString suffix;      //!< 連番の後 (拡張子の前) に付ける固定文字列
	int start = 1;             //!< 開始番号
	int step = 1;              //!< 増分 (1件ごとにこの数だけ増える)
	int width = 2;             //!< 連番の桁数 (0埋め)。0 なら連番自体を付けない
	bool change_ext = false;   //!< true なら拡張子を new_ext に変更する (false なら元の拡張子を維持)
	UnicodeString new_ext;     //!< change_ext=true のときの新しい拡張子 ("." の有無どちらでも可)。
	                           //!< 空文字列なら拡張子を削除する
};

/// 大文字/小文字変換の方向
enum class CaseMode {
	Upper,  //!< 大文字化
	Lower,  //!< 小文字化
};

/// 大文字/小文字変換の設定
struct CaseOptions {
	CaseMode mode = CaseMode::Upper;
	bool only_base = false;   //!< true なら拡張子を除いた部分だけ変換する (拡張子はそのまま)
};

//---------------------------------------------------------------------------
/// プレビュー1行の状態
enum class RowStatus {
	Unchanged,  //!< 新しい名前が元の名前と完全に一致 (何もしない)
	Ok,         //!< 変更あり、実行してよい
	Invalid,    //!< 新しい名前が空、または使用できない文字を含む
	Conflict,   //!< 他の対象、または既存のファイル/ディレクトリと衝突する
};

/// プレビュー1行
struct PreviewRow {
	UnicodeString old_name;
	UnicodeString new_name;
	bool is_dir = false;
	RowStatus status = RowStatus::Unchanged;
};

/// プレビュー全体 (`targets` と同じ順序・同じ件数の `rows` を持つ)
struct RenamePlan {
	std::vector<PreviewRow> rows;
	bool pattern_error = false;   //!< 正規表現が不正で計算自体ができなかった
	UnicodeString error;          //!< pattern_error 時の説明文
};

/**
 * @brief 正規表現置換のプレビューを作る (衝突判定込み)
 * @param dir 対象の親ディレクトリ (既存ファイルとの衝突判定に使う。実在しなくてもよい)
 * @param targets 対象一覧
 * @param opt 置換条件
 * @return プレビュー。pattern_error が true の場合 rows は空
 */
RenamePlan BuildRegexPlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                          const RegexOptions &opt);

/**
 * @brief 連番付与のプレビューを作る (衝突判定込み)
 */
RenamePlan BuildSerialPlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                           const SerialOptions &opt);

/**
 * @brief 大文字/小文字変換のプレビューを作る (衝突判定込み)
 */
RenamePlan BuildCasePlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                         const CaseOptions &opt);

/**
 * @brief 各行の状態 (`RowStatus`) を確定する
 * @details `Build*Plan` が内部で呼ぶので通常は直接呼ぶ必要はない。プレビューの
 * 計算方法を自作する場合 (テスト等) のために公開している。
 * @param plan rows は計算済み (old_name/new_name/is_dir) で、status は無視して上書きする
 * @param dir 対象の親ディレクトリ (既存ファイルとの衝突判定に使う)
 * @param targets plan.rows と同じ順序・同じ件数であること (対応関係の判定に使う)
 */
void ResolveConflicts(RenamePlan &plan, const UnicodeString &dir,
                     const std::vector<RenameTarget> &targets);

//---------------------------------------------------------------------------
/// 実行結果
/// 実際に改名された1件 (取り消し用の記録に使う)
struct AppliedRename {
	UnicodeString old_name;  //!< 元の名前 (パスを含まない)
	UnicodeString new_name;  //!< 変更後の名前 (同上)
};

struct RenameExecResult {
	int success_count = 0;                  //!< 成功した件数
	int skipped_count = 0;                  //!< 変更なし、または不正/衝突のためスキップした件数
	std::vector<UnicodeString> failures;    //!< 失敗した項目の説明 ("元 -> 新: 理由")
	/// **実際に名前が変わったものだけ**。`UndoRename` の記録に使う
	/// (失敗して元に戻した分は入らない)
	std::vector<AppliedRename> applied;
};

/**
 * @brief プランを実行する (`rename_File` を使う)
 * @details `status==Ok` の行だけを対象にする。対象が2件以上のときは、リネーム
 * 同士の衝突 (`a→b, b→c` のような連鎖や入れ替え) を避けるため、常に
 * 「全件を一時名へ→一時名から最終名へ」の2段階で実行する。
 * @param dir 対象の親ディレクトリ
 * @param plan `Build*Plan`/`ResolveConflicts` で計算済みのプラン
 * @return 集計結果
 */
RenameExecResult ExecutePlan(const UnicodeString &dir, const RenamePlan &plan);

}  // namespace rename_core

#endif  // NYANFI_GUI_RENAME_H
