/**
 * @file gui/compare.cpp
 * @brief 左右の比較の実装 (設計は gui/compare.h)
 */
#include "gui/compare.h"

#include <algorithm>
#include <map>

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

}  // namespace compare
