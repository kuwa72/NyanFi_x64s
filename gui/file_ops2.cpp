/**
 * @file gui/file_ops2.cpp
 * @brief ファイル操作の続きの実装 (設計は gui/file_ops2.h)
 */
#include "gui/file_ops2.h"

#include <algorithm>
#include <memory>
#include <string>

#include "gui/clone_name.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace file_ops2 {

namespace {

/// 中間改名に使う一時名 (VCL の `$~NF%04u.~TMP`, MainFrm.cpp:26637 と同じ)
UnicodeString temp_name_for(const UnicodeString &path, int index)
{
	UnicodeString tmp = ExtractFilePath(path);
	tmp.cat_sprintf(_T("$~NF%04u.~TMP"), index);
	return tmp;
}

bool exists_any(const UnicodeString &path)
{
	return file_exists(path) || dir_exists(path);
}

/// 配下のサブディレクトリを再帰的に集める (VCL の get_SubDirs 相当。
/// Global.cpp にあってビルド対象外なので同じ意味を書いた)
void collect_subdirs(const UnicodeString &dir, std::vector<UnicodeString> &out)
{
	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	TSearchRec sr;
	if (FindFirst(base + _T("*"), faDirectory, sr) != 0) return;
	do {
		if (!(sr.Attr & faDirectory)) continue;
		if (SameStr(sr.Name, _T(".")) || SameStr(sr.Name, _T(".."))) continue;
		const UnicodeString sub = base + sr.Name;
		out.push_back(sub);
		collect_subdirs(sub, out);
	} while (FindNext(sr) == 0);
	FindClose(sr);
}

/// UTF-8 のバイト列にする
std::string to_utf8(const UnicodeString &s)
{
	if (s.IsEmpty()) return std::string();
	const int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), NULL, 0, NULL, NULL);
	if (n <= 0) return std::string();
	std::string out(static_cast<std::size_t>(n), '\0');
	::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.Length(), &out[0], n, NULL, NULL);
	return out;
}

UnicodeString from_utf8(const char *p, int len)
{
	if (len <= 0) return EmptyStr;
	const int n = ::MultiByteToWideChar(CP_UTF8, 0, p, len, NULL, 0);
	if (n <= 0) return EmptyStr;
	std::wstring out(static_cast<std::size_t>(n), L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, p, len, &out[0], n);
	return UnicodeString(out.c_str(), n);
}

}  // namespace

