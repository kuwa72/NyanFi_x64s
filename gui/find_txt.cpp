/**
 * @file gui/find_txt.cpp
 * @brief gui/find_txt.h の実装
 */
#include "gui/find_txt.h"

#include "usr_str.h"

namespace find_txt {

namespace {

bool known_code_page(int cp)
{
	switch (cp) {
	case 932:
	case 1252:
	case 1200:
	case 1201:
	case 20127:
	case 20932:
	case 65000:
	case 65001:
		return true;
	default:
		return false;
	}
}

}  // namespace

//---------------------------------------------------------------------------
Availability ResolveAvailability(bool binary)
{
	Availability a;
	a.binary_panel = binary;
	a.bytes = binary;
	a.word = !binary;
	a.regex = !binary;
	a.migemo = !binary;
	a.highlight = true;
	a.code_page = binary;
	return a;
}

//---------------------------------------------------------------------------
Options Normalize(const Options &in, bool binary)
{
	Options out = in;
	if (!binary) out.bytes = false;
	if (out.bytes) {
		out.whole_word = false;
		out.regex = false;
		out.migemo = false;
	}
	else if (out.migemo) {
		// VCL MigemoCheckBoxClick: Migemo をONにするとRegExはOFF
		out.regex = false;
	}
	if (!known_code_page(out.code_page)) out.code_page = 932;
	return out;
}

//---------------------------------------------------------------------------
bool Validate(const Options &opt, UnicodeString &error_out)
{
	error_out = EmptyStr;
	if (opt.keyword.IsEmpty()) {
		error_out = _T("検索文字列を入力してください");
		return false;
	}
	if (opt.regex && !chk_RegExPtn(opt.keyword)) {
		error_out = _T("正規表現が正しくありません");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool LineMatches(const UnicodeString &line, const Options &input)
{
	// LineMatches はテキスト行用の照合なので、bytes / Migemo は未実装を反映して
	// 必ず false にする（リテラル検索に黙って落としたくない）。
	const Options opt = Normalize(input, /*binary=*/true);
	if (opt.bytes || opt.migemo || opt.keyword.IsEmpty()) return false;

	try {
		if (opt.regex) {
			UnicodeString pattern = opt.keyword;
			if (opt.whole_word) pattern = _T("\\b(?:") + pattern + _T(")\\b");
			TRegExOptions re_opt;
			if (!opt.case_sensitive) re_opt << roIgnoreCase;
			TRegEx re(pattern, re_opt);
			return re.IsMatch(line);
		}
		if (!opt.whole_word) {
			return opt.case_sensitive ? ContainsStr(line, opt.keyword)
				: ContainsText(line, opt.keyword);
		}
		// VCL は空白を含む検索語を 1 つの語句として Pos/pos_i で探す。
		// find_mlt は空白区切りで OR 検索するため、ここでは既存の is_word を
		// 使って全候補を単語境界付きで確認する。
		UnicodeString rest = line;
		int offset = 0;
		while (!rest.IsEmpty()) {
			const int p = opt.case_sensitive ? rest.Pos(opt.keyword) : pos_i(opt.keyword, rest);
			if (p == 0) return false;
			const int absolute = offset + p;
			if (is_word(line, absolute, opt.keyword.Length())) return true;
			offset = absolute + opt.keyword.Length() - 1;
			if (offset >= line.Length()) return false;
			rest = line.SubString(offset + 1);
		}
		return false;
	}
	catch (...) {
		return false;
	}
}

//---------------------------------------------------------------------------
int FindNextLine(const std::vector<UnicodeString> &lines, const Options &opt,
                 int from_line, Direction direction)
{
	const int n = static_cast<int>(lines.size());
	if (n <= 0 || opt.keyword.IsEmpty() || opt.bytes) return -1;

	int current = from_line;
	if (current < 0) current = n - 1;
	if (current >= n) current = 0;
	for (int step = 1; step <= n; ++step) {
		const int i = direction == Direction::Down
			? (current + step) % n
			: (current - step + n * 2) % n;
		if (LineMatches(lines[static_cast<std::size_t>(i)], opt)) return i;
	}
	return -1;
}

//---------------------------------------------------------------------------
int DirectionIndex(Direction direction)
{
	return direction == Direction::Up ? 0 : 1;
}

//---------------------------------------------------------------------------
Direction DirectionFromIndex(int index)
{
	return index == 0 ? Direction::Up : Direction::Down;
}

}  // namespace find_txt
