/**
 * @file gui/links.cpp
 * @brief リンク作成とタイムスタンプ調整の実装 (設計は gui/links.h)
 */
#include "gui/links.h"

#include <objbase.h>
#include <shlobj.h>

#include "gui/view_state.h"
#include "usr_file_ex.h"

namespace links {

//---------------------------------------------------------------------------
bool CanCreateHardLink(const UnicodeString &src_root, const UnicodeString &dst_root,
                       const UnicodeString &dst_fs)
{
	// ハードリンクはボリュームをまたげない (MainFrm.cpp:15707)
	if (!SameText(src_root, dst_root)) return false;
	return SameText(dst_fs, _T("NTFS"));
}

//---------------------------------------------------------------------------
UnicodeString ShortcutNameFor(const UnicodeString &name)
{
	return name + _T(".lnk");
}

namespace {

/// COM を必要な間だけ初期化する。
/// **既に初期化済みなら何もしない** (wx の GUI スレッドは OLE を初期化済み)。
/// これが無いと、COM を初期化していないスレッド (テストや将来のワーカー) から
/// 呼んだときに CoCreateInstance が必ず失敗する
class ScopedCom {
public:
	ScopedCom()
	{
		const HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		// S_FALSE = 既に初期化済み。RPC_E_CHANGED_MODE = 別のモードで初期化済み。
		// どちらも自分では解除しない
		owned_ = (hr == S_OK);
	}
	~ScopedCom() { if (owned_) ::CoUninitialize(); }
	ScopedCom(const ScopedCom &) = delete;
	ScopedCom &operator=(const ScopedCom &) = delete;

private:
	bool owned_ = false;
};

/// ショートカット (.lnk) を1つ作る
bool make_shortcut(const UnicodeString &target, const UnicodeString &link_path,
                   UnicodeString &error_out)
{
	const ScopedCom com;

	IShellLinkW *link = NULL;
	HRESULT hr = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
	                                IID_IShellLinkW, reinterpret_cast<void **>(&link));
	if (FAILED(hr) || link == NULL) {
		error_out = _T("ShellLink を作成できません");
		return false;
	}

	bool ok = false;
	link->SetPath(target.c_str());
	link->SetWorkingDirectory(ExcludeTrailingPathDelimiter(ExtractFilePath(target)).c_str());

	IPersistFile *pf = NULL;
	hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&pf));
	if (SUCCEEDED(hr) && pf != NULL) {
		hr = pf->Save(link_path.c_str(), TRUE);
		ok = SUCCEEDED(hr);
		if (!ok) error_out = _T("ショートカットを保存できません");
		pf->Release();
	}
	else {
		error_out = _T("IPersistFile を取得できません");
	}
	link->Release();
	return ok;
}

}  // namespace

//---------------------------------------------------------------------------
file_ops::FileOpResult CreateLinks(const std::vector<UnicodeString> &paths,
                                   const UnicodeString &dst_dir, LinkKind kind)
{
	file_ops::FileOpResult result;
	const UnicodeString base = IncludeTrailingPathDelimiter(dst_dir);

	for (const UnicodeString &src : paths) {
		const UnicodeString name = ExtractFileName(ExcludeTrailingPathDelimiter(src));
		const UnicodeString dst = base + ((kind == LinkKind::Shortcut)? ShortcutNameFor(name) : name);

		// 上書きしない (規約: 上書きを既定にしない)
		if (file_exists(dst) || dir_exists(dst)) {
			result.skipped_existing++;
			continue;
		}

		const bool is_dir = dir_exists(src);
		UnicodeString error;
		bool ok = false;

		switch (kind) {
		case LinkKind::Shortcut:
			ok = make_shortcut(src, dst, error);
			break;

		case LinkKind::Hard:
			// ハードリンクはファイルにしか張れない
			if (is_dir) { error = _T("ディレクトリにはハードリンクを張れません"); break; }
			ok = (::CreateHardLinkW(dst.c_str(), src.c_str(), NULL) != 0);
			if (!ok) error = _T("ハードリンクを作成できません");
			break;

		case LinkKind::Symbolic: {
			DWORD flags = is_dir? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
			// 開発者モードなら管理者権限なしで作れる
			flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
			ok = (::CreateSymbolicLinkW(dst.c_str(), src.c_str(), flags) != 0);
			if (!ok) {
				// フラグを解釈しない古い Windows 向けにもう一度
				ok = (::CreateSymbolicLinkW(dst.c_str(), src.c_str(),
				                            is_dir? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) != 0);
			}
			if (!ok) error = _T("シンボリックリンクを作成できません (管理者権限か開発者モードが要ります)");
			break;
		}
		}

		if (ok) result.success_count++;
		else result.failures.push_back(name + _T(": ") + error);
	}
	return result;
}

//---------------------------------------------------------------------------
TDateTime SetDirTimeRecursive(const UnicodeString &dir, bool show_hidden, bool show_system)
{
	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	TDateTime newest = 0.0;

	TSearchRec sr;
	if (FindFirst(base + "*", faAnyFile, sr) == 0) {
		do {
			if (SameStr(sr.Name, ".") || SameStr(sr.Name, "..")) continue;
			// 表示していないファイルは数えない (task_thread.cpp:1694-1695)
			if (!view_state::IsListedByAttr(sr.Attr, show_hidden, show_system)) continue;

			const TDateTime t = ((sr.Attr & faDirectory) != 0)
				? SetDirTimeRecursive(base + sr.Name, show_hidden, show_system)
				: sr.TimeStamp;
			if (t > newest) newest = t;
		} while (FindNext(sr) == 0);
		FindClose(sr);
	}

	// 配下に何も無ければ触らない (0 で上書きすると 1899年になってしまう)
	if (static_cast<double>(newest) <= 0.0) return newest;

	HANDLE h = ::CreateFileW(ExcludeTrailingPathDelimiter(base).c_str(), FILE_WRITE_ATTRIBUTES,
	                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
	                         FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		SYSTEMTIME st = {};
		DateTimeToSystemTime(newest, st);
		FILETIME lt = {}, ft = {};
		if (::SystemTimeToFileTime(&st, &lt) && ::LocalFileTimeToFileTime(&lt, &ft)) {
			::SetFileTime(h, NULL, NULL, &ft);
		}
		::CloseHandle(h);
	}
	return newest;
}

}  // namespace links
