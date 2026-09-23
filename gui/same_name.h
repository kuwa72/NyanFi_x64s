/**
 * @file gui/same_name.h
 * @brief 同名ファイル処理ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 版の `src/SameDlg.cpp` (`TSameNameDlg`) のモード選択、
 *          全件適用の排他、同一パス時の強制変更、信息欄の比較表示を
 *          実測して切り出したもの。wx の入力は `gui/same_name_dialog.h`。
 *
 *          VCL の呼び出し元は MainFrm.cpp:3999-4019（タスクコピー）、
 *          9847-9867（解凍コピー）、37948-37979（ダウンロード）、
 *          38094-38142（アップロード）にある。
 *
 *          未移植 (未実装扱い):
 *          - TTaskThread / FTP / Git に対する実コピー処理
 *          - `CopyAll=false` の全衝突列挙（各タスクごとの再表示）
 *          - 情報の色分け・ファイル名の PathNameOut 描画
 */
#ifndef NYANFI_GUI_SAME_NAME_H
#define NYANFI_GUI_SAME_NAME_H

#include <functional>

namespace same_name {

/// VCL の CPYMD_* (0〜4)
enum class Mode {
	Overwrite = 0,
	KeepNewer = 1,
	Skip = 2,
	AutoRename = 3,
	ManualRename = 4,
};

/// ダイアログに表示する1組のファイル
struct Context {
	UnicodeString source;
	UnicodeString destination;
	Int64 source_size = 0;
	Int64 destination_size = 0;
	double source_time = 0;
	double destination_time = 0;
	bool same_path = false;
	int task_no = -1;
	UnicodeString initial_name;
};

/// ダイアログの戻り値
struct Options {
	Mode mode = Mode::Overwrite;
	bool copy_all = true;
	UnicodeString rename_name;
};

/// 同一パスで許可されないモードを判定する
bool IsModeEnabled(Mode mode, bool same_path);

/// 同一パス時に VCL と同じモードへ正規化する
Mode NormalizeMode(Mode mode, bool same_path);

/// モード・全件適用の相互排他と初期名を整える
Options NormalizeOptions(const Context &context, const Options &options);

/// このモードで実コピーへ進めるか (最新だけ/スキップの判定)
bool ShouldCopy(Mode mode, Int64 source_size, double source_time,
                Int64 destination_size, double destination_time);

/// VCL の自動改名形式で空いている名前を作る
UnicodeString MakeAutoRenamePath(const UnicodeString &source, const UnicodeString &dest_dir,
                                 bool source_is_dir,
                                 const std::function<bool(const UnicodeString &)> &taken,
                                 int limit = 10000);

/// 手動改名の初期名
UnicodeString DefaultRenameName(const Context &context);

/// 情報欄の比較文
UnicodeString SizeSummary(Int64 source_size, Int64 destination_size);
UnicodeString TimeSummary(double source_time, double destination_time);

}  // namespace same_name

#endif  // NYANFI_GUI_SAME_NAME_H
