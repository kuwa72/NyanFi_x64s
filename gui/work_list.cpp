/**
 * @file gui/work_list.cpp
 * @brief ワークリストの実装 (設計は gui/work_list.h)
 */
#include "gui/work_list.h"

#include <algorithm>
#include <string>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace work_list {

namespace {

/// UnicodeString を UTF-8 のバイト列にする
std::string to_utf8(const UnicodeString &s)
{
	if (s.IsEmpty()) return std::string();
	const int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), NULL, 0, NULL, NULL);
	if (n <= 0) return std::string();
	std::string out(static_cast<std::size_t>(n), '\0');
	::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), &out[0], n, NULL, NULL);
	return out;
}

/// UTF-8 のバイト列を UnicodeString にする
UnicodeString from_utf8(const char *p, int len)
{
	if (len <= 0) return EmptyStr;
	const int n = ::MultiByteToWideChar(CP_UTF8, 0, p, len, NULL, 0);
	if (n <= 0) return EmptyStr;
	std::wstring out(static_cast<std::size_t>(n), L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, p, len, &out[0], n);
	return UnicodeString(out.c_str(), n);
}

/// ファイルを丸ごと読む
bool read_all(const UnicodeString &path, std::string &out, UnicodeString &error_out)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		error_out = _T("開けません");
		return false;
	}
	char buf[16 * 1024];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return true;
}

/// バイト列をファイルへ書く
bool write_all(const UnicodeString &path, const std::string &bytes, UnicodeString &error_out)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		error_out = _T("書き込めません");
		return false;
	}
	bool ok = true;
	if (!bytes.empty()) {
		DWORD written = 0;
		ok = (::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != 0)
		     && (written == bytes.size());
		if (!ok) error_out = _T("書き込みに失敗しました");
	}
	::CloseHandle(h);
	return ok;
}

/// 実体を見て size / stamp / attr / missing を埋める
void fill_from_disk(WorkItem &itm)
{
	if (itm.is_separator) return;

	const bool exists = itm.is_dir? dir_exists(itm.path) : file_exists(itm.path);
	if (!exists) {
		itm.missing = true;
		itm.size    = 0;
		itm.attr    = 0;
		return;
	}
	itm.missing = false;
	itm.attr    = file_GetAttr(itm.path);
	itm.stamp   = get_file_age(itm.path);
	itm.size    = itm.is_dir? 0 : get_file_size(itm.path);
}

}  // namespace

