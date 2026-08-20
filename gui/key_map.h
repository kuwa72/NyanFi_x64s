/**
 * @file gui/key_map.h
 * @brief キー入力をコマンド名に変換する
 *
 * NyanFi の入力処理は「キー名 → コマンド名 → 実行」の3段で、キー名の生成は
 * 移植済みの `get_KeyStr()` (usr_key.cpp)、カーソルキーの割り当ては
 * `get_CsrKeyCmd()` (usr_cmdlist.cpp) がすでに持っている。ここはその2つに
 * wx のキーイベントを橋渡しするだけで、割り当て表は VCL 版と同じ
 * 「キー名=コマンド名」の TStringList で保持する。
 *
 * 将来 ini から読み込む際も、この表に流し込めばよい (VCL 版の [Key] セクションと
 * 同じ形式)。
 */
#ifndef NYANFI_GUI_KEY_MAP_H
#define NYANFI_GUI_KEY_MAP_H

#include <memory>

#include <wx/wx.h>

/**
 * @brief キー割り当て表
 */
class KeyMap {
public:
	KeyMap();

	/// wx のキーイベントを NyanFi のキー名 ("DOWN" / "Ctrl+F5" など) にする
	static UnicodeString KeyStrOf(const wxKeyEvent &event);

	/// キー名に割り当てられたコマンド名を返す。無ければ空文字列
	UnicodeString Lookup(const UnicodeString &key_str) const;

	/// 割り当てを追加・上書きする
	void Assign(const UnicodeString &key_str, const UnicodeString &command);

	/// 現在の割り当て一覧 ("キー名=コマンド名")
	const TStringList *Entries() const { return entries_.get(); }

private:
	void LoadDefaults();

	std::unique_ptr<TStringList> entries_;
};

#endif  // NYANFI_GUI_KEY_MAP_H
