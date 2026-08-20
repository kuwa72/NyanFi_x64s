/**
 * @file compat/sysutils.h
 * @brief System.SysUtils / System.StrUtils の自由関数と例外の互換シム
 *
 * 対象コードでの実測: SameText 179 / SameStr 93 / ContainsText 35 / Trim 32 /
 * StartsStr 32 / ReplaceStr 29 / SplitString 25 / IncludeTrailingPathDelimiter 23 /
 * StringOfChar 22 / EndsStr 22 / ContainsStr 22 / FindFirst 17 /
 * ExcludeTrailingPathDelimiter 15 / FindNext 14 / FindClose 11 / ExtractFileName 10 /
 * SysErrorMessage 9 / ChangeFileExt 9 / Format 9。
 *
 * 注意: windows.h の A/W マクロと衝突する名前 (DeleteFile /
 * GetEnvironmentVariable / GetTempPath など) は compat/config.h で #undef 済み。
 * Win32 版を呼びたい箇所は明示的に ...W 付きの名前を使うこと。
 */
#ifndef NYANFI_COMPAT_SYSUTILS_H
#define NYANFI_COMPAT_SYSUTILS_H

#include <exception>

#include "compat/config.h"
#include "compat/datetime.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
// 例外 (Sysutils::Exception 階層)
//---------------------------------------------------------------------------
class Exception : public std::exception {
public:
	explicit Exception(const UnicodeString &msg);
	const char *what() const noexcept override;

	UnicodeString GetMessage() const { return message_; }
	compat::ROProperty<Exception, UnicodeString, &Exception::GetMessage> Message{this};

private:
	UnicodeString message_;
	std::string narrow_;  // what() 用のキャッシュ
};

class EAbort : public Exception {
public:
	EAbort() : Exception(UnicodeString()) {}
};
class EConvertError : public Exception {
public:
	using Exception::Exception;
};
class EInOutError : public Exception {
public:
	using Exception::Exception;
};
class EOSError : public Exception {
public:
	using Exception::Exception;
};

/// SysUtils::Abort。EAbort を投げて処理を打ち切る
[[noreturn]] void Abort();

//---------------------------------------------------------------------------
// ファイル属性 (System.SysUtils の faXXX)
//---------------------------------------------------------------------------
constexpr int faReadOnly = 0x00000001;
constexpr int faHidden = 0x00000002;
constexpr int faSysFile = 0x00000004;
constexpr int faVolumeID = 0x00000008;
constexpr int faDirectory = 0x00000010;
constexpr int faArchive = 0x00000020;
constexpr int faNormal = 0x00000080;
constexpr int faTemporary = 0x00000100;
constexpr int faSymLink = 0x00000400;
constexpr int faCompressed = 0x00000800;
constexpr int faEncrypted = 0x00004000;
constexpr int faAnyFile = 0x000001FF;

/**
 * @brief TSearchRec 互換
 * @details FindFirst / FindNext / FindClose で使う。Attr は faXXX 相当。
 */
struct TSearchRec {
	int Attr = 0;
	UnicodeString Name;
	Int64 Size = 0;
	TDateTime TimeStamp;
	int Time = 0;  //!< 旧 API 互換。DOS 形式の日時
	WIN32_FIND_DATAW FindData{};
	HANDLE FindHandle = INVALID_HANDLE_VALUE;
	UnicodeString ExcludeAttr;  //!< 未使用 (RTL 互換のための場所埋め)
};

int FindFirst(const UnicodeString &path, int attr, TSearchRec &rec);  //!< 成功 0
int FindNext(TSearchRec &rec);                                        //!< 成功 0
void FindClose(TSearchRec &rec);

//---------------------------------------------------------------------------
// グローバル定数 (実測: EmptyStr 195)
//---------------------------------------------------------------------------
extern const UnicodeString EmptyStr;   //!< 空文字列
extern const UnicodeString sLineBreak; //!< 改行 (Windows なので "\r\n")
constexpr wchar_t PathDelim = L'\\';
constexpr wchar_t DriveDelim = L':';
constexpr wchar_t PathSep = L';';

//---------------------------------------------------------------------------
// 文字列 (System.SysUtils)
//---------------------------------------------------------------------------
UnicodeString IntToStr(Int64 value);
UnicodeString UIntToStr(UInt64 value);
UnicodeString IntToHex(Int64 value, int digits);
int StrToInt(const UnicodeString &s);
int StrToIntDef(const UnicodeString &s, int defValue);
Int64 StrToInt64(const UnicodeString &s);
Int64 StrToInt64Def(const UnicodeString &s, Int64 defValue);
double StrToFloat(const UnicodeString &s);
double StrToFloatDef(const UnicodeString &s, double defValue);
UnicodeString FloatToStr(double value);
UnicodeString FormatFloat(const UnicodeString &format, double value);
UnicodeString Format(const UnicodeString &format, ...);  //!< printf 互換で代替する

