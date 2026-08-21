/**
 * @file gui/file_ops.h
 * @brief ファイル操作 (コピー・移動・名前変更・ディレクトリ作成・削除)
 *
 * @details Phase 2 の骨格に「ファイル操作」を追加する部分。wxWidgets には
 * 依存しない (`gui/main_frame.cpp` から呼ばれる)。`nyanfi_gui_core`
 * (ルート CMakeLists.txt) に入れて `tests/core/test_gui_file_ops.cpp` から
 * 直接テストできるようにしてある。
 *
 * 依存する既存コード (すべて `src/usr_file_ex.h`、1ファイル単位):
 * `copy_File` / `move_File` / `rename_File` / `create_Dir` /
 * `file_exists` / `dir_exists`。
 *
 * # 実装上の判断 (推測・簡略化した点)
 *
 * - **削除はゴミ箱へ送るだけ**。`SHFileOperationW` (`FOF_ALLOWUNDO`) を使う。
 *   完全削除は実装しない。フラグは `src/Global.cpp` の `delete_File()`
 *   (use_trash=true の場合) と同じ `FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
 *   FOF_SILENT` を踏襲した (GUI 側の確認ダイアログで既に確認済みのため、
 *   シェル自身の確認・進捗 UI は出さない)。
 * - **既存ファイルは上書きしない**。`copy_File`/`move_File` はどちらも
 *   Win32 API 側で上書きが起こる実装 (`CopyFile` の `bFailIfExists=FALSE`、
 *   `MoveFileEx` の `MOVEFILE_REPLACE_EXISTING`) だが、呼び出し前に
 *   `file_exists`/`dir_exists` で存在確認し、存在するなら呼ばずにスキップする。
 * - **ディレクトリの再帰コピーは自前実装** (`usr_file_ex.h` はファイル単位のみ)。
 *   `FindFirst`/`FindNext` で列挙し、ディレクトリは `create_Dir` (無ければ作成、
 *   既存なら中に**マージ**して個別にスキップ判定する)、ファイルは `copy_File`。
 * - **ディレクトリの移動は `move_File` (`MoveFileEx`) をそのまま使う**。
 *   同一ボリュームなら瞬時にリネームで完了するが、ボリュームを跨ぐ
 *   ディレクトリの移動は Win32 の仕様上失敗する。跨ぐ場合の再帰移動
 *   (コピー+削除) は実装しない。失敗として報告し、利用者にコピー→削除の
 *   個別操作を促す。
 * - **名前の変更はファイル・ディレクトリを区別しない**。`rename_File`
 *   (`MoveFile`) は同一ボリューム内であればディレクトリの名前変更にも使える
 *   (Win32 の仕様)。
 */
#ifndef NYANFI_GUI_FILE_OPS_H
#define NYANFI_GUI_FILE_OPS_H

#include <vector>

namespace file_ops {

/// バッチ操作 (Copy/Move) の結果
struct FileOpResult {
	int success_count = 0;      //!< 成功した項目数 (ディレクトリは自身+配下の合計)
	int skipped_existing = 0;   //!< 宛先に同名の項目が既にあり、上書きを避けてスキップした数
	std::vector<UnicodeString> failures;  //!< 失敗した項目の説明 ("パス: 理由")
};

/**
 * @brief 結果を日本語の要約文にする (GUI のメッセージ表示用)
 * @param result 対象の結果
 * @return 件数の要約 + (失敗があれば) 先頭数件の理由
 */
UnicodeString Summarize(const FileOpResult &result);

/**
 * @brief 複数項目をコピーする
 * @param items コピー元のフルパスの一覧 (ファイルまたはディレクトリ)
 * @param dst_dir コピー先ディレクトリ (末尾の "\\" は無くてよい)
 * @return 集計結果
 */
FileOpResult CopyItems(const std::vector<UnicodeString> &items, const UnicodeString &dst_dir);

/**
 * @brief 複数項目を移動する
 * @param items 移動元のフルパスの一覧 (ファイルまたはディレクトリ)
 * @param dst_dir 移動先ディレクトリ (末尾の "\\" は無くてよい)
 * @return 集計結果
 */
FileOpResult MoveItems(const std::vector<UnicodeString> &items, const UnicodeString &dst_dir);

/**
 * @brief 名前を変更する (同一ディレクトリ内、ファイル・ディレクトリ共通)
 * @param dir 対象の親ディレクトリ
 * @param old_name 現在の名前 (パスを含まない)
 * @param new_name 新しい名前 (パスを含まない)
 * @param error_out [o] 失敗時の理由
 * @return true 成功
 */
bool RenameItem(const UnicodeString &dir, const UnicodeString &old_name,
                 const UnicodeString &new_name, UnicodeString &error_out);

/**
 * @brief ディレクトリを作成する
 * @param dir 作成先の親ディレクトリ
 * @param name 新しいディレクトリ名 (パスを含まない)
 * @param error_out [o] 失敗時の理由
 * @return true 成功
 */
bool MakeDirectory(const UnicodeString &dir, const UnicodeString &name, UnicodeString &error_out);

/**
 * @brief 複数項目をゴミ箱へ送る (完全削除はしない)
 * @param paths 削除対象のフルパスの一覧 (ファイルまたはディレクトリ)
 * @param error_out [o] 失敗時の理由
 * @param owner 確認・進捗 UI の親ウィンドウ (無くてもよい)
 * @return true 成功 (1件も対象が無い場合も true)
 */
bool SendToTrash(const std::vector<UnicodeString> &paths, UnicodeString &error_out, HWND owner = NULL);


/**
 * @brief コピー/移動先が元と同じか、その配下かを判定する
 * @details ディレクトリを自分自身の配下にコピーすると無限再帰になり、ディスクを
 *          埋め尽くす。操作前にこれで弾く。大小文字は無視する。
 * @param src 元のパス
 * @param dst 先のパス
 * @return bool 同じか配下なら true (操作してはいけない)
 */
bool IsSameOrInside(const UnicodeString &src, const UnicodeString &dst);

}  // namespace file_ops

#endif  // NYANFI_GUI_FILE_OPS_H
