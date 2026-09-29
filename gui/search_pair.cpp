/**
 * @file gui/search_pair.cpp
 * @brief 対応括弧・HTMLブロック・選択文字列検索の実装 (wx 非依存)
 */
#include "gui/search_pair.h"

#include <algorithm>

#include "gui/find_txt.h"
#include "usr_str.h"

namespace search_pair {

//---------------------------------------------------------------------------
// BracketPair
//---------------------------------------------------------------------------

const std::vector<BracketPair> &DefaultPairs()
{
	static const std::vector<BracketPair> pairs = {
		{_T('（'), _T('）')}, {_T('〔'), _T('〕')}, {_T('［'), _T('］')},
		{_T('｛'), _T('｝')}, {_T('〈'), _T('〉')}, {_T('《'), _T('》')},
		{_T('「'), _T('」')}, {_T('『'), _T('』')}, {_T('【'), _T('】')},
		{_T('('),  _T(')')},  {_T('['),  _T(']')},  {_T('{'),  _T('}')},
		{_T('｢'), _T('｣')},
	};
	return pairs;
}

bool IsOpen(wchar_t ch)
{
	const auto &pairs = DefaultPairs();
	return std::any_of(pairs.begin(), pairs.end(),
		[ch](const auto &p) { return p.open == ch; });
}

bool IsClose(wchar_t ch)
{
	const auto &pairs = DefaultPairs();
	return std::any_of(pairs.begin(), pairs.end(),
		[ch](const auto &p) { return p.close == ch; });
}

wchar_t Match(wchar_t open_ch)
{
	const auto &pairs = DefaultPairs();
	auto it = std::find_if(pairs.begin(), pairs.end(),
		[open_ch](const auto &p) { return p.open == open_ch; });
	return it != pairs.end() ? it->close : 0;
}

//---------------------------------------------------------------------------
// PairPattern
//---------------------------------------------------------------------------

PairPattern GetPairPattern(const UnicodeString &ext)
{
	UnicodeString e = ext;
	if (!e.IsEmpty() && e[0] != _T('.')) e = _T(".") + e;
	e = e.LowerCase();

	if (e == _T(".dfm")) return PairPattern::Pascal;
	if (e == _T(".pas")) return PairPattern::Pascal;
	if (e == _T(".cpp") || e == _T(".h") || e == _T(".c") || e == _T(".hpp") ||
	    e == _T(".cc") || e == _T(".cxx") || e == _T(".hxx"))
		return PairPattern::Cpp;
	if (e == _T(".vbs") || e == _T(".vb") || e == _T(".mac")) return PairPattern::Vbs;
	if (e == _T(".pod") || e == _T(".pl") || e == _T(".pm")) return PairPattern::Perl;
	return PairPattern::None;
}

bool GetRegexPair(PairPattern pattern, UnicodeString &begin_ptn, UnicodeString &end_ptn)
{
	switch (pattern) {
	case PairPattern::Pascal:
		begin_ptn = _T("(^\\s*(((else\\s)?begin)|case|try|record)\\b)|(\\w+\\s=\\s(class|interface|record)\\b)");
		end_ptn   = _T("^\\s*end[;).]?\\b");
		return true;
	case PairPattern::Cpp:
		begin_ptn = _T("^\\s*#\\s*if\\w*");
		end_ptn   = _T("^\\s*#\\s*endif");
		return true;
	case PairPattern::Vbs:
		begin_ptn = _T("^\\s*sub\\s+\\w+\\b");
		end_ptn   = _T("^\\s*end\\s+sub\\b");
		return true;
	case PairPattern::Perl:
		begin_ptn = _T("^=pod\\b");
		end_ptn   = _T("^=cut\\b");
		return true;
	default:
		return false;
	}
}

//---------------------------------------------------------------------------
// SearchPairCore
//---------------------------------------------------------------------------

int SearchPairCore(const std::vector<UnicodeString> &lines, int cur_y,
                   const UnicodeString &begin_ptn, const UnicodeString &end_ptn)
{
	if (cur_y < 0 || cur_y >= (int)lines.size()) return -1;

	TRegExOptions opt;
	opt << roIgnoreCase;

	const UnicodeString &s = lines[cur_y];

	// begin --> end (下方向)
	if (TRegEx::IsMatch(s, begin_ptn, opt)) {
		int lvl = 0;
		for (int i = cur_y + 1; i < (int)lines.size(); i++) {
			const UnicodeString &ls = lines[i];
			if (TRegEx::IsMatch(ls, end_ptn, opt)) {
				if (lvl == 0) return i;
				lvl--;
			} else if (TRegEx::IsMatch(ls, begin_ptn, opt)) {
				lvl++;
			}
		}
	}
	// end --> begin (上方向)
	else if (TRegEx::IsMatch(s, end_ptn, opt)) {
		int lvl = 0;
		for (int i = cur_y - 1; i >= 0; i--) {
			const UnicodeString &ls = lines[i];
			if (TRegEx::IsMatch(ls, begin_ptn, opt)) {
				if (lvl == 0) return i;
				lvl--;
			} else if (TRegEx::IsMatch(ls, end_ptn, opt)) {
				lvl++;
			}
		}
	}
	return -1;
}

//---------------------------------------------------------------------------
// SearchSelection
//---------------------------------------------------------------------------

int SearchSelection(const std::vector<UnicodeString> &lines,
                    const UnicodeString &sel_text, int cur_y, bool up)
{
	if (cur_y < 0 || cur_y >= (int)lines.size()) return -1;

	UnicodeString s = sel_text;
	if (s.IsEmpty()) return -1;

	find_txt::Options opt;
	opt.keyword = s;
	find_txt::Direction dir = up ? find_txt::Direction::Up : find_txt::Direction::Down;
	return find_txt::FindNextLine(lines, opt, cur_y, dir);
}

//---------------------------------------------------------------------------
// SearchLink
//---------------------------------------------------------------------------

int SearchLink(const std::vector<UnicodeString> &lines, int cur_y, bool up)
{
	if (cur_y < 0 || cur_y >= (int)lines.size()) return -1;

	find_txt::Options opt;
	opt.keyword = LINK_MATCH_PTN;
	opt.regex = true;
	find_txt::Direction dir = up ? find_txt::Direction::Up : find_txt::Direction::Down;
	return find_txt::FindNextLine(lines, opt, cur_y, dir);
}

//---------------------------------------------------------------------------
// LINK_MATCH_PTN (VCL TxtViewer.h:15)
//---------------------------------------------------------------------------

const wchar_t *LINK_MATCH_PTN =
	L"(https?://[\\w/:%#$&?()~.=+-]+)|(file:///[^*?\"<>|)）]+\\.[a-zA-Z0-9]+)|(mailto:[a-zA-Z0-9]+[\\w.-]*@[\\w.-]+)";

//---------------------------------------------------------------------------
// ParsePairParam: "/開始/;/終了/" 形式の分解
//---------------------------------------------------------------------------

bool ParsePairParam(const UnicodeString &param, UnicodeString &begin_ptn, UnicodeString &end_ptn)
{
	// VCL (TxtViewer.cpp:4620-4640): "/～/;～/" 形式
	// 例: "/begin/;/end/" → begin_ptn="begin", end_ptn="end"
	begin_ptn = EmptyStr;
	end_ptn = EmptyStr;

	UnicodeString p = param;
	if (!StartsStr(_T("/"), p)) return false;

	// 最初の '/' を飛ばす
	p = p.SubString(2, p.Length() - 1);

	// 次の '/' までが開始パターン
	const int slash1 = p.Pos(_T('/'));
	if (slash1 <= 0) return false;
	begin_ptn = p.SubString(1, slash1 - 1);
	p = p.SubString(slash1 + 1, p.Length() - slash1);

	// ';' で区切られた後の '/' までが終了パターン
	if (!StartsStr(_T(";"), p)) return false;
	p = p.SubString(2, p.Length() - 1);
	const int slash2 = p.Pos(_T('/'));
	if (slash2 <= 0) return false;
	end_ptn = p.SubString(1, slash2 - 1);

	return !begin_ptn.IsEmpty() && !end_ptn.IsEmpty();
}

//---------------------------------------------------------------------------
// FindBracket: カーソル位置の括弧から対応する括弧を探す
//---------------------------------------------------------------------------

std::pair<int, int> FindBracket(const std::vector<UnicodeString> &lines, int cur_x, int cur_y, bool down)
{
	if (cur_y < 0 || cur_y >= (int)lines.size()) return {-1, -1};
	if (cur_x < 1) return {-1, -1};

	const UnicodeString &cur_line = lines[static_cast<std::size_t>(cur_y)];
	if (cur_x > (int)cur_line.Length()) return {-1, -1};

	const wchar_t ch = cur_line.c_str()[cur_x - 1];
	if (!IsOpen(ch) && !IsClose(ch)) return {-1, -1};

	if (IsOpen(ch)) {
		// 開き括弧 → 閉き括弧を下方向に探す
		const wchar_t close_ch = Match(ch);
		int lvl = 0;
		for (int i = cur_y; i < (int)lines.size(); ++i) {
			const UnicodeString &lbuf = lines[static_cast<std::size_t>(i)];
			const int start_x = (i == cur_y) ? cur_x : 1;
			for (int j = start_x; j <= (int)lbuf.Length(); ++j) {
				const wchar_t c = lbuf.c_str()[j - 1];
				if (c == close_ch) {
					if (lvl == 0) return {j, i};
					--lvl;
				}
				else if (c == ch) {
					++lvl;
				}
			}
		}
	}
	else {
		// 閉じ括弧 → 開き括弧を上方向に探す
		const wchar_t open_ch = Match(ch);
		int lvl = 0;
		for (int i = cur_y; i >= 0; --i) {
			const UnicodeString &lbuf = lines[static_cast<std::size_t>(i)];
			const int start_x = (i == cur_y) ? cur_x : (int)lbuf.Length();
			for (int j = start_x; j >= 1; --j) {
				const wchar_t c = lbuf.c_str()[j - 1];
				if (c == open_ch) {
					if (lvl == 0) return {j, i};
					--lvl;
				}
				else if (c == ch) {
					++lvl;
				}
			}
		}
	}
	return {-1, -1};
}

}  // namespace search_pair
