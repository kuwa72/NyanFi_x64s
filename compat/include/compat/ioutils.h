/**
 * @file compat/ioutils.h
 * @brief System.IOUtils (TDirectory / TFile / TSearchOption) の互換シム
 *
 * 対象コードでの実測 (Phase 0 の probe.sh 対象は usr_migemo.cpp のみ):
 *   - usr_migemo.cpp: `TSearchOption opt = TSearchOption::soAllDirectories;`
 *     `TDirectory::GetFiles(PathName, "migemo-dict", opt)` (再帰的にファイル一覧を取得)
 *   - usr_excmd.cpp / MainFrm.cpp (Phase 0 対象外だが同じ System.IOUtils 系):
 *     `TFile::AppendAllText(path, text, encoding)`
 *
 * `TSearchOption` は `enum TSearchOption { soTopDirectoryOnly, soAllDirectories };`
 * のような非スコープ enum として定義する。C++11 以降は非スコープ enum の
 * 列挙子も `EnumName::Enumerator` の形で修飾アクセスできるため、
 * `TSearchOption::soAllDirectories` という実測の書き方がそのまま成立する。
 */
#ifndef NYANFI_COMPAT_IOUTILS_H
#define NYANFI_COMPAT_IOUTILS_H

#include "compat/config.h"
#include "compat/ustring.h"

class TEncoding;  //!< compat/encoding.h で定義 (循環回避のため前方宣言に留める)

//---------------------------------------------------------------------------
/// System.IOUtils::TSearchOption 互換
enum TSearchOption { soTopDirectoryOnly, soAllDirectories };

//---------------------------------------------------------------------------
/**
 * @brief System.IOUtils::TDirectory 互換 (静的メソッドのみ、Phase 0 で必要な分だけ実装)
 */
class TDirectory {
public:
	TDirectory() = delete;

	/**
	 * @brief Path 配下で SearchPattern に一致するファイルのフルパス一覧を取得する
	 * @details SearchPattern はワイルドカード (`*` / `?`) を含む Win32 の
	 *          FindFirstFile 形式。soAllDirectories 指定時はサブディレクトリも
	 *          再帰的に走査する (実測の呼び出し形どおり)。
	 */
	static TStringDynArray GetFiles(const UnicodeString &path, const UnicodeString &searchPattern,
	                                 TSearchOption searchOption = soTopDirectoryOnly);

	static bool Exists(const UnicodeString &path);
};

//---------------------------------------------------------------------------
/**
 * @brief System.IOUtils::TFile 互換 (静的メソッドのみ、Phase 0 で必要な分だけ実装)
 */
class TFile {
public:
	TFile() = delete;

	/// 指定エンコードでテキストをファイル末尾に追記する (無ければ新規作成)。BOM は書かない。
	static void AppendAllText(const UnicodeString &path, const UnicodeString &contents, TEncoding *encoding);
	/// エンコード省略版。実測の呼び出しは全て明示的に TEncoding::UTF8 等を渡しているため
	/// 推測にはなるが、既定は UTF8 とする (判断は報告に明記)。
	static void AppendAllText(const UnicodeString &path, const UnicodeString &contents);
};

namespace System {
namespace IOUtils {
using ::TDirectory;
using ::TFile;
using ::TSearchOption;
}  // namespace IOUtils
using namespace IOUtils;
}  // namespace System

namespace IOUtils = System::IOUtils;

#endif  // NYANFI_COMPAT_IOUTILS_H
