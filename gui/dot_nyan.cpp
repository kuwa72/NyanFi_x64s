/**
 * @file gui/dot_nyan.cpp
 * @brief gui/dot_nyan.h の実装
 */
#include "gui/dot_nyan.h"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "usr_file_ex.h"

namespace dot_nyan {
namespace {

std::wstring to_w(const UnicodeString &value)
{
	return value.wstr();
}

UnicodeString to_u(const std::wstring &value)
{
	return UnicodeString(value.c_str(), static_cast<int>(value.size()));
}

bool is_space(wchar_t ch)
{
	return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == L'\f';
}

wchar_t lower(wchar_t ch)
{
	return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch - L'A' + L'a') : ch;
}

std::wstring lower_copy(std::wstring value)
{
	for (wchar_t &ch : value) ch = lower(ch);
	return value;
}

std::wstring trim_copy(const std::wstring &value)
{
	std::size_t first = 0;
	while (first < value.size() && is_space(value[first])) ++first;
	std::size_t last = value.size();
	while (last > first && is_space(value[last - 1])) --last;
	return value.substr(first, last - first);
}

int parse_int(const std::wstring &text, int fallback)
{
	if (text.empty()) return fallback;
	wchar_t *end = nullptr;
	const long value = std::wcstol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != L'\0' || value < std::numeric_limits<int>::min() ||
	    value > std::numeric_limits<int>::max()) {
		return fallback;
	}
	return static_cast<int>(value);
}

bool parse_bool(const std::wstring &text)
{
	const std::wstring value = lower_copy(trim_copy(text));
	return value == L"1" || value == L"true" || value == L"yes" || value == L"on";
}

int tri_state(const std::wstring &text)
{
	const int value = parse_int(text, -1);
	return value == 1 ? 1 : value == 0 ? 2 : 0;
}

int icon_state(const std::wstring &text)
{
	const int value = parse_int(text, -1);
	if (value == 1) return 1;
	if (value == 0) return 2;
	if (value == 2) return 3;
	return 0;
}

void append_line(std::wostringstream &out, const std::wstring &key, const std::wstring &value)
{
	out << key << L"=" << value << L"\r\n";
}

void append_bool(std::wostringstream &out, const wchar_t *key, bool value)
{
	append_line(out, key, value ? L"1" : L"0");
}

void append_int(std::wostringstream &out, const wchar_t *key, int value)
{
	append_line(out, key, std::to_wstring(value));
}

bool read_file_bytes(const UnicodeString &path, std::string &bytes, UnicodeString &error)
{
	HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		error = _T("設定ファイルを開けません");
		return false;
	}
	const DWORD size = ::GetFileSize(handle, nullptr);
	if (size == INVALID_FILE_SIZE) {
		::CloseHandle(handle);
		error = _T("設定ファイルのサイズを取得できません");
		return false;
	}
	bytes.assign(static_cast<std::size_t>(size), '\0');
	DWORD read = 0;
	if (size != 0 && (!::ReadFile(handle, bytes.data(), size, &read, nullptr) || read != size)) {
		::CloseHandle(handle);
		error = _T("設定ファイルを読み込めません");
		return false;
	}
	::CloseHandle(handle);
	return true;
}

