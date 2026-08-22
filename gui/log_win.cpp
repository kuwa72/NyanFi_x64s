/**
 * @file gui/log_win.cpp
 * @brief ログの蓄積・整形・書き出しの実装
 *
 * @details 設計・実測の根拠は gui/log_win.h の冒頭コメントを参照。
 */
#include "gui/log_win.h"

#include <string>

#include "gui/file_info.h"
#include "gui/file_item.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace log_win {

namespace {

/// UnicodeString を UTF-8 のバイト列にする (gui/work_list.cpp と同じ手法)
std::string ToUtf8(const UnicodeString &s)
{
	if (s.IsEmpty()) return std::string();
	const int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), NULL, 0, NULL, NULL);
	if (n <= 0) return std::string();
	std::string out(static_cast<std::size_t>(n), '\0');
	::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), &out[0], n, NULL, NULL);
	return out;
}

/// バイト列をファイルへ書く
bool WriteAllBytes(const UnicodeString &path, const std::string &bytes, UnicodeString &error_out)
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

}  // namespace

//---------------------------------------------------------------------------
wchar_t StatusChar(LogStatus status)
{
	switch (status) {
	case LogStatus::Overwrite: return L'O';
	case LogStatus::Newer:     return L'N';
	case LogStatus::Skipped:   return L'S';
	case LogStatus::Warning:   return L'W';
	case LogStatus::Error:     return L'E';
	case LogStatus::Canceled:  return L'C';
	case LogStatus::Info:
	default:                  return L' ';
	}
}

//---------------------------------------------------------------------------
LogBuffer::LogBuffer(int max_lines)
	: max_lines_(max_lines)
{
}

//---------------------------------------------------------------------------
void LogBuffer::Push(LogLine line)
{
	lines_.push_back(line);
	Trim();
}

//---------------------------------------------------------------------------
void LogBuffer::Trim()
{
	if (max_lines_ <= 0) return;
	while (static_cast<int>(lines_.size()) > max_lines_) lines_.erase(lines_.begin());
}

//---------------------------------------------------------------------------
void LogBuffer::Add(LogStatus status, const UnicodeString &text, bool show_time)
{
	LogLine line;
	line.status    = status;
	line.stamp     = Now();
	line.text      = text;
	line.show_time = show_time;
	Push(line);
}

//---------------------------------------------------------------------------
void LogBuffer::AddRaw(const UnicodeString &text)
{
	// AddLogStrings (Global.cpp:14618) は TStringList::Text の代入で
	// 改行区切りに分割してから1行ずつ追加する。ここも同じ挙動にする
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Text = text;
	for (int i = 0; i < lst->Count; i++) {
		LogLine line;
		line.stamp = Now();
		line.text  = lst->Strings[i];
		line.raw   = true;
		Push(line);
	}
}

//---------------------------------------------------------------------------
void LogBuffer::AddBlankIfNeeded()
{
	// AddLogCr (Global.cpp:14624): 最後の行が空でなければ空行を足す
	if (!lines_.empty() && lines_.back().text.IsEmpty()) return;
	if (lines_.empty()) return;  // VCL は Count>0 のときだけ判定する (空リストには足さない)

	LogLine blank;
	blank.stamp = Now();
	blank.raw   = true;
	Push(blank);
}

//---------------------------------------------------------------------------
void LogBuffer::StartGroup(const UnicodeString &text, int task_no)
{
	// StartLog (Global.cpp:14503)
	if (text.IsEmpty()) {
		LogLine blank;
		blank.stamp = Now();
		blank.raw   = true;
		Push(blank);
		return;
	}

	// 直前が空行でなく、かつ「開始行」でもなければ空行を1つ挟む。
	// VCL は文字列パターン (時刻位置 + "開始" を含むか) で判定しているが、
	// ここは構造体に is_start を持っているので直接それを見る
	// (log_win.h 冒頭コメントの「推測ではなく設計上の単純化」を参照)
	if (!lines_.empty()) {
		const LogLine &last = lines_.back();
		if (!last.text.IsEmpty() && !last.is_start) {
			LogLine blank;
			blank.stamp = Now();
			blank.raw   = true;
			Push(blank);
		}
	}

	LogLine line;
	line.stamp     = Now();
	line.text      = text;
	line.show_time = true;
	line.is_start  = true;
	line.task_no   = task_no;
	Push(line);
}

//---------------------------------------------------------------------------
void LogBuffer::EndGroup(const UnicodeString &text, const UnicodeString &result_summary)
{
	// EndLog (Global.cpp:14534): 本文 + "終了" + 結果カウント、時刻表示あり。
	// 「圧縮/解凍」のバルーン通知 (NotifyPrimNyan) は wx 側の話なのでここには無い
	UnicodeString msg = text;
	msg += _T("終了");
	msg += result_summary;
	Add(LogStatus::Info, msg, /*show_time=*/true);
}

//---------------------------------------------------------------------------
void LogBuffer::Clear()
{
	lines_.clear();
}

//---------------------------------------------------------------------------
int LogBuffer::CountOf(LogStatus status) const
{
	int cnt = 0;
	for (std::size_t i = 0; i < lines_.size(); i++) if (lines_[i].status == status) cnt++;
	return cnt;
}

//---------------------------------------------------------------------------
UnicodeString FormatLine(const LogLine &line)
{
	if (line.raw) return line.text;
	if (line.text.IsEmpty()) return EmptyStr;

	UnicodeString time_str = line.show_time ? FormatDateTime(_T("hh:nn:ss "), line.stamp) : UnicodeString(EmptyStr);

	if (line.is_start) {
		UnicodeString prefix;
		if (line.task_no >= 0) prefix.sprintf(_T("%u>"), line.task_no + 1); else prefix = _T(">>");
		return prefix + time_str + ReplaceStr(line.text, _T("\t"), _T(" ---> "));
	}

	UnicodeString body = line.text;
	if (line.status != LogStatus::Info) {
		UnicodeString ch; ch.sprintf(_T("%c"), StatusChar(line.status));
		body = ch + body;
	}
	return UnicodeString(_T(" >")) + time_str + body;
}

