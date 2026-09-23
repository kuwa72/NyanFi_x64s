/**
 * @file gui/compare.cpp
 * @brief 左右の比較の実装 (設計は gui/compare.h)
 */
#include "gui/compare.h"

#include <algorithm>
#include <map>

#include "gui/f_misc_ops.h"

namespace compare {

//---------------------------------------------------------------------------
bool IsSameItem(const FileItem &a, const FileItem &b, MatchBy how)
{
	if (!SameText(a.name, b.name)) return false;

	switch (how) {
	case MatchBy::Name:     return true;
	case MatchBy::NameSize: return a.size == b.size;
	case MatchBy::NameTime:
		// 更新日時は秒より細かい差を無視する。ファイルシステムをまたぐと
		// 分解能が違い (FAT は2秒単位)、同じファイルでも一致しなくなるため
		return std::abs(static_cast<double>(a.stamp) - static_cast<double>(b.stamp))
		       < (2.0 / (24.0 * 60.0 * 60.0));
	case MatchBy::Content:
		// 内容の比較はハッシュが要る。ここでは名前とサイズまでを見て、
		// 実際の判定は呼び出し側に任せる
		return a.size == b.size;
	}
	return false;
}

//---------------------------------------------------------------------------
std::vector<int> IndicesOnlyHere(const std::vector<FileItem> &items,
                                 const std::vector<FileItem> &others, MatchBy how)
{
	std::vector<int> out;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const FileItem &it = items[i];
		if (it.is_parent || it.is_dir) continue;  // ファイルだけ

		bool found = false;
		for (const FileItem &o : others) {
			if (o.is_parent || o.is_dir) continue;
			if (IsSameItem(it, o, how)) { found = true; break; }
		}
		if (!found) out.push_back(static_cast<int>(i));
	}
	return out;
}

//---------------------------------------------------------------------------
std::vector<DiffRow> DiffDirectories(const std::vector<FileItem> &left,
                                     const std::vector<FileItem> &right, MatchBy how)
{
	// 名前 (大文字小文字を区別しない) で突き合わせる
	std::map<UnicodeString, const FileItem *> r_map;
	for (const FileItem &r : right) {
		if (r.is_parent || r.is_dir) continue;
		r_map[r.name.UpperCase()] = &r;
	}

	std::vector<DiffRow> rows;
	for (const FileItem &l : left) {
		if (l.is_parent || l.is_dir) continue;

		const auto it = r_map.find(l.name.UpperCase());
		if (it == r_map.end()) {
			DiffRow row;
			row.name = l.name;
			row.in_left = true;
			rows.push_back(row);
			continue;
		}

		if (!IsSameItem(l, *it->second, how)) {
			DiffRow row;
			row.name = l.name;
			row.in_left = true;
			row.in_right = true;
			row.differs = true;
			rows.push_back(row);
		}
		// 同じものは**入れない** (違いだけを見たいため)
		r_map.erase(it);
	}

	// 右にしか無いもの
	for (const auto &kv : r_map) {
		DiffRow row;
		row.name = kv.second->name;
		row.in_right = true;
		rows.push_back(row);
	}

	std::sort(rows.begin(), rows.end(), [](const DiffRow &a, const DiffRow &b) {
		return CompareText(a.name, b.name) < 0;
	});
	return rows;
}

//---------------------------------------------------------------------------
DiffDirSource ResolveDiffDirSource(const UnicodeString &param)
{
	// VCL (MainFrm.cpp の DiffDirActionExecute) は AL→DL→ダイアログの順に見る
	if (f_misc_ops::HasParamToken(param, _T("AL"))) return DiffDirSource::AllPreset;
	if (f_misc_ops::HasParamToken(param, _T("DL"))) return DiffDirSource::DefaultPreset;
	return DiffDirSource::Dialog;
}

//---------------------------------------------------------------------------
UnicodeString NormalizeDiffIncMask(const UnicodeString &mask)
{
	// VCL (DiffDlg.cpp の FormClose): 空なら "*.*" にする
	if (mask.Trim().IsEmpty()) return UnicodeString(_T("*.*"));
	return mask;
}