std::wstring decode_bytes(const std::string &bytes, UnicodeString &error)
{
	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xff &&
	    static_cast<unsigned char>(bytes[1]) == 0xfe) {
		std::wstring result;
		result.reserve((bytes.size() - 2) / 2);
		for (std::size_t i = 2; i + 1 < bytes.size(); i += 2) {
			result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) |
			                                     (static_cast<unsigned char>(bytes[i + 1]) << 8)));
		}
		return result;
	}
	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xfe &&
	    static_cast<unsigned char>(bytes[1]) == 0xff) {
		std::wstring result;
		result.reserve((bytes.size() - 2) / 2);
		for (std::size_t i = 2; i + 1 < bytes.size(); i += 2) {
			result.push_back(static_cast<wchar_t>((static_cast<unsigned char>(bytes[i]) << 8) |
			                                     static_cast<unsigned char>(bytes[i + 1])));
		}
		return result;
	}
	UINT code_page = CP_UTF8;
	DWORD flags = MB_ERR_INVALID_CHARS;
	int length = ::MultiByteToWideChar(code_page, flags, bytes.data(),
	                                    static_cast<int>(bytes.size()), nullptr, 0);
	if (length <= 0 && !bytes.empty()) {
		code_page = CP_ACP;
		flags = 0;
		length = ::MultiByteToWideChar(code_page, flags, bytes.data(),
		                               static_cast<int>(bytes.size()), nullptr, 0);
	}
	if (length < 0 || (bytes.empty() && code_page != CP_UTF8)) {
		error = _T("設定ファイルの文字コードを判別できません");
		return std::wstring();
	}
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	if (length > 0) {
		::MultiByteToWideChar(code_page, flags, bytes.data(), static_cast<int>(bytes.size()),
		                       result.data(), length);
	}
	return result;
}

}  // namespace

Options DefaultOptions()
{
	Options options;
	options.no_order = true;
	return options;
}

Options NormalizeOptions(const Options &input)
{
	Options options = input;
	options.sort_mode = (options.sort_mode >= 0 && options.sort_mode <= 6) ? options.sort_mode : 0;
	options.show_hidden = (options.show_hidden >= 0 && options.show_hidden <= 2) ? options.show_hidden : 0;
	options.show_system = (options.show_system >= 0 && options.show_system <= 2) ? options.show_system : 0;
	options.show_byte_size = (options.show_byte_size >= 0 && options.show_byte_size <= 2) ? options.show_byte_size : 0;
	options.show_icon = (options.show_icon >= 0 && options.show_icon <= 3) ? options.show_icon : 0;
	options.sync_lr = (options.sync_lr >= 0 && options.sync_lr <= 2) ? options.sync_lr : 0;
	if (!options.exe_commands.IsEmpty() && options.exe_commands[1] == L'@') {
		options.exe_commands = options.exe_commands.SubString(2);
	}
	return options;
}

bool IsNoOrder(const Options &options)
{
	// no_order は「順序指定なし」チェックボックスそのものを表す。
	// キーが存在して全項目 0 の設定も、解析時には false になる。
	return options.no_order;
}

