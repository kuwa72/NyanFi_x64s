/**
 * @file compat/config.h
 * @brief プラットフォーム設定・C++Builder 固有キーワード・Delphi スカラ型の別名
 *
 * C++Builder は暗黙に vcl.h を読み込むため、既存ソースは Windows ヘッダも
 * Delphi のスカラ型も宣言なしで使っている。このヘッダはその土台を再現する。
 * 実際の取り込みは compat/vcl_shim.h を強制インクルード (-include / /FI) して行う。
 */
#ifndef NYANFI_COMPAT_CONFIG_H
#define NYANFI_COMPAT_CONFIG_H

#if !defined(_WIN32)
#	error "NyanFi は Windows 専用です (issue #1: クロスプラットフォーム化は目的外)"
#endif

#ifndef UNICODE
#	define UNICODE
#endif
#ifndef _UNICODE
#	define _UNICODE
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif

// dlgs.h (コモンダイアログのコントロール ID) は lst1..lst16 / edt1.. / cmb1.. /
// psh1.. といった短い識別子をマクロにしてしまう。C++Builder の vcl.h はこれを
// 取り込んでおらず、既存コードやテストの変数名 (lst2 など) と衝突するため、
// windows.h より先にインクルードガードを立てて無効化する。
// (Windows SDK 側のガード名 _DLGSH_INCLUDED_ と同じ)
#ifndef _DLGSH_INCLUDED_
#	define _DLGSH_INCLUDED_
#	define NYANFI_SUPPRESSED_DLGS_H
#endif

#include <windows.h>
#include <tchar.h>

#include <cstddef>
#include <cstdint>

// C++Builder は暗黙に C ランタイムのヘッダを取り込んでいたため、既存コードは
// fabs / modf / exp / log / memcpy などを宣言なしで使っている (usr_exif.cpp ほか)。
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//---------------------------------------------------------------------------
// windows.h が A/W マクロで潰してしまう名前のうち、RTL 側に同名の関数がある
// ものを外す。RTL 版のシグネチャは compat/sysutils.h で宣言する。
//---------------------------------------------------------------------------
// 外すのは「RTL に同名の関数があるもの」だけに限る。CopyFile / MoveFile /
// RemoveDirectory は RTL 側に同名が無く、既存コードが `::CopyFile(...)` の形で
// Win32 版を直接呼んでいるため外してはいけない。
#undef DeleteFile
#undef GetEnvironmentVariable
#undef GetTempPath

//---------------------------------------------------------------------------
// C++Builder 固有のキーワード
//---------------------------------------------------------------------------
// __fastcall は x86_64 では呼び出し規約として無視されるため、clang-cl /
// mingw-w64 のいずれでもそのまま受理される (警告が出る場合のみ無効化)。
#ifndef PACKAGE
#	define PACKAGE
#endif
#ifndef __published
#	define __published public
#endif

//---------------------------------------------------------------------------
// Delphi スカラ型の別名 (System.hpp 相当)
//---------------------------------------------------------------------------
using Int8 = std::int8_t;
using Int16 = std::int16_t;
using Int32 = std::int32_t;
using Int64 = std::int64_t;
using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

using Shortint = std::int8_t;
using Smallint = std::int16_t;
using Integer = std::int32_t;
using Byte = std::uint8_t;
using Word = std::uint16_t;
using Cardinal = std::uint32_t;
using LongInt = std::int32_t;
using LongWord = std::uint32_t;
using NativeInt = std::intptr_t;
using NativeUInt = std::uintptr_t;
using Single = float;
using Double = double;
using Extended = long double;
using Currency = double;
using Boolean = bool;
using ByteBool = bool;
using WordBool = bool;

using WideChar = wchar_t;
using AnsiChar = char;
using Char = wchar_t;
using UCS4Char = char32_t;

using Pointer = void *;
using PWideChar = wchar_t *;
using PChar = wchar_t *;
using PAnsiChar = char *;

namespace System {
using ::AnsiChar;
using ::Boolean;
using ::Byte;
using ::Cardinal;
using ::Extended;
using ::Int64;
using ::Integer;
using ::LongInt;
using ::LongWord;
using ::NativeInt;
using ::NativeUInt;
using ::Pointer;
using ::Single;
using ::Smallint;
using ::WideChar;
using ::Word;
}  // namespace System

#endif  // NYANFI_COMPAT_CONFIG_H
