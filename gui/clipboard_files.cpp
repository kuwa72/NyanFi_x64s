/**
 * @file gui/clipboard_files.cpp
 * @brief クリップボード経由のファイル操作の判断 (設計は gui/clipboard_files.h)
 */
#include "gui/clipboard_files.h"

#include "gui/file_ops.h"

namespace clipboard_files {

//---------------------------------------------------------------------------
bool IsMoveEffect(unsigned int effect)
{
	// VCL と同じくビットで見る (MainFrm.cpp:28702)。エクスプローラは
	// DROPEFFECT_MOVE | DROPEFFECT_LINK のように複数立てることがある
	return (effect & DROPEFFECT_MOVE) != 0;
}

//---------------------------------------------------------------------------
PasteCheck ValidatePasteTargets(const std::vector<UnicodeString> &paths,
                                const UnicodeString &dst_dir)
{
	PasteCheck out;
	const UnicodeString dst = ExcludeTrailingPathDelimiter(dst_dir);

	for (const UnicodeString &p : paths) {
		const UnicodeString src = ExcludeTrailingPathDelimiter(p);

		// 自分自身または配下へは貼れない (無限再帰になる)
		if (file_ops::IsSameOrInside(src, dst)) {
			out.rejected.push_back(p + _T(": 自分自身または配下のディレクトリです"));
			continue;
		}
		// 元と同じディレクトリなら何も起きない
		if (SameText(ExcludeTrailingPathDelimiter(ExtractFilePath(src)), dst)) {
			out.rejected.push_back(p + _T(": 貼り付け先が元と同じディレクトリです"));
			continue;
		}
		out.accepted.push_back(p);
	}
	return out;
}

}  // namespace clipboard_files
