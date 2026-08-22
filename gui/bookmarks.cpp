/**
 * @file gui/bookmarks.cpp
 * @brief 栞マークの実装 (設計は gui/bookmarks.h)
 */
#include "gui/bookmarks.h"

#include <algorithm>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace bookmarks {

//---------------------------------------------------------------------------
int FindNext(const std::vector<bool> &marked, int cursor)
{
	const int n = static_cast<int>(marked.size());
	int wrap = -1;  // カーソル位置以前で最初に見つかったマーク (折り返し先)

	for (int i = 0; i < n; ++i) {
		if (i <= cursor && wrap != -1) continue;  // 折り返し先は最初の1件だけ覚える
		if (!marked[static_cast<std::size_t>(i)]) continue;
		if (i <= cursor) wrap = i; else return i;
	}
	return wrap;
}

//---------------------------------------------------------------------------
int FindPrev(const std::vector<bool> &marked, int cursor)
{
	const int n = static_cast<int>(marked.size());
	int wrap = -1;

	for (int i = n - 1; i >= 0; --i) {
		if (i >= cursor && wrap != -1) continue;
		if (!marked[static_cast<std::size_t>(i)]) continue;
		if (i >= cursor) wrap = i; else return i;
	}
	return wrap;
}

//---------------------------------------------------------------------------
bool Toggle(UsrIniFile &ini, const UnicodeString &path, const UnicodeString &memo)
{
	if (path.IsEmpty()) return false;
	return ini.FileMark(path, -1, memo);
}

//---------------------------------------------------------------------------
bool IsMarked(UsrIniFile &ini, const UnicodeString &path)
{
	if (path.IsEmpty()) return false;
	return ini.IsMarked(path);
}

//---------------------------------------------------------------------------
UnicodeString MemoOf(UsrIniFile &ini, const UnicodeString &path)
{
	if (path.IsEmpty()) return EmptyStr;
	return ini.GetMarkMemo(path);
}

//---------------------------------------------------------------------------
int ClearOf(UsrIniFile &ini, const std::vector<UnicodeString> &paths)
{
	int n = 0;
	for (std::size_t i = 0; i < paths.size(); ++i) {
		if (paths[i].IsEmpty() || !ini.IsMarked(paths[i])) continue;
		ini.FileMark(paths[i], 0);
		++n;
	}
	return n;
}

//---------------------------------------------------------------------------
std::vector<Mark> All(UsrIniFile &ini)
{
	std::vector<Mark> out;

	for (int i = 0; i < ini.MarkIdxList->Count; ++i) {
		const UnicodeString dir = ini.MarkIdxList->Strings[i];
		TStringList *klist = static_cast<TStringList *>(ini.MarkIdxList->Objects[i]);
		if (klist == NULL) continue;

		for (int j = 0; j < klist->Count; ++j) {
			// 1行は "名前 <TAB> メモ <TAB> 日時" (UIniFile.cpp::FileMark)
			UnicodeString rest = klist->Strings[j];
			Mark m;
			const UnicodeString name = split_tkn(rest, _T("\t"));
			m.memo  = split_tkn(rest, _T("\t"));
			m.stamp = rest;
			// 書庫の中は "書庫名/中のパス"、通常はディレクトリ + 名前
			m.path = ends_PathDlmtr(dir)? (dir + name) : (dir + _T("/") + name);
			out.push_back(m);
		}
	}

	std::sort(out.begin(), out.end(), [](const Mark &a, const Mark &b) {
		return ::StrCmpLogicalW(a.path.c_str(), b.path.c_str()) < 0;
	});
	return out;
}

//---------------------------------------------------------------------------
int TrimMissing(UsrIniFile &ini)
{
	// 実際の削除は移植済みの CheckMarkItems() に任せる (UNC や書庫の扱いを
	// 書き直すと VCL と食い違うため)。件数だけ前後の差で数える
	const int before = static_cast<int>(All(ini).size());
	ini.CheckMarkItems();
	const int after = static_cast<int>(All(ini).size());
	return before - after;
}

//---------------------------------------------------------------------------
std::vector<bool> MarkedFlags(UsrIniFile &ini, const std::vector<UnicodeString> &paths)
{
	std::vector<bool> flags;
	flags.reserve(paths.size());
	for (std::size_t i = 0; i < paths.size(); ++i) {
		flags.push_back(!paths[i].IsEmpty() && ini.IsMarked(paths[i]));
	}
	return flags;
}

}  // namespace bookmarks
