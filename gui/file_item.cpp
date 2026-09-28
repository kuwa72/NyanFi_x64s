/**
 * @file gui/file_item.cpp
 * @brief 一覧の並べ替え・マスク絞り込みの実装
 */
#include "gui/file_item.h"

#include <vector>

#include "usr_file_ex.h"
#include "usr_str.h"

//---------------------------------------------------------------------------
UnicodeString FullPathOfItem(const UnicodeString &dir, const FileItem &item)
{
	if (!item.full_path.IsEmpty()) return item.full_path;
	if (item.name.IsEmpty()) return EmptyStr;
	return IncludeTrailingPathDelimiter(dir) + item.name;
}

namespace {

/// 自然順の名前比較 (StrCmpLogicalW)。これは usr_str.cpp の comp_NaturalOrder が
/// TStringList に対して行っているのと全く同じ呼び出しで、意味は同一
int compare_name(const FileItem &a, const FileItem &b)
{
	return ::StrCmpLogicalW(a.name.c_str(), b.name.c_str());
}

/// 拡張子の比較。同じ拡張子なら名前で決める (Global.cpp::SortComp_Ext の
/// 「拡張子が同じなら第2ソート」に相当する簡略版として名前を使う)
int compare_ext(const FileItem &a, const FileItem &b)
{
	const UnicodeString ea = ExtractFileExt(a.name);
	const UnicodeString eb = ExtractFileExt(b.name);
	const int r = ::StrCmpLogicalW(ea.c_str(), eb.c_str());
	return (r != 0) ? r : compare_name(a, b);
}

/// サイズの比較。ディレクトリの size は -1 の番兵値なので、dirs_first を外して
/// サイズ順にすると常に先頭に来る (実サイズの計算は Global.cpp 側の CalcDirSize
/// 相当が必要で、今回のスコープ外)
int compare_size(const FileItem &a, const FileItem &b)
{
	if (a.size == b.size) return compare_name(a, b);
	return (a.size < b.size) ? -1 : 1;
}

/// 更新日時の比較。TDateTime は operator double() を持つので算術比較できる
int compare_date(const FileItem &a, const FileItem &b)
{
	const double da = a.stamp;
	const double db = b.stamp;
	if (da == db) return compare_name(a, b);
	return (da < db) ? -1 : 1;
}

/// 属性の比較
int compare_attr(const FileItem &a, const FileItem &b)
{
	if (a.attr == b.attr) return compare_name(a, b);
	return a.attr - b.attr;
}

}  // namespace

//---------------------------------------------------------------------------
/**
 * @details
 * comp_NaturalOrder / comp_AscendOrder / comp_DescendOrder (usr_str.h) は
 * あえて使っていない。理由:
 *
 * - comp_NaturalOrder は `StrCmpLogicalW(List->Strings[i], ...)` を呼ぶだけの
 *   薄いラッパーで、対象は TStringList (文字列 + Objects) 前提。ここでの名前比較
 *   (compare_name) は同じ StrCmpLogicalW を直接呼んでおり、挙動は完全に同一
 * - comp_AscendOrder / comp_DescendOrder は src 内の実際の使用箇所を確認すると
 *   (GenInfDlg.cpp / HistDlg.cpp / TxtViewer.cpp) いずれも「行指向の汎用文字列
 *   リスト」向けで、ファイル一覧本体のソートには使われていない。内部で
 *   extract_top_num_str により先頭の数値列を優先比較する仕様があり、これは
 *   ファイル名向けの自然順 (StrCmpLogicalW が数値ラン単位で数値比較する) とは
 *   別の挙動。実際のファイル一覧ソート (Global.cpp の SortComp_Name/Ext/Time/
 *   Size/Attr、未移植) は型付きフィールドを直接比較しており、本関数もそれに
 *   合わせて型付きフィールドを直接比較する
 */
int CompareFileItems(const FileItem &a, const FileItem &b, SortKey key, bool descending, bool dirs_first)
{
	// ".." は常に先頭 (Global.cpp の SortComp_* が is_up を最優先するのと同じ)
	if (a.is_parent != b.is_parent) return a.is_parent ? -1 : 1;
	if (a.is_parent) return 0;

	// ディレクトリを先に集める (NyanFi の DirSortMode が 5「区別しない」以外の
	// ときに常にこの扱いになるのに相当)
	if (dirs_first && a.is_dir != b.is_dir) return a.is_dir ? -1 : 1;

	int result = 0;
	switch (key) {
	case SortKey::Name: result = compare_name(a, b); break;
	case SortKey::Ext:  result = compare_ext(a, b);  break;
	case SortKey::Date: result = compare_date(a, b); break;
	case SortKey::Size: result = compare_size(a, b); break;
	case SortKey::Attr: result = compare_attr(a, b); break;
	}
	return descending ? -result : result;
}

//---------------------------------------------------------------------------
/**
 * @details MainFrm.cpp::ApplyPathMask / SplitMasksFD (GUI グローバル依存で
 * 未移植) と同じアルゴリズムを、移植済みの下位関数だけで書き直したもの。
 * 呼んでいるのはいずれも usr_str.cpp のテスト付き実装:
 * split_strings_semicolon / remove_end_s / remove_top_s / str_match / StartsStr。
 *
 * 元の実装 (ApplyPathMask) は file_rec::n_name (正規化した名前) に対して
 * str_match するが、ここでは FileItem::name をそのまま使う (パス無しのファイル
 * 名という点は同じ)
 */
