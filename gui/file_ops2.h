/**
 * @file gui/file_ops2.h
 * @brief ファイル操作の続き (クローン・構造コピー・名前の入れ替え・改名の取り消し)
 *
 * @details `gui/file_ops.h` が大きくなってきたので機能群16以降の分を分けた。
 *          VCL 版の該当は `src/MainFrm.cpp` の `CloneAction` (14330行) /
 *          `CopyDirAction` (15097行) / `SwapNameAction` (26579行) /
 *          `UndoRenameAction` (27335行) / `CreateDirsDlgAction` (15665行) /
 *          `CreateTestFileAction` (15914行)。
 *
 *          **破壊的な操作なので、途中で失敗したら元に戻すところまで書く。**
 *          一括リネームで「2段目に失敗するとファイルが一時名のまま残る」
 *          不具合を実際に作ったので (報告書 §12)、同じ形の処理は必ず巻き戻す。
 */
#ifndef NYANFI_GUI_FILE_OPS2_H
#define NYANFI_GUI_FILE_OPS2_H

#include <vector>

#include "gui/file_ops.h"

namespace file_ops2 {

//---------------------------------------------------------------------------
// クローン作成 (Clone / CloneToCurr)
//---------------------------------------------------------------------------
/**
 * @brief 元と同じ内容を、空いている名前で作る
 * @param paths 元のフルパス
 * @param dst_dir 作る先のディレクトリ
 * @param fmt 名前の書式 (空なら既定の `\N_\SN(1)`。gui/clone_name.h)
 * @return 成功・失敗の件数と失敗の内訳
 * @details 宛先の名前は `clone_name::MakeUnique` が決める。
 *          **同じ呼び出しの中で作る予定の名前も「使用済み」として避ける**
 *          (連続して2つクローンすると同じ名前を狙ってしまうため)。
 *          ディレクトリは中身ごと複製する (`file_ops::CopyItems` と同じ経路)
 */
file_ops::FileOpResult CloneItems(const std::vector<UnicodeString> &paths,
                                  const UnicodeString &dst_dir, const UnicodeString &fmt);

//---------------------------------------------------------------------------
// ディレクトリ構造のコピー (CopyDir)
//---------------------------------------------------------------------------
/**
 * @brief ディレクトリの構造だけを複製する (ファイルは複製しない)
 * @param dirs 元のディレクトリのフルパス
 * @param dst_dir 作る先
 * @param recursive true なら配下のサブディレクトリも作る
 * @return 作成できた数と失敗の内訳。既にあるものは skipped_existing に数える
 * @details **ファイルは1つも作らない。**構造だけ欲しいときに使う
 */
file_ops::FileOpResult CopyDirStructure(const std::vector<UnicodeString> &dirs,
                                        const UnicodeString &dst_dir, bool recursive);

//---------------------------------------------------------------------------
// ディレクトリの一括作成 (CreateDirsDlg)
//---------------------------------------------------------------------------
/**
 * @brief 名前を並べたものからディレクトリをまとめて作る
 * @param names 作る名前。相対なら base からの相対、`a\b\c` のような多段も可
 * @param base 相対名の基準ディレクトリ
 * @return 作成できた数と失敗の内訳。既にあるものは skipped_existing
 * @details VCL の `create_ForceDirs` と同じく**途中のディレクトリも作る**
 */
file_ops::FileOpResult CreateDirs(const std::vector<UnicodeString> &names,
                                  const UnicodeString &base);

//---------------------------------------------------------------------------
// 名前の入れ替え (SwapName)
//---------------------------------------------------------------------------
/**
 * @brief 2つの項目の名前を入れ替える
 * @param a 一方のフルパス
 * @param b もう一方のフルパス
 * @param error_out 失敗した理由
 * @return 入れ替えられたら true
 * @details VCL (MainFrm.cpp:26579) と同じで、**それぞれの拡張子は元のまま**。
 *          入れ替わるのは名前の主部だけ (`ChangeFileExt(相手, 自分の拡張子)`)。
 *
 *          手順も同じ: 先に両方を一時名 (`$~NF000n.~TMP`) へ退避し、
 *          **どちらかが失敗したら退避した分を元に戻して中止する**。
 *          直接入れ替えると「片方だけ改名された状態」で止まりうるため
 */
bool SwapNames(const UnicodeString &a, const UnicodeString &b, UnicodeString &error_out);

//---------------------------------------------------------------------------
// 改名ログと取り消し (UndoRename)
//---------------------------------------------------------------------------
/// 改名1件分 (`renamelog.txt` の1行 = `旧 <TAB> 新`)
struct RenameRecord {
	UnicodeString old_path;
	UnicodeString new_path;
};

/// 改名ログの置き場所 (実行ファイルと同じディレクトリの `renamelog.txt`。
/// VCL の `RENLOG_FILE`, src/Global.h:159 と同じ名前・同じ場所)
UnicodeString RenameLogPath();

/**
 * @brief 改名ログを書き出す (VCL の `save_RenLog`, Global.cpp:6409)
 * @param records 直前に行った改名
 * @param error_out 失敗した理由
 * @return 書けたら true
 * @details **UTF-8 (BOM 付き)**。VCL と同じ形式なので、VCL 版の
 *          `UndoRename` からも読める
 */
bool SaveRenameLog(const std::vector<RenameRecord> &records, UnicodeString &error_out);

/// 改名ログを読む。無ければ空を返す
std::vector<RenameRecord> LoadRenameLog();

/**
 * @brief 改名を元に戻す (UndoRename)
 * @param records 戻す対象 (`LoadRenameLog()` の結果)
 * @return 戻せた数と失敗の内訳
 * @details **新旧の名前が交差しているときは一時名を経由する** (VCL と同じ)。
 *          例えば `a→b, b→a` を素直に戻すと衝突する。
 *          交差の判定は「ある記録の新名が、別の記録の旧名と一致するか」
 */
file_ops::FileOpResult UndoRenames(const std::vector<RenameRecord> &records);

/**
 * @brief 一時名を経由する必要があるか (UndoRenames の内部判断。テスト用に公開)
 * @param records 対象
 * @return 交差していれば true
 */
bool NeedsTempStep(const std::vector<RenameRecord> &records);

//---------------------------------------------------------------------------
// テストファイルの作成 (CreateTestFile)
//---------------------------------------------------------------------------
/**
 * @brief 指定サイズのファイルを作る
 * @param dir 作る先
 * @param name ファイル名 (count>1 なら主部に連番が付く)
 * @param size 1個あたりのバイト数
 * @param count 個数
 * @return 作成できた数と失敗の内訳。既にあるものは skipped_existing
 * @details VCL は `fsutil file createnew` を起動する (MainFrm.cpp:15951) が、
 *          ここは `SetFilePointerEx` + `SetEndOfFile` で直接作る。
 *          外部コマンドに依存しないぶん、**中身が未定義**なのは同じ
 *          (fsutil も 0 埋めを保証しない)
 */
file_ops::FileOpResult CreateTestFiles(const UnicodeString &dir, const UnicodeString &name,
                                       Int64 size, int count);

/**
 * @brief `1234` `10K` `2M` `1G` のようなサイズ文字列をバイト数にする
 * @param text 入力
 * @return バイト数。解釈できなければ -1
 * @details VCL (MainFrm.cpp:15930) と同じ単位。大文字小文字は区別しない
 */
Int64 ParseSize(const UnicodeString &text);

/**
 * @brief 連番付きのテストファイル名を組み立てる
 * @param name 元の名前 (拡張子を含んでよい)
 * @param index 0 始まりの番号
 * @param count 総数 (1 なら連番を付けない)
 * @return 組み立てた名前
 * @details VCL は総数の**桁数**でゼロ詰めする (`%0*u`, nwd = 入力欄の文字数)。
 *          ここは総数の桁数を使う (入力欄の文字数という概念が無いため。
 *          `count=10` なら `name01.txt`〜`name10.txt`)
 */
UnicodeString TestFileName(const UnicodeString &name, int index, int count);

}  // namespace file_ops2

#endif  // NYANFI_GUI_FILE_OPS2_H