//---------------------------------------------------------------------------
std::vector<WorkItem> ParseLines(const std::vector<UnicodeString> &lines)
{
	std::vector<WorkItem> items;
	items.reserve(lines.size());

	for (std::size_t i = 0; i < lines.size(); ++i) {
		UnicodeString lbuf = lines[i];
		if (lbuf.IsEmpty() || StartsStr(_T(";"), lbuf)) continue;

		// split_tkn は区切りの手前を返し、lbuf を残りに書き換える (src/usr_str.cpp)
		const UnicodeString fnam = split_tkn(lbuf, _T("\t"));
		const UnicodeString anam = lbuf;
		if (fnam.IsEmpty() && anam.IsEmpty()) continue;

		WorkItem itm;
		if (fnam.IsEmpty() && is_separator(anam)) {
			itm.alias        = anam;
			itm.is_separator = true;
			items.push_back(itm);
			continue;
		}
		// パスが空で別名だけの行は VCL も捨てる (file_rec を作らずに落ちる)
		if (fnam.IsEmpty()) continue;

		itm.is_dir = ends_PathDlmtr(fnam);
		itm.path   = itm.is_dir? ExcludeTrailingPathDelimiter(fnam) : fnam;
		itm.alias  = anam;
		items.push_back(itm);
	}
	return items;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> FormatLines(const std::vector<WorkItem> &items)
{
	std::vector<UnicodeString> lines;
	lines.reserve(items.size());

	for (std::size_t i = 0; i < items.size(); ++i) {
		const WorkItem &itm = items[i];
		if (itm.path.IsEmpty() && itm.alias.IsEmpty()) continue;

		UnicodeString lbuf = (itm.is_dir && !itm.path.IsEmpty())?
		                        IncludeTrailingPathDelimiter(itm.path) : itm.path;
		lbuf += _T("\t");
		lbuf += itm.alias;
		lines.push_back(lbuf);
	}
	return lines;
}

//---------------------------------------------------------------------------
bool Load(const UnicodeString &path, bool auto_delete,
          std::vector<WorkItem> &items_out, UnicodeString &error_out)
{
	items_out.clear();

	const UnicodeString full = to_absolute_name(path);
	if (!file_exists(full)) {
		error_out = _T("ファイルがありません");
		return false;
	}

	std::string bytes;
	if (!read_all(full, bytes, error_out)) return false;

	// BOM を落とす。VCL 側 (TStrings::SaveToFile + TEncoding::UTF8) が付ける
	std::size_t pos = 0;
	if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
	    && static_cast<unsigned char>(bytes[1]) == 0xBB
	    && static_cast<unsigned char>(bytes[2]) == 0xBF) {
		pos = 3;
	}

	// 行に分ける (CRLF / LF / CR のどれでも)
	std::vector<UnicodeString> lines;
	std::size_t start = pos;
	for (std::size_t i = pos; i <= bytes.size(); ++i) {
		const bool eof = (i == bytes.size());
		if (!eof && bytes[i] != '\n' && bytes[i] != '\r') continue;
		lines.push_back(from_utf8(bytes.data() + start, static_cast<int>(i - start)));
		if (eof) break;
		if (bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
		start = i + 1;
	}

	items_out = ParseLines(lines);
	for (std::size_t i = 0; i < items_out.size(); ++i) fill_from_disk(items_out[i]);

	if (auto_delete) RemoveMissing(items_out);
	return true;
}

//---------------------------------------------------------------------------
bool Save(const UnicodeString &path, const std::vector<WorkItem> &items,
          UnicodeString &error_out)
{
	if (path.IsEmpty()) {
		error_out = _T("保存先が指定されていません");
		return false;
	}

	const std::vector<UnicodeString> lines = FormatLines(items);

	std::string bytes = "\xEF\xBB\xBF";  // BOM (VCL 版が読めるように必ず付ける)
	for (std::size_t i = 0; i < lines.size(); ++i) {
		bytes += to_utf8(lines[i]);
		bytes += "\r\n";
	}
	return write_all(path, bytes, error_out);
}

//---------------------------------------------------------------------------
int IndexOfPath(const std::vector<WorkItem> &items, const UnicodeString &path)
{
	for (std::size_t i = 0; i < items.size(); ++i) {
		if (items[i].is_separator) continue;
		if (SameText(items[i].path, path)) return static_cast<int>(i);
	}
	return -1;
}

//---------------------------------------------------------------------------
bool Add(std::vector<WorkItem> &items, const UnicodeString &path, int at)
{
	if (path.IsEmpty()) return false;
	if (IndexOfPath(items, path) != -1) return false;

	WorkItem itm;
	itm.is_dir = dir_exists(path);
	itm.path   = ExcludeTrailingPathDelimiter(path);
	if (!itm.is_dir && !file_exists(itm.path)) return false;
	fill_from_disk(itm);

	if (at < 0 || at > static_cast<int>(items.size())) items.push_back(itm);
	else items.insert(items.begin() + at, itm);
	return true;
}

//---------------------------------------------------------------------------
void InsertSeparator(std::vector<WorkItem> &items, int at)
{
	WorkItem sep;
	sep.alias        = _T("-");
	sep.is_separator = true;

	if (at < 0 || at + 1 >= static_cast<int>(items.size())) items.push_back(sep);
	else items.insert(items.begin() + (at + 1), sep);
}

//---------------------------------------------------------------------------
int RemoveMissing(std::vector<WorkItem> &items)
{
	const std::size_t before = items.size();
	items.erase(std::remove_if(items.begin(), items.end(),
	                           [](const WorkItem &w) { return w.missing; }),
	            items.end());
	return static_cast<int>(before - items.size());
}

//---------------------------------------------------------------------------
bool HasSeparator(const std::vector<WorkItem> &items)
{
	for (std::size_t i = 0; i < items.size(); ++i) {
		if (items[i].is_separator) return true;
	}
	return false;
}

//---------------------------------------------------------------------------
namespace {

/// 選択が1件も無ければカーソル位置だけを対象にする (VCL の sel_cnt==0 の分岐)
bool targeted(const std::vector<WorkItem> &items, int index, int cursor, int marked_count)
{
	if (marked_count > 0) return items[static_cast<std::size_t>(index)].marked;
	return index == cursor;
}

int marked_count_of(const std::vector<WorkItem> &items)
{
	int n = 0;
	for (std::size_t i = 0; i < items.size(); ++i) {
		if (items[i].marked) ++n;
	}
	return n;
}

}  // namespace

//---------------------------------------------------------------------------
bool MoveUp(std::vector<WorkItem> &items, int &cursor)
{
	const int n = static_cast<int>(items.size());
	if (n == 0) return false;

	const int sel_cnt = marked_count_of(items);
	if (sel_cnt == 0 && (cursor < 0 || cursor >= n)) return false;
	// 先頭が対象なら全体が動けない (VCL も何もせず返る)
	if (targeted(items, 0, cursor, sel_cnt)) return false;

	for (int i = 1; i < n; ++i) {
		if (targeted(items, i, cursor, sel_cnt)) {
			std::swap(items[static_cast<std::size_t>(i)], items[static_cast<std::size_t>(i - 1)]);
		}
	}
	if (cursor > 0) --cursor;
	return true;
}

//---------------------------------------------------------------------------
bool MoveDown(std::vector<WorkItem> &items, int &cursor)
{
	const int n = static_cast<int>(items.size());
	if (n == 0) return false;

	const int sel_cnt = marked_count_of(items);
	if (sel_cnt == 0 && (cursor < 0 || cursor >= n)) return false;
	if (targeted(items, n - 1, cursor, sel_cnt)) return false;

	for (int i = n - 2; i >= 0; --i) {
		if (targeted(items, i, cursor, sel_cnt)) {
			std::swap(items[static_cast<std::size_t>(i)], items[static_cast<std::size_t>(i + 1)]);
		}
	}
	if (cursor < n - 1) ++cursor;
	return true;
}

//---------------------------------------------------------------------------
bool MoveSelectedTo(std::vector<WorkItem> &items, int &cursor)
{
	const int n = static_cast<int>(items.size());
	if (cursor < 0 || cursor >= n) return false;
	if (marked_count_of(items) == 0) return false;

	std::vector<WorkItem> cut;
	int ins_idx = cursor;
	for (int i = 0; i < static_cast<int>(items.size());) {
		if (items[static_cast<std::size_t>(i)].marked) {
			cut.push_back(items[static_cast<std::size_t>(i)]);
			items.erase(items.begin() + i);
			if (i < ins_idx) --ins_idx;
		}
		else ++i;
	}

	for (int i = static_cast<int>(cut.size()) - 1; i >= 0; --i) {
		cut[static_cast<std::size_t>(i)].marked = false;
		items.insert(items.begin() + ins_idx, cut[static_cast<std::size_t>(i)]);
	}
	cursor = ins_idx;
	return true;
}

//---------------------------------------------------------------------------
std::vector<FileItem> ToFileItems(const std::vector<WorkItem> &items)
{
	std::vector<FileItem> out;
	out.reserve(items.size());

	for (std::size_t i = 0; i < items.size(); ++i) {
		const WorkItem &w = items[i];
		FileItem f;
		f.is_separator = w.is_separator;
		f.alias        = w.alias;
		f.marked       = w.marked;
		if (w.is_separator) {
			out.push_back(f);
			continue;
		}
		f.name      = ExtractFileName(w.path);
		f.full_path = w.path;
		f.is_dir    = w.is_dir;
		f.size      = w.is_dir? -1 : w.size;
		f.stamp     = w.stamp;
		f.attr      = w.attr;
		f.missing   = w.missing;
		out.push_back(f);
	}
	return out;
}

//---------------------------------------------------------------------------
void ApplyMarks(std::vector<WorkItem> &items, const std::vector<FileItem> &from)
{
	const std::size_t n = std::min(items.size(), from.size());
	for (std::size_t i = 0; i < n; ++i) items[i].marked = from[i].marked;
}

}  // namespace work_list
