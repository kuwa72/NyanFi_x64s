/**
 * @file compat/src/ioutils.cpp
 * @brief compat/ioutils.h の実装
 */
#include "compat/ioutils.h"

#include <vector>

#include "compat/encoding.h"
#include "compat/streams.h"
#include "compat/sysutils.h"

namespace {

/// path 配下 (再帰指定なら全サブディレクトリも) を走査して pattern に一致するファイルの
/// フルパスを out に集める
void CollectFiles(const UnicodeString &dir, const UnicodeString &pattern, TSearchOption option,
                   std::vector<UnicodeString> &out)
{
	const UnicodeString baseDir = IncludeTrailingPathDelimiter(dir);

	// 実測 (compat/src/sysutils.cpp のコメント参照): FindFirst の attr は
	// faAnyFile を渡すと ExcludeAttr=0 になり "." / ".." を含む全エントリが
	// そのまま返る。ディレクトリ/隠し/システム属性の絞り込みは Attr を見て
	// 自前で行う (sysutils.cpp の他の呼び出し元と同じやり方に合わせる)。
	TSearchRec rec;
	if (FindFirst(baseDir + pattern, faAnyFile, rec) == 0) {
		do {
			if (!(rec.Attr & faDirectory)) out.push_back(baseDir + rec.Name);
		} while (FindNext(rec) == 0);
		FindClose(rec);
	}

	if (option == soAllDirectories) {
		TSearchRec drec;
		if (FindFirst(baseDir + "*", faAnyFile, drec) == 0) {
			do {
				if ((drec.Attr & faDirectory) && drec.Name != UnicodeString(_T(".")) &&
				    drec.Name != UnicodeString(_T("..")))
					CollectFiles(baseDir + drec.Name, pattern, option, out);
			} while (FindNext(drec) == 0);
			FindClose(drec);
		}
	}
}

}  // namespace

TStringDynArray TDirectory::GetFiles(const UnicodeString &path, const UnicodeString &searchPattern,
                                      TSearchOption searchOption)
{
	std::vector<UnicodeString> results;
	CollectFiles(path, searchPattern, searchOption, results);

	TStringDynArray arr;
	arr.Length = static_cast<int>(results.size());
	for (int i = 0; i < arr.Length; ++i) arr[i] = results[static_cast<std::size_t>(i)];
	return arr;
}

bool TDirectory::Exists(const UnicodeString &path) { return DirectoryExists(path); }

//---------------------------------------------------------------------------
void TFile::AppendAllText(const UnicodeString &path, const UnicodeString &contents, TEncoding *encoding)
{
	TEncoding *enc = encoding ? encoding : TEncoding::UTF8;

	// 既存ファイルなら末尾に追記、無ければ新規作成 (BOM は付けない: Append の性質上、
	// 既存内容の続きとして書くものなので前置きの BOM は書かない、という判断)
	const bool exists = FileExists(path);
	std::unique_ptr<TFileStream> fs(
		new TFileStream(path, exists ? (fmOpenReadWrite | fmShareDenyWrite) : fmCreate));
	fs->Seek(0, soFromEnd);

	const TBytes bytes = enc->GetBytes(contents);
	if (bytes.Length > 0) fs->WriteBuffer(bytes, bytes.Length);
}

void TFile::AppendAllText(const UnicodeString &path, const UnicodeString &contents)
{
	// 実測の呼び出しは全て明示的にエンコードを渡しており、この無指定版は実測が無い。
	// System.IOUtils の既定 (UTF8) に合わせる (報告に明記した推測)。
	AppendAllText(path, contents, TEncoding::UTF8);
}
