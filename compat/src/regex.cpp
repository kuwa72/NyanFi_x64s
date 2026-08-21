/**
 * @file regex.cpp
 * @brief compat/regex.h の実装。バックエンドは std::wregex (ECMAScript構文) に
 *        一本化してある。Phase 1 で PCRE2 へ切り替える際はこのファイルだけを
 *        書き換えればよい。
 */
#include "compat/regex.h"

#include <cwchar>
#include <cwctype>

namespace {

//---------------------------------------------------------------------------
/// TRegExOptions → std::regex_constants フラグへの変換
/// (roExplicitCapture / roSingleLine / roIgnorePatternSpace / roNotEmpty は
///  std::wregex に対応機能が無いため意図的に無視している。src/ では未使用)
std::regex_constants::syntax_option_type to_syntax_options(const TRegExOptions &opt)
{
	auto flags = std::regex_constants::ECMAScript;
	if (opt.Contains(roIgnoreCase)) flags |= std::regex_constants::icase;
	if (opt.Contains(roMultiLine)) flags |= std::regex_constants::multiline;
	if (opt.Contains(roCompiled)) flags |= std::regex_constants::optimize;
	return flags;
}

//---------------------------------------------------------------------------
/// std::wregex を構築する。無効なパターンは ERegularExpressionError を送出する
std::wregex compile_pattern(const UnicodeString &pattern, const TRegExOptions &opt)
{
	try {
		return std::wregex(pattern.c_str(), static_cast<std::size_t>(pattern.Length()), to_syntax_options(opt));
	}
	catch (const std::regex_error &e) {
		UnicodeString msg;
		msg.sprintf(_T("正規表現パターンが不正です: %hs"), e.what());
		throw ERegularExpressionError(msg);
	}
}

//---------------------------------------------------------------------------
/// std::wsmatch の 1 グループから TGroup を作る
TGroup make_group(const std::wsmatch &m, std::size_t i)
{
	TGroup g;
	g.Success = m[i].matched;
	if (g.Success) {
		//1 始まりにする (Delphi TGroup.Index 互換)
		g.Index = static_cast<int>(m.position(i)) + 1;
		g.Length = static_cast<int>(m.length(i));
		//m[i].first / .second は std::wstring::const_iterator であり、生ポインタへ
		//暗黙変換できない実装があるため、一旦 std::wstring 経由でコピーする
		g.Value = UnicodeString(std::wstring(m[i].first, m[i].second));
	}
	return g;
}

//---------------------------------------------------------------------------
/// std::wsmatch から TMatch を作る (offset は探索開始位置。0 始まりの文字数)
TMatch make_match(const std::wsmatch &m, int offset)
{
	TMatch mt;
	mt.Success = true;
	mt.Index = static_cast<int>(m.position(0)) + offset + 1;
	mt.Length = static_cast<int>(m.length(0));
	mt.Value = UnicodeString(std::wstring(m[0].first, m[0].second));

	mt.Groups.Item.reserve(m.size());
	for (std::size_t i = 0; i < m.size(); ++i) mt.Groups.Item.push_back(make_group(m, i));
	mt.Groups.Count = static_cast<int>(mt.Groups.Item.size());

	return mt;
}

//---------------------------------------------------------------------------
/// 置換文字列内のグループ参照を ECMAScript の $N 形式に正規化する
///
/// src/ での実測では TRegEx::Replace の置換文字列は \1 \2 ... 形式 (12 箇所) と
/// $1 形式 (2 箇所、いずれも "(...)*" パターンの $1) の 2 通りが混在している。
/// 実機の Delphi TRegEx.Replace が \N 形式を本当にグループ参照として解釈するのか
/// (それとも単に "\1" という文字列を出力するバグなのか) はソースだけでは判別
/// できなかったため、両方をグループ参照として救済し、意図が壊れないようにした。
/// $ 単独 (数字が続かない) は $$ にエスケープし、std::regex_replace 固有の
/// $&, $`, $' 等の特殊シーケンスとして誤解釈されないようにする。
std::wstring convert_replacement(const UnicodeString &replacement)
{
	std::wstring out;
	const wchar_t *p = replacement.c_str();
	const int len = replacement.Length();
	for (int i = 0; i < len; ++i) {
		const wchar_t c = p[i];
		if (c == L'\\' && i + 1 < len && iswdigit(p[i + 1])) {
			out += L'$';
			++i;
			while (i < len && iswdigit(p[i])) out += p[i++];
			--i;
		}
		else if (c == L'$') {
			if (i + 1 < len && iswdigit(p[i + 1])) {
				out += c;  //既に $N 形式。そのまま通す
			}
			else {
				out += L"$$";
			}
		}
		else {
			out += c;
		}
	}
	return out;
}

}  // namespace

