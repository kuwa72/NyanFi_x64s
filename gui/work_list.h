/**
 * @file gui/work_list.h
 * @brief ワークリスト (任意のファイルを集めた一覧) の状態と .nwl の読み書き
 *
 * @details VCL 版の該当は `src/Global.cpp` の `load_WorkList` (7827行) /
 *          `save_WorkList` (7914行)、並べ替えは `src/MainFrm.cpp` の
 *          `ItemTmpUpActionExecute` (20009行) / `ItemTmpDownActionExecute`
 *          (19977行) / `ItemTmpMoveActionExecute` (20041行)。
 *
 *          wx に依存しない (規約8)。ここに判断を置き、`gui/main_frame.cpp` は
 *          受け渡しだけにする。
 */
#ifndef NYANFI_GUI_WORK_LIST_H
#define NYANFI_GUI_WORK_LIST_H

#include <vector>

#include "gui/file_item.h"

namespace work_list {

/// ワークリストの1件
struct WorkItem {
	UnicodeString path;         //!< フルパス。ディレクトリでも末尾の区切りは付けない
	UnicodeString alias;        //!< 別名 (空なら本来の名前を出す)
	bool is_dir = false;        //!< ディレクトリか
	bool is_separator = false;  //!< 区切り行 (path は空、alias は "-")
	bool missing = false;       //!< 実体が見つからなかった (VCL の faInvalid 相当)
	bool marked = false;        //!< 選択中か (並べ替えの対象を決めるのに使う)

