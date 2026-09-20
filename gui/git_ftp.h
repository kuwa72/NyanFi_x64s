/**
 * @file gui/git_ftp.h
 * @brief FTP/Git 外部連携の純関数層 (wx 非依存, Issue #42)
 *
 * @details VCL 版の該当を実測して wx 非依存に切り出したもの (src 必読済み):
 *   - git 呼び出し: src/Global.cpp GitShellExe (13723行) のコマンドライン組み立てと
 *     split_GitWarning (13775行) の警告分離
 *   - git log: src/GitView.cpp UpdateCommitList (292-455行) の log コマンド組み立てと
 *     タブ分割・装飾解釈、残り件数の rev-list --count (417-430行)
 *   - git grep: src/GitGrep.cpp GrepStartActionExecute (112-210行) の grep コマンド
 *     組み立てと結果行パターン RESLINE_MATCH_PTN (GitGrep.h:21)
 *   - git diff: src/GitView.cpp UpdateDiffList (547-618行) の diff/stash show、
 *     UpdateBranchList (460-538行) の tag --sort
 *   - FTP: src/FtpDlg.cpp MakeHostItem (176-190行)/HostListBoxClick (142-163行) の
 *     ホスト設定 CSV と src/MainFrm.cpp FTPConnectActionExecute (38244-38362行) の
 *     host/port/SSL 解決。接続実体 (Indy TIdFTP) は libcurl 置換対象のため、
 *     ここでは URL/SSL/パッシブへの写像だけを固定する
 *     (libcurl は curl ライセンス=MIT 派生のため MIT 可。リンクは wx 層の仕事)
 *   - 外部 DLL 境界: src/usr_arc.cpp (書庫 DLL 群)、src/usr_migemo.h (migemo.dll)、
 *     src/usr_xd2tx.h (xd2txlib.dll)、git.exe (CmdGitExe) の既知ファイル名
 *
 *   実行そのもの (CreateProcess/Execute_cmdln、libcurl 転送、git.exe の有無) は
 *   呼び出し元 (wx 層) が行う。この層は「何を実行するか・出力をどう読むか」だけを
 *   扱い、GitRunner 差し替えでモック/ドライランできる。
 *   UI は最小 (ログ出力+結果リスト) のため wx ダイアログは作らない。
 *   接続後のログは gui/log_win.h、結果リストは既存の一覧に載せる想定。
 *
 *   パスワードの cipher/uncipher (src/Global.cpp) は Global 依存のため境界外。
 *   FtpHost::pass_stored は保存形式のまま (不透明なトークンとして) 扱う。
 *
 *   このヘッダ自体は wx を含まず、nyanfi_gui_core (ルート CMakeLists.txt) で
 *   ビルド・テストする (規約8)。
 */
#ifndef NYANFI_GUI_GIT_FTP_H
#define NYANFI_GUI_GIT_FTP_H

#include <functional>
#include <vector>

