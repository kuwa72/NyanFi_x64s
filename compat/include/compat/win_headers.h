/**
 * @file compat/win_headers.h
 * @brief シェル統合 / COM / WIC 用の Windows ヘッダをまとめて取り込む
 *
 * 既存ソースはこれらを自分で include していない。C++Builder では Vcl.* 経由で
 * 取り込まれていたため、傘ヘッダ側で同じ状態を作る必要がある。
 *
 * 実測 (issue #1 の記載どおりシェル統合が中核機能):
 *   IShellFolder 21 / IContextMenu 3 / IWICImagingFactory 7 / DeviceIoControl 10
 *   SHGetKnownFolderPath / StrCmpLogicalW / ADS / ドライブ列挙
 */
#ifndef NYANFI_COMPAT_WIN_HEADERS_H
#define NYANFI_COMPAT_WIN_HEADERS_H

#include "compat/config.h"

#include <objbase.h>
#include <objidl.h>
#include <ocidl.h>
#include <olectl.h>
#include <propidl.h>
#include <propsys.h>

#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shobjidl.h>

#include <wincodec.h>
#include <wincodecsdk.h>

#include <commctrl.h>
#include <winioctl.h>

// src/usr_file_ex.h が自前で FILE_RENAME_INFORMATION を定義している。
// compat/mingw_patch.h の改名回避を効かせるため、システム側を必ず先に読む。
#include <winternl.h>

// IsWindows10OrGreater 等 (usr_scale.h::get_SysMetricsForPPI が使用)。
// VerifyVersionInfoW ベースの inline 関数群で、WINVER 指定に関係なく利用できる。
#include <versionhelpers.h>

// shlobj.h 以降でも A/W マクロが張り直されるため、RTL 側と衝突する名前を再度外す
// (外すのは RTL に同名関数があるものだけ。§config.h と同じ方針)
#undef DeleteFile
#undef GetEnvironmentVariable
#undef GetTempPath

//---------------------------------------------------------------------------
// GetSystemMetricsForDpi (usr_scale.h::get_SysMetricsForPPI が使用)
//
// Windows 10 (1607/RS1) 以降の API だが、本プロジェクトは WINVER=0x0601 (Win7)
// を指定しているため、mingw-w64 のヘッダ (実 Windows SDK でも同様) は
// `#if WINVER >= 0x0605` の範囲外として宣言を出さない。実行時には Windows 10
// 以降の user32.dll にエクスポートされている関数なので、ここで宣言だけ補う
// (呼び出し元は IsWindows10OrGreater() で分岐しているため、Windows 7/8 実行時
// にこの宣言だけで呼び出し自体が発生することはない)。
//---------------------------------------------------------------------------
#if WINVER < 0x0605
extern "C" WINUSERAPI int WINAPI GetSystemMetricsForDpi(int nIndex, UINT dpi);
#endif

#endif  // NYANFI_COMPAT_WIN_HEADERS_H
