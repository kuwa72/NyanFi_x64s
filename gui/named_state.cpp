/**
 * @file gui/named_state.cpp
 * @brief gui/named_state.h の実装
 */
#include "gui/named_state.h"

#include <memory>

#include "UIniFile.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace named_state {

namespace {

//---------------------------------------------------------------------------
// 結果リスト・タブグループの生バイト列とのやりとり (gui/work_list.cpp と同じ手法。
// gui/ 配下の各ファイルはこの手のヘルパーをそれぞれ自前で持つのが既存の流儀
// (gui/convert_ops.cpp / gui/text_ops.cpp / gui/file_ops2.cpp も同様))
//---------------------------------------------------------------------------

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

/// UTF-8 (BOM 付き) のバイト列を行に分ける (CRLF / LF / CR のどれでも)。BOM は落とす
std::vector<UnicodeString> split_utf8_lines(const std::string &bytes)
{
	std::size_t pos = 0;
	if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
	    && static_cast<unsigned char>(bytes[1]) == 0xBB
	    && static_cast<unsigned char>(bytes[2]) == 0xBF) {
		pos = 3;
	}

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
	return lines;
}

//---------------------------------------------------------------------------
// タブグループの ini 書式で使う小さな変換 (gui/tabs.cpp の同名関数と同じ考え方。
// TabManager の private な tabs_ には触れないので、ここで独立に持つ)
//---------------------------------------------------------------------------

/// このモジュール専用のセクション名。gui/tabs.cpp の "WxGuiTabs" (アプリ再起動時の
/// セッション復元) とは別物。こちらはユーザーが名前を付けて保存する共有可能なファイル
const UnicodeString kTabGroupSection = _T("TabGroup");

int sort_key_to_int(SortKey key)
{
	return static_cast<int>(key);
}

SortKey int_to_sort_key(int v)
{
	switch (v) {
	case static_cast<int>(SortKey::Ext):  return SortKey::Ext;
	case static_cast<int>(SortKey::Date): return SortKey::Date;
	case static_cast<int>(SortKey::Size): return SortKey::Size;
	case static_cast<int>(SortKey::Attr): return SortKey::Attr;
	default: return SortKey::Name;
	}
}

}  // namespace

//===========================================================================
// 共通
//===========================================================================
UnicodeString ExtensionOf(Kind kind)
{
	switch (kind) {
	case Kind::TabGroup:   return _T(".ini");
	case Kind::ResultList: return _T(".txt");
	case Kind::FindSet:    return _T(".ini");
	}
	return _T(".ini");
}

//===========================================================================
// タブグループ
//===========================================================================
void WriteTabGroupIni(UsrIniFile &ini, const std::vector<TabState> &tabs, int current)
{
	ini.EraseSection(kTabGroupSection);
	ini.WriteInteger(kTabGroupSection, _T("Count"), static_cast<int>(tabs.size()));
	ini.WriteInteger(kTabGroupSection, _T("Current"), current);

	for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
		const TabState &tab = tabs[static_cast<std::size_t>(i)];
		UnicodeString prefix;
		prefix.sprintf(_T("Tab%02d_"), i);

		for (int p = 0; p < 2; ++p) {
			const PaneTabState &pane = tab.panes[p];
			UnicodeString key;

			ini.WriteString(kTabGroupSection, key.sprintf(_T("%sDir%d"), prefix.c_str(), p), pane.directory);
			ini.WriteInteger(kTabGroupSection, key.sprintf(_T("%sSortKey%d"), prefix.c_str(), p),
			                  sort_key_to_int(pane.sort_key));
			ini.WriteBool(kTabGroupSection, key.sprintf(_T("%sSortDesc%d"), prefix.c_str(), p),
			              pane.sort_descending);
			ini.WriteBool(kTabGroupSection, key.sprintf(_T("%sDirsFirst%d"), prefix.c_str(), p),
			              pane.dirs_first);
		}
	}
}

