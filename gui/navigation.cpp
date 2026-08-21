/**
 * @file gui/navigation.cpp
 * @brief gui/navigation.h の実装
 */
#include "gui/navigation.h"

#include "usr_file_ex.h"
#include "usr_str.h"

//---------------------------------------------------------------------------
bool IncrementalSearch::Backspace()
{
	if (word_.IsEmpty()) return false;
	delete_end(word_);  //!< usr_str.h の移植済み関数 (末尾1文字を削る)
	return true;
}

//---------------------------------------------------------------------------
bool IncrementalSearchMatch(const UnicodeString &name, const UnicodeString &keyword)
{
	if (keyword.IsEmpty()) return false;
	return contains_word_and_or(name, keyword, false);
}

//---------------------------------------------------------------------------
int FindIncrementalSearchMatch(const std::vector<UnicodeString> &names, const UnicodeString &keyword,
                                int start_index, bool forward)
{
	const int count = static_cast<int>(names.size());
	if (count == 0 || keyword.IsEmpty()) return -1;

	for (int step = 1; step <= count; ++step) {
		const int idx = forward ? (start_index + step) % count : ((start_index - step) % count + count) % count;
		if (IncrementalSearchMatch(names[static_cast<std::size_t>(idx)], keyword)) return idx;
	}
	return -1;
}

//---------------------------------------------------------------------------
void DirHistory::Navigate(const UnicodeString &path)
{
	if (pos_ >= 0 && SameText(entries_[static_cast<std::size_t>(pos_)], path)) return;

	// 履歴の途中から新しいディレクトリへ移動した場合、その先の「進む」履歴を捨てる
	if (pos_ + 1 < static_cast<int>(entries_.size())) {
		entries_.erase(entries_.begin() + (pos_ + 1), entries_.end());
	}

	entries_.push_back(path);
	++pos_;

	// 上限を超えたら古いものから捨てる (VCL 版の MaxDirHistory と同じ考え方)
	if (static_cast<int>(entries_.size()) > max_entries_) {
		const int overflow = static_cast<int>(entries_.size()) - max_entries_;
		entries_.erase(entries_.begin(), entries_.begin() + overflow);
		pos_ -= overflow;
	}
}

//---------------------------------------------------------------------------
UnicodeString DirHistory::Back()
{
	if (!CanBack()) return EmptyStr;
	--pos_;
	return entries_[static_cast<std::size_t>(pos_)];
}

//---------------------------------------------------------------------------
UnicodeString DirHistory::Forward()
{
	if (!CanForward()) return EmptyStr;
	++pos_;
	return entries_[static_cast<std::size_t>(pos_)];
}

//---------------------------------------------------------------------------
UnicodeString DirHistory::JumpTo(int index)
{
	if (index < 0 || index >= static_cast<int>(entries_.size())) return EmptyStr;
	pos_ = index;
	return entries_[static_cast<std::size_t>(pos_)];
}

//---------------------------------------------------------------------------
UnicodeString DriveTypeLabel(unsigned int drive_type)
{
	// Global.cpp (SetDriveInfo 相当箇所) の type_str への割り当てと同じ文言 (実測)
	switch (drive_type) {
	case DRIVE_REMOVABLE: return _T("リムーバブル・メディア");
	case DRIVE_FIXED:     return _T("ハードディスク");
	case DRIVE_REMOTE:    return _T("ネットワーク・ドライブ");
	case DRIVE_CDROM:     return _T("CD-ROMドライブ");
	case DRIVE_RAMDISK:   return _T("RAMディスク");
	default:              return EmptyStr;
	}
}

//---------------------------------------------------------------------------
bool ResolveDirectoryInput(const UnicodeString &input, const UnicodeString &base_dir, UnicodeString &resolved_out)
{
	UnicodeString s = input.Trim();
	if (s.IsEmpty()) return false;

	s = get_actual_path(s);            // %VAR%/%ExePath%/$X/$D の展開 (cv_env_var 経由)
	s = to_absolute_name(s, base_dir);  // 相対パス・".." の解決
	if (s.IsEmpty()) return false;

	s = IncludeTrailingPathDelimiter(s);
	if (!dir_exists(s)) return false;

	resolved_out = s;
	return true;
}

//---------------------------------------------------------------------------
// ディレクトリ・スタック (設計と VCL の該当行は gui/navigation.h を参照)
//---------------------------------------------------------------------------
void DirStack::Push(const UnicodeString &path, int cursor)
{
	if (path.IsEmpty()) return;
	Entry e;
	e.path = path;
	e.cursor = cursor;
	// VCL は Insert(0, ...) で**先頭に挿入**する (MainFrm.cpp:24100)
	entries_.insert(entries_.begin(), e);
}

//---------------------------------------------------------------------------
bool DirStack::Pop(Entry &out, const std::function<bool(const UnicodeString &)> &exists)
{
	// 存在しなくなったディレクトリは読み飛ばす (MainFrm.cpp:23715)。
	// 消えたディレクトリでスタックが詰まらないようにするため
	while (!entries_.empty()) {
		const Entry e = entries_.front();
		entries_.erase(entries_.begin());
		if (exists(e.path)) {
			out = e;
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
UnicodeString NextDriveOf(const std::vector<UnicodeString> &drives,
                          const UnicodeString &current, bool forward)
{
	if (drives.empty()) return EmptyStr;

	// VCL は「現在より辞書順で大きい最初のもの。無ければ先頭」
	// (MainFrm.cpp:22368-22372)。一覧中の位置を +1 するのではない
	if (forward) {
		for (const UnicodeString &d : drives) {
			if (CompareText(current, d) < 0) return d;
		}
		return drives.front();
	}

	// 前へ回るのは VCL の PrevDrive。向きを逆にしたもの
	for (auto it = drives.rbegin(); it != drives.rend(); ++it) {
		if (CompareText(current, *it) > 0) return *it;
	}
	return drives.back();
}
