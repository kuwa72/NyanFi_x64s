/**
 * @file gui/archive.cpp
 * @brief 書庫の操作の実装 (設計は gui/archive.h)
 */
#include "gui/archive.h"

#include <memory>

#include "usr_arc.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace archive {

namespace {

/// UserArcUnit は書庫 DLL を読み込むので、最初に使うときだけ作って使い回す
/// (VCL も起動時に1つだけ作る)
UserArcUnit *unit()
{
	static std::unique_ptr<UserArcUnit> instance(new UserArcUnit(NULL));
	return instance.get();
}

/// 書庫として開けるかを確かめ、駄目なら理由を返す
bool ensure_available(const UnicodeString &path, UnicodeString &error_out)
{
	if (!file_exists(path)) {
		error_out = _T("書庫が見つかりません");
		return false;
	}
	if (unit()->GetArcType(path) == 0) {
		error_out = _T("対応していない書庫の形式です");
		return false;
	}
	if (!unit()->IsAvailable(path)) {
		// 書庫 DLL が入っていない場合はここに来る。UserArcUnit が理由を持つ
		error_out = unit()->ErrMsg.IsEmpty()
			? _T("書庫を扱う DLL が見つかりません (7-zip32.dll など)")
			: unit()->ErrMsg;
		return false;
	}
	return true;
}

}  // namespace

//---------------------------------------------------------------------------
UnicodeString DefaultArchiveBaseName(const UnicodeString &cursor_name,
                                     const std::vector<UnicodeString> &selected)
{
	// MainFrm.cpp:23331-23335 と同じ順序
	if (!cursor_name.IsEmpty()) return ChangeFileExt(cursor_name, EmptyStr);
	for (const UnicodeString &s : selected) {
		if (!s.IsEmpty()) return ChangeFileExt(s, EmptyStr);
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
bool LooksLikeArchive(const UnicodeString &path)
{
	return unit()->GetArcType(path) != 0;
}

//---------------------------------------------------------------------------
bool ListEntries(const UnicodeString &archive_path, std::vector<Entry> &out,
                 UnicodeString &error_out)
{
	out.clear();
	if (!ensure_available(archive_path, error_out)) return false;

	std::unique_ptr<TStringList> lst(new TStringList());
	if (!unit()->GetFileList(archive_path, lst.get())) {
		error_out = unit()->ErrMsg.IsEmpty()? _T("書庫の一覧を取得できません") : unit()->ErrMsg;
		return false;
	}

	for (int i = 0; i < lst->Count; i++) {
		Entry e;
		e.name = lst->Strings[i];
		e.is_dir = EndsStr(_T("\\"), e.name) || EndsStr(_T("/"), e.name);
		out.push_back(e);
	}
	return true;
}

//---------------------------------------------------------------------------
bool TestArchive(const UnicodeString &archive_path, UnicodeString &error_out)
{
	if (!ensure_available(archive_path, error_out)) return false;

	// 一覧が取れることをもって「読める」と判断する。
	// 書庫 DLL の CheckArchive は形式ごとに意味が違い、対応していない DLL もある
	std::vector<Entry> entries;
	if (!ListEntries(archive_path, entries, error_out)) return false;
	return true;
}

//---------------------------------------------------------------------------
bool Extract(const UnicodeString &archive_path, const UnicodeString &dst_dir,
             UnicodeString &error_out)
{
	if (!ensure_available(archive_path, error_out)) return false;
	if (!dir_exists(dst_dir)) {
		error_out = _T("展開先のディレクトリがありません");
		return false;
	}

	// ow_sw = false で上書きしない (規約: 上書きを既定にしない)
	if (!unit()->UnPack(archive_path, dst_dir, EmptyStr, true, false, false)) {
		error_out = unit()->ErrMsg.IsEmpty()? _T("展開に失敗しました") : unit()->ErrMsg;
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool Create(const UnicodeString &archive_path, const UnicodeString &src_dir,
            const std::vector<UnicodeString> &names, UnicodeString &error_out)
{
	if (names.empty()) {
		error_out = _T("詰めるものがありません");
		return false;
	}
	if (file_exists(archive_path)) {
		// 上書きしない
		error_out = _T("同名の書庫が既にあります");
		return false;
	}

	const int arc_t = unit()->GetArcType(archive_path);
	if (arc_t == 0) {
		error_out = _T("拡張子から書庫の形式を判別できません");
		return false;
	}
	if (!unit()->IsAvailable(arc_t)) {
		error_out = unit()->ErrMsg.IsEmpty()
			? _T("書庫を扱う DLL が見つかりません (7-zip32.dll など)")
			: unit()->ErrMsg;
		return false;
	}

	// UserArcUnit::Pack は「元ディレクトリ」と「空白区切りのファイル名」を取る。
	// 名前に空白が入りうるので、VCL と同じく引用符で囲む
	UnicodeString files;
	for (const UnicodeString &n : names) {
		if (!files.IsEmpty()) files += _T(" ");
		files += _T("\"") + n + _T("\"");
	}

	if (!unit()->Pack(arc_t, archive_path, src_dir, files)) {
		error_out = unit()->ErrMsg.IsEmpty()? _T("書庫の作成に失敗しました") : unit()->ErrMsg;
		return false;
	}
	return true;
}

}  // namespace archive
