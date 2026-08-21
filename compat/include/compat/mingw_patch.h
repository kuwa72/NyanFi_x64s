/**
 * @file compat/mingw_patch.h
 * @brief mingw-w64 のヘッダと Windows SDK の差分を埋める
 *
 * ここに入るのは「本番ターゲット (clang-cl + Windows SDK) では不要で、
 * ローカル検証用の mingw-w64 でだけ必要な回避」だけ。逆に本番でも必要な
 * 対応をここに混ぜないこと。
 */
#ifndef NYANFI_COMPAT_MINGW_PATCH_H
#define NYANFI_COMPAT_MINGW_PATCH_H

#include "compat/config.h"

#if defined(__MINGW32__)

//---------------------------------------------------------------------------
// FILE_RENAME_INFORMATION
//
// Windows SDK の winternl.h はこの構造体を公開していない (ntifs.h / DDK 側)
// ため、src/usr_file_ex.h は自前で定義している。mingw-w64 の winternl.h は
// 定義済みなので二重定義になる。src/ を書き換えない方針なので、src 側の定義を
// 別名へ退避させて衝突を避ける。
//
// 前提: compat/win_headers.h が先に <winternl.h> を取り込んでいること。
// (取り込みが後になると、この改名がシステムヘッダ側にかかってしまう)
//---------------------------------------------------------------------------
#define _FILE_RENAME_INFORMATION _NYANFI_FILE_RENAME_INFORMATION
#define FILE_RENAME_INFORMATION NYANFI_FILE_RENAME_INFORMATION
#define PFILE_RENAME_INFORMATION PNYANFI_FILE_RENAME_INFORMATION

//---------------------------------------------------------------------------
// WIC の補間モード
//
// mingw-w64 の wincodec.h は Windows 10 で追加された高品位モードを持たない。
// 値は Windows SDK の wincodec.h と同じ。
//---------------------------------------------------------------------------
#ifndef WICBitmapInterpolationModeHighQualityCubic
#	define WICBitmapInterpolationModeHighQualityCubic ((WICBitmapInterpolationMode)4)
#endif

#endif  // __MINGW32__

#endif  // NYANFI_COMPAT_MINGW_PATCH_H