	Int64 size = 0;             //!< サイズ (ディレクトリと missing は 0)
	TDateTime stamp;            //!< 最終更新日時
	int attr = 0;               //!< 属性 (faXXX)
};

//---------------------------------------------------------------------------
// .nwl の書式
//---------------------------------------------------------------------------
/**
 * @brief .nwl の各行を解釈する (実体の存在確認はしない)
 * @param lines ファイルの各行 (改行コードを含まない)
 * @return 解釈できた項目
 * @details VCL の `load_WorkList` と同じ規則:
 *          - 空行と `;` で始まる行は読み飛ばす
 *          - 1行は `パス <TAB> 別名`。TAB 以降が無ければ別名は空
 *          - パスが空で別名が `-` だけなら区切り行 (`is_separator`)
 *          - パスの末尾が区切り文字ならディレクトリ (`is_dir`。末尾は落とす)
 *          - パスと別名の両方が空の行は捨てる
 */
std::vector<WorkItem> ParseLines(const std::vector<UnicodeString> &lines);

/**
 * @brief 項目を .nwl の各行に書き起こす
 * @param items 対象
 * @return 書き出す行
 * @details VCL の `save_WorkList` と同じで、**別名が空でも TAB は必ず付く**
 *          (`lbuf.cat_sprintf("\t%s", ...)`)。ディレクトリは末尾に区切り文字を
 *          付ける。区切り行は `\t-` の形で残る。
 *          パスも別名も空の項目は書き出さない (VCL の `is_dummy && alias 空` に相当)
 */
std::vector<UnicodeString> FormatLines(const std::vector<WorkItem> &items);

//---------------------------------------------------------------------------
// ファイルとのやりとり
//---------------------------------------------------------------------------
/**
 * @brief .nwl を読み込む
 * @param path 読み込むファイル (相対指定なら実行ファイルの位置から解決する)
 * @param auto_delete true なら実体の無い項目を捨てる (VCL の `AutoDelWorkList`)。
 *        false なら残して `missing` を立てる
 * @param items_out 読み込んだ項目
 * @param error_out 失敗した理由
 * @return 読み込めたら true
 * @details 各項目について実体を見に行き、サイズ・日時・属性を埋める
 */
bool Load(const UnicodeString &path, bool auto_delete,
          std::vector<WorkItem> &items_out, UnicodeString &error_out);

/**
 * @brief .nwl に書き出す
 * @param path 書き出し先
 * @param items 対象
 * @param error_out 失敗した理由
 * @return 書き出せたら true
 * @details **UTF-8 (BOM 付き)** で書く。VCL は `saveto_TextUTF8`
 *          (`TEncoding::UTF8` 付きの `TStrings::SaveToFile`) なので BOM が付く。
 *          BOM を落とすと VCL 版が ANSI として読み、日本語の別名が壊れる
 */
bool Save(const UnicodeString &path, const std::vector<WorkItem> &items,
          UnicodeString &error_out);

//---------------------------------------------------------------------------
// 中身の操作
//---------------------------------------------------------------------------
/**
 * @brief 同じパスの項目を探す
 * @param items 対象
 * @param path 探すフルパス
 * @return 見つかった添字。無ければ -1
 * @details **大文字小文字を区別しない。** VCL は `TStringList::IndexOf` で
 *          探しており、`CaseSensitive` が既定の false のままなので
 *          区別しない (`src/Global.cpp::CreStringList`)
 */
int IndexOfPath(const std::vector<WorkItem> &items, const UnicodeString &path);

/**
 * @brief 項目を追加する (既にあれば何もしない)
 * @param items 対象
 * @param path 追加するフルパス
 * @param at 挿入位置。範囲外 (-1 など) なら末尾に足す
 * @return 追加したら true、既にあったら false
 * @details 実体を見てサイズ・日時・属性を埋める。実体が無ければ足さない
 */
bool Add(std::vector<WorkItem> &items, const UnicodeString &path, int at = -1);

/// 区切り行を挿入する (at の**次**に入れる。VCL の InsSeparator と同じ)
void InsertSeparator(std::vector<WorkItem> &items, int at);

/// 実体の無い項目を取り除く。取り除いた件数を返す (VCL の WorkList "DI")
int RemoveMissing(std::vector<WorkItem> &items);

/// 区切り行が1つでもあるか (VCL の WorkListHasSep。並べ替えを止める判断に使う)
bool HasSeparator(const std::vector<WorkItem> &items);

//---------------------------------------------------------------------------
// 並べ替え (VCL の ItemTmpUp / ItemTmpDown / ItemTmpMove)
//---------------------------------------------------------------------------
/**
 * @brief 選択項目を1つ上へ動かす
 * @param items 対象 (その場で並べ替える)
 * @param cursor カーソル位置 (呼び出し後の位置に書き換える)
 * @return 動かしたら true
 * @details VCL の `ItemTmpUpActionExecute` と同じ:
 *          - **先頭が選択されていたら何もしない** (詰まっているので全体が動けない)
 *          - 選択が1件も無ければカーソル位置の1件を動かす
 *          - 添字の小さい方から順に1つずつ入れ替える (連続した選択の塊が保たれる)
 *          - カーソルは1つ上がる (0 より上には行かない)
 */
bool MoveUp(std::vector<WorkItem> &items, int &cursor);

/// 選択項目を1つ下へ動かす (MoveUp の対称。末尾が選択されていたら何もしない)
bool MoveDown(std::vector<WorkItem> &items, int &cursor);

/**
 * @brief 選択項目をカーソル位置へまとめて動かす
 * @param items 対象 (その場で並べ替える)
 * @param cursor 移動先 (呼び出し後は挿入先の先頭に書き換える)
 * @return 動かしたら true (選択が1件も無ければ false)
 * @details VCL の `ItemTmpMoveActionExecute` と同じで、
 *          **選択項目を抜き取ってから挿入位置を詰めた分だけ手前へずらす**。
 *          抜き取った項目の選択は解除される
 */
bool MoveSelectedTo(std::vector<WorkItem> &items, int &cursor);

//---------------------------------------------------------------------------
// 表示への受け渡し
//---------------------------------------------------------------------------
/// 一覧ペインに渡す形 (FileItem) にする。並び順はそのまま
std::vector<FileItem> ToFileItems(const std::vector<WorkItem> &items);

/// ペインから戻ってきた選択状態 (FileItem::marked) を書き戻す。
/// **並び順が変わっていないことが前提** (添字で対応付ける)
void ApplyMarks(std::vector<WorkItem> &items, const std::vector<FileItem> &from);

}  // namespace work_list

#endif  // NYANFI_GUI_WORK_LIST_H