//---------------------------------------------------------------------------
file_ops::FileOpResult CloneItems(const std::vector<UnicodeString> &paths,
                                  const UnicodeString &dst_dir, const UnicodeString &fmt)
{
	file_ops::FileOpResult result;

	// この呼び出しの中で作る予定の名前も「使用済み」として避ける。
	// そうしないと2件目が1件目と同じ名前を狙う
	std::vector<UnicodeString> planned;
	const auto taken = [&planned](const UnicodeString &path) {
		if (exists_any(path)) return true;
		for (std::size_t i = 0; i < planned.size(); ++i) {
			if (SameText(planned[i], path)) return true;
		}
		return false;
	};

	for (const UnicodeString &src : paths) {
		const UnicodeString name = ExtractFileName(ExcludeTrailingPathDelimiter(src));
		const bool is_dir = dir_exists(src);

		const UnicodeString dst = clone_name::MakeUnique(fmt, src, dst_dir, is_dir, taken);
		if (dst.IsEmpty()) {
			result.failures.push_back(name + _T(": 空いている名前が見つかりません"));
			continue;
		}
		planned.push_back(dst);

		// 名前まで指定して複製する。**「元の名前でコピーしてから改名」にはしない**
		// (同じディレクトリへのクローンだと、元のファイルと名前がぶつかって
		//  コピー自体がスキップされてしまう。実際にそう書いてテストで落ちた)。
		// 失敗の理由は CopyItemTo が result に積む
		file_ops::CopyItemTo(src, dst, result);
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult CopyDirStructure(const std::vector<UnicodeString> &dirs,
                                        const UnicodeString &dst_dir, bool recursive)
{
	file_ops::FileOpResult result;
	const UnicodeString base = IncludeTrailingPathDelimiter(dst_dir);

	for (const UnicodeString &src_in : dirs) {
		const UnicodeString src = ExcludeTrailingPathDelimiter(src_in);
		if (!dir_exists(src)) {
			result.failures.push_back(ExtractFileName(src) + _T(": ディレクトリではありません"));
			continue;
		}
		// 自分自身や配下へ作らせない (規約: 破壊的な機能を足すとき)
		if (file_ops::IsSameOrInside(src, base)) {
			result.failures.push_back(ExtractFileName(src) + _T(": 自分自身または配下へは作れません"));
			continue;
		}

		std::vector<UnicodeString> targets;
		targets.push_back(src);
		if (recursive) collect_subdirs(src, targets);

		const UnicodeString src_parent = IncludeTrailingPathDelimiter(ExtractFilePath(src));
		for (std::size_t i = 0; i < targets.size(); ++i) {
			// 元の親からの相対を、作る先にそのまま生やす
			UnicodeString rel = targets[i];
			rel.Delete(1, src_parent.Length());
			const UnicodeString dst = base + rel;

			if (dir_exists(dst)) { result.skipped_existing++; continue; }
			if (create_ForceDirs(dst)) result.success_count++;
			else result.failures.push_back(rel + _T(": 作成できません"));
		}
	}
	return result;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult CreateDirs(const std::vector<UnicodeString> &names,
                                  const UnicodeString &base)
{
	file_ops::FileOpResult result;

	for (const UnicodeString &raw : names) {
		const UnicodeString name = Trim(raw);
		if (name.IsEmpty()) continue;

		const UnicodeString path = to_absolute_name(name, base);
		if (dir_exists(path)) { result.skipped_existing++; continue; }
		if (file_exists(path)) {
			result.failures.push_back(name + _T(": 同名のファイルがあります"));
			continue;
		}
		if (create_ForceDirs(path)) result.success_count++;
		else result.failures.push_back(name + _T(": 作成できません"));
	}
	return result;
}

//---------------------------------------------------------------------------
bool SwapNames(const UnicodeString &a, const UnicodeString &b, UnicodeString &error_out)
{
	if (SameText(a, b)) { error_out = _T("同じ項目です"); return false; }
	if (!exists_any(a) || !exists_any(b)) { error_out = _T("項目が見つかりません"); return false; }
	// 親子関係はややこしいので VCL も非対応 (MainFrm.cpp:26608)
	if (file_ops::IsSameOrInside(a, b) || file_ops::IsSameOrInside(b, a)) {
		error_out = _T("親子関係にある項目どうしは入れ替えられません");
		return false;
	}

	// 拡張子はそれぞれ元のまま。入れ替わるのは名前の主部だけ
	const UnicodeString ext_a = get_extension(a);
	const UnicodeString ext_b = get_extension(b);
	const UnicodeString new_a = ChangeFileExt(b, ext_a);
	const UnicodeString new_b = ChangeFileExt(a, ext_b);

	if (SameText(new_a, a) && SameText(new_b, b)) {
		error_out = _T("入れ替えても名前が変わりません");
		return false;
	}

	// 中間改名。**片方でも失敗したら戻して中止する**
	const UnicodeString tmp_a = temp_name_for(a, 0);
	const UnicodeString tmp_b = temp_name_for(b, 1);
	if (exists_any(tmp_a) || exists_any(tmp_b)) {
		error_out = _T("一時ファイル ($~NF*.~TMP) が残っています");
		return false;
	}

	if (!::MoveFileW(a.c_str(), tmp_a.c_str())) {
		error_out = _T("中間処理に失敗しました");
		return false;
	}
	if (!::MoveFileW(b.c_str(), tmp_b.c_str())) {
		::MoveFileW(tmp_a.c_str(), a.c_str());  // 巻き戻す
		error_out = _T("中間処理に失敗しました");
		return false;
	}

	if (!::MoveFileW(tmp_a.c_str(), new_a.c_str())) {
		::MoveFileW(tmp_a.c_str(), a.c_str());
		::MoveFileW(tmp_b.c_str(), b.c_str());
		error_out = _T("名前を変更できません");
		return false;
	}
	if (!::MoveFileW(tmp_b.c_str(), new_b.c_str())) {
		::MoveFileW(new_a.c_str(), a.c_str());
		::MoveFileW(tmp_b.c_str(), b.c_str());
		error_out = _T("名前を変更できません");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
UnicodeString RenameLogPath()
{
	return ExtractFilePath(Application->ExeName) + _T("renamelog.txt");
}

//---------------------------------------------------------------------------
bool SaveRenameLog(const std::vector<RenameRecord> &records, UnicodeString &error_out)
{
	const UnicodeString path = RenameLogPath();

	// BOM 付き UTF-8。VCL の saveto_TextUTF8 と同じ形式にして、
	// VCL 版の UndoRename からも読めるようにする
	std::string bytes = "\xEF\xBB\xBF";
	for (std::size_t i = 0; i < records.size(); ++i) {
		bytes += to_utf8(records[i].old_path);
		bytes += "\t";
		bytes += to_utf8(records[i].new_path);
		bytes += "\r\n";
	}

	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) { error_out = _T("改名ログを書けません"); return false; }
	DWORD written = 0;
	const bool ok = (::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != 0)
	                && (written == bytes.size());
	::CloseHandle(h);
	if (!ok) error_out = _T("改名ログの書き込みに失敗しました");
	return ok;
}

//---------------------------------------------------------------------------
std::vector<RenameRecord> LoadRenameLog()
{
	std::vector<RenameRecord> out;
	const UnicodeString path = RenameLogPath();

	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return out;

	std::string bytes;
	char buf[8192];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) bytes.append(buf, n);
	::CloseHandle(h);

	std::size_t pos = 0;
	if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
	    && static_cast<unsigned char>(bytes[1]) == 0xBB
	    && static_cast<unsigned char>(bytes[2]) == 0xBF) {
		pos = 3;
	}

	std::size_t start = pos;
	for (std::size_t i = pos; i <= bytes.size(); ++i) {
		const bool eof = (i == bytes.size());
		if (!eof && bytes[i] != '\n' && bytes[i] != '\r') continue;

		UnicodeString line = from_utf8(bytes.data() + start, static_cast<int>(i - start));
		if (!line.IsEmpty()) {
			RenameRecord rec;
			rec.old_path = split_tkn(line, _T("\t"));
			rec.new_path = line;
			if (!rec.old_path.IsEmpty() && !rec.new_path.IsEmpty()) out.push_back(rec);
		}
		if (eof) break;
		if (bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
		start = i + 1;
	}
	return out;
}

//---------------------------------------------------------------------------
bool NeedsTempStep(const std::vector<RenameRecord> &records)
{
	// ある記録の「新しい名前」が別の記録の「元の名前」と一致するなら、
	// 素直に戻すと途中で衝突する (a→b, b→a など)
	for (std::size_t i = 0; i < records.size(); ++i) {
		for (std::size_t j = 0; j < records.size(); ++j) {
			if (i == j) continue;
			if (SameText(records[j].old_path, records[i].new_path)) return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult UndoRenames(const std::vector<RenameRecord> &records)
{
	file_ops::FileOpResult result;
	if (records.empty()) return result;

	const bool via_temp = NeedsTempStep(records);
	std::vector<UnicodeString> staged(records.size());

	if (via_temp) {
		// いったん全部を一時名へ。途中で失敗したら退避した分を戻して中止する
		for (std::size_t i = 0; i < records.size(); ++i) {
			const UnicodeString tmp = temp_name_for(records[i].new_path, static_cast<int>(i));
			if (!::MoveFileW(records[i].new_path.c_str(), tmp.c_str())) {
				for (std::size_t k = 0; k < i; ++k) {
					::MoveFileW(staged[k].c_str(), records[k].new_path.c_str());
				}
				result.failures.push_back(_T("中間処理に失敗したので元に戻しました"));
				return result;
			}
			staged[i] = tmp;
		}
	}
	else {
		for (std::size_t i = 0; i < records.size(); ++i) staged[i] = records[i].new_path;
	}

	for (std::size_t i = 0; i < records.size(); ++i) {
		const UnicodeString &to = records[i].old_path;
		if (exists_any(to)) {
			// 戻し先が塞がっている。**一時名のままにしない**ので元の場所へ返す
			if (via_temp) ::MoveFileW(staged[i].c_str(), records[i].new_path.c_str());
			result.skipped_existing++;
			continue;
		}
		if (::MoveFileW(staged[i].c_str(), to.c_str())) {
			result.success_count++;
		}
		else {
			if (via_temp) ::MoveFileW(staged[i].c_str(), records[i].new_path.c_str());
			result.failures.push_back(ExtractFileName(to) + _T(": 戻せません"));
		}
	}
	return result;
}

//---------------------------------------------------------------------------
Int64 ParseSize(const UnicodeString &text)
{
	UnicodeString s = Trim(text);
	if (s.IsEmpty()) return -1;

	Int64 unit = 1;
	const UnicodeString tail = s.SubString(s.Length(), 1).UpperCase();
	if      (tail == UnicodeString(_T("K"))) unit = 1024LL;
	else if (tail == UnicodeString(_T("M"))) unit = 1024LL * 1024;
	else if (tail == UnicodeString(_T("G"))) unit = 1024LL * 1024 * 1024;
	if (unit != 1) s.Delete(s.Length(), 1);

	s = Trim(s);
	if (s.IsEmpty()) return -1;
	for (int i = 1; i <= s.Length(); ++i) {
		if (s[i] < L'0' || s[i] > L'9') return -1;
	}

	const Int64 n = StrToInt64Def(s, -1);
	if (n < 0) return -1;
	return n * unit;
}

//---------------------------------------------------------------------------
UnicodeString TestFileName(const UnicodeString &name, int index, int count)
{
	if (count <= 1) return name;

	const UnicodeString base = ChangeFileExt(name, EmptyStr);
	const UnicodeString ext = get_extension(name);
	// 総数の桁数でゼロ詰めする (count=10 なら 01〜10)
	const int width = IntToStr(count).Length();

	UnicodeString out;
	out.cat_sprintf(_T("%s%0*u%s"), base.c_str(), width, index + 1, ext.c_str());
	return out;
}

//---------------------------------------------------------------------------
file_ops::FileOpResult CreateTestFiles(const UnicodeString &dir, const UnicodeString &name,
                                       Int64 size, int count)
{
	file_ops::FileOpResult result;
	const UnicodeString base = IncludeTrailingPathDelimiter(dir);

	for (int i = 0; i < count; ++i) {
		const UnicodeString fname = TestFileName(name, i, count);
		const UnicodeString path = base + fname;

		// 上書きしない (規約: 上書きを既定にしない)
		if (exists_any(path)) { result.skipped_existing++; continue; }

		HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
		                         FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			result.failures.push_back(fname + _T(": 作成できません"));
			continue;
		}

		LARGE_INTEGER li;
		li.QuadPart = size;
		const bool ok = (::SetFilePointerEx(h, li, NULL, FILE_BEGIN) != 0) && (::SetEndOfFile(h) != 0);
		::CloseHandle(h);

		if (ok) {
			result.success_count++;
		}
		else {
			// 途中まで作ったものを残さない
			::DeleteFileW(path.c_str());
			result.failures.push_back(fname + _T(": サイズを設定できません"));
		}
	}
	return result;
}

}  // namespace file_ops2
