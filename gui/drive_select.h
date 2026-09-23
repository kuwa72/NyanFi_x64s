/**
 * @file gui/drive_select.h
 * @brief ドライブ選択ダイアログの wx 非依存な判断ロジック
 *
 * @details VCL の実測元は `src/DriveDlg.cpp` (`TSelDriveDlg`) と
 *          `src/DriveDlg.dfm`。実測した呼び出し位置/行番号は以下のとおり。
 *          - `src/DriveDlg.cpp:135-200` `UpdateDriveList` が
 *            `DriveInfoList` を accessible で絞り、7列の行を作る
 *          - `src/DriveDlg.cpp:365-409` `DriveGridKeyDown` が英字キーを
 *            前方一致させ、Enter/英字キーでドライブを選ぶ
 *          - `src/DriveDlg.cpp:427-431` `OptCheckBoxClick` が
 *            OnlyAcc/Icon/LargeIcon の変更で一覧を更新する
 *          - `src/DriveDlg.cpp:445-478` `ShowDriveMenu` の
 *            NetConnect/NetDisConnect/Rename/ReloadList/DriveGraph 等の処理
 *          - VCL からの呼び出しは `src/MainFrm.cpp:16855-16866`
 *            (`DriveListActionExecute` → `TSelDriveDlg::ShowModal`)
 *          - コマンド表は `src/usr_cmdlist.cpp:81` の `F:DriveList`
 *
 *          ここは wx に依存しない「表示対象・選択先・容量・操作可否」のみを
 *          持つ。Windows のドライブ列挙/空き容量取得は既存の
 *          `get_available_drive_list`/`get_drive_type` と Win32 API を薄い
 *          dialog 層 (`drive_select_dialog.cpp`) で使う。
 *
 *          未移植 (未実装扱い):
 *          - `get_DriveInfoList` の Global 状態 (アイコン、バス種類、SSD/RAM、
 *            仮想ドライブ、ejectable、UNC の完全情報) への依存
 *          - 取り出し/トレイ/プロパティ/Explorer を開く実操作
 *          - Network Connect/Disconnect、ボリューム名変更、DriveGraph、
 *            ポップアップメニュー、Sort/列幅/ini 位置/外部からの定期更新
 *            (`MainFrm.cpp:1940`) の完全再現
 */
#ifndef NYANFI_GUI_DRIVE_SELECT_H
#define NYANFI_GUI_DRIVE_SELECT_H

#include <vector>

#include "usr_str.h"

namespace drive_select {

/// Win32 のドライブ種別。wx/dialog 層で raw UINT に変換する。
enum class DriveKind {
	Unknown = 0,
	NoRoot,
	Removable,
	Fixed,
	Remote,
	CdRom,
	RamDisk,
};

/// 1 ドライブ分の表示情報。VCL の drive_info から wx が必要とする項目だけを複製。
struct DriveInfo {
	UnicodeString path;             //!< "C:\\" 形式
	UnicodeString label;            //!< ボリューム名
	UnicodeString type_name;        //!< 表示用の種類名
	UnicodeString filesystem;       //!< NTFS/FAT32 等
	UnicodeString mount_path;       //!< 仮想ドライブの表示用 (未取得なら空)
	UnicodeString unc_path;         //!< ネットワークドライブの UNC (未取得なら空)
	DriveKind kind = DriveKind::Unknown;
	bool accessible = false;        //!< アクセス可能か
	bool ejectable = false;         //!< 取り出し可能か (実測取得は未移植)
	bool virtual_drive = false;     //!< 仮想ドライブか
	long long total_bytes = 0;      //!< 全体容量
	long long free_bytes = 0;       //!< 空き容量
};

/// ダイアログの初期表示オプション。
struct Options {
	bool only_accessible = true;
	bool show_icons = true;
	bool large_icons = false;
	bool to_root = true;
};

/// 容量の正規化結果。VCL は値を取得できない場合セルを空にする。
struct Capacity {
	bool valid = false;
	long long used = 0;
	long long free = 0;
	long long total = 0;
};

/// OnlyAcc の判定を適用した一覧を返す。入力順は維持する。
std::vector<DriveInfo> VisibleDrives(const std::vector<DriveInfo> &drives,
                                     const Options &options);

/// "C:\\" から "C" を返す。形式が不正なら空文字列。
UnicodeString DriveLetter(const DriveInfo &drive);

/**
 * @brief 選択したドライブの移動先を作る。
 * @param to_root VCL の ToRootCheckBox と同じ指定。実測の分岐は
 *        `ToRoot=true` で "C:"、false で "C:\\" (`src/DriveDlg.cpp:382-389`)。
 */
UnicodeString SelectionPath(const DriveInfo &drive, bool to_root);

/// 一覧の2列目に表示するボリューム/UNC/仮想ドライブ名。
UnicodeString DisplayLabel(const DriveInfo &drive);

/// キー文字列 (通常は1文字) に前方一致するドライブの添字。無ければ -1。
int FindByKey(const std::vector<DriveInfo> &drives, const UnicodeString &key);

/// アクセス可能で形式が正しいドライブか。
bool CanSelect(const DriveInfo &drive);

/// VCL `EjectDriveActionUpdate` 相当。ejectable または仮想ドライブ。
bool CanEject(const DriveInfo &drive);

/// VCL `EjectTrayActionUpdate` 相当。CD-ROM だけ。
bool CanOpenTray(const DriveInfo &drive);

/// 3/4/5 列 (使用/空き/全体) のセル文字列。容量取得不能なら空。
/// @param column VCL の列番号 (0 始まり)
UnicodeString FormatCapacityCell(const DriveInfo &drive, int column);

/// 総容量・空き容量から使用容量を計算する。0/負値/ free > total は不正。
Capacity CapacityFor(const DriveInfo &drive);

}  // namespace drive_select

#endif  // NYANFI_GUI_DRIVE_SELECT_H