ParseResult ParseConfig(const UnicodeString &text)
{
	ParseResult result;
	result.options = DefaultOptions();
	std::map<std::wstring, std::wstring> values;
	const std::wstring source = to_w(text);
	std::wstring line;
	bool has_order_key = false;
	auto consume = [&](const std::wstring &raw) {
		const std::wstring item = trim_copy(raw);
		if (item.empty() || item[0] == L';' || item[0] == L'#') return;
		const std::size_t equal = item.find(L'=');
		if (equal == std::wstring::npos) return;
		const std::wstring key = lower_copy(trim_copy(item.substr(0, equal)));
		if (key.empty()) return;
		values[key] = trim_copy(item.substr(equal + 1));
		if (key == L"naturalorder" || key == L"dscnameorder" || key == L"smallorder" ||
		    key == L"oldorder" || key == L"dscattrorder") {
			has_order_key = true;
		}
	};
	for (std::size_t i = 0; i <= source.size(); ++i) {
		if (i == source.size() || source[i] == L'\r' || source[i] == L'\n') {
			if (i < source.size() && source[i] == L'\r' && i + 1 < source.size() && source[i + 1] == L'\n') {
				++i;
			}
			consume(line);
			line.clear();
		} else {
			line.push_back(source[i]);
		}
	}

	Options &options = result.options;
	const auto value = [&](const wchar_t *key) -> const std::wstring & {
		static const std::wstring empty;
		const auto it = values.find(lower_copy(std::wstring(key)));
		return it == values.end() ? empty : it->second;
	};
	const std::wstring sort = value(_T("SortMode"));
	if (!sort.empty()) {
		const std::wstring ids = L"fedsau";
		const std::size_t pos = ids.find(lower(sort[0]));
		if (pos != std::wstring::npos) options.sort_mode = static_cast<int>(pos) + 1;
	}
	options.no_order = !has_order_key;
	options.natural_order = parse_bool(value(_T("NaturalOrder")));
	options.dsc_name_order = parse_bool(value(_T("DscNameOrder")));
	options.small_order = parse_bool(value(_T("SmallOrder")));
	options.old_order = parse_bool(value(_T("OldOrder")));
	options.dsc_attr_order = parse_bool(value(_T("DscAttrOrder")));
	options.show_hidden = tri_state(value(_T("ShowHideAtr")));
	options.show_system = tri_state(value(_T("ShowSystemAtr")));
	options.show_byte_size = tri_state(value(_T("ShowByteSize")));
	options.show_icon = icon_state(value(_T("ShowIcon")));
	options.sync_lr = tri_state(value(_T("SyncLR")));
	options.path_mask = to_u(value(_T("PathMask")));
	options.grep_mask = to_u(value(_T("GrepMask")));
	options.list_width = to_u(value(_T("ListWidth")));
	options.play_sound = to_u(value(_T("PlaySound")));
	options.bg_image = to_u(value(_T("BgImage")));
	options.description = to_u(value(_T("Description")));
	options.exe_commands = to_u(value(_T("ExeCommands")));
	if (!options.exe_commands.IsEmpty() && options.exe_commands[1] == L'@') {
		options.exe_commands = options.exe_commands.SubString(2);
	}
	options.handled = parse_bool(value(_T("Handled")));
	for (std::size_t i = 0; i < kColorNames.size(); ++i) options.colors[i] = to_u(value(kColorNames[i]));
	options = NormalizeOptions(options);
	result.ok = true;
	return result;
}

UnicodeString SerializeConfig(const Options &input)
{
	const Options options = NormalizeOptions(input);
	std::wostringstream out;
	out.imbue(std::locale::classic());
	const std::wstring ids = L"FEDSAU";
	if (options.sort_mode > 0 && options.sort_mode <= static_cast<int>(ids.size())) {
		append_line(out, L"SortMode", std::wstring(1, ids[static_cast<std::size_t>(options.sort_mode - 1)]));
	}
	if (!IsNoOrder(options)) {
		append_bool(out, L"NaturalOrder", options.natural_order);
		append_bool(out, L"DscNameOrder", options.dsc_name_order);
		append_bool(out, L"SmallOrder", options.small_order);
		append_bool(out, L"OldOrder", options.old_order);
		append_bool(out, L"DscAttrOrder", options.dsc_attr_order);
	}
	if (options.show_hidden > 0) append_int(out, L"ShowHideAtr", options.show_hidden == 1 ? 1 : 0);
	if (options.show_system > 0) append_int(out, L"ShowSystemAtr", options.show_system == 1 ? 1 : 0);
	if (options.show_byte_size > 0) append_int(out, L"ShowByteSize", options.show_byte_size == 1 ? 1 : 0);
	if (options.show_icon > 0) {
		const int value = options.show_icon == 1 ? 1 : options.show_icon == 2 ? 0 : 2;
		append_int(out, L"ShowIcon", value);
	}
	if (options.sync_lr > 0) append_int(out, L"SyncLR", options.sync_lr == 1 ? 1 : 0);
	if (!options.path_mask.IsEmpty()) append_line(out, L"PathMask", to_w(options.path_mask));
	if (!options.grep_mask.IsEmpty()) append_line(out, L"GrepMask", to_w(options.grep_mask));
	if (!options.list_width.IsEmpty()) append_line(out, L"ListWidth", to_w(options.list_width));
	if (!options.play_sound.IsEmpty()) append_line(out, L"PlaySound", to_w(options.play_sound));
	if (!options.bg_image.IsEmpty()) append_line(out, L"BgImage", to_w(options.bg_image));
	if (!options.description.IsEmpty()) append_line(out, L"Description", to_w(options.description));
	if (!options.exe_commands.IsEmpty()) {
		append_line(out, L"ExeCommands", L"@" + to_w(options.exe_commands));
	}
	if (options.handled) append_line(out, L"Handled", L"1");
	for (std::size_t i = 0; i < kColorNames.size(); ++i) {
		if (!options.colors[i].IsEmpty()) append_line(out, kColorNames[i], to_w(options.colors[i]));
	}
	return to_u(out.str());
}

