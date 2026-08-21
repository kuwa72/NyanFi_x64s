/**
 * @file gui/settings.h
 * @brief GUI (wx 版) の設定の永続化 (ini)
 *
 * @details
 * VCL 版は ini の読み書きに `UsrIniFile` (src/UIniFile.cpp) を使っている。
 * このクラスも同じ `UsrIniFile` をそのまま使う。理由:
 *  - GUI 依存が無く (ポインタ引数の一部が TForm/TComboBox 等だが、この
 *    クラスが使う ReadString/WriteString/ReadInteger/WriteInteger/ReadBool/
 *    WriteBool/UpdateFile はすべて UnicodeString/int/bool だけの版で済む)、
 *    nyanfi_core に既に組み込まれているのでそのまま呼べる
 *
 * ただし保存先は VCL 版と同じ ini ファイルにしない。`UsrIniFile::UpdateFile()`
 * はコンストラクタで読み込んだ全セクションを毎回まるごと書き直す実装で、
 * 読み込み時にコメント行・空行を捨てる (`LoadValues` 参照) ため、VCL 版の
 * 既存 ini を直接開いて保存すると値は保たれても見た目 (コメント/空行/
 * セクション順) が変わってしまう。ユーザーの既存 ini を一切変えないよう、
 * GUI 版専用の別ファイル `<実行ファイル名>_wx.ini` に保存する
 * (`DefaultIniPath()`)。
 *
 * ini のセクション/キー名はこのクラス専用の新規のもの。VCL 版に対応する
 * ものは無いため、実測ではなく Phase 2 向けに決めたもの (推測ではなく新規設計)。
 */
#ifndef NYANFI_GUI_SETTINGS_H
#define NYANFI_GUI_SETTINGS_H

#include <memory>

#include "UIniFile.h"

//---------------------------------------------------------------------------
/// ウィンドウの位置・サイズ・最大化状態
struct WindowState {
	int left = -1;       //!< -1 = 未保存 (OS の既定位置に任せる)
	int top = -1;        //!< 同上
	int width = 1000;    //!< 既定値は gui/main_frame.cpp の初期サイズに合わせた
	int height = 640;
	bool maximized = false;
};

//---------------------------------------------------------------------------
/**
 * @brief GUI 版の設定 (ini 永続化)
 */
class Settings {
public:
	/// @param ini_path 保存先 ini のフルパス。無ければ既定値のまま (Load 相当)
	explicit Settings(const UnicodeString &ini_path);

	/// ini から読み込み直す (コンストラクタからも呼ばれる)
	void Load();

	/// 変更があれば ini に書き込む。書き込むのはこのクラスが管理する
	/// セクション/キーのみ (UsrIniFile が読み込んだ他セクションはそのまま
	/// 保持されて書き戻される)
	bool Save();

	/// 実行ファイルと同じ場所の既定の設定ファイルパス (`<exe名>_wx.ini`)
	static UnicodeString DefaultIniPath();

	/// 同じ ini ファイルを共有するための参照 (gui/tabs.h の TabManager::
	/// SaveToIni/LoadFromIni に渡す用)。別ファイルにすると UsrIniFile が
	/// それぞれ別に全セクションを持ち直し、片方の UpdateFile がもう片方の
	/// 書き込みを消してしまうため、同じ UsrIniFile インスタンスを共有する
	UsrIniFile &Ini() { return *ini_; }

	WindowState Window;
	UnicodeString LeftDir;   //!< 左ペインの最後に開いていたディレクトリ
	UnicodeString RightDir;  //!< 右ペインの最後に開いていたディレクトリ

private:
	std::unique_ptr<UsrIniFile> ini_;
};

#endif  // NYANFI_GUI_SETTINGS_H
