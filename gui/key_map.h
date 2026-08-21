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
 * 実測: VCL 版 (src/OptDlg.cpp の InpKeyBtnClick / ExpKeyBtnClick) は ini の
 * "KeyFuncList" セクションに、1行 1エントリで
 *     <モード文字>:<キー名>=<コマンド名>
 * の形式 (TStringList の Name=Value) で持っている。モード文字は
 * usr_cmdlist.cpp の `ScrModeIdStr = "FSVIL"` の1文字 (F=ファイルペイン /
 * S=検索 / V=ビューア / I=画像ビューア / L=リスト、src/OptDlg.cpp の
 * GetCmdModeStr 参照)。キー名は SELECT+ (選択操作用の修飾子、src/Global.cpp
 * の KeyStr_SELECT) が付くことや、2ストロークキーを表す '~' 区切り
 * ("Ctrl+K~D" 等、src/OptDlg.cpp の IsFirstCmdKey 参照) を含むことがある。
 *
 * Phase 2 の骨格はファイルペイン (F) 1面しか無く、SELECT+ (選択操作) と
 * 2ストロークキーには対応していないため、ini 読み込みは
 *  - モード文字が "F" のエントリだけを対象にする
 *  - SELECT+ が付くもの、キー名に '~' を含むものは読み飛ばす (対応する
 *    仕組みが無いため。無言で誤動作させるよりは何もしない方を選んだ)
 * という単純化をしている (ParseKeyFuncListEntry)。将来 F 以外のモードや
 * 選択操作、2ストロークキーに対応したら、この単純化を見直すこと。
 */
#ifndef NYANFI_GUI_KEY_MAP_H
#define NYANFI_GUI_KEY_MAP_H

#include <memory>

// wxKeyEvent は KeyStrOf() の引数の型としてしか使わない (メンバとしては
// 持たない) ので、前方宣言だけにして <wx/wx.h> の依存を避ける。これにより
// tests/core/test_gui_settings.cpp のような wx 非リンクのテストからも
// このヘッダを直接インクルードできる (実装は key_map.cpp 側で <wx/wx.h> を
// 読み込む)。
class wxKeyEvent;

/**
 * @brief キー割り当て表
 */
class KeyMap {
public:
	KeyMap();

	/// wx のキーイベントを NyanFi のキー名 ("DOWN" / "Ctrl+F5" など) にする
	static UnicodeString KeyStrOf(const wxKeyEvent &event);

	/// wx のキーコード (`wxKeyEvent::GetKeyCode()`) を Windows の仮想キーコードにする。
	/// 対応するものが無ければ 0。
	///
	/// **wx の `GetKeyCode()` は仮想キーコードではない。** 英数字と一部の制御キー
	/// (BackSpace=8 / Tab=9 / Enter=13 / Esc=27 / Space=32) は VK と同値だが、
	/// 矢印・PgUp/PgDn・Home/End・Ins/Del・F1〜F12 は `WXK_START` (300) からの
	/// 連番 (wx/defs.h の `wxKeyCode`) で、VK とはまったく別の値になる。
	/// これを `get_KeyStr(WORD, TShiftState)` にそのまま渡すと、英数字だけ動いて
	/// カーソル移動や F キーが無反応になる (実際にそうなっていた。報告書 §16.5)。
	///
	/// MSW では `GetRawKeyCode()` が `WM_KEYDOWN` の wParam = 仮想キーコードその物
	/// なので `KeyStrOf()` はそちらを優先する。この関数はそれが取れなかったときの
	/// 経路で、OEM キー (`:` `@` `[` など) は wx が ASCII に畳んでいるため戻せない。
	static WORD VkFromWxKeyCode(int wx_keycode);

	/// キー名に割り当てられたコマンド名を返す。無ければ空文字列
	UnicodeString Lookup(const UnicodeString &key_str) const;

	/// 割り当てを追加・上書きする
	void Assign(const UnicodeString &key_str, const UnicodeString &command);

	/// 現在の割り当て一覧 ("キー名=コマンド名")
	const TStringList *Entries() const { return entries_.get(); }

	/// ini の "KeyFuncList" セクション (モード "F" のみ) を読み込み、
	/// 既定の割り当てを上書きする。ini が存在しない/セクションが無い/
	/// 読み込めるエントリが無い場合は何もしない (既定のまま)
	void LoadFromIni(const UnicodeString &ini_path);

	/// KeyFuncList の1エントリ (Name="F:Ctrl+Q" 等, Value=コマンド名) を
	/// Phase 2 骨格が扱える形 (key_str, command) に変換する。
	/// モードが "F" 以外、SELECT+ 付き、2ストローク ('~' を含む) は
	/// 対応していないため false を返す
	static bool ParseKeyFuncListEntry(const UnicodeString &name, const UnicodeString &value,
	                                   UnicodeString &key_str_out, UnicodeString &command_out);

private:
	void LoadDefaults();

	std::unique_ptr<TStringList> entries_;
};

#endif  // NYANFI_GUI_KEY_MAP_H