UnicodeString Trim(const UnicodeString &s);
UnicodeString TrimLeft(const UnicodeString &s);
UnicodeString TrimRight(const UnicodeString &s);
UnicodeString UpperCase(const UnicodeString &s);
UnicodeString LowerCase(const UnicodeString &s);
UnicodeString AnsiUpperCase(const UnicodeString &s);
UnicodeString AnsiLowerCase(const UnicodeString &s);
int CompareStr(const UnicodeString &a, const UnicodeString &b);
int CompareText(const UnicodeString &a, const UnicodeString &b);
bool SameStr(const UnicodeString &a, const UnicodeString &b);
bool SameText(const UnicodeString &a, const UnicodeString &b);
UnicodeString StringOfChar(wchar_t ch, int count);
UnicodeString StringReplace(const UnicodeString &s, const UnicodeString &oldPattern,
                            const UnicodeString &newPattern, bool replaceAll, bool ignoreCase);
UnicodeString QuotedStr(const UnicodeString &s);
UnicodeString AnsiQuotedStr(const UnicodeString &s, wchar_t quote);
UnicodeString AnsiExtractQuotedStr(const wchar_t *&src, wchar_t quote);
UnicodeString SysErrorMessage(DWORD errorCode);

//---------------------------------------------------------------------------
// 文字列 (System.StrUtils)
//---------------------------------------------------------------------------
bool ContainsStr(const UnicodeString &text, const UnicodeString &sub);
bool ContainsText(const UnicodeString &text, const UnicodeString &sub);
bool StartsStr(const UnicodeString &sub, const UnicodeString &text);
bool StartsText(const UnicodeString &sub, const UnicodeString &text);
bool EndsStr(const UnicodeString &sub, const UnicodeString &text);
bool EndsText(const UnicodeString &sub, const UnicodeString &text);
UnicodeString ReplaceStr(const UnicodeString &text, const UnicodeString &from, const UnicodeString &to);
UnicodeString ReplaceText(const UnicodeString &text, const UnicodeString &from, const UnicodeString &to);
UnicodeString LeftStr(const UnicodeString &s, int count);
UnicodeString RightStr(const UnicodeString &s, int count);
UnicodeString MidStr(const UnicodeString &s, int start, int count);
UnicodeString DupeString(const UnicodeString &s, int count);
UnicodeString ReverseString(const UnicodeString &s);
int PosEx(const UnicodeString &sub, const UnicodeString &text, int offset = 1);
bool MatchStr(const UnicodeString &s, const TStringDynArray &values);
bool MatchText(const UnicodeString &s, const TStringDynArray &values);
UnicodeString IfThen(bool condition, const UnicodeString &whenTrue, const UnicodeString &whenFalse);

/// 区切り文字集合で分割する (Delphi の SplitString と同じく空要素も残す)
TStringDynArray SplitString(const UnicodeString &s, const UnicodeString &delimiters);

//---------------------------------------------------------------------------
// パス・ファイル (System.SysUtils / System.IOUtils 相当)
//---------------------------------------------------------------------------
UnicodeString ExtractFileName(const UnicodeString &fileName);
UnicodeString ExtractFilePath(const UnicodeString &fileName);
UnicodeString ExtractFileDir(const UnicodeString &fileName);
UnicodeString ExtractFileExt(const UnicodeString &fileName);
UnicodeString ExtractFileDrive(const UnicodeString &fileName);
UnicodeString ChangeFileExt(const UnicodeString &fileName, const UnicodeString &extension);
UnicodeString IncludeTrailingPathDelimiter(const UnicodeString &path);
UnicodeString ExcludeTrailingPathDelimiter(const UnicodeString &path);
UnicodeString ExtractRelativePath(const UnicodeString &baseName, const UnicodeString &destName);
UnicodeString ExpandFileName(const UnicodeString &fileName);
bool IsPathDelimiter(const UnicodeString &s, int index);

bool FileExists(const UnicodeString &fileName);
bool DirectoryExists(const UnicodeString &directory);
bool DeleteFile(const UnicodeString &fileName);
bool RenameFile(const UnicodeString &oldName, const UnicodeString &newName);
bool CreateDir(const UnicodeString &dir);
bool RemoveDir(const UnicodeString &dir);
bool ForceDirectories(const UnicodeString &dir);
int FileGetAttr(const UnicodeString &fileName);
int FileSetAttr(const UnicodeString &fileName, int attr);
TDateTime FileAge(const UnicodeString &fileName);
UnicodeString GetCurrentDir();
bool SetCurrentDir(const UnicodeString &dir);
UnicodeString GetEnvironmentVariable(const UnicodeString &name);

//---------------------------------------------------------------------------
namespace System {
namespace Sysutils {
using ::EAbort;
using ::EConvertError;
using ::Exception;
using ::TSearchRec;
}  // namespace Sysutils
using namespace Sysutils;
}  // namespace System

namespace Sysutils = System::Sysutils;
namespace SysUtils = System::Sysutils;

#endif  // NYANFI_COMPAT_SYSUTILS_H
