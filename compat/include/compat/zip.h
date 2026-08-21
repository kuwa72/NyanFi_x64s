/**
 * @file compat/zip.h
 * @brief System.Zip 相当の互換シム — **宣言のみ (未実装)**
 *
 * 規約4 と同じ方針で、メンバ関数の定義を置かない。呼ぶとリンクエラーになるので
 * 実装漏れが静かに隠れない。ZIP の読み出しを実装する時はここを埋める。
 *
 * 実呼び出し箇所 (grep 実測、全部で 2 箇所):
 *
 * - src/Global.cpp:4368 `ExtractInZipImg()`
 *     std::unique_ptr<TZipFile> zp(new TZipFile());
 *     zp->Open(arc_file, zmRead);
 *     for (int i=0; i<zp->FileCount; i++) { UnicodeString fnam = zp->FileName[i]; ... }
 *     zp->Extract(znam, TempPathA, false);
 *     zp->Close();
 *   → ZIP 内の画像を 1 枚取り出してサムネイル表示に使う機能。
 *
 * - src/MainFrm.cpp:30068 `更新アーカイブの展開`
 *     zp->FileInfo[i].ModifiedDateTime / zp->Extract(fnam, TempPathA)
 *     catch (EZipException &e)
 *   → 本家サーバから落とした更新アーカイブの自己更新。
 *     phase3-plan.md §4 で「フォークでは意味が変わる」として保留中の CheckUpdate 系。
 *
 * どちらも Phase 3 の後半に回る機能で、いま ZIP デコーダを持ち込む理由が無い。
 * **未実装であることを明示するため、意図的に定義を書いていない。**
 */
#ifndef NYANFI_COMPAT_ZIP_H
#define NYANFI_COMPAT_ZIP_H

#include "compat/config.h"
#include "compat/datetime.h"
#include "compat/exception.h"
#include "compat/property.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// TZipFile.Open のモード (Delphi の TZipMode)
enum TZipMode { zmClosed, zmRead, zmWrite };

/// ZIP 操作の例外 (Delphi の EZipException)
class EZipException : public Exception {
public:
	using Exception::Exception;
};

//---------------------------------------------------------------------------
/**
 * @brief System.Zip の TZipFile 相当 — **宣言のみ。呼ぶとリンクエラー**
 * @details 添字プロパティは所有者ポインタを持つプロキシで表現してある
 *          (compat/property.h と同じ手口)。実装するときに公開形を変えずに
 *          済むようにするため。プロキシの operator[] も定義していないので、
 *          読むだけでリンクエラーになる。
 */
class TZipFile {
public:
	/// FileInfo[i] が返すヘッダ。src が触るのは ModifiedDateTime だけ
	struct TZipHeader {
		unsigned int ModifiedDateTime = 0;  //!< DOS 形式の日時 (FileDateToDateTime に渡される)
	};

	TZipFile();
	~TZipFile();
	TZipFile(const TZipFile &) = delete;
	TZipFile &operator=(const TZipFile &) = delete;

	void Open(const UnicodeString &zipFileName, TZipMode openMode);
	void Close();
	/// @param fileName     ZIP 内のパス
	/// @param path         展開先ディレクトリ
	/// @param createSubdirs ZIP 内のディレクトリ構造を再現するか
	void Extract(const UnicodeString &fileName, const UnicodeString &path, bool createSubdirs = true);

	int GetFileCount() const;

	/// `zp->FileName[i]` / `zp->FileInfo[i]` の添字プロパティ
	template <class T>
	class IndexedProperty {
	public:
		explicit IndexedProperty(TZipFile *owner) : owner_(owner) {}
		IndexedProperty(const IndexedProperty &) = delete;
		IndexedProperty &operator=(const IndexedProperty &) = delete;

		T operator[](int index) const;  //!< 未実装 (定義しない)

	private:
		TZipFile *owner_;
	};

	compat::ROProperty<TZipFile, int, &TZipFile::GetFileCount> FileCount{this};
	IndexedProperty<UnicodeString> FileName{this};
	IndexedProperty<TZipHeader> FileInfo{this};
};

//---------------------------------------------------------------------------
namespace System {
namespace Zip {
using ::EZipException;
using ::TZipFile;
using ::TZipMode;
using ::zmClosed;
using ::zmRead;
using ::zmWrite;
}  // namespace Zip
}  // namespace System

namespace Zip = System::Zip;

#endif  // NYANFI_COMPAT_ZIP_H
