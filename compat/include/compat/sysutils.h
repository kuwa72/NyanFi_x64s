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

#include "compat/config.h"
#include "compat/datetime.h"
#include "compat/exception.h"
#include "compat/ustring.h"

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
constexpr int faInvalid = -1;  //!< シム追加: FileGetAttr の失敗値 (usr_file_ex.cpp で使用)

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
	int ExcludeAttrMask = 0;   //!< シム追加: FindFirst/FindNext 間で属性フィルタを保持する内部用フィールド
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
/// Delphi の自由関数 Pos。1 始まりで、見つからなければ 0 (実測: usr_str.cpp:953)
int Pos(const UnicodeString &sub, const UnicodeString &text, int offset = 1);
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
/// シム追加のオーバーロード (usr_file_ex.cpp の `FileAge(fnam, ft)` 呼び出しに対応)。
/// 実 Delphi RTL の `function FileAge(const AFileName: string; out FileDateTime: TDateTime): Boolean;` 相当
bool FileAge(const UnicodeString &fileName, TDateTime &fileDateTime);
UnicodeString GetCurrentDir();
bool SetCurrentDir(const UnicodeString &dir);
UnicodeString GetEnvironmentVariable(const UnicodeString &name);

//---------------------------------------------------------------------------
// ディスク容量 (System.SysUtils)
//
// 実呼び出し箇所 (grep 実測):
//   DiskSize 3 (src/Global.cpp:6147 / src/DriveDlg.cpp:176 / src/MainFrm.cpp:6735)
//   DiskFree 2 (src/DriveDlg.cpp:177 / src/MainFrm.cpp:6736)
// いずれも `int dn = (char)dstr[1] - 'A' + 1;` で **1=A の 1 始まり**の
// ドライブ番号を作り、戻り値を `__int64` で受けて `sTotal>0 && sFree>=0` で
// 妥当性を見ている。したがって単位はバイト、失敗は負値 (-1)。
//---------------------------------------------------------------------------
/// ドライブの総容量 (バイト)。drive は 0=カレント, 1=A, 2=B ... 失敗時 -1
Int64 DiskSize(Byte drive);
/// ドライブの空き容量 (バイト)。呼び出し元に割り当て可能な量を返す。失敗時 -1
Int64 DiskFree(Byte drive);

//---------------------------------------------------------------------------
// バージョン情報 (System.SysUtils)
//
// 実呼び出し箇所: src/Global.cpp:1186 / src/NyanFi.cpp:196。どちらも
//   `unsigned mj, mi, bl; GetProductVersion(Application->ExeName, mj, mi, bl)`
// で、その後 `mj*100 + mi*10 + bl` を 100 で割って "V%.2f" と表示する。
// src/NyanFi.cbproj の VerInfo_Keys は ProductVersion=16.2.7.0 なので
// 16 → "V16.27" になる。つまり
//   Major = HIWORD(dwProductVersionMS) / Minor = LOWORD(dwProductVersionMS)
//   Build = HIWORD(dwProductVersionLS)
// の対応。第4フィールド (LOWORD(LS)) は捨てる。
//---------------------------------------------------------------------------
/**
 * @brief 実行ファイルの ProductVersion を読む
 * @param fileName 対象ファイル
 * @param major    [out] 第1フィールド
 * @param minor    [out] 第2フィールド
 * @param build    [out] 第3フィールド
 * @return バージョンリソースを読めたら true (失敗時、出力は 0)
 */
bool GetProductVersion(const UnicodeString &fileName, unsigned &major, unsigned &minor, unsigned &build);

//---------------------------------------------------------------------------
// OS バージョン (System.SysUtils の TOSVersion)
//
// 実呼び出し箇所: src/Global.cpp:2278 の 1 行だけ
//   `ret_str.sprintf(_T("%u.%u.%u "), TOSVersion::Major, TOSVersion::Minor, TOSVersion::Build);`
// レジストリから採れなかったときのフォールバックで、直後に
// ビルド番号で Windows 11 を判定している。
//
// 実 C++Builder の TOSVersion はほかに Name / Platform / Architecture /
// ServicePackMajor / ToString() を持つが、src では使われていないので
// 足していない (必要になったらここに追加する)。
//---------------------------------------------------------------------------
/**
 * @brief OS のバージョン番号
 * @details **実装は RtlGetVersion (ntdll) を使う**。Delphi の TOSVersion は
 *          GetVersionEx を使うが、Windows 8.1 以降は互換性マニフェストが
 *          無いと 6.2 を返してしまい、直後の Windows 11 判定
 *          (ビルド番号 >= 22000) が成立しなくなる。マニフェストの有無に
 *          関わらず正しい値が要るので RtlGetVersion を選んだ (シム独自の判断)。
 */
struct TOSVersion {
	static const int Major;  //!< メジャーバージョン (Windows 10 / 11 は 10)
	static const int Minor;  //!< マイナーバージョン
	static const int Build;  //!< ビルド番号 (22000 以上なら Windows 11)
};

//---------------------------------------------------------------------------
/// System.SysUtils::TGUID 相当。Delphi の TGUID は Win32 の GUID と
/// 同じレイアウトで、`::SHGetKnownFolderPath(guid, ...)` にそのまま渡せる
/// (実測: src/usr_shell.cpp:1853-1855 がこの使い方をする唯一の箇所)
using TGUID = ::GUID;

/**
 * @brief "{xxxxxxxx-xxxx-...}" 形式の文字列を GUID にする
 * @details 実測: `src/usr_shell.cpp:1853` の
 *          `TGUID guid = StringToGUID(s); ::SHGetKnownFolderPath(guid, ...)`
 *          1箇所だけ。`s` は ini の `KnownGuidStrToPath` に渡される既知フォルダの
 *          GUID 文字列。
 * @note Delphi は変換に失敗すると EConvertError を投げる。呼び出し側は
 *       `try { ... } catch (...) { ret_str = EmptyStr; }` で受けているので、
 *       ここも失敗時に例外を投げる (黙って空 GUID を返すと、SHGetKnownFolderPath が
 *       別のフォルダを返しかねない)
 */
TGUID StringToGUID(const UnicodeString &s);

//---------------------------------------------------------------------------
namespace System {
namespace Sysutils {
using ::DiskFree;
using ::StringToGUID;
using ::TGUID;
using ::DiskSize;
using ::EAbort;
using ::EConvertError;
using ::Exception;
using ::GetProductVersion;
using ::TOSVersion;
using ::TSearchRec;
}  // namespace Sysutils
using namespace Sysutils;
}  // namespace System

namespace Sysutils = System::Sysutils;
namespace SysUtils = System::Sysutils;

#endif  // NYANFI_COMPAT_SYSUTILS_H