//---------------------------------------------------------------------------
TRegEx::TRegEx(const UnicodeString &pattern, TRegExOptions options)
	: re_(compile_pattern(pattern, options)), compiled_(true)
{
}

//---------------------------------------------------------------------------
bool TRegEx::IsMatch(const UnicodeString &input) const
{
	if (!compiled_) return false;
	return std::regex_search(input.c_str(), input.c_str() + input.Length(), re_);
}

//---------------------------------------------------------------------------
TMatch TRegEx::Match(const UnicodeString &input) const
{
	if (!compiled_) return TMatch();
	std::wsmatch m;
	std::wstring s(input.c_str(), static_cast<std::size_t>(input.Length()));
	if (!std::regex_search(s, m, re_)) return TMatch();
	return make_match(m, 0);
}

//---------------------------------------------------------------------------
TMatchCollection TRegEx::Matches(const UnicodeString &input) const
{
	TMatchCollection result;
	if (!compiled_) return result;

	std::wstring s(input.c_str(), static_cast<std::size_t>(input.Length()));
	auto begin = std::wsregex_iterator(s.begin(), s.end(), re_);
	auto end = std::wsregex_iterator();
	for (auto it = begin; it != end; ++it) result.Item.push_back(make_match(*it, 0));
	result.Count = static_cast<int>(result.Item.size());
	return result;
}

//---------------------------------------------------------------------------
UnicodeString TRegEx::Replace(const UnicodeString &input, const UnicodeString &replacement) const
{
	if (!compiled_) return input;
	std::wstring s(input.c_str(), static_cast<std::size_t>(input.Length()));
	std::wstring rep = convert_replacement(replacement);
	std::wstring out = std::regex_replace(s, re_, rep);
	return UnicodeString(out);
}

//---------------------------------------------------------------------------
bool TRegEx::IsMatch(const UnicodeString &input, const UnicodeString &pattern, TRegExOptions options)
{
	return TRegEx(pattern, options).IsMatch(input);
}

//---------------------------------------------------------------------------
TMatch TRegEx::Match(const UnicodeString &input, const UnicodeString &pattern, TRegExOptions options)
{
	return TRegEx(pattern, options).Match(input);
}

//---------------------------------------------------------------------------
TMatchCollection TRegEx::Matches(const UnicodeString &input, const UnicodeString &pattern, TRegExOptions options)
{
	return TRegEx(pattern, options).Matches(input);
}

//---------------------------------------------------------------------------
UnicodeString TRegEx::Replace(const UnicodeString &input, const UnicodeString &pattern,
                               const UnicodeString &replacement, TRegExOptions options)
{
	return TRegEx(pattern, options).Replace(input, replacement);
}

//---------------------------------------------------------------------------
UnicodeString TRegEx::Escape(const UnicodeString &input)
{
	//ECMAScript の特殊文字をエスケープする (Delphi の Escape とは実装が異なるが、
	//同じバックエンド (std::wregex) にそのまま渡すので Match/Replace 側の挙動は
	//一貫する)
	static const wchar_t *special = L"\\^$.|?*+()[]{}";
	UnicodeString out;
	const wchar_t *p = input.c_str();
	const int len = input.Length();
	for (int i = 0; i < len; ++i) {
		if (wcschr(special, p[i])) out += L'\\';
		out += p[i];
	}
	return out;
}

//---------------------------------------------------------------------------
TStringDynArray TRegEx::Split(const UnicodeString &input, const UnicodeString &pattern, TRegExOptions options)
{
	TStringDynArray result;
	TRegEx re(pattern, options);
	if (!re.compiled_) return result;

	std::wstring s(input.c_str(), static_cast<std::size_t>(input.Length()));
	std::wsregex_token_iterator it(s.begin(), s.end(), re.re_, -1);
	std::wsregex_token_iterator end;

	std::vector<UnicodeString> tmp;
	for (; it != end; ++it) tmp.push_back(UnicodeString(std::wstring(*it)));

	result.Length = static_cast<int>(tmp.size());
	for (std::size_t i = 0; i < tmp.size(); ++i) result[static_cast<int>(i)] = tmp[i];
	return result;
}