//---------------------------------------------------------------------------
DiffDirOptions AllDiffPreset()
{
	// VCL (MainFrm.cpp の AL 分岐): マスクは *.*、除外は無し、
	// サブディレクトリは対象にする
	DiffDirOptions opt;
	opt.inc_mask = _T("*.*");
	opt.exc_mask = EmptyStr;
	opt.exc_dir = EmptyStr;
	opt.sub_dir = true;
	return opt;
}

//---------------------------------------------------------------------------
DiffDirOptions DefaultDiffPreset(const UnicodeString &inc_mask, const UnicodeString &exc_mask,
                                 const UnicodeString &exc_dir, bool sub_dir)
{
	// VCL (MainFrm.cpp の DL 分岐): ini の保存値をそのまま使う
	DiffDirOptions opt;
	opt.inc_mask = NormalizeDiffIncMask(inc_mask);
	opt.exc_mask = exc_mask;
	opt.exc_dir = exc_dir;
	opt.sub_dir = sub_dir;
	return opt;
}

//---------------------------------------------------------------------------
namespace {

/// `;` 区切りのどれかに合えば true (空リストは「全部」とみなす)
bool MatchesAnyMask(const TStringDynArray &masks, const UnicodeString &name)
{
	if (masks.Length == 0) return true;
	for (int i = 0; i < masks.Length; ++i) {
		if (str_match(masks[i], name)) return true;
	}
	return false;
}

}  // namespace

std::vector<FileItem> FilterDiffItems(const std::vector<FileItem> &items,
                                      const UnicodeString &inc_mask,
                                      const UnicodeString &exc_mask)
{
	// VCL (MainFrm.cpp の DiffDirActionExecute) は対象マスクで列挙し、
	// 除外マスクに合うものを落とす。こちらは表示中の一覧に同じ絞り込みを掛ける
	const TStringDynArray inc = split_strings_semicolon(NormalizeDiffIncMask(inc_mask), true);
	const TStringDynArray exc = split_strings_semicolon(exc_mask, true);

	std::vector<FileItem> out;
	for (const FileItem &it : items) {
		if (it.is_parent || it.is_dir) continue;
		if (!MatchesAnyMask(inc, it.name)) continue;
		if (exc.Length > 0 && MatchesAnyMask(exc, it.name)) continue;
		out.push_back(it);
	}
	return out;
}

//---------------------------------------------------------------------------
// 同名ファイルの比較 (TFileCompDlg / src/CompDlg.cpp)
//---------------------------------------------------------------------------

/// VCL の `TimeTolerance` 既定値 (2000ms) に相当する許容誤差 (日単位)
const double kCompTimeToleranceSec = 2.0 / (24.0 * 60.0 * 60.0);

CompEnable ResolveCompEnabled(const CompOptions &o, bool all_dir_has_size, bool ftp_either,
                              bool arc_either, bool sel_mask_available)
{
	// VCL (TFileCompDlg::OkActionUpdate) と同じ組合せ
	CompEnable en;
	en.size = !o.cmp_dir || all_dir_has_size;
	en.hash = !o.cmp_dir && o.size_mode == CompSizeMode::Equal && !ftp_either;
	en.alg = en.hash;
	en.id = !o.cmp_dir && (o.size_mode == CompSizeMode::Ignore
	                       || o.size_mode == CompSizeMode::Equal)
	        && !arc_either && !ftp_either;
	en.cmp_arc = o.cmp_dir;
	en.sel_mask = sel_mask_available;
	return en;
}

//---------------------------------------------------------------------------
CompOptions ApplyExclusiveOpt(CompOptions o, bool hash_clicked)
{
	// VCL (TFileCompDlg::OptRadioGroupClick): 選んだ側 (>0) 以外を「無視」にする
	if (hash_clicked) {
		if (o.hash_mode != CompHashMode::Ignore) o.id_mode = CompIdMode::Ignore;
	}
	else {
		if (o.id_mode != CompIdMode::Ignore) o.hash_mode = CompHashMode::Ignore;
	}
	return o;
}

//---------------------------------------------------------------------------
bool CompSizeMet(CompSizeMode mode, Int64 left, Int64 right)
{
	switch (mode) {
	case CompSizeMode::Ignore:  return true;
	case CompSizeMode::Unequal: return left != right;
	case CompSizeMode::Equal:   return left == right;
	case CompSizeMode::Greater: return left > right;
	case CompSizeMode::Less:    return left < right;
	}
	return true;
}

