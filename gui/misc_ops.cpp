/**
 * @file gui/misc_ops.cpp
 * @brief misc_ops の実装 (設計は gui/misc_ops.h)
 */
#include "gui/misc_ops.h"

#include <lm.h>

#include "usr_str.h"

namespace misc_ops {

//---------------------------------------------------------------------------
bool HasInvalidNameChar(const UnicodeString &name)
{
	// Windows のファイル名に使えない文字 (パス区切りを含む)
	static const wchar_t *const kInvalid = L"\\/:*?\"<>|";
	for (int i = 1; i <= name.Length(); ++i) {
		const wchar_t c = name[i];
		if (c < 0x20) return true;
		for (const wchar_t *p = kInvalid; *p != L'\0'; ++p) {
			if (c == *p) return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
UnicodeString NameFromClipboard(const UnicodeString &clipboard_text, UnicodeString &error_out)
{
	if (clipboard_text.IsEmpty()) {
		error_out = _T("クリップボードが空です");
		return EmptyStr;
	}

	// 引用符を外し、パスが入っていれば末尾の要素だけを使う (VCL と同じ)。
	// 改行が入っていることがあるので先頭行だけを見る
	UnicodeString s = clipboard_text;
	const int nl = s.Pos(_T("\r"));
	if (nl > 0) s = s.SubString(1, nl - 1);
	const int nl2 = s.Pos(_T("\n"));
	if (nl2 > 0) s = s.SubString(1, nl2 - 1);

	s = ExtractFileName(exclude_quot(Trim(s)));
	if (s.IsEmpty()) {
		error_out = _T("クリップボードからファイル名を取り出せません");
		return EmptyStr;
	}
	if (HasInvalidNameChar(s)) {
		// 弾かないと rename が失敗するだけで理由が分からない
		error_out = _T("ファイル名に使えない文字が含まれています: ") + s;
		return EmptyStr;
	}
	return s;
}

//---------------------------------------------------------------------------
bool EnumLocalShares(std::vector<ShareEntry> &out, UnicodeString &error_out)
{
	out.clear();

	PSHARE_INFO_2 buf = NULL;
	DWORD read = 0, total = 0;
	DWORD resume = 0;
	const NET_API_STATUS st = ::NetShareEnum(NULL, 2, reinterpret_cast<LPBYTE *>(&buf),
	                                          MAX_PREFERRED_LENGTH, &read, &total, &resume);
	if (st != NERR_Success && st != ERROR_MORE_DATA) {
		error_out = _T("共有フォルダの一覧を取得できません (管理者権限が要る場合があります)");
		return false;
	}

	for (DWORD i = 0; i < read; ++i) {
		const UnicodeString name(reinterpret_cast<const wchar_t *>(buf[i].shi2_netname));
		// 管理共有 (C$ / ADMIN$ / IPC$) は除く。VCL の一覧に合わせたこちらの判断
		if (EndsStr(_T("$"), name)) continue;

		ShareEntry e;
		e.name = name;
		e.path = UnicodeString(reinterpret_cast<const wchar_t *>(buf[i].shi2_path));
		e.remark = UnicodeString(reinterpret_cast<const wchar_t *>(buf[i].shi2_remark));
		out.push_back(e);
	}
	if (buf != NULL) ::NetApiBufferFree(buf);
	return true;
}

}  // namespace misc_ops