bool MatchPathMask(const UnicodeString &mask, const UnicodeString &name, bool is_dir)
{
	const UnicodeString trimmed = mask.Trim();
	if (trimmed.IsEmpty() || SameStr(trimmed, _T("*"))) return true;

	const TStringDynArray tokens = split_strings_semicolon(trimmed, true);

	std::vector<UnicodeString> f_list, d_list;
	for (int i = 0; i < tokens.Length; ++i) {
		UnicodeString tok = tokens[i];
		if (remove_end_s(tok, _T("\\"))) d_list.push_back(tok); else f_list.push_back(tok);
	}

	// 除外指定 (!) しか無いリストには * を補う (SplitMasksFD と同じ)
	auto has_non_exclude = [](const std::vector<UnicodeString> &v) {
		for (const UnicodeString &m : v) {
			if (!StartsStr(_T("!"), m)) return true;
		}
		return false;
	};
	if (!has_non_exclude(d_list)) d_list.push_back(_T("*"));
	if (!has_non_exclude(f_list)) f_list.push_back(_T("*"));

	const std::vector<UnicodeString> &list = is_dir ? d_list : f_list;
	bool match = false, excluded = false;
	for (UnicodeString m : list) {
		if (remove_top_s(m, _T("!"))) {
			if (str_match(m, name)) excluded = true;
		}
		else if (SameStr(m, _T("*")) || str_match(m, name)) {
			match = true;
		}
	}
	return match && !excluded;
}

//---------------------------------------------------------------------------
UnicodeString GetBaseName(const UnicodeString &name)
{
	// VCL の get_base_name (src/Global.cpp) 相当。
	// 先頭の '.' は拡張子とみなさない (.gitignore → ".gitignore")
	const int dot_pos = name.LastDelimiter(_T("."));
	if (dot_pos <= 1) return name;  // ドット無し、または先頭のみ
	return name.SubString(1, dot_pos - 1);
}

//---------------------------------------------------------------------------
UnicodeString GetExtension(const UnicodeString &name)
{
	// VCL の get_extension (src/Global.cpp) 相当。
	// 先頭の '.' は拡張子とみなさない (.gitignore → "")
	const int dot_pos = name.LastDelimiter(_T("."));
	if (dot_pos <= 1) return EmptyStr;
	return name.SubString(dot_pos, name.Length() - dot_pos + 1);
}

//---------------------------------------------------------------------------
namespace {

/// テキストファイルの拡張子一覧 (VCL の is_TextFile の簡易版)
bool IsTextExtension(const UnicodeString &ext)
{
	static const UnicodeString kTextExts[] = {
		_T(".txt"), _T(".c"), _T(".h"), _T(".cpp"), _T(".hpp"), _T(".cc"), _T(".hh"),
		_T(".cxx"), _T(".hxx"), _T(".cs"), _T(".java"), _T(".py"), _T(".rb"), _T(".pl"),
		_T(".php"), _T(".js"), _T(".ts"), _T(".html"), _T(".htm"), _T(".css"), _T(".xml"),
		_T(".json"), _T(".yaml"), _T(".yml"), _T(".md"), _T(".ini"), _T(".cfg"), _T(".log"),
		_T(".sh"), _T(".bat"), _T(".cmd"), _T(".ps1"), _T(".sql"), _T(".csv"), _T(".tsv"),
	};
	for (const UnicodeString &e : kTextExts) {
		if (SameText(e, ext)) return true;
	}
	return false;
}

}  // namespace

//---------------------------------------------------------------------------
UnicodeString FindNextSameName(const std::vector<FileItem> &items, int idx, bool only_text)
{
	if (idx < 0 || idx >= static_cast<int>(items.size())) return EmptyStr;

	const UnicodeString bnam = GetBaseName(items[idx].name);
	if (bnam.IsEmpty()) return EmptyStr;

	int idx0 = -1, idx1 = -1;
	for (int i = 0; i < static_cast<int>(items.size()); i++) {
		if (i == idx) continue;  // 自分自身は「次」ではない
		const FileItem &itm = items[i];
		if (itm.is_dir || itm.is_parent) continue;
		if (!SameText(GetBaseName(itm.name), bnam)) continue;
		if (only_text && !IsTextExtension(GetExtension(itm.name))) continue;
		if (i < idx) idx0 = i; else idx1 = i;
	}
	const int next = (idx1 != -1) ? idx1 : idx0;
	return (next != -1) ? items[next].name : EmptyStr;
}

//---------------------------------------------------------------------------
UnicodeString GetSrcHdrName(const UnicodeString &fnam)
{
	// VCL の get_SrcHdrName (src/Global.cpp:3965-3981) の簡易版。
	// .c/.cc/.cpp/.cxx → .h/.hh/.hpp/.hxx、逆も。
	const UnicodeString fext = GetExtension(fnam);
	if (fext.IsEmpty()) return EmptyStr;

	UnicodeString new_ext;
	if (SameText(fext, _T(".c"))) new_ext = _T(".h");
	else if (SameText(fext, _T(".cc"))) new_ext = _T(".hh");
	else if (SameText(fext, _T(".cpp"))) new_ext = _T(".hpp");
	else if (SameText(fext, _T(".cxx"))) new_ext = _T(".hxx");
	else if (SameText(fext, _T(".h"))) new_ext = _T(".c");
	else if (SameText(fext, _T(".hh"))) new_ext = _T(".cc");
	else if (SameText(fext, _T(".hpp"))) new_ext = _T(".cpp");
	else if (SameText(fext, _T(".hxx"))) new_ext = _T(".cxx");
	else return EmptyStr;

	// 拡張子を置換
	const int dot_pos = fnam.LastDelimiter(_T("."));
	if (dot_pos <= 0) return EmptyStr;
	return fnam.SubString(1, dot_pos - 1) + new_ext;
}
