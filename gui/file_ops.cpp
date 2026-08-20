/**
 * @file gui/file_ops.cpp
 * @brief ファイル操作の実装
 *
 * @details 設計上の判断は gui/file_ops.h の冒頭コメントを参照。
 */
#include "gui/file_ops.h"

#include <algorithm>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace file_ops {

namespace {

/// items の各パスから末尾の要素名を取り出す (ExtractFileName は末尾の "\\" が
/// あると空文字列を返すので、あらかじめ取り除く)
UnicodeString LastPathElement(const UnicodeString &path)
{
	return ExtractFileName(ExcludeTrailingPathDelimiter(path));
}


/**
 * @brief ディレクトリ1件を再帰的にコピーする
 * @details 宛先ディレクトリが既に存在する場合はマージする (中身を1件ずつ
 * 存在チェックしてスキップするので、既存ファイルを上書きすることはない)。
 */
bool CopyDirRecursive(const UnicodeString &src_dir, const UnicodeString &dst_dir, FileOpResult &result)
{
	const UnicodeString dst_nam = ExcludeTrailingPathDelimiter(dst_dir);

	// 注意: file_exists() は GetFileAttributes ベースで「パスが存在するか」を
	// 見ているだけなので、ディレクトリに対しても true を返す。ここで衝突として
	// 弾きたいのは「同名の“ファイル”が既にあってディレクトリを作れない」場合
	// だけなので、dir_exists() で除外してから判定する (でないと、マージしたい
	// 既存ディレクトリまで衝突扱いになってしまう)
	if (file_exists(dst_nam) && !dir_exists(dst_nam)) {
		// 同名のファイルがあり、型が違うため衝突する
		result.failures.push_back(dst_nam + _T(": 同名のファイルが存在するためコピーできません"));
		return false;
	}

	if (!dir_exists(dst_nam)) {
		if (!create_Dir(dst_nam)) {
			result.failures.push_back(dst_nam + _T(": ディレクトリを作成できません"));
			return false;
		}
		++result.success_count;
	}

	const UnicodeString src_p = IncludeTrailingPathDelimiter(src_dir);
	const UnicodeString dst_p = IncludeTrailingPathDelimiter(dst_nam);

	bool ok = true;
	TSearchRec sr;
	if (FindFirst(src_p + _T("*"), faAnyFile, sr) == 0) {
		do {
			if (SameStr(sr.Name, _T(".")) || SameStr(sr.Name, _T(".."))) continue;

			const UnicodeString s = src_p + sr.Name;
			const UnicodeString d = dst_p + sr.Name;

			if (sr.Attr & faDirectory) {
				if (!CopyDirRecursive(s, d, result)) ok = false;
			}
			else if (file_exists(d)) {
				++result.skipped_existing;
			}
			else if (copy_File(s, d)) {
				++result.success_count;
			}
			else {
				result.failures.push_back(d + _T(": コピーに失敗しました"));
				ok = false;
			}
		} while (FindNext(sr) == 0);
		FindClose(sr);
	}

	return ok;
}

/// 1件 (ファイルまたはディレクトリ) をコピーする
void CopyOneItem(const UnicodeString &src, const UnicodeString &dst, FileOpResult &result)
{
	if (dir_exists(src)) {
		// 自分自身、または自分の配下へのコピーは無限再帰になるので弾く
		if (IsSameOrInside(src, dst)) {
			result.failures.push_back(src + _T(": コピー先がコピー元自身か、その配下です"));
			return;
		}
		CopyDirRecursive(src, dst, result);
		return;
	}

	if (SameText(ExcludeTrailingPathDelimiter(src), ExcludeTrailingPathDelimiter(dst))) {
		result.failures.push_back(src + _T(": コピー元とコピー先が同じです"));
		return;
	}

	if (!file_exists(src)) {
		result.failures.push_back(src + _T(": 元のファイルが見つかりません"));
		return;
	}

	if (file_exists(dst) || dir_exists(dst)) {
		++result.skipped_existing;
		return;
	}

	if (copy_File(src, dst)) {
		++result.success_count;
	}
	else {
		result.failures.push_back(dst + _T(": コピーに失敗しました"));
	}
}

/// 1件 (ファイルまたはディレクトリ) を移動する
void MoveOneItem(const UnicodeString &src, const UnicodeString &dst, FileOpResult &result)
{
	const bool src_is_dir = dir_exists(src);
	if (!src_is_dir && !file_exists(src)) {
		result.failures.push_back(src + _T(": 元のファイルが見つかりません"));
		return;
	}

	// 自分自身、または自分の配下への移動。Win32 も拒否するが、意味の分かる
	// メッセージにするためここで弾く
	if (src_is_dir && IsSameOrInside(src, dst)) {
		result.failures.push_back(src + _T(": 移動先が移動元自身か、その配下です"));
		return;
	}

	if (file_exists(dst) || dir_exists(dst)) {
		++result.skipped_existing;
		return;
	}

	// move_File (MoveFileEx + MOVEFILE_COPY_ALLOWED) はファイルならドライブを
	// 跨いでも動くが、ディレクトリは同一ボリュームのリネームでしか成功しない
	// (Win32 の仕様)。跨ぐ場合の再帰移動は実装せず、失敗として報告する。
	if (move_File(src, dst)) {
		++result.success_count;
	}
	else if (src_is_dir) {
		result.failures.push_back(dst + _T(": 移動に失敗しました (ディレクトリはドライブを跨げません)"));
	}
	else {
		result.failures.push_back(dst + _T(": 移動に失敗しました"));
	}
}

}  // namespace

