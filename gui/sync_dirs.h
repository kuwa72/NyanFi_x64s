/**
 * @file gui/sync_dirs.h
 * @brief 同期コピー設定の判断ロジック (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/SyncDlg.cpp` (`TRegSyncDlg`) と
 *          `src/Global.cpp` (`get_SyncDirList` / `has_SyncDir` / `is_SyncDir`)。
 *          同期設定一覧 (`SyncDirList`) の1行は
 *          `"タイトル","有効:1/無効:0","オプション","dir1","dir2",...`
 *          (`SyncDlg.cpp::OkButtonClick` の正規化コメントを実測)。
 *            - 有効/無効 (`[1]`) はチェック状態 (`FormShow` で `equal_0` を
 *              見て `Checked` に入れるのを実測)。追加時は前の選択を引き継ぐ
 *              (`MakeRegItem(idx)`。新規は無効)
 *            - オプション (`[2]`) は上書き `O`・同期削除 `D` の組み合わせ
 *              (`MakeRegItem` の `opt +=` を実測)
 *            - 追加はディレクトリが2件以上ないと不可 (`AddRegActionUpdate`
 *              の `DirListBox->Count>=2` を実測)
 *            - 確定時に5項目未満 (dir が2件未満) の不正行は落とし、dir には
 *              末尾区切りを付けて正規化する (`OkButtonClick` を実測)
 *            - 対象解決は `get_SyncDirList` を実測: 登録 dir の配下なら
 *              サブディレクトリ部分を付け替えた対応先を返す。無効行・
 *              該当なし行は飛ばす。レコード上限 50
 *              (`get_csv_array(..., 50)`) も合わせた
 *
 *          未移植 (未実装扱い。落とさない):
 *          - ディレクトリ選択 UI (`SelectDirEx` のフォルダ参照)
 *          - 登録のドラッグ並べ替え (`DragMode = dmAutomatic`)
 *          - `dir_exists` による存在確認 (`ResolveTargets` は候補を返し、
 *            存在確認は呼び出し側が行う責務分離)
 *          - `is_SyncDir` の S/[1][2] 判定 (呼び出し側で未使用のため)
 */
#ifndef NYANFI_GUI_SYNC_DIRS_H
#define NYANFI_GUI_SYNC_DIRS_H

#include <vector>

#include "usr_str.h"

class UsrIniFile;

namespace sync_dirs {

// SyncDirList の CSV 取得上限 (src/Global.cpp の get_csv_array(..., 50) と同じ)。
// src/Global.h は VCL の巨大ヘッダのため直接は読まず値を複写する
constexpr int kCsvMaxItems = 50;

//---------------------------------------------------------------------------
// 同期コピー設定の1項目 ("タイトル","有効","オプション","dir1","dir2",...)
//---------------------------------------------------------------------------
struct SyncEntry {
	UnicodeString title;              //!< 登録名 ([0]。空なら「登録N」)
	bool enabled = false;             //!< 有効か ([1] が "1")
	bool overwrite = false;           //!< 上書きするか ([2] に "O")
	bool sync_delete = false;         //!< 同期削除するか ([2] に "D")
	std::vector<UnicodeString> dirs;  //!< 同期対象ディレクトリ ([3..])
};

//---------------------------------------------------------------------------
// 対象解決の結果 (get_SyncDirList を実測)
//---------------------------------------------------------------------------
struct Resolved {
	std::vector<UnicodeString> targets;  //!< [0] は入力 dir 自身。対応先を続ける
	UnicodeString option;                //!< 当たった行のオプション (O/D)。不発なら空
};

//---------------------------------------------------------------------------
// CSV レコードとの変換
//---------------------------------------------------------------------------

/// CSV の1行を読む
SyncEntry ParseRecord(const UnicodeString &record);

/// CSV の1行に書く (MakeRegItem を実測: 空名は DefaultTitle で補う)
UnicodeString FormatRecord(const SyncEntry &entry, int index_for_default = -1);

/// 空名のときの既定名 (MakeRegItem の `ret_str.sprintf("登録%u", Count+1)`)
UnicodeString DefaultTitle(int count);

//---------------------------------------------------------------------------
//妥当性 (OkButtonClick / AddRegActionUpdate を実測)
//---------------------------------------------------------------------------

/// dir が2件以上あるか (CSV で5項目以上に相当)
bool IsValid(const SyncEntry &entry);

/// 追加ボタンが有効か (DirListBox->Count>=2)
bool CanAdd(int dir_count);

/// 確定時の正規化。不正行は空を返す。dir には末尾区切りを付ける
UnicodeString NormalizeRecord(const UnicodeString &record);

//---------------------------------------------------------------------------
// 対象解決 (get_SyncDirList を実測。dir_exists 確認は呼び出し側)
//---------------------------------------------------------------------------

/// dnam 配下の同期対応先を求める。del_sw のとき D 無し行は飛ばす
Resolved ResolveTargets(const UnicodeString &dnam,
                        const std::vector<SyncEntry> &entries, bool del_sw);

//---------------------------------------------------------------------------
// 設定一覧の保持 (SyncDirList 相当。ini 永続化付き)
//---------------------------------------------------------------------------
class SyncDirStore {
public:
	SyncDirStore() = default;

	const std::vector<SyncEntry> &Items() const { return items_; }
	std::vector<SyncEntry> &MutableItems() { return items_; }

	void SaveToIni(UsrIniFile &ini) const;
	void LoadFromIni(UsrIniFile &ini);

private:
	std::vector<SyncEntry> items_;
};

}  // namespace sync_dirs

#endif  // NYANFI_GUI_SYNC_DIRS_H