//---------------------------------------------------------------------------
bool ReadTabGroupIni(UsrIniFile &ini, std::vector<TabState> &tabs_out, int &current_out)
{
	const int count = ini.ReadInteger(kTabGroupSection, _T("Count"), 0);
	if (count <= 0) return false;

	std::vector<TabState> loaded;
	loaded.reserve(static_cast<std::size_t>(count));

	for (int i = 0; i < count; ++i) {
		TabState tab;
		UnicodeString prefix;
		prefix.sprintf(_T("Tab%02d_"), i);

		for (int p = 0; p < 2; ++p) {
			PaneTabState &pane = tab.panes[p];
			UnicodeString key;

			pane.directory = ini.ReadString(kTabGroupSection, key.sprintf(_T("%sDir%d"), prefix.c_str(), p),
			                                 EmptyStr);
			pane.sort_key = int_to_sort_key(
				ini.ReadInteger(kTabGroupSection, key.sprintf(_T("%sSortKey%d"), prefix.c_str(), p), 0));
			pane.sort_descending =
				ini.ReadBool(kTabGroupSection, key.sprintf(_T("%sSortDesc%d"), prefix.c_str(), p), false);
			pane.dirs_first =
				ini.ReadBool(kTabGroupSection, key.sprintf(_T("%sDirsFirst%d"), prefix.c_str(), p), true);
		}
		loaded.push_back(tab);
	}
	if (loaded.empty()) return false;

	tabs_out = std::move(loaded);
	const int saved_current = ini.ReadInteger(kTabGroupSection, _T("Current"), 0);
	current_out = (saved_current >= 0 && saved_current < static_cast<int>(tabs_out.size())) ? saved_current : 0;
	return true;
}

