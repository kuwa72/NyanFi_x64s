/**
 * @file gui/git_ftp.cpp
 * @brief gui/git_ftp.h の実装 (wx 非依存)
 */
#include "gui/git_ftp.h"

#include "usr_file_ex.h"
#include "usr_str.h"

namespace git_ftp {

namespace {

// log --pretty の field 数 (graph + 6項目)
constexpr int kLogFieldCount = 7;

}  // namespace

//---------------------------------------------------------------------------
UnicodeString BuildLogCommand(const GitLogQuery &q)
{
	UnicodeString prm = _T("log --graph");
	if (q.branches_only && q.commit_id.IsEmpty()) prm += _T(" --branches");
	if (q.limit > 0) prm.cat_sprintf(_T(" -%u"), static_cast<unsigned>(q.limit));
	if (!q.filter_name.IsEmpty()) prm += _T(" --follow");
	prm += _T(" --date=format:\"%Y/%m/%d %H:%M:%S\""
			  " --pretty=format:\"\t%H\t%P\t%ad\t%s\t%d\t%an\"");
	if (!q.commit_id.IsEmpty()) prm.cat_sprintf(_T(" %s"), q.commit_id.c_str());
	if (!q.filter_name.IsEmpty()) prm.cat_sprintf(_T(" \"%s\""), q.filter_name.c_str());
	return prm;
}

//---------------------------------------------------------------------------
GitLogEntry ParseLogLine(const UnicodeString &line)
{
	GitLogEntry e;
	TStringDynArray ibuf = split_strings_tab(line);
	if (ibuf.Length <= 0) return e;

	e.graph = ibuf[0];
	if (ibuf.Length != kLogFieldCount) return e;

	e.hash = ibuf[1];
	e.parent = ibuf[2];
	e.date_str = ibuf[3];
	e.subject = ibuf[4];
	e.decorations = ibuf[5];
	e.author = ibuf[6];

	// 装飾の解釈 (UpdateCommitList 379-395行)
	UnicodeString br_str = get_in_paren(ibuf[5]);
	TStringDynArray b_buf = SplitString(br_str, _T(","));
	for (int j = 0; j < b_buf.Length; j++) {
		UnicodeString ss = Trim(b_buf[j]);
		if (remove_top_text(ss, _T("tag: "))) {
			if (!e.tags.IsEmpty()) e.tags += _T("\t");
			e.tags += ss;
		}
		else if (ContainsStr(ss, _T("/"))) {
			if (!EndsStr(_T("/HEAD"), ss)) e.branch_remote = ss;
		}
		else {
			e.is_head = e.is_head || StartsStr(_T("HEAD -> "), ss) || SameStr(ss, _T("HEAD"));
			if (!e.branch.IsEmpty()) e.branch += _T(",");
			e.branch += ss;
		}
	}
	return e;
}

//---------------------------------------------------------------------------
std::vector<GitLogEntry> ParseLogOutput(const std::vector<UnicodeString> &lines)
{
	std::vector<GitLogEntry> result;
	for (const UnicodeString &line : lines) result.push_back(ParseLogLine(line));
	return result;
}

//---------------------------------------------------------------------------
UnicodeString BuildRevListCountCommand(const UnicodeString &parent,
									   const UnicodeString &filter_name)
{
	UnicodeString prm;
	prm.sprintf(_T("rev-list --count %s"), parent.c_str());
	if (!filter_name.IsEmpty())
		prm.cat_sprintf(_T(" -- %s"), add_quot_if_spc(filter_name).c_str());
	return prm;
}

//---------------------------------------------------------------------------
UnicodeString ShortHash(const UnicodeString &hash)
{
	return hash.SubString(1, 7);
}

//---------------------------------------------------------------------------
UnicodeString BuildGitCommandLine(const UnicodeString &git_exe, const UnicodeString &params)
{
	UnicodeString cmdln = add_quot_if_spc(git_exe);
	if (!params.IsEmpty()) cmdln.cat_sprintf(_T(" %s"), params.c_str());
	return cmdln;
}

//---------------------------------------------------------------------------
bool IsGitWarningLine(const UnicodeString &line)
{
	return StartsText(_T("warning:"), line) || StartsText(_T("fatal:"), line);
}

//---------------------------------------------------------------------------
GitOutput SplitGitWarning(const std::vector<UnicodeString> &lines)
{
	GitOutput out;
	for (const UnicodeString &line : lines) {
		if (IsGitWarningLine(line)) {
			bool dup = false;
			for (const UnicodeString &w : out.warnings) {
				if (SameStr(w, line)) {
					dup = true;
					break;
				}
			}
			if (!dup) out.warnings.push_back(line);
		}
		else {
			out.lines.push_back(line);
		}
	}
	return out;
}

//---------------------------------------------------------------------------
UnicodeString BuildGrepCommand(const GitGrepOptions &opt)
{
	UnicodeString prm = _T("grep -n");
	if (!opt.case_sensitive) prm += _T(" -i");
	if (opt.whole_word) prm += _T(" -w");

	UnicodeString kwd = Trim(opt.keyword);
	if (opt.use_regex) {
		prm += _T(" -E");
		// git grep -E に \d が無いため [0-9] に直す (GitGrep.cpp:128)
		kwd = TRegEx::Replace(kwd, _T("([^\\\\])?(\\\\d)"), _T("\\1[0-9]"));
	}

	if (StartsStr(_T("-e "), kwd)) {
		prm += (_T(" ") + kwd);
	}
	else {
		TStringList wlst;
		get_find_wd_list(kwd, &wlst);
		if (wlst.Count == 1) {
			prm.cat_sprintf(_T(" \"%s\""), wlst.Strings[0].c_str());
		}
		else {
			for (int i = 0; i < wlst.Count; i++) {
				prm.cat_sprintf(_T(" -e %s"), wlst.Strings[i].c_str());
			}
		}
	}

	if (!opt.commit_id.IsEmpty()) prm += (_T(" ") + opt.commit_id);
	if (!opt.path_mask.IsEmpty()) prm += (_T(" -- \"") + opt.path_mask + _T("\""));
	return prm;
}

//---------------------------------------------------------------------------
GitGrepHit ParseGrepLine(const UnicodeString &line)
{
	GitGrepHit hit;
	TRegExOptions re_opt;
	re_opt << roIgnoreCase;
	TMatch mt = TRegEx::Match(line, _T("^([0-9a-f]{7,}:)?(.+/)?(.+):(\\d+):(.+)"), re_opt);
	if (!mt.Success) return hit;
	hit.valid = true;
	hit.hash_prefix = mt.Groups.Item[1].Value;
	hit.dir = mt.Groups.Item[2].Value;
	hit.file = mt.Groups.Item[3].Value;
	hit.line_no = mt.Groups.Item[4].Value.ToIntDef(0);
	hit.text = mt.Groups.Item[5].Value;
	return hit;
}

//---------------------------------------------------------------------------
UnicodeString BuildDiffCommand(const GitDiffQuery &q)
{
	UnicodeString prm = _T("diff --stat-width=120 ");
	if (q.is_work) {
		;
	}
	else if (q.is_index) {
		prm += _T("--cached");
	}
	else if (!q.parent.IsEmpty() || !q.commit_id.IsEmpty()) {
		prm += (q.parent + _T(" ") + q.commit_id);
	}
	if (!q.filter_name.IsEmpty()) prm.cat_sprintf(_T(" -- \"%s\""), q.filter_name.c_str());
	return prm;
}

//---------------------------------------------------------------------------
UnicodeString BuildStashShowCommand(const UnicodeString &stash)
{
	return _T("stash show ") + stash;
}

//---------------------------------------------------------------------------
UnicodeString BuildTagCommand(bool descending)
{
	UnicodeString prm = _T("tag --sort=");
	if (descending) prm += _T("-");
	prm += _T("v:refname");
	return prm;
}

//---------------------------------------------------------------------------
std::vector<GitLogEntry> RunGitLog(const GitRunner &runner, const UnicodeString &workdir,
								   const GitLogQuery &q, GitOutput *warnings_out)
{
	GitRunResult r = runner(BuildLogCommand(q), workdir);
	if (!r.ok || r.exit_code != 0) return {};
	GitOutput out = SplitGitWarning(r.lines);
	if (warnings_out) *warnings_out = out;
	return ParseLogOutput(out.lines);
}

//---------------------------------------------------------------------------
UnicodeString BuildHostItem(const FtpHost &h)
{
	UnicodeString optstr;
	if (!h.passive) optstr += _T("PORT;");
	if (h.security == FtpSecurity::ExplicitTls)
		optstr += _T("EXPLICIT;");
	else if (h.security == FtpSecurity::ImplicitTls)
		optstr += _T("IMPLICIT;");
	if (h.last_dir) optstr += _T("LastDir;");
	if (h.sync_lr) optstr += _T("SyncLR;");

	return make_csv_rec_str(
		{h.name, h.address, h.user, h.pass_stored, h.host_dir, h.local_dir, optstr});
}

//---------------------------------------------------------------------------
FtpHost ParseHostItem(const UnicodeString &line)
{
	FtpHost h;
	TStringDynArray itm = get_csv_array(line, 7, true);
	if (itm.Length < 7) return h;
	h.name = itm[0];
	h.address = itm[1];
	h.user = itm[2];
	h.pass_stored = itm[3];
	h.host_dir = itm[4];
	h.local_dir = itm[5];

	UnicodeString opt = itm[6];
	h.passive = !ContainsText(opt, _T("PORT"));
	if (ContainsText(opt, _T("EXPLICIT")))
		h.security = FtpSecurity::ExplicitTls;
	else if (ContainsText(opt, _T("IMPLICIT")))
		h.security = FtpSecurity::ImplicitTls;
	h.last_dir = ContainsText(opt, _T("LastDir"));
	h.sync_lr = ContainsText(opt, _T("SyncLR"));
	return h;
}

//---------------------------------------------------------------------------
FtpEndpoint ResolveEndpoint(const FtpHost &h)
{
	FtpEndpoint ep;
	ep.passive = h.passive;
	ep.security = h.security;
	ep.host = get_tkn(h.address, _T(":"));
	int port_no = get_tkn_r(h.address, _T(":")).ToIntDef(0);
	if (port_no > 0) {
		ep.port = port_no;
	}
	else {
		ep.port = (h.security == FtpSecurity::ImplicitTls) ? 990 : 21;
	}
	return ep;
}

//---------------------------------------------------------------------------
FtpTransferMode DecideTransferMode(const UnicodeString &ext,
								   const UnicodeString &text_ext_list)
{
	return test_FileExt(ext, text_ext_list) ? FtpTransferMode::Ascii
											: FtpTransferMode::Binary;
}

//---------------------------------------------------------------------------
CurlSpec BuildCurlSpec(const FtpEndpoint &ep, const UnicodeString &remote_path)
{
	CurlSpec spec;
	spec.passive = ep.passive;
	const bool implicit = (ep.security == FtpSecurity::ImplicitTls);
	spec.use_ssl = implicit ? 2 : (ep.security == FtpSecurity::ExplicitTls ? 1 : 0);

	const int def_port = implicit ? 990 : 21;
	UnicodeString hostport = ep.host;
	if (ep.port != def_port) hostport.cat_sprintf(_T(":%d"), ep.port);

	UnicodeString path = yen_to_slash(remote_path);
	while (StartsStr(_T("/"), path)) path.Delete(1, 1);
	spec.url = (implicit ? _T("ftps://") : _T("ftp://")) + hostport + _T("/") + path;
	return spec;
}

//---------------------------------------------------------------------------
UnicodeString ToFtpDisplayPath(const UnicodeString &host, const UnicodeString &path)
{
	UnicodeString p = yen_to_slash(path);
	while (StartsStr(_T("/"), p)) p.Delete(1, 1);
	return host + _T("/") + p;
}

//---------------------------------------------------------------------------
UnicodeString DefaultToolFile(ExternalTool tool)
{
	switch (tool) {
	case ExternalTool::Git: return _T("git.exe");
	case ExternalTool::Unrar: return _T("unrar64j.dll");
	case ExternalTool::Migemo: return _T("migemo.dll");
	case ExternalTool::Xd2tx: return _T("xd2txlib.dll");
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
bool IsKnownToolFile(const UnicodeString &file)
{
	static const wchar_t *kKnown[] = {_T("git.exe"),      _T("7-zip64.dll"), _T("unlha64.dll"),
									  _T("cab64.dll"),    _T("tar64.dll"),   _T("unrar64j.dll"),
									  _T("uniso64.dll"),  _T("migemo.dll"),  _T("xd2txlib.dll")};
	const UnicodeString base = ExtractFileName(file);
	for (const wchar_t *known : kKnown) {
		if (SameText(base, UnicodeString(known))) return true;
	}
	return false;
}

}  // namespace git_ftp
