/**
 * @file gui/menu_def.h
 * @brief メニューバーの定義表 (wx 非依存)
 *
 * VCL 版 (src/MainFrm.dfm の TMainMenu, 358項目) の全移植ではなく、
 * 実装済みコマンドだけを載せた基本メニュー (Issue #89)。キー発見可能性
 * (#89) のため各項目のショートカットは KeyMap::FindKey() で逆引きする。
 *
 * 未実装コマンドは載せない (メニューから起動不可にしてはならない)。
 */
#ifndef NYANFI_GUI_MENU_DEF_H
#define NYANFI_GUI_MENU_DEF_H

#include <vector>

#include "compat/ustring.h"

namespace menu_def {

/// メニュー1項目。separator のとき true
struct Item {
	/// トップレベルメニュー名 (例: "ファイル")。空=直前のメニューに属する
	UnicodeString menu;
	/// 表示ラベル (例: "新規ファイル")。空=セパレータ
	UnicodeString label;
	/// 実行コマンド名 (例: "NewFile")。セパレータでは空
	UnicodeString command;
};

/// メニュー表示用のラベルを作る。キーがある場合は "ラベル (キー)"。
/// "\t" 接尾は付けない (Issue #105: wx がメニューアクセラレータ表を
/// 自動生成してキー入力を横取りするため。キー処理は OnCharHook に一元化)
UnicodeString DisplayLabel(const UnicodeString &label, const UnicodeString &key);

/// 基本メニューの全項目。順序=表示順
const std::vector<Item> &Items();

}  // namespace menu_def

#endif
