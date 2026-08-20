/**
 * @file vcl_shim.h
 * @brief VCL 互換シムの傘ヘッダ
 *
 * C++Builder は各翻訳単位に暗黙で vcl.h を読み込む。既存ソースを 1 行も
 * 書き換えずにビルドするため、このヘッダを強制インクルード
 * (clang-cl: /FI, gcc/clang: -include) して同じ状態を作る。
 * 設定は CMake の nyanfi_target_vcl_shim() が行う。
 */
#ifndef NYANFI_VCL_SHIM_H
#define NYANFI_VCL_SHIM_H

#include "compat/config.h"
#include "compat/win_headers.h"
#include "compat/mingw_patch.h"

#include "compat/cominterface.h"
#include "compat/property.h"
#include "compat/vcl_forward.h"
#include "compat/set.h"
#include "compat/ustring.h"

#include "compat/datetime.h"
#include "compat/exception.h"
#include "compat/math.h"
#include "compat/sysutils.h"

#include "compat/application.h"
#include "compat/classes.h"
#include "compat/controls.h"
#include "compat/streams.h"

#include "compat/encoding.h"
#include "compat/graphics.h"
#include "compat/json.h"
#include "compat/regex.h"
#include "compat/registry.h"

#endif  // NYANFI_VCL_SHIM_H
