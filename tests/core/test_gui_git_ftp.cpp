/**
 * @file tests/core/test_gui_git_ftp.cpp
 * @brief gui/git_ftp.cpp (FTP/Git 外部連携の純関数層) の回帰テスト
 *
 * @details Issue #42 (port/phase3-gitftp)。VCL 版の該当は実測:
 *   - git プロセス呼び出し層: src/Global.cpp GitShellExe (13723行) の
 *     コマンドライン組み立て (`add_quot_if_spc(CmdGitExe) + " " + prm`) と
 *     警告分離 split_GitWarning (13775行, `warning:`/`fatal:` 前方一致・重複除去)
 *   - git log: src/GitView.cpp UpdateCommitList (292行) の
 *     `log --graph ... --pretty=format:"\t%H\t%P\t%ad\t%s\t%d\t%an"` 組み立てと
 *     タブ分割・装飾 (`(...)` 内の `tag:`/リモート/ブランチ/HEAD) の解釈、
 *     残り件数の `rev-list --count` (418行)
 *   - git grep: src/GitGrep.cpp GrepStartActionExecute (112行) の
 *     `grep -n [-i] [-w] [-E] ... [commit] [-- "mask"]` 組み立てと
 *     結果行パターン RESLINE_MATCH_PTN (GitGrep.h:21)
 *   - git diff: src/GitView.cpp UpdateDiffList (547行) の
 *     `diff --stat-width=120 [--cached|parent commit]` と
 *     `stash show <name>` (561行)、`tag --sort=[-]v:refname` (505行)
 *   - FTP ホスト設定: src/FtpDlg.cpp MakeHostItem (176行) の CSV/OPT 組み立てと
 *     HostListBoxClick (142行) の解釈、src/MainFrm.cpp FTPConnectActionExecute
 *     (38244行) の host/port/SSL 解決。接続自体 (Indy TIdFTP) は libcurl への
 *     置換対象のため、ここでは URL/SSL/パッシブへの写像だけを固定する
 *     (libcurl は curl ライセンス=MIT 派生のため MIT 可)
 *   - 外部 DLL 境界: src/usr_arc.cpp (書庫 DLL 群)、src/usr_migemo.h
 *     (migemo.dll)、src/usr_xd2tx.h (xd2txlib.dll) + git.exe (CmdGitExe) の
 *     既知ファイル名の整理
 *
 *   UI は最小 (ログ出力+結果リスト) のため wx ダイアログは作らず、
 *   実行は呼び出し元 (wx 層) が行い、ここでは組み立て・解釈・ドライラン
 *   (GitRunner への差し替え) だけを扱う。GitBusy/ヒント/描画は対象外。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/git_ftp.h"

using git_ftp::FtpHost;
using git_ftp::GitLogQuery;

//===========================================================================
// BuildLogCommand: UpdateCommitList (GitView.cpp 348-354行)
//===========================================================================

TEST_CASE("BuildLogCommand: 基本形 (graph+日付書式+pretty)")
{
	GitLogQuery q;
	q.limit = 100;
	UnicodeString cmd = git_ftp::BuildLogCommand(q);
	CHECK(cmd == _T("log --graph -100 --date=format:\"%Y/%m/%d %H:%M:%S\" --pretty=format:\"\t%H\t%P\t%ad\t%s\t%d\t%an\""));
}

TEST_CASE("BuildLogCommand: --branches は commit 指定なしのときだけ")
{
	GitLogQuery q;
	q.branches_only = true;
	CHECK(ContainsStr(git_ftp::BuildLogCommand(q), _T("--branches")));

	q.commit_id = _T("abc1234");
	CHECK_FALSE(ContainsStr(git_ftp::BuildLogCommand(q), _T("--branches")));
	CHECK(ContainsStr(git_ftp::BuildLogCommand(q), _T("abc1234")));
}

TEST_CASE("BuildLogCommand: フィルタ指定で --follow と引用パス")
{
	GitLogQuery q;
	q.filter_name = _T("src/MainFrm.cpp");
	UnicodeString cmd = git_ftp::BuildLogCommand(q);
	CHECK(ContainsStr(cmd, _T("--follow")));
	CHECK(ContainsStr(cmd, _T("\"src/MainFrm.cpp\"")));
}

TEST_CASE("BuildLogCommand: limit<=0 なら件数制限なし")
{
	GitLogQuery q;
	q.limit = 0;
	CHECK_FALSE(ContainsStr(git_ftp::BuildLogCommand(q), _T(" -0")));
}

//===========================================================================
// ParseLogLine: UpdateCommitList のタブ分割・装飾解釈 (361-410行)
//===========================================================================

TEST_CASE("ParseLogLine: 通常コミット (hash/親/日時/件名/装飾/author)")
{
	// graph \t hash \t parent \t date \t subject \t decorations \t author
	UnicodeString line =
		_T("*") _T("\t") _T("0123456789abcdef0123456789abcdef01234567") _T("\t")
		_T("89abcdef0123456789abcdef0123456789abcdef01") _T("\t") _T("2026/01/02 03:04:05") _T("\t")
		_T("subject here") _T("\t") _T("(HEAD -> main, tag: v1.0, origin/main)") _T("\t") _T("Author");
	git_ftp::GitLogEntry e = git_ftp::ParseLogLine(line);
	CHECK(e.graph == _T("*"));
	CHECK(e.hash == _T("0123456789abcdef0123456789abcdef01234567"));
	CHECK(e.parent == _T("89abcdef0123456789abcdef0123456789abcdef01"));
	CHECK(e.subject == _T("subject here"));
	CHECK(e.author == _T("Author"));
	CHECK(e.is_head);
	CHECK(ContainsStr(e.branch, _T("main")));
	CHECK(ContainsStr(e.tags, _T("v1.0")));
	CHECK(ContainsStr(e.branch_remote, _T("origin/main")));
}

TEST_CASE("ParseLogLine: /HEAD 付きリモートは除外される")
{
	UnicodeString line =
		_T("*") _T("\t") _T("aaaa") _T("\t") _T("bbbb") _T("\t") _T("2026/01/02 03:04:05") _T("\t")
		_T("s") _T("\t") _T("(origin/HEAD, origin/main)") _T("\t") _T("A");
	git_ftp::GitLogEntry e = git_ftp::ParseLogLine(line);
	CHECK_FALSE(e.is_head);
	CHECK(e.branch_remote == _T("origin/main"));
}

TEST_CASE("ParseLogLine: グラフだけの行も落とさない")
{
	git_ftp::GitLogEntry e = git_ftp::ParseLogLine(_T("*"));
	CHECK(e.graph == _T("*"));
	CHECK(e.hash.IsEmpty());
}

//===========================================================================
// BuildRevListCountCommand / ShortHash
//===========================================================================

TEST_CASE("BuildRevListCountCommand: 残り件数 (418-419行)")
{
	CHECK(git_ftp::BuildRevListCountCommand(_T("abc"), EmptyStr) == _T("rev-list --count abc"));
	CHECK(git_ftp::BuildRevListCountCommand(_T("abc"), _T("f.cpp")) == _T("rev-list --count abc -- f.cpp"));
}

TEST_CASE("ShortHash: 先頭7文字")
{
	CHECK(git_ftp::ShortHash(_T("0123456789abcdef")) == _T("0123456"));
	CHECK(git_ftp::ShortHash(_T("ab")) == _T("ab"));
}

//===========================================================================
// SplitGitWarning / IsGitWarningLine: split_GitWarning (13775-13790行)
//===========================================================================

TEST_CASE("SplitGitWarning: warning:/fatal: を分離し重複を除く")
{
	std::vector<UnicodeString> lines = {_T("warning: foo"), _T("ok"), _T("warning: foo"),
									   _T("fatal: bar")};
	git_ftp::GitOutput out = git_ftp::SplitGitWarning(lines);
	CHECK(out.lines.size() == 1);
	CHECK(out.lines[0] == _T("ok"));
	CHECK(out.warnings.size() == 2);
}

TEST_CASE("IsGitWarningLine: 前方一致・大小無視")
{
	CHECK(git_ftp::IsGitWarningLine(_T("warning: x")));
	CHECK(git_ftp::IsGitWarningLine(_T("FATAL: x")));
	CHECK_FALSE(git_ftp::IsGitWarningLine(_T("no warning: x")));
}

//===========================================================================
// BuildGitCommandLine: GitShellExe (13734行)
//===========================================================================

TEST_CASE("BuildGitCommandLine: 空白付き exe は引用される")
{
	CHECK(git_ftp::BuildGitCommandLine(_T("C:\\Program Files\\Git\\bin\\git.exe"), _T("status")) ==
		  _T("\"C:\\Program Files\\Git\\bin\\git.exe\" status"));
	CHECK(git_ftp::BuildGitCommandLine(_T("git.exe"), EmptyStr) == _T("git.exe"));
}

//===========================================================================
// BuildGrepCommand: GrepStartActionExecute (121-152行)
//===========================================================================

TEST_CASE("BuildGrepCommand: 単一語")
{
	git_ftp::GitGrepOptions opt;
	opt.keyword = _T("foo");
	CHECK(git_ftp::BuildGrepCommand(opt) == _T("grep -n -i \"foo\""));
}

TEST_CASE("BuildGrepCommand: 複数語は -e 並び、大文字小文字区別・単語・正規表現")
{
	git_ftp::GitGrepOptions opt;
	opt.keyword = _T("foo bar");
	opt.case_sensitive = true;
	opt.whole_word = true;
	opt.use_regex = true;
	UnicodeString cmd = git_ftp::BuildGrepCommand(opt);
	CHECK(cmd == _T("grep -n -w -E -e foo -e bar"));
}

TEST_CASE("BuildGrepCommand: -e 始まりはそのまま、commit と path を付加")
{
	git_ftp::GitGrepOptions opt;
	opt.keyword = _T("-e foo");
	opt.commit_id = _T("HEAD");
	opt.path_mask = _T("*.cpp");
	CHECK(git_ftp::BuildGrepCommand(opt) == _T("grep -n -i -e foo HEAD -- \"*.cpp\""));
}

//===========================================================================
// ParseGrepLine: RESLINE_MATCH_PTN (GitGrep.h:21)
//===========================================================================

TEST_CASE("ParseGrepLine: commit付き・ディレクトリ付き")
{
	git_ftp::GitGrepHit h = git_ftp::ParseGrepLine(_T("abc1234:src/foo.cpp:10:text here"));
	CHECK(h.valid);
	CHECK(h.hash_prefix == _T("abc1234:"));
	CHECK(h.dir == _T("src/"));
	CHECK(h.file == _T("foo.cpp"));
	CHECK(h.line_no == 10);
	CHECK(h.text == _T("text here"));
}

TEST_CASE("ParseGrepLine: 素朴な path:行:本文")
{
	git_ftp::GitGrepHit h = git_ftp::ParseGrepLine(_T("foo.cpp:3:body:with:colons"));
	CHECK(h.valid);
	CHECK(h.file == _T("foo.cpp"));
	CHECK(h.line_no == 3);
	CHECK(h.text == _T("body:with:colons"));
}

TEST_CASE("ParseGrepLine: 不正行は invalid")
{
	CHECK_FALSE(git_ftp::ParseGrepLine(_T("no match here")).valid);
	CHECK_FALSE(git_ftp::ParseGrepLine(EmptyStr).valid);
}

//===========================================================================
// BuildDiffCommand / BuildStashShowCommand / BuildTagCommand
//===========================================================================

TEST_CASE("BuildDiffCommand: work/index/通常 (574-582行)")
{
	git_ftp::GitDiffQuery q;
	CHECK(git_ftp::BuildDiffCommand(q) == _T("diff --stat-width=120 "));

	q.is_work = true;
	CHECK(git_ftp::BuildDiffCommand(q) == _T("diff --stat-width=120 "));

	q.is_work = false;
	q.is_index = true;
	CHECK(git_ftp::BuildDiffCommand(q) == _T("diff --stat-width=120 --cached"));

	q.is_index = false;
	q.parent = _T("ppp");
	q.commit_id = _T("ccc");
	q.filter_name = _T("a.cpp");
	CHECK(git_ftp::BuildDiffCommand(q) == _T("diff --stat-width=120 ppp ccc -- \"a.cpp\""));
}

TEST_CASE("BuildStashShowCommand / BuildTagCommand")
{
	CHECK(git_ftp::BuildStashShowCommand(_T("stash@{0}")) == _T("stash show stash@{0}"));
	CHECK(git_ftp::BuildTagCommand(false) == _T("tag --sort=v:refname"));
	CHECK(git_ftp::BuildTagCommand(true) == _T("tag --sort=-v:refname"));
}

//===========================================================================
// RunGitLog: モック/ドライランでテスト可能 (受け入れ条件)
//===========================================================================

TEST_CASE("RunGitLog: Runner 差し替えで解釈まで通る")
{
	git_ftp::GitRunner runner = [](const UnicodeString &params, const UnicodeString &workdir)
		-> git_ftp::GitRunResult {
		CHECK(ContainsStr(params, _T("log --graph")));
		CHECK(workdir == _T("C:\\repo"));
		git_ftp::GitRunResult r;
		r.ok = true;
		r.exit_code = 0;
		r.lines.push_back(
			UnicodeString(_T("*")) + _T("\t") + _T("0123456789abcdef0123456789abcdef01234567") +
			_T("\t") + _T("p") + _T("\t") + _T("2026/01/02 03:04:05") + _T("\t") + _T("s") +
			_T("\t") + _T("(HEAD -> main)") + _T("\t") + _T("A"));
		r.lines.push_back(_T("warning: w"));
		return r;
	};
	git_ftp::GitOutput warn;
	GitLogQuery q;
	std::vector<git_ftp::GitLogEntry> v = git_ftp::RunGitLog(runner, _T("C:\\repo"), q, &warn);
	CHECK(v.size() == 1);
	CHECK(v[0].is_head);
	CHECK(warn.warnings.size() == 1);
}

TEST_CASE("RunGitLog: 失敗時は空")
{
	git_ftp::GitRunner runner = [](const UnicodeString &, const UnicodeString &) {
		git_ftp::GitRunResult r;
		r.ok = false;
		return r;
	};
	GitLogQuery q;
	CHECK(git_ftp::RunGitLog(runner, _T("C:\\repo"), q).empty());
}

//===========================================================================
// FTP ホスト設定: MakeHostItem (176-190行) / HostListBoxClick (142-163行)
//===========================================================================

TEST_CASE("BuildHostItem/ParseHostItem: 往復する")
{
	FtpHost h;
	h.name = _T("myhost");
	h.address = _T("example.com:2121");
	h.user = _T("user");
	h.pass_stored = _T("TOKEN");
	h.host_dir = _T("/pub");
	h.local_dir = _T("C:\\tmp");
	h.passive = false;
	h.security = git_ftp::FtpSecurity::ExplicitTls;
	h.sync_lr = true;
	UnicodeString item = git_ftp::BuildHostItem(h);
	CHECK(ContainsStr(item, _T("PORT;")));
	CHECK(ContainsStr(item, _T("EXPLICIT;")));
	CHECK(ContainsStr(item, _T("SyncLR;")));
	FtpHost back = git_ftp::ParseHostItem(item);
	CHECK(back.name == _T("myhost"));
	CHECK(back.address == _T("example.com:2121"));
	CHECK(back.user == _T("user"));
	CHECK(back.pass_stored == _T("TOKEN"));
	CHECK(back.host_dir == _T("/pub"));
	CHECK_FALSE(back.passive);
	CHECK(back.security == git_ftp::FtpSecurity::ExplicitTls);
	CHECK(back.sync_lr);
	CHECK_FALSE(back.last_dir);
}

TEST_CASE("ParseHostItem: anonymous はそのまま通る")
{
	FtpHost h;
	h.name = _T("n");
	h.address = _T("a");
	h.user = _T("anonymous");
	h.pass_stored = _T("");
	UnicodeString item = git_ftp::BuildHostItem(h);
	FtpHost back = git_ftp::ParseHostItem(item);
	CHECK(back.user == _T("anonymous"));
	CHECK(back.passive);
	CHECK(back.security == git_ftp::FtpSecurity::None);
}

//===========================================================================
// ResolveEndpoint: FTPConnectActionExecute (38283-38304行)
//===========================================================================

TEST_CASE("ResolveEndpoint: 既定ポート (plain/explicit=21, implicit=990)")
{
	FtpHost h;
	h.address = _T("example.com");
	h.security = git_ftp::FtpSecurity::None;
	git_ftp::FtpEndpoint ep = git_ftp::ResolveEndpoint(h);
	CHECK(ep.host == _T("example.com"));
	CHECK(ep.port == 21);

	h.security = git_ftp::FtpSecurity::ImplicitTls;
	CHECK(git_ftp::ResolveEndpoint(h).port == 990);

	h.address = _T("example.com:2121");
	CHECK(git_ftp::ResolveEndpoint(h).port == 2121);
}

//===========================================================================
// DecideTransferMode / BuildCurlSpec / ToFtpDisplayPath
//===========================================================================

TEST_CASE("DecideTransferMode: テキスト拡張子なら ASCII")
{
	// リスト形式は VCL の FTPTextModeFExt (".txt.htm.html..."、ドット連結) に合わせる
	CHECK(git_ftp::DecideTransferMode(_T(".txt"), _T(".txt.htm.html")) ==
		  git_ftp::FtpTransferMode::Ascii);
	CHECK(git_ftp::DecideTransferMode(_T(".EXE"), _T(".txt.htm.html")) ==
		  git_ftp::FtpTransferMode::Binary);
}

TEST_CASE("BuildCurlSpec: libcurl への写像 (URL/SSL/パッシブ)")
{
	git_ftp::FtpEndpoint ep;
	ep.host = _T("example.com");
	ep.port = 21;
	ep.passive = true;
	ep.security = git_ftp::FtpSecurity::ExplicitTls;
	git_ftp::CurlSpec spec = git_ftp::BuildCurlSpec(ep, _T("/pub/a.txt"));
	CHECK(spec.url == _T("ftp://example.com/pub/a.txt"));
	CHECK(spec.use_ssl == 1);
	CHECK(spec.passive);

	ep.security = git_ftp::FtpSecurity::ImplicitTls;
	ep.port = 990;
	spec = git_ftp::BuildCurlSpec(ep, _T("/a.txt"));
	CHECK(spec.url == _T("ftps://example.com/a.txt"));
	CHECK(spec.use_ssl == 2);

	ep.security = git_ftp::FtpSecurity::None;
	ep.port = 2121;
	spec = git_ftp::BuildCurlSpec(ep, _T("/a.txt"));
	CHECK(spec.url == _T("ftp://example.com:2121/a.txt"));
	CHECK(spec.use_ssl == 0);
}

TEST_CASE("ToFtpDisplayPath: host/パスの表示 (9189行)")
{
	CHECK(git_ftp::ToFtpDisplayPath(_T("example.com"), _T("\\pub\\a.txt")) ==
		  _T("example.com/pub/a.txt"));
}

//===========================================================================
// 外部ツール境界: git.exe + DLL 群
//===========================================================================

TEST_CASE("DefaultToolFile/IsKnownToolFile: 既知の外部ファイル")
{
	CHECK(git_ftp::DefaultToolFile(git_ftp::ExternalTool::Git) == _T("git.exe"));
	CHECK(git_ftp::IsKnownToolFile(_T("git.exe")));
	CHECK(git_ftp::IsKnownToolFile(_T("migemo.dll")));
	CHECK(git_ftp::IsKnownToolFile(_T("xd2txlib.dll")));
	CHECK(git_ftp::IsKnownToolFile(_T("unrar64j.dll")));
	CHECK_FALSE(git_ftp::IsKnownToolFile(_T("unknown.dll")));
}
