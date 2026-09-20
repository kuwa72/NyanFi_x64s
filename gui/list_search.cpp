/**
 * @file gui/list_search.cpp
 * @brief gui/list_search.h の実装
 */
#include "gui/list_search.h"

#include "gui/navigation.h"
#include "usr_str.h"

namespace list_search {

//---------------------------------------------------------------------------
bool IsErrorLine(const UnicodeString &line)
{
	// VCL 版 (MainFrm.cpp:13383) のパターンそのまま
	try {
		return TRegEx::IsMatch(line, _T("^.>([ECW]|(     [45]\\d{2})) .*"));
	}
	catch (...) {
		return false;
	}
}

//---------------------------------------------------------------------------
int FindNextError(const std::vector<UnicodeString> &lines, int from, bool forward)
{
	const int count = static_cast<int>(lines.size());
	if (count == 0) return -1;

	// VCL 版と同じく折り返さない (MainFrm.cpp:13384-13390)。
	// 現在位置自体は対象に含めない (down なら idx0+1、up なら idx0-1 から探す)
	if (forward) {
		for (int i = from + 1; i < count; ++i) {
			if (IsErrorLine(lines[static_cast<std::size_t>(i)])) return i;
		}
	}
	else {
		for (int i = from - 1; i >= 0; --i) {
			if (IsErrorLine(lines[static_cast<std::size_t>(i)])) return i;
		}
	}
	return -1;
}

//---------------------------------------------------------------------------
bool ToggleMigemoMode(bool is_migemo, bool dict_ready)
{
	// VCL 版 (MainFrm.cpp:12066): (!is_Migemo && DictReady)
	return (!is_migemo && dict_ready);
}

//---------------------------------------------------------------------------
bool SetNormalMode(bool /*is_migemo*/)
{
	// VCL 版 (MainFrm.cpp:12066): NormalMode なら無条件で false
	return false;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> FilterKeywordHistory(const std::vector<UnicodeString> &history,
                                                 const std::vector<UnicodeString> &names)
{
	// VCL 版 (MainFrm.cpp:12097-12114): マッチ可能なものを抽出する
	std::vector<UnicodeString> result;
	for (const UnicodeString &kwd : history) {
		for (const UnicodeString &name : names) {
			if (IncrementalSearchMatch(name, kwd)) {
				result.push_back(kwd);
				break;
			}
		}
	}
	return result;
}

//---------------------------------------------------------------------------
std::vector<int> CollectMatchedIndices(const std::vector<bool> &matched)
{
	std::vector<int> result;
	for (std::size_t i = 0; i < matched.size(); ++i) {
		if (matched[i]) result.push_back(static_cast<int>(i));
	}
	return result;
}

//---------------------------------------------------------------------------
int FindFromTop(const std::vector<UnicodeString> &names, const UnicodeString &keyword)
{
	// 先頭を含めて最初の一致 (ヘッダのコメントを参照)
	return FindIncrementalSearchMatch(names, keyword, -1, true);
}

}  // namespace list_search