/**
 * @brief コピー先がコピー元と同じか、その配下かを判定する
 * @details ディレクトリを自分自身の配下にコピーすると、作った先を再び走査して
 *          無限に再帰し、ディスクを埋め尽くす。同一パスへのコピーも同様に
 *          無限再帰になる。どちらも実害が出るので、操作前に弾く。
 *
 *          比較は大小文字を無視する (Windows のパスは大小文字を区別しない)。
 *          `\` の有無を揃えたうえで、`src` + `\` が `dst` の先頭に一致するかを見る。
 * @param src コピー元 (ディレクトリ)
 * @param dst コピー先
 * @return bool 同じか配下なら true (コピーしてはいけない)
 */
bool IsSameOrInside(const UnicodeString &src, const UnicodeString &dst)
{
	const UnicodeString s = ExcludeTrailingPathDelimiter(src);
	const UnicodeString d = ExcludeTrailingPathDelimiter(dst);

	if (SameText(s, d)) return true;                       // 同じパス
	return StartsText(IncludeTrailingPathDelimiter(s), d);  // dst が src の配下
}

//---------------------------------------------------------------------------
UnicodeString Summarize(const FileOpResult &result)
{
	UnicodeString s;
	s.sprintf(_T("成功 %d 件 / 上書き回避のスキップ %d 件 / 失敗 %d 件"),
	          result.success_count, result.skipped_existing, static_cast<int>(result.failures.size()));

	if (!result.failures.empty()) {
		s += _T("\n\n失敗した項目:\n");
		const int show = std::min<int>(static_cast<int>(result.failures.size()), 10);
		for (int i = 0; i < show; ++i) s += _T("・") + result.failures[static_cast<std::size_t>(i)] + _T("\n");
		if (static_cast<int>(result.failures.size()) > show) {
			s.cat_sprintf(_T("...ほか %d 件\n"), static_cast<int>(result.failures.size()) - show);
		}
	}
	return s;
}

//---------------------------------------------------------------------------
FileOpResult CopyItems(const std::vector<UnicodeString> &items, const UnicodeString &dst_dir)
{
	FileOpResult result;
	const UnicodeString dst_p = IncludeTrailingPathDelimiter(dst_dir);
	for (const UnicodeString &src : items) {
		CopyOneItem(ExcludeTrailingPathDelimiter(src), dst_p + LastPathElement(src), result);
	}
	return result;
}

//---------------------------------------------------------------------------
FileOpResult MoveItems(const std::vector<UnicodeString> &items, const UnicodeString &dst_dir)
{
	FileOpResult result;
	const UnicodeString dst_p = IncludeTrailingPathDelimiter(dst_dir);
	for (const UnicodeString &src : items) {
		MoveOneItem(ExcludeTrailingPathDelimiter(src), dst_p + LastPathElement(src), result);
	}
	return result;
}

//---------------------------------------------------------------------------
bool RenameItem(const UnicodeString &dir, const UnicodeString &old_name,
                 const UnicodeString &new_name, UnicodeString &error_out)
{
	if (new_name.IsEmpty()) {
		error_out = _T("名前が空です");
		return false;
	}
	if (SameStr(old_name, new_name)) return true;  // 変更なし

	const UnicodeString base = IncludeTrailingPathDelimiter(dir);
	const UnicodeString old_path = base + old_name;
	const UnicodeString new_path = base + new_name;

	if (!file_exists(old_path) && !dir_exists(old_path)) {
		error_out = _T("元のファイルが見つかりません");
		return false;
	}
	if (file_exists(new_path) || dir_exists(new_path)) {
		error_out = _T("同名のファイルまたはディレクトリが既に存在します");
		return false;
	}

	// rename_File (MoveFile) は同一ボリューム内ならディレクトリの名前変更にも使える
	if (!rename_File(old_path, new_path)) {
		error_out = _T("名前の変更に失敗しました");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool MakeDirectory(const UnicodeString &dir, const UnicodeString &name, UnicodeString &error_out)
{
	if (name.IsEmpty()) {
		error_out = _T("名前が空です");
		return false;
	}

	const UnicodeString path = IncludeTrailingPathDelimiter(dir) + name;
	if (file_exists(path) || dir_exists(path)) {
		error_out = _T("同名のファイルまたはディレクトリが既に存在します");
		return false;
	}
	if (!create_Dir(path)) {
		error_out = _T("ディレクトリを作成できませんでした");
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
/**
 * @details フラグは src/Global.cpp の delete_File() (use_trash=true) と同じ
 * FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT を踏襲した。GUI 側で既に
 * 確認ダイアログを出しているため、シェル自身の確認・進捗 UI は出さない。
 */
bool SendToTrash(const std::vector<UnicodeString> &paths, UnicodeString &error_out, HWND owner)
{
	if (paths.empty()) return true;

	// SHFileOperationW の pFrom は各要素を "\0" で区切り、末尾を "\0\0" にした
	// 連結文字列 (複数選択の削除で使う形式)
	std::vector<wchar_t> buf;
	for (const UnicodeString &p : paths) {
		const wchar_t *w = p.c_str();
		buf.insert(buf.end(), w, w + p.Length());
		buf.push_back(L'\0');
	}
	buf.push_back(L'\0');

	SHFILEOPSTRUCTW op;
	ZeroMemory(&op, sizeof(op));
	op.hwnd   = owner;
	op.wFunc  = FO_DELETE;
	op.pFrom  = buf.data();
	op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

	const int rc = ::SHFileOperationW(&op);
	if (rc != 0 || op.fAnyOperationsAborted) {
		UnicodeString msg;
		msg.sprintf(_T("ゴミ箱への移動に失敗しました (コード: %d)"), rc);
		error_out = msg;
		return false;
	}
	return true;
}

}  // namespace file_ops
