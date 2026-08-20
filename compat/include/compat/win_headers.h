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

// shlobj.h 以降でも A/W マクロが張り直されるため、RTL 側と衝突する名前を再度外す
// (外すのは RTL に同名関数があるものだけ。§config.h と同じ方針)
#undef DeleteFile
#undef GetEnvironmentVariable
#undef GetTempPath

#endif  // NYANFI_COMPAT_WIN_HEADERS_H