//---------------------------------------------------------------------------
bool CompTimeMet(CompTimeMode mode, double left_stamp, double right_stamp)
{
	// VCL: WithinPastMilliSeconds で 2秒の許容誤差を付けてから大小を見る
	const bool same = std::abs(left_stamp - right_stamp) < kCompTimeToleranceSec;
	switch (mode) {
	case CompTimeMode::Ignore:  return true;
	case CompTimeMode::Unequal: return !same;
	case CompTimeMode::Equal:   return same;
	case CompTimeMode::Newer:   return !same && left_stamp > right_stamp;
	case CompTimeMode::Older:   return !same && left_stamp < right_stamp;
	}
	return true;
}

//---------------------------------------------------------------------------
namespace {

/// 比較対象か (親 (..) は対象外。ディレクトリは cmp_dir のときだけ)
bool IsCompTarget(const FileItem &it, bool cmp_dir)
{
	if (it.is_parent) return false;
	if (it.is_dir && !cmp_dir) return false;
	return true;
}

/// 名前の照合 (CS 指定時は大小を区別)
bool CompSameName(const FileItem &a, const FileItem &b, bool case_sensitive)
{
	return case_sensitive? SameStr(a.name, b.name) : SameText(a.name, b.name);
}

}  // namespace

//---------------------------------------------------------------------------
CompResult CompareSameNames(const std::vector<FileItem> &left,
                            const std::vector<FileItem> &right, const CompOptions &o,
                            const CompProbes &probes)
{
	CompResult res;
	for (std::size_t i = 0; i < left.size(); ++i) {
		const FileItem &l = left[i];
		if (!IsCompTarget(l, o.cmp_dir)) continue;
		res.left_count++;

		for (std::size_t j = 0; j < right.size(); ++j) {
			const FileItem &r = right[j];
			if (!IsCompTarget(r, o.cmp_dir)) continue;
			// ディレクトリ比較で無いときはディレクトリとファイルを突き合わせない
			if (!o.cmp_dir && l.is_dir != r.is_dir) continue;
			if (!CompSameName(l, r, o.case_sensitive)) continue;

			// 条件は AND 結合 (VCL と同じ)
			if (!CompTimeMet(o.time_mode, static_cast<double>(l.stamp),
			                 static_cast<double>(r.stamp))) continue;
			if (!CompSizeMet(o.size_mode, l.size, r.size)) continue;
			// ハッシュはサイズが同じ組でのみ計算する (VCL と同じ前置き)。
			// 一致/不一致の判定は指定した条件方向で行う
			if (o.hash_mode != CompHashMode::Ignore && l.size == r.size) {
				if (!probes.hash_equal) continue;  // 判定出来なければ非一致扱い
				const bool eq = probes.hash_equal(l, r);
				if (o.hash_mode == CompHashMode::Equal && !eq) continue;
				if (o.hash_mode == CompHashMode::Unequal && eq) continue;
			}
			if (o.id_mode != CompIdMode::Ignore) {
				if (!probes.identity_equal) continue;  // 判定出来なければ非一致扱い
				const bool eq = probes.identity_equal(l, r);
				if (o.id_mode == CompIdMode::Equal && !eq) continue;
				if (o.id_mode == CompIdMode::Unequal && eq) continue;
			}

			res.left_hits.push_back(static_cast<int>(i));
			if (o.sel_opp) res.right_hits.push_back(static_cast<int>(j));
		}
	}
	return res;
}

//---------------------------------------------------------------------------
std::vector<bool> ReverseCompareSelection(const std::vector<FileItem> &items,
                                          std::vector<bool> selected, bool cmp_dir)
{
	// VCL (MainFrm.cpp:14710付近): 比較対象になった項目だけを反転する
	const std::size_t n = selected.size() < items.size()? selected.size() : items.size();
	for (std::size_t i = 0; i < n; ++i) {
		// .. は常に対象外。ディレクトリは cmp_dir のときだけ
		if (items[i].is_parent) continue;
		if (items[i].is_dir && !cmp_dir) continue;
		selected[i] = !selected[i];
	}
	return selected;
}

}  // namespace compare
