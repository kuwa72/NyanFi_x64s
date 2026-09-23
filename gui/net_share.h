/**
 * @file gui/net_share.h
 * @brief ネットワーク共有ダイアログの判断ロジック (wx 非依存)
 *
 * @details VCL 実測:
 * - `src/usr_cmdlist.cpp:221` に `F:ShareList`
 * - `src/MainFrm.cpp:25862-25892` が現在コンピュータ名を `TNetShareDlg` に渡す
 * - `src/ShareDlg.cpp:85-91/172-225` が UNC 共有の一覧取得・接続・再取得
 *
 * wx の表示は `gui/net_share_dialog.h`、OS 依存の列挙/接続は
 * `gui/misc_ops.h` の移植済み関数だけを使う。
 *
 * 未移植 (未実装扱い):
 * - リモート `NetShareEnum` と `WNetAddConnection3W` による接続
 * - `TNetShareDlg` のライブラリ/検索設定/ディレクトリ選択モード
 */
#ifndef NYANFI_GUI_NET_SHARE_H
#define NYANFI_GUI_NET_SHARE_H

#include <vector>

#include "usr_str.h"

namespace net_share {

/// 入力検証の結果。ok のとき value を正規化済み入力として使う。
struct ValidationResult {
	bool ok = false;
	UnicodeString value;
	UnicodeString error;
};

/// 共有1件。VCL の SHARE_INFO_1 から wx 側へ渡す情報だけを持つ。
struct ShareItem {
	UnicodeString name;
	UnicodeString local_path;
	UnicodeString remark;
};

/// 最初の共有列挙の結果。
enum class ShareListResult {
	Success,
	Failure,
};

/// 接続操作の結果 (VCL UpdateShareList の WNetAddConnection3W 分岐)。
enum class ConnectResult {
	NotAttempted,
	Success,
	Cancelled,
	Failure,
};

/// 共有一覧取得・接続の次動作。
enum class ConnectionAction {
	UseExisting,
	Connect,
	Cancel,
	ShowError,
};

/// コンピュータ名 (host または \\host) を \\host に正規化する。
ValidationResult NormalizeComputer(const UnicodeString &input);

/// \\server\share 形式を検証し、末尾区切りを除いて正規化する。
ValidationResult NormalizeUncPath(const UnicodeString &input);

/// VCL UpdateShareList と同じ接続可否の分岐。
ConnectionAction ResolveConnectionAction(ShareListResult list_result,
                                         ConnectResult connect_result);

/// VCL の yen_to_delimiter 相当 (\\host -> //host)。
UnicodeString FormatComputerCaption(const UnicodeString &computer);

/// \\host\share を組み立てる。入力不正時は空文字列。
UnicodeString MakeUncSharePath(const UnicodeString &computer, const UnicodeString &share);

/// 一覧表示用。共有の UNC にローカルパス・コメントを付ける。
UnicodeString FormatShareRow(const UnicodeString &computer, const ShareItem &item);

/// 管理共有を除外し、filter で大小文字を無視して絞り込み、共有名順に並べる。
std::vector<ShareItem> SortFilterShares(const std::vector<ShareItem> &items,
                                        const UnicodeString &filter);

}  // namespace net_share

#endif  // NYANFI_GUI_NET_SHARE_H