Options MergeInherited(const Options &child, const Options &inherited)
{
	Options result = child;
	if (result.sort_mode == 0) result.sort_mode = inherited.sort_mode;
	if (result.show_hidden == 0) result.show_hidden = inherited.show_hidden;
	if (result.show_system == 0) result.show_system = inherited.show_system;
	if (result.show_byte_size == 0) result.show_byte_size = inherited.show_byte_size;
	if (result.show_icon == 0) result.show_icon = inherited.show_icon;
	if (result.sync_lr == 0) result.sync_lr = inherited.sync_lr;
	if (result.path_mask.IsEmpty()) result.path_mask = inherited.path_mask;
	if (result.grep_mask.IsEmpty()) result.grep_mask = inherited.grep_mask;
	if (result.list_width.IsEmpty()) result.list_width = inherited.list_width;
	if (result.play_sound.IsEmpty()) result.play_sound = inherited.play_sound;
	if (result.bg_image.IsEmpty()) result.bg_image = inherited.bg_image;
	if (result.description.IsEmpty()) result.description = inherited.description;
	if (result.exe_commands.IsEmpty()) result.exe_commands = inherited.exe_commands;
	if (!result.handled) result.handled = inherited.handled;
	for (std::size_t i = 0; i < result.colors.size(); ++i) {
		if (result.colors[i].IsEmpty()) result.colors[i] = inherited.colors[i];
	}
	return NormalizeOptions(result);
}

UnicodeString ConfigName(const UnicodeString &directory)
{
	const std::wstring path = to_w(directory);
	if (path.empty()) return _T(".nyanfi");
	if (path.back() == L'\\' || path.back() == L'/') return to_u(path + L".nyanfi");
	return to_u(path + L"\\.nyanfi");
}

bool SaveConfig(const UnicodeString &path, const Options &options, UnicodeString &error)
{
	const std::wstring text = to_w(SerializeConfig(options));
	const int length = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
	                                         nullptr, 0, nullptr, nullptr);
	if (length < 0) {
		error = _T("UTF-8 に変換できません");
		return false;
	}
	std::string bytes(static_cast<std::size_t>(length), '\0');
	if (length > 0) {
		::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(),
		                       length, nullptr, nullptr);
	}
	HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
	                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		error = _T("設定ファイルを保存できません");
		return false;
	}
	DWORD written = 0;
	const bool ok = length == 0 || (::WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()),
	                                           &written, nullptr) && written == bytes.size());
	::CloseHandle(handle);
	if (!ok) {
		error = _T("設定ファイルを保存できません");
		return false;
	}
	error = EmptyStr;
	return true;
}

bool LoadConfig(const UnicodeString &path, Options &options, UnicodeString &error)
{
	std::string bytes;
	if (!read_file_bytes(path, bytes, error)) return false;
	const std::wstring text = decode_bytes(bytes, error);
	if (!error.IsEmpty()) return false;
	const ParseResult parsed = ParseConfig(to_u(text));
	if (!parsed.ok) {
		error = parsed.error.IsEmpty() ? _T("設定ファイルを解析できません") : parsed.error;
		return false;
	}
	options = parsed.options;
	error = EmptyStr;
	return true;
}

bool DeleteConfig(const UnicodeString &path)
{
	return ::DeleteFileW(path.c_str()) != FALSE;
}

}  // namespace dot_nyan
