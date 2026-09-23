/**
 * @file gui/backup_settings.h
 * @brief バックアップ設定の判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/BakDlg.cpp` (`TBackupDlg`) の FormShow/FormClose、
 *          設定保存、日付条件の入力を `src/MainFrm.cpp:13715-13755` の
 *          接続とセットにして切り出したもの。
 *
 *          未移植 (未実装扱い):
 *          - 実バックアップタスクの実行 (`TaskConfig` / `TTaskThread`)
 *          - コンボボックスの履歴永続化と DelSetup/MakeNbt の実ファイル保存
 *          - 同期先の実ファイル存在確認 (候補の解決だけを行う)
 */
#ifndef NYANFI_GUI_BACKUP_SETTINGS_H
#define NYANFI_GUI_BACKUP_SETTINGS_H

#include <vector>

#include "gui/sync_dirs.h"

namespace backup_settings {

/// 日付条件の比較種別
enum class DateKind {
	None,
	Before,
	BeforeOrEqual,
	OnOrEqual,
	AfterOrEqual,
	After,
};

/// バックアップ設定の入力値
struct Options {
	UnicodeString include_mask = _T("*");
	UnicodeString exclude_mask;
	UnicodeString skip_dirs;
	bool sub_dirs = false;
	bool mirror = false;
	bool sync = false;
	UnicodeString date_condition;
	bool confirm = true;
};

/// 保存する1設定
struct Setup {
	UnicodeString name;
	Options options;
};

/**
 * @brief 日付条件文字列を検証する
 * @param text VCL の `{<|=|>}` 相対/絶対指定、`TD`、`CP`
 * @param kind_out 比較種別
 * @param normalized_out 検証後の文字列 (空なら条件なし)
 * @param error_out 失敗時の理由
 */
bool ParseDateCondition(const UnicodeString &text, DateKind &kind_out,
                        UnicodeString &normalized_out, UnicodeString &error_out);

/// VCL の `name=csv,...` 形式に1行書く
UnicodeString FormatSetupRecord(const Setup &setup);

/// VCL の設定行を読む
bool ParseSetupRecord(const UnicodeString &record, Setup &setup_out);

/// 設定名で検索する (VCL と同じ大小無視)
int FindSetupIndex(const std::vector<Setup> &setups, const UnicodeString &name);

/// 既存なら置換、なければ先頭に追加する (VCL SaveSetupActionExecute 相当)
void UpsertSetup(std::vector<Setup> &setups, const UnicodeString &name, const Options &options);

/// 設定を削除する。存在しない場合は false
bool DeleteSetup(std::vector<Setup> &setups, const UnicodeString &name);

/// バックアップの入力値と元/先を検証する
bool ValidateOptions(const Options &options, const UnicodeString &source_dir,
                     const UnicodeString &dest_dir, UnicodeString &error_out);

/// 同期設定から実の同期先候補を並べる。sync=false なら元目录だけ
std::vector<UnicodeString> ResolveDestinations(
    const UnicodeString &dest_dir, bool sync,
    const std::vector<sync_dirs::SyncEntry> &sync_settings);

/// VCL の「コマンドファイルとして保存」の本文を作る
UnicodeString MakeCommandText(const UnicodeString &source_dir, const UnicodeString &dest_dir,
                              const Setup &setup);

}  // namespace backup_settings

#endif  // NYANFI_GUI_BACKUP_SETTINGS_H
