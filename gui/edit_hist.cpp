/**
 * @file gui/edit_hist.cpp
 * @brief gui/edit_hist.h の実装
 */
#include "gui/edit_hist.h"

#include <algorithm>

#include "UIniFile.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace edit_hist {

namespace {

UnicodeString path_of(const UnicodeString &record)
{
	const UnicodeString path = get_csv_item(record, 0);
	return path.IsEmpty()? record : path;
}

UnicodeString normalized_directory(const UnicodeString &path)
{
	return IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(path));
}

UnicodeString display_name(const UnicodeString &path)
{
	const UnicodeString base = get_base_name(path);
	const UnicodeString ext = get_extension(path);
	return ext.IsEmpty()? base : base + ext;
}

}  // namespace

//---------------------------------------------------------------------------
int ModeIndex(Mode mode)
{
	switch (mode) {
	case Mode::CurrentPath:      return 1;
	case Mode::CurrentDirectory: return 2;
	case Mode::All:              return 0;
	}
	return 0;
}

//---------------------------------------------------------------------------
Mode ModeFromIndex(int index)
{
	switch (index) {
	case 1:  return Mode::CurrentPath;
	case 2:  return Mode::CurrentDirectory;
	default: return Mode::All;
	}
}

//---------------------------------------------------------------------------
UnicodeString ModeTitle(Mode mode)
{
	switch (mode) {
	case Mode::CurrentPath:      return _T("最近編集したファイル一覧 - 現在の場所以下");
	case Mode::CurrentDirectory: return _T("最近編集したファイル一覧 - 現在の場所");
	case Mode::All:              return _T("最近編集したファイル一覧");
	}
	return _T("最近編集したファイル一覧");
}

//---------------------------------------------------------------------------
bool MatchesMode(const UnicodeString &path, const UnicodeString &current_path, Mode mode)
{
	if (mode == Mode::All) return true;
	if (current_path.IsEmpty()) return false;

	if (mode == Mode::CurrentPath) {
		// VCL は CurPathName (通常は末尾区切り付き) に対して StartsText。
		// wx 側へ渡されるパスに区切りが無くても同じ境界を保つ。
		return StartsText(normalized_directory(current_path), path);
	}

	return SameText(ExcludeTrailingPathDelimiter(ExtractFilePath(path)),
	                ExcludeTrailingPathDelimiter(current_path));
}

//---------------------------------------------------------------------------
bool MatchesFilter(const UnicodeString &path, const UnicodeString &filter, bool migemo)
{
	if (filter.IsEmpty()) return true;

	const UnicodeString name = ExtractFileName(path);
	if (!migemo) {
		// Migemo 無効時は VCL の GetRegExPtn が検索語を実体化するため、
		// ここでは正規表現メタ文字を含意なく部分一致とする。
		return ContainsText(name, filter);
	}

	if (!chk_RegExPtn(filter)) return false;
	TRegExOptions options;
	options << roIgnoreCase;
	return TRegEx::IsMatch(name, filter, options);
}

//---------------------------------------------------------------------------
std::vector<Entry> BuildEntries(const history::HistoryList &history, const Context &context)
{
	std::vector<Entry> result;
	const std::vector<UnicodeString> &records = history.Entries();
	result.reserve(records.size());
	for (const UnicodeString &record : records) {
		const UnicodeString path = path_of(record);
		if (path.IsEmpty()) continue;
		if (!MatchesMode(path, context.current_path, context.mode)) continue;
		if (!MatchesFilter(path, context.filter, context.migemo)) continue;

		Entry entry;
		entry.path = path;
		entry.name = display_name(path);
		entry.location = ExtractFilePath(path);
		result.push_back(entry);
	}
	return result;
}

//---------------------------------------------------------------------------
UnicodeString StatusText(int shown, int total, int selected)
{
	UnicodeString text;
	text.sprintf(_T("表示: %d/%d  選択: %d"), shown, total, selected);
	return text;
}

//---------------------------------------------------------------------------
int RemoveEntry(history::HistoryList &history, const UnicodeString &path)
{
	if (path.IsEmpty()) return 0;

	const std::vector<UnicodeString> records = history.Entries();
	int removed = 0;
	for (const UnicodeString &record : records) {
		if (!SameText(path_of(record), path)) continue;
		const std::size_t before = history.Entries().size();
		history.Remove(record);
		const std::size_t after = history.Entries().size();
		removed += static_cast<int>(before - after);
	}
	return removed;
}

//---------------------------------------------------------------------------
int ApplyExcludedPaths(history::HistoryList &history, const UnicodeString &patterns)
{
	if (patterns.IsEmpty()) return 0;

	const TStringDynArray list = split_strings_semicolon(patterns);
	if (list.Length == 0) return 0;

	const std::vector<UnicodeString> records = history.Entries();
	int removed = 0;
	for (const UnicodeString &record : records) {
		const UnicodeString directory = ExtractFilePath(path_of(record));
		if (directory.IsEmpty()) continue;
		for (int i = 0; i < list.Length; ++i) {
			UnicodeString pattern = Trim(list[i]);
			if (pattern.IsEmpty()) continue;
			pattern = cv_env_str(pattern);
			if (pattern.IsEmpty()) continue;
			if (ContainsText(directory, pattern)) {
				removed += RemoveEntry(history, path_of(record));
				break;
			}
		}
	}
	return removed;
}

//---------------------------------------------------------------------------
Request ParseRequest(const UnicodeString &param)
{
	Request request;
	const TStringDynArray list = split_strings_semicolon(param);
	for (int i = 0; i < list.Length; ++i) {
		if (SameText(list[i], _T("FF"))) request.focus_filter = true;
		if (SameText(list[i], _T("AC"))) request.clear_all = true;
	}
	return request;
}

//---------------------------------------------------------------------------
bool CanDelete(int selected, int count)
{
	return count > 0 && selected >= 0 && selected < count;
}

//---------------------------------------------------------------------------
bool CanClearAll(int count)
{
	return count > 0;
}

//---------------------------------------------------------------------------
void LoadPreferences(UsrIniFile &ini, Preferences &preferences)
{
	preferences.mode = ModeFromIndex(ini.ReadIntGen(_T("EditHistOptMode"), 0));
	preferences.migemo = ini.ReadBoolGen(_T("EditHistMigemo"), false);
	preferences.status_bar = ini.ReadBoolGen(_T("EditHistSttBar"), true);
	preferences.filter_width = std::clamp(ini.ReadIntGen(_T("EditHistFilterWidth"), 200), 60, 1000);
	preferences.excluded_paths = ini.ReadString(SCT_Option, _T("NoEditHistPath"), EmptyStr);
}

//---------------------------------------------------------------------------
void SavePreferences(UsrIniFile &ini, const Preferences &preferences)
{
	ini.WriteIntGen(_T("EditHistOptMode"), ModeIndex(preferences.mode));
	ini.WriteBoolGen(_T("EditHistMigemo"), preferences.migemo);
	ini.WriteBoolGen(_T("EditHistSttBar"), preferences.status_bar);
	ini.WriteIntGen(_T("EditHistFilterWidth"), std::clamp(preferences.filter_width, 60, 1000));
	ini.WriteString(SCT_Option, _T("NoEditHistPath"), preferences.excluded_paths);
}

}  // namespace edit_hist