//---------------------------------------------------------------------------
bool SaveTabGroup(const UnicodeString &path, const std::vector<TabState> &tabs, int current,
                   UnicodeString &error_out)
{
	if (path.IsEmpty()) {
		error_out = _T("保存先が指定されていません");
		return false;
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(path));
	WriteTabGroupIni(*ini, tabs, current);
	if (!ini->UpdateFile(true)) {
		error_out = _T("書き込めません");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool LoadTabGroup(const UnicodeString &path, std::vector<TabState> &tabs_out, int &current_out,
                   UnicodeString &error_out)
{
	const UnicodeString full = to_absolute_name(path);
	if (!file_exists(full)) {
		error_out = _T("ファイルがありません");
		return false;
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(full));
	if (!ReadTabGroupIni(*ini, tabs_out, current_out)) {
		error_out = _T("有効なタブがありません");
		return false;
	}
	return true;
}

//===========================================================================
// 結果リスト
//===========================================================================
std::vector<FileItem> ParseResultListLines(const std::vector<UnicodeString> &lines, UnicodeString &title_out)
{
	title_out = EmptyStr;
	std::vector<FileItem> items;

	// 先頭行が ";[ResultList]" でなければ不正な形式 (VCL の USTR_IllegalFormat と同じ判定)
	if (lines.empty() || !SameText(lines[0], _T(";[ResultList]"))) return items;

	static const UnicodeString kFindPathPrefix = _T("Find_Path=");

	for (std::size_t i = 0; i < lines.size(); ++i) {
		UnicodeString lbuf = lines[i];
		if (lbuf.IsEmpty()) continue;

		// 検索情報 (";" で始まる行。先頭の ";[ResultList]" もここを通る)
		if (StartsStr(_T(";"), lbuf)) {
			UnicodeString rest = lbuf;
			rest.Delete(1, 1);  // 先頭の ';' を落とす (VCL の remove_top_s と同じ)
			if (rest.Pos(kFindPathPrefix) == 1) {
				title_out = rest.SubString(kFindPathPrefix.Length() + 1,
				                            rest.Length() - kFindPathPrefix.Length());
			}
			continue;
		}

		// 項目 (.nwl と同じ「パス <TAB> 別名」書式)
		UnicodeString lbuf2 = lbuf;
		const UnicodeString fnam = split_tkn(lbuf2, _T("\t"));
		const UnicodeString anam = lbuf2;
		if (fnam.IsEmpty() && anam.IsEmpty()) continue;

		if (fnam.IsEmpty() && is_separator(anam)) {
			FileItem sep;
			sep.is_separator = true;
			sep.alias        = anam;
			items.push_back(sep);
			continue;
		}
		// パスが空で別名だけの行は VCL も落とす
		if (fnam.IsEmpty()) continue;

		FileItem itm;
		itm.is_dir    = ends_PathDlmtr(fnam);
		itm.full_path = itm.is_dir ? ExcludeTrailingPathDelimiter(fnam) : fnam;
		itm.name      = ExtractFileName(itm.full_path);
		itm.alias     = anam;
		items.push_back(itm);
	}
	return items;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> FormatResultListLines(const UnicodeString &title, const std::vector<FileItem> &items)
{
	std::vector<UnicodeString> lines;
	lines.push_back(_T(";[ResultList]"));

	UnicodeString meta;
	meta.sprintf(_T(";Find_Path=%s"), title.c_str());
	lines.push_back(meta);

	for (std::size_t i = 0; i < items.size(); ++i) {
		const FileItem &itm = items[i];
		if (itm.is_parent) continue;  // VCL の fp->is_up 除外と同じ

		if (itm.is_separator) {
			lines.push_back(_T("\t-"));
			continue;
		}

		const UnicodeString path = itm.full_path.IsEmpty() ? itm.name : itm.full_path;
		if (path.IsEmpty() && itm.alias.IsEmpty()) continue;

		UnicodeString lbuf = (itm.is_dir && !path.IsEmpty()) ? IncludeTrailingPathDelimiter(path) : path;
		lbuf += _T("\t");
		lbuf += itm.alias;
		lines.push_back(lbuf);
	}
	return lines;
}

//---------------------------------------------------------------------------
bool SaveResultList(const UnicodeString &path, const UnicodeString &title, const std::vector<FileItem> &items,
                     UnicodeString &error_out)
{
	if (path.IsEmpty()) {
		error_out = _T("保存先が指定されていません");
		return false;
	}

	const std::vector<UnicodeString> lines = FormatResultListLines(title, items);

	std::string bytes = "\xEF\xBB\xBF";  // BOM (saveto_TextUTF8 相当。VCL 版が読めるように必ず付ける)
	for (std::size_t i = 0; i < lines.size(); ++i) {
		bytes += to_utf8(lines[i]);
		bytes += "\r\n";
	}
	return write_all(path, bytes, error_out);
}

//---------------------------------------------------------------------------
bool LoadResultList(const UnicodeString &path, UnicodeString &title_out, std::vector<FileItem> &items_out,
                     UnicodeString &error_out)
{
	items_out.clear();
	title_out = EmptyStr;

	const UnicodeString full = to_absolute_name(path);
	if (!file_exists(full)) {
		error_out = _T("ファイルがありません");
		return false;
	}

	std::string bytes;
	if (!read_all(full, bytes, error_out)) return false;

	const std::vector<UnicodeString> lines = split_utf8_lines(bytes);
	if (lines.empty() || !SameText(lines[0], _T(";[ResultList]"))) {
		error_out = _T("結果リストの形式ではありません");
		return false;
	}

	const std::vector<FileItem> parsed = ParseResultListLines(lines, title_out);

	// VCL と同じく、実体が見つからない項目は黙って読み飛ばす
	// (MainFrm.cpp:21485-21489。gui/work_list.h の「missing のまま残す」方針とは違う。
	// UNC の可用性チェック (is_InvalidUnc) は Global.cpp 全体を巻き込むため実装していない。
	// ここでは存在確認だけで判定する。未検証)
	for (std::size_t i = 0; i < parsed.size(); ++i) {
		const FileItem &itm = parsed[i];
		if (itm.is_separator) {
			items_out.push_back(itm);
			continue;
		}
		const bool exists = itm.is_dir ? dir_exists(itm.full_path) : file_exists(itm.full_path);
		if (!exists) continue;

		FileItem filled = itm;
		filled.attr  = file_GetAttr(filled.full_path);
		filled.stamp = get_file_age(filled.full_path);
		filled.size  = filled.is_dir ? -1 : get_file_size(filled.full_path);
		items_out.push_back(filled);
	}
	return true;
}

//===========================================================================
// 検索設定
//===========================================================================
namespace {
const UnicodeString kFindSettingsSection = _T("FindSettings");
}

void WriteFindSetIni(UsrIniFile &ini, const FindSet &set)
{
	ini.EraseSection(kFindSettingsSection);
	const UnicodeString &sct = kFindSettingsSection;

	ini.WriteBool(sct, _T("PathSort"), set.path_sort);
	ini.WriteInteger(sct, _T("SortMode"), set.sort_mode);
	ini.WriteBool(sct, _T("ResLink"), set.res_link);
	ini.WriteBool(sct, _T("DirLink"), set.dir_link);

	if (set.is_tag) {
		ini.WriteString(sct, _T("FindType"), _T("TAG"));
		ini.WriteBool(sct, _T("TAG_all"), set.tag_all);
		ini.WriteString(sct, _T("Path"), set.path);
		ini.WriteString(sct, _T("Keywd"), set.keywd);
		ini.WriteBool(sct, _T("And"), set.match_and);
	}
	else if (set.is_mark) {
		ini.WriteString(sct, _T("FindType"), _T("MARK"));
		ini.WriteString(sct, _T("Path"), set.path);
	}
	else if (set.is_dup_icon) {
		ini.WriteString(sct, _T("FindType"), _T("DICON"));
		// ini の値に生の "\r\n" は書けないので "/" に変換する (VCL の save_FindSettings と同じ)
		ini.WriteString(sct, _T("Icons"), ReplaceStr(set.icons, _T("\r\n"), _T("/")));
	}
	else if (set.is_hard_link) {
		ini.WriteString(sct, _T("FindType"), _T("HLINK"));
		ini.WriteString(sct, _T("Name"), set.link_name);
	}
	else {
		ini.WriteString(sct, _T("Path"), set.path);
		ini.WriteString(sct, _T("DirList"), set.dir_list);
		ini.WriteString(sct, _T("SkipDir"), set.skip_dir);
		ini.WriteBool(sct, _T("Dir"), set.target_dir);
		ini.WriteBool(sct, _T("Both"), set.target_both);
		ini.WriteBool(sct, _T("SubDir"), set.sub_dir);
		ini.WriteBool(sct, _T("Arc"), set.include_arc);
		ini.WriteBool(sct, _T("xTrash"), set.exclude_trash);
		ini.WriteString(sct, _T("Mask"), set.mask);

		if (!set.keywd.IsEmpty()) {
			ini.WriteString(sct, _T("Keywd"), set.keywd);
			ini.WriteBool(sct, _T("RegEx"), set.reg_ex);
			ini.WriteBool(sct, _T("And"), set.match_and);
			ini.WriteBool(sct, _T("Case"), set.match_case);
		}

		if (set.dt_mode > 0) {
			ini.WriteInteger(sct, _T("DT_mode"), set.dt_mode);
			if (set.dt_rel != 0) {
				ini.WriteInteger(sct, _T("DT_rel"), set.dt_rel);
			}
			else {
				ini.WriteString(sct, _T("DT_value"), FormatDateTime(_T("yyyy'/'mm'/'dd hh:nn:ss"), set.dt_value));
				ini.WriteString(sct, _T("DT_str"), set.dt_str);
			}
		}

		if (set.sz_mode > 0) {
			ini.WriteInteger(sct, _T("SZ_mode"), set.sz_mode);
			ini.WriteInt64(sct, _T("SZ_value"), set.sz_value);
		}

		if (set.at_mode > 0) {
			ini.WriteInteger(sct, _T("AT_mode"), set.at_mode);
			ini.WriteInteger(sct, _T("AT_value"), set.at_value);
		}
	}
}

//---------------------------------------------------------------------------
bool ReadFindSetIni(UsrIniFile &ini, FindSet &out)
{
	const UnicodeString &sct = kFindSettingsSection;
	if (!ini.SectionExists(sct)) return false;

	out = FindSet();  // 既定値に戻す

	const UnicodeString find_typ = ini.ReadString(sct, _T("FindType"));
	out.is_tag = SameText(find_typ, _T("TAG"));
	if (out.is_tag) out.tag_all = ini.ReadBool(sct, _T("TAG_all"));

	out.is_mark = SameText(find_typ, _T("MARK"));

	out.is_dup_icon = SameText(find_typ, _T("DICON"));
	if (out.is_dup_icon) out.icons = ReplaceStr(ini.ReadString(sct, _T("Icons")), _T("/"), _T("\r\n"));

	out.is_hard_link = SameText(find_typ, _T("HLINK"));
	if (out.is_hard_link) out.link_name = ini.ReadString(sct, _T("Name"));

	out.path_sort = ini.ReadBool(sct, _T("PathSort"));
	out.sort_mode = ini.ReadInteger(sct, _T("SortMode"), -1);
	out.res_link  = ini.ReadBool(sct, _T("ResLink"));
	out.dir_link  = ini.ReadBool(sct, _T("DirLink"));

	out.path     = ini.ReadString(sct, _T("Path"));
	out.dir_list = ini.ReadString(sct, _T("DirList"));
	out.skip_dir = ini.ReadString(sct, _T("SkipDir"));

	out.target_dir    = ini.ReadBool(sct, _T("Dir"));
	out.target_both   = ini.ReadBool(sct, _T("Both"));
	out.sub_dir       = ini.ReadBool(sct, _T("SubDir"));
	out.include_arc   = ini.ReadBool(sct, _T("Arc"));
	out.exclude_trash = ini.ReadBool(sct, _T("xTrash"));
	out.mask          = ini.ReadString(sct, _T("Mask"));
	out.keywd         = ini.ReadString(sct, _T("Keywd"));
	out.reg_ex        = ini.ReadBool(sct, _T("RegEx"));
	out.match_and     = ini.ReadBool(sct, _T("And"));
	out.match_case    = ini.ReadBool(sct, _T("Case"));

	out.dt_mode = ini.ReadInteger(sct, _T("DT_mode"), 0);
	if (out.dt_mode > 0) {
		out.dt_rel = ini.ReadInteger(sct, _T("DT_rel"), 0);
		if (out.dt_rel != 0) {
			out.dt_value = IncDay(Today(), out.dt_rel);
		}
		else {
			out.dt_str = ini.ReadString(sct, _T("DT_str"));
			try {
				out.dt_value = str_to_DateTime(ini.ReadString(sct, _T("DT_value")));
			}
			catch (...) {
				out.dt_value = 0;
			}
		}
	}

	out.sz_mode = ini.ReadInteger(sct, _T("SZ_mode"), 0);
	if (out.sz_mode > 0) out.sz_value = ini.ReadInt64(sct, _T("SZ_value"), 0);

	out.at_mode = ini.ReadInteger(sct, _T("AT_mode"), 0);
	if (out.at_mode > 0) out.at_value = ini.ReadInteger(sct, _T("AT_value"), 0);

	return true;
}

//---------------------------------------------------------------------------
bool SaveFindSet(const UnicodeString &path, const FindSet &set, UnicodeString &error_out)
{
	if (path.IsEmpty()) {
		error_out = _T("保存先が指定されていません");
		return false;
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(path));
	WriteFindSetIni(*ini, set);
	if (!ini->UpdateFile(true)) {
		error_out = _T("書き込めません");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool LoadFindSet(const UnicodeString &path, FindSet &out, UnicodeString &error_out)
{
	const UnicodeString full = to_absolute_name(path);
	if (!file_exists(full)) {
		error_out = _T("ファイルがありません");
		return false;
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(full));
	if (!ReadFindSetIni(*ini, out)) {
		error_out = _T("検索設定が見つかりません");
		return false;
	}
	return true;
}

}  // namespace named_state
