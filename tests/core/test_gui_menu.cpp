/**
 * @file tests/core/test_gui_menu.cpp
 * @brief gui/menu_def.cpp (メニュー定義表) の回帰テスト
 *
 * @details
 * Issue #89: メニューに載せるのは実装済みコマンドのみ。表記のtypoや
 * 未実装コマンドの混入をここで検出する。キー表示は KeyMap::FindKey() の
 * 逆引きに任せるため、このテストでは表の完全性 (空欄・重複) だけを見る。
 */
#include "doctest/doctest.h"

#include <set>

#include "gui/key_map.h"
#include "gui/menu_def.h"

TEST_CASE("menu: 定義表に空欄・重複が無い")
{
	const std::vector<menu_def::Item> &items = menu_def::Items();
	CHECK(!items.empty());

	std::set<UnicodeString> seen;
	UnicodeString cur_menu;
	for (const menu_def::Item &it : items) {
		if (!it.menu.IsEmpty()) cur_menu = it.menu;
		// 先頭項目はメニュー名を持つ
		CHECK(!cur_menu.IsEmpty());
		if (it.label.IsEmpty()) {
			// セパレータはコマンドも空
			CHECK(it.command.IsEmpty());
			continue;
		}
		CHECK(!it.command.IsEmpty());
		const UnicodeString key = cur_menu + _T("\x1f") + it.label;
		CHECK(seen.count(key) == 0);
		seen.insert(key);
	}
}

TEST_CASE("menu: 全コマンドはキー割り当て表に存在する")
{
	// メニュー掲載コマンドは全てショートカット表示付き (#89 の発見可能性)。
	// FindKey が空でない = 既定キー表に載っている = typo が無い。
	KeyMap km;
	for (const menu_def::Item &it : menu_def::Items()) {
		if (it.command.IsEmpty()) continue;
		CHECK(!km.FindKey(it.command).IsEmpty());
	}
}