namespace git_ftp {

//---------------------------------------------------------------------------
// git log: コマンド組み立てと出力の解釈 (GitView.cpp UpdateCommitList)
//---------------------------------------------------------------------------

/// log の条件 (GitViewer の表示状態に対応)
struct GitLogQuery {
	bool branches_only = false;  ///< ShowBranchesAction に対応 (--branches)
	int limit = 100;             ///< HistoryLimit に対応 (<=0 で制限なし)
	UnicodeString commit_id;     ///< CommitID/対象コミット (空なら HEAD)
	UnicodeString filter_name;   ///< FilterName (空でなければ --follow + パス引用)
};

/**
 * @brief log コマンドのパラメータを作る (UpdateCommitList 348-354行)
 * @details `log --graph [--branches] [-N] [--follow]
 *          --date=format:"%Y/%m/%d %H:%M:%S"
 *          --pretty=format:"\t%H\t%P\t%ad\t%s\t%d\t%an" [commit] ["filter"]`
 *          の順序は VCL と同じ。--branches は commit 指定なしのときだけ付く。
 */
UnicodeString BuildLogCommand(const GitLogQuery &q);

/// log の1行分の解釈結果 (git_rec に対応。時刻は文字列のまま)
struct GitLogEntry {
	UnicodeString graph;       ///< グラフ文字 (ibuf[0])
	UnicodeString hash;        ///< コミットハッシュ (ibuf[1])
	UnicodeString parent;      ///< 親ハッシュ (ibuf[2])
	UnicodeString date_str;    ///< 日時文字列 (ibuf[3])
	UnicodeString subject;     ///< 件名 (ibuf[4])
	UnicodeString decorations; ///< ブランチ等の装飾全体 (ibuf[5])
	UnicodeString author;      ///< Author名 (ibuf[6])
	UnicodeString branch;      ///< ローカルブランチ ("," 区切り)
	UnicodeString branch_remote;  ///< リモートブランチ (/HEAD は除く)
	UnicodeString tags;        ///< タグ ("\t" 区切り)
	bool is_head = false;      ///< HEAD を指すか
};

/**
 * @brief log の1行を解釈する (UpdateCommitList 363-411行)
 * @details タブ分割し、7 field あれば hash/親/日時/件名/装飾/author を取る。
 *          装飾は `( )` 内を `,` で割り、`tag: X`→タグ、(`/` を含み `/HEAD` で
 *          終わらない)→リモート、それ以外→ブランチ (`HEAD -> X`/`HEAD` は HEAD 扱い)。
 */
GitLogEntry ParseLogLine(const UnicodeString &line);

/// 複数行を解釈する (空行・グラフだけの行も落とさない。VCL と同じ)
std::vector<GitLogEntry> ParseLogOutput(const std::vector<UnicodeString> &lines);

/**
 * @brief 残り件数のコマンド (UpdateCommitList 418-419行)
 * @details `rev-list --count <parent> [-- <filter>]`
 */
UnicodeString BuildRevListCountCommand(const UnicodeString &parent,
									   const UnicodeString &filter_name);

/**
 * @brief ハッシュの短縮形 (先頭7文字。GitView.cpp 405行)
 */
UnicodeString ShortHash(const UnicodeString &hash);

//---------------------------------------------------------------------------
// git 実行の枠: コマンドラインと警告分離 (Global.cpp GitShellExe)
//---------------------------------------------------------------------------

/**
 * @brief git.exe のコマンドラインを作る (GitShellExe 13734行)
 * @details `add_quot_if_spc(CmdGitExe) + " " + prm`。VCL は prm が空でも
 *          末尾に空白を付けるが、ここでは付けない (実行結果は同じ)。
 */
UnicodeString BuildGitCommandLine(const UnicodeString &git_exe, const UnicodeString &params);

/// 警告判定 (split_GitWarning 13782行。`warning:`/`fatal:` の前方一致・大小無視)
bool IsGitWarningLine(const UnicodeString &line);

/// git 出力の本体と警告の分離結果
struct GitOutput {
	std::vector<UnicodeString> lines;     ///< 本体 (警告行を除いたもの)
	std::vector<UnicodeString> warnings;  ///< 警告 (重複除去済み)
};

/**
 * @brief 出力から警告行を分離する (split_GitWarning 13779-13790行)
 * @details `warning:`/`fatal:` で始まる行を抜き、重複を除いて警告側へ入れる。
 */
GitOutput SplitGitWarning(const std::vector<UnicodeString> &lines);

//---------------------------------------------------------------------------
// git grep: コマンド組み立てと結果の解釈 (GitGrep.cpp)
//---------------------------------------------------------------------------

/// grep の条件 (GitGrepForm の検索条件に対応)
struct GitGrepOptions {
	UnicodeString keyword;     ///< 検索語 (FindComboBox に対応。Trim 済みとして扱う)
	UnicodeString commit_id;   ///< 対象コミット (空ならワークツリー)
	UnicodeString path_mask;   ///< パス絞り込み (PathComboBox に対応)
	bool case_sensitive = false;  ///< CaseCheckBox (偽なら -i)
	bool whole_word = false;      ///< WordCheckBox (-w)
	bool use_regex = false;       ///< RegExCheckBox (-E。`\d` は `[0-9]` に直す)
};

/**
 * @brief grep コマンドのパラメータを作る (GrepStartActionExecute 121-152行)
 * @details `grep -n [-i] [-w] [-E] ("kwd" | -e 語...) [commit] [-- "mask"]`。
 *          `-e ` 始まりはそのまま付け、複数語は get_find_wd_list で割って
 *          `-e` 並びにする。正規表現時は git grep -E に `\d` が無いため
 *          `[0-9]` に置換する (128行)。
 */
UnicodeString BuildGrepCommand(const GitGrepOptions &opt);

/// grep 結果の1行 (RESLINE_MATCH_PTN に対応)
struct GitGrepHit {
	bool valid = false;        ///< パターンに一致したか
	UnicodeString hash_prefix; ///< `abcdef1:` 部分 (無ければ空)
	UnicodeString dir;         ///< ディレクトリ部分 (`src/` 等。無ければ空)
	UnicodeString file;        ///< ファイル名
	int line_no = 0;           ///< 行番号
	UnicodeString text;        ///< 本文 (コロンを含み得る)
};

/**
 * @brief grep 結果の1行を解釈する (GitGrep.cpp 180行)
 * @details パターン `^([0-9a-f]{7,}:)?(.+/)?(.+):(\d+):(.+)`。
 *          一致しなければ valid=false。
 */
GitGrepHit ParseGrepLine(const UnicodeString &line);

//---------------------------------------------------------------------------
// git diff/stash/tag (UpdateDiffList/UpdateBranchList)
//---------------------------------------------------------------------------

/// diff の条件 (選択中のコミット種別に対応)
struct GitDiffQuery {
	bool is_work = false;      ///< ワーキングツリー (パラメータなし)
	bool is_index = false;     ///< インデックス (--cached)
	UnicodeString parent;      ///< 親ハッシュ (空なら GIT_NULL_ID を渡すのは呼び出し元の仕事)
	UnicodeString commit_id;   ///< 対象ハッシュ
	UnicodeString filter_name; ///< 絞り込みパス
};

/**
 * @brief diff コマンドのパラメータを作る (UpdateDiffList 574-582行)
 * @details `diff --stat-width=120 [--cached|parent commit] [-- "filter"]`。
 *          work/index とも偽で parent/commit が空なら素の `diff --stat-width=120 `。
 */
UnicodeString BuildDiffCommand(const GitDiffQuery &q);

/// `stash show <name>` (UpdateDiffList 561行)
UnicodeString BuildStashShowCommand(const UnicodeString &stash);

/// `tag --sort=[-]v:refname` (UpdateBranchList 505-506行)
UnicodeString BuildTagCommand(bool descending);

//---------------------------------------------------------------------------
// モック/ドライラン用の実行境界 (受け入れ: 主要操作がテスト可能)
//---------------------------------------------------------------------------

/// git 実行の結果 (GitShellExe の戻り・終了コード・出力に対応)
struct GitRunResult {
	bool ok = false;                      ///< プロセス起動・通信ができたか
	long exit_code = 0;                   ///< git の終了コード
	std::vector<UnicodeString> lines;     ///< 標準出力の行
};

/// 実行の差し替え点。本番は Execute_cmdln 包み、テストは canned 応答を返す
using GitRunner = std::function<GitRunResult(const UnicodeString &params,
											 const UnicodeString &workdir)>;

/**
 * @brief log を実行して解釈する (UpdateCommitList の実行部分)
 * @param warnings_out 警告の受け皿 (null 可)
 * @details 起動失敗・終了コード非0なら空を返す。警告は分離して warnings_out に入れる。
 */
std::vector<GitLogEntry> RunGitLog(const GitRunner &runner, const UnicodeString &workdir,
								   const GitLogQuery &q, GitOutput *warnings_out = nullptr);

//---------------------------------------------------------------------------
// FTP ホスト設定 (FtpDlg.cpp MakeHostItem/HostListBoxClick)
//---------------------------------------------------------------------------

/// TLS の種類 (SSLComboBox に対応)
enum class FtpSecurity {
	None,         ///< 暗号化しない
	ExplicitTls,  ///< Explicit TLS (EXPLICIT)
	ImplicitTls,  ///< Implicit TLS (IMPLICIT)
};

/// ホスト設定の1件 (ホスト一覧の CSV 1行に対応)
struct FtpHost {
	UnicodeString name;        ///< ホスト名 (表示名)
	UnicodeString address;     ///< `host[:port]` 形式
	UnicodeString user;        ///< ユーザーID
	UnicodeString pass_stored; ///< パスワード (保存形式のまま。不透明に扱う)
	UnicodeString host_dir;    ///< ホスト開始ディレクトリ
	UnicodeString local_dir;   ///< ローカル開始ディレクトリ
	bool passive = true;       ///< PASV か (PORT 指定が無ければ真)
	FtpSecurity security = FtpSecurity::None;
	bool last_dir = false;     ///< LastDir
	bool sync_lr = false;      ///< SyncLR
};

/**
 * @brief ホスト設定の CSV 1行を作る (MakeHostItem 176-190行)
 * @details OPT 部は `PORT;` (非PASV時) + `EXPLICIT;`/`IMPLICIT;` +
 *          `LastDir;` + `SyncLR;` の順。7 field の CSV。
 */
UnicodeString BuildHostItem(const FtpHost &h);

/**
 * @brief ホスト設定の CSV 1行を解釈する (HostListBoxClick 142-163行)
 * @details OPT 部の `PORT`/`EXPLICIT`/`IMPLICIT`/`LastDir`/`SyncLR` を読む。
 *          パスワードの uncipher は境界外のためそのまま返す。
 */
FtpHost ParseHostItem(const UnicodeString &line);

/// 接続先の解決結果 (FTPConnectActionExecute 38283-38304行に対応)
struct FtpEndpoint {
	UnicodeString host;  ///< ホスト名 (ポート部を除く)
	int port = 21;       ///< ポート (省略時は plain/explicit=21, implicit=990)
	bool passive = true;
	FtpSecurity security = FtpSecurity::None;
};

/**
 * @brief ホスト設定から接続先を解決する
 * @details host は `:` の前、port は `:` の後ろ (無ければ既定)。
 */
FtpEndpoint ResolveEndpoint(const FtpHost &h);

/// 転送モード (TransferType に対応)
enum class FtpTransferMode { Ascii, Binary };

/**
 * @brief 拡張子から転送モードを決める (38030/38161行)
 * @details `test_FileExt(ext, FTPTextModeFExt)` が真なら ASCII、偽なら Binary。
 */
FtpTransferMode DecideTransferMode(const UnicodeString &ext,
								   const UnicodeString &text_ext_list);

/// libcurl への写像 (接続実体は wx 層の仕事。ここでは値だけ決める)
struct CurlSpec {
	UnicodeString url;  ///< `ftp(s)://host[:port]/path`
	int use_ssl = 0;    ///< 0=なし, 1=explicit (USE_SSL_ALL), 2=implicit (ftps://)
	bool passive = true;
};

/**
 * @brief 接続先とリモートパスから libcurl 用の値を組み立てる
 * @details implicit なら `ftps://` + 既定 990、それ以外は `ftp://` + 既定 21。
 *          既定ポートは省略する。パスは `\` を `/` に直す。
 */
CurlSpec BuildCurlSpec(const FtpEndpoint &ep, const UnicodeString &remote_path);

/**
 * @brief FTP パスの表示形 (MainFrm.cpp 9189行)
 * @details `host + "/" + yen_to_slash(path)`。
 */
UnicodeString ToFtpDisplayPath(const UnicodeString &host, const UnicodeString &path);

//---------------------------------------------------------------------------
// 外部 DLL/実行ファイルの境界
//---------------------------------------------------------------------------

/// 外部連携の相手
enum class ExternalTool {
	Git,     ///< git.exe (CmdGitExe/GitExists)
	Unrar,   ///< 書庫 DLL 群 (usr_arc.cpp)
	Migemo,  ///< migemo.dll (usr_migemo.h)
	Xd2tx,   ///< xd2txlib.dll (usr_xd2tx.h)
};

/**
 * @brief 既定のファイル名
 * @details Git=git.exe、Unrar=unrar64j.dll (書庫 DLL 群の代表)、
 *          Migemo=migemo.dll、Xd2tx=xd2txlib.dll。
 */
UnicodeString DefaultToolFile(ExternalTool tool);

/**
 * @brief 既知の外部ファイルか (大文字小文字を区別しない)
 * @details 書庫 DLL 群は `7-zip64.dll|unlha64.dll|cab64.dll|tar64.dll|
 *          unrar64j.dll|uniso64.dll` (usr_arc.cpp:40) のいずれか。
 */
bool IsKnownToolFile(const UnicodeString &file);

}  // namespace git_ftp

#endif  // NYANFI_GUI_GIT_FTP_H
