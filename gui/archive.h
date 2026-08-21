/**
 * @file gui/archive.h
 * @brief 書庫の操作 (wx 非依存)
 *
 * @details 実体は移植済みの `src/usr_arc.cpp` (`UserArcUnit`) が持つ。
 *          **外部の書庫 DLL** (7-zip32.dll / UNLHA32.dll など) を動的に読むので、
 *          それらが入っていない環境では「利用できない」と返る。VCL 版も同じ前提。
 *
 *          ここは wx とのつなぎと、名前の決め方などの純粋な判断を置く (規約8)。
 */
#ifndef NYANFI_GUI_ARCHIVE_H
#define NYANFI_GUI_ARCHIVE_H

#include <vector>

namespace archive {

/**
 * @brief 作る書庫の既定の名前 (拡張子なし) を決める
 * @param cursor_name カーソル位置の名前 (無ければ空)
 * @param selected 選択されている名前
 * @return 拡張子を除いた主部。決められなければ空
 * @details 実測 (MainFrm.cpp:23331-23335): **カーソル位置の主部**を使い、
 *          それが空なら**最初に選択されている項目の主部**を使う。
 *          ディレクトリ名は使わない
 */
UnicodeString DefaultArchiveBaseName(const UnicodeString &cursor_name,
                                     const std::vector<UnicodeString> &selected);

/// 書庫の1項目 (一覧表示用)
struct Entry {
	UnicodeString name;   //!< 書庫内のパス
	Int64 size = 0;       //!< 展開後のサイズ
	bool is_dir = false;
};

/// 書庫として扱える拡張子か (実際に開けるかは DLL の有無による)
bool LooksLikeArchive(const UnicodeString &path);

/**
 * @brief 書庫の中身を一覧する
 * @param archive_path 書庫のパス
 * @param out 取り出した一覧
 * @param error_out 失敗した理由
 * @return 取り出せたら true
 */
bool ListEntries(const UnicodeString &archive_path, std::vector<Entry> &out,
                 UnicodeString &error_out);

/**
 * @brief 書庫の正当性を検査する (TestArchive)
 * @return 問題なければ true
 */
bool TestArchive(const UnicodeString &archive_path, UnicodeString &error_out);

/**
 * @brief 書庫を展開する (UnPack)
 * @param archive_path 書庫のパス
 * @param dst_dir 展開先
 * @param error_out 失敗した理由
 * @return 展開できたら true
 * @details **上書きはしない** (規約: 上書きを既定にしない)。既存と同名の項目は
 *          書庫 DLL 側の既定に従ってスキップされる
 */
bool Extract(const UnicodeString &archive_path, const UnicodeString &dst_dir,
             UnicodeString &error_out);

/**
 * @brief 書庫を作る (Pack)
 * @param archive_path 作る書庫のフルパス (拡張子で形式が決まる)
 * @param src_dir 元のディレクトリ
 * @param names 詰めるものの名前 (パスを含まない)
 * @param error_out 失敗した理由
 * @return 作れたら true
 */
bool Create(const UnicodeString &archive_path, const UnicodeString &src_dir,
            const std::vector<UnicodeString> &names, UnicodeString &error_out);

}  // namespace archive

#endif  // NYANFI_GUI_ARCHIVE_H