//---------------------------------------------------------------------------
UnicodeString MakeLogHeader(const UnicodeString &cmd, const UnicodeString &name,
                             bool is_dir, bool full_path, int width)
{
	// FTP等 '/' 区切りのパスは一旦 '\' に寄せてから整形し、戻す (make_LogHdr と同じ)
	bool slash = StartsStr(_T("/"), name);
	UnicodeString nm = slash ? slash_to_yen(name) : name;

	UnicodeString lnam = is_dir
		? (UnicodeString(_T("[")) + (full_path ? ExcludeTrailingPathDelimiter(nm) : get_dir_name(nm)) + _T("]"))
		: (full_path ? warn_pathname_RLO(nm) : warn_filename_RLO(nm));

	if (slash) lnam = yen_to_slash(lnam);

	UnicodeString msg;
	msg.sprintf(_T(" %6s "), cmd.c_str());
	if (width > 0) msg.cat_sprintf(_T("%-*s"), width, lnam.c_str()); else msg += lnam;
	return msg;
}

//---------------------------------------------------------------------------
UnicodeString FormatResultCount(int ok_cnt, int er_cnt, int sk_cnt, int ng_cnt)
{
	// get_ResCntStr (Global.cpp:14563)。出力順は OK, NG, ERR, SKIP (VCLのまま。
	// 引数の並び (ok, er, sk, ng) とは順序が違うが、直さず踏襲する)
	UnicodeString ret_str;
	if (ok_cnt > 0) ret_str.cat_sprintf(_T("  OK:%u"),   ok_cnt);
	if (ng_cnt > 0) ret_str.cat_sprintf(_T("  NG:%u"),   ng_cnt);
	if (er_cnt > 0) ret_str.cat_sprintf(_T("  ERR:%u"),  er_cnt);
	if (sk_cnt > 0) ret_str.cat_sprintf(_T("  SKIP:%u"), sk_cnt);
	return ret_str;
}

//---------------------------------------------------------------------------
bool SaveTo(const UnicodeString &path, const std::vector<LogLine> &lines, UnicodeString &error_out)
{
	if (path.IsEmpty()) {
		error_out = _T("保存先が指定されていません");
		return false;
	}

	std::string bytes = "\xEF\xBB\xBF";  // BOM (VCL 側の TEncoding::UTF8 読み込みに合わせる)
	for (std::size_t i = 0; i < lines.size(); i++) {
		bytes += ToUtf8(FormatLine(lines[i]));
		bytes += "\r\n";
	}
	return WriteAllBytes(path, bytes, error_out);
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> FormatFileInfo(const UnicodeString &path)
{
	// LogFileInfoCore (MainFrm.cpp:21724) 相当。アーカイブ内ファイルの
	// arc_DspPath 付加は未移植のため対象外 (log_win.h 冒頭コメント参照)
	std::vector<UnicodeString> out;

	TSearchRec sr;
	if (FindFirst(path, faAnyFile, sr) != 0) {
		out.push_back(_T("ファイルが見つかりません: ") + path);
		return out;
	}

	FileItem item;
	item.name   = ExtractFileName(path);
	item.is_dir = (sr.Attr & faDirectory) != 0;
	item.attr   = sr.Attr;
	item.size   = item.is_dir ? -1 : sr.Size;
	item.stamp  = sr.TimeStamp;
	FindClose(sr);

	out.push_back(UnicodeString(_T("  FLINFO ")) + yen_to_slash(path));

	std::unique_ptr<TStringList> lst(new TStringList());
	try {
		BuildFileInfoLines(path, item, lst.get());
	}
	catch (...) {
		// 解析関数側の例外は LogFileInfoCore 側 (呼び出し元) が捕捉している。
		// ここでは「情報が取れなかった」ことだけを残す
		lst->Add(_T("(情報の取得に失敗しました)"));
	}
	for (int i = 0; i < lst->Count; i++) out.push_back(lst->Strings[i]);

	return out;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> FormatAboutLines()
{
	// ListNyanFi (MainFrm.cpp:20947) の簡略版。詳細は log_win.h 冒頭コメント参照
	std::vector<UnicodeString> out;

	wchar_t exe_path[MAX_PATH] = {0};
	::GetModuleFileNameW(NULL, exe_path, MAX_PATH);
	out.push_back(UnicodeString(_T("実行パス: ")) + UnicodeString(exe_path));

#if defined(_WIN64)
	out.push_back(_T("アーキテクチャ: x64"));
#else
	out.push_back(_T("アーキテクチャ: x86"));
#endif

#if defined(NDEBUG)
	out.push_back(_T("ビルド構成: Release"));
#else
	out.push_back(_T("ビルド構成: Debug"));
#endif

	UnicodeString compiler;
#if defined(__clang__)
	compiler.sprintf(_T("Clang %d.%d.%d"), __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
	compiler.sprintf(_T("GCC %d.%d.%d"), __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	compiler = _T("不明");
#endif
	out.push_back(UnicodeString(_T("コンパイラ: ")) + compiler);

	out.push_back(UnicodeString(_T("ビルド日時: ")) + UnicodeString(__DATE__) + " " + UnicodeString(__TIME__));

	out.push_back(_T("URL: https://github.com/Nekomimi1958/NyanFi_x64s"));

	return out;
}

}  // namespace log_win
