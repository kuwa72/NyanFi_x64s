/// C++Builder の Vcl.ToolWin.hpp に対応する互換ヘッダへの転送
///
/// `TToolWindow` (TToolBar / TCoolBar の基底) のユニット。src では
/// `TToolBar` を保持するフォームが 10ファイル以上 include しているが、
/// `TToolWindow` 自体を直接使う箇所は無い (grep で確認)。
/// `TToolBar` は compat/gui_stubs.h にある。
#ifndef NYANFI_FWD_VCL_TOOLWIN_HPP
#define NYANFI_FWD_VCL_TOOLWIN_HPP

#include "compat/gui_stubs.h"

#endif
