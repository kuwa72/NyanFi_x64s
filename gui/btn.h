/**
 * @file gui/btn.h
 * @brief ツールバーボタン設定の CSV・並べ替え・保存ロジック (wx 非依存)
 *
 * @details VCL の `TToolBtnDlg` は `src/BtnDlg.cpp:23-356`、一覧の実体は
 *          `ToolBtnList`/`ToolBtnListV`/`ToolBtnListI` で、項目の実形式は
 *          `src/BtnDlg.cpp:317-320` と同じ 3 項目 CSV (表示名,コマンド,
 *          アイコン) である。実呼び出しは `src/MainFrm.cpp:36789-36805`。
 *          wx 版は現在のモードにないリストを空として扱い、編集結果だけを
 *          wx 専用 ini に保存する (ボタン配置の実描画は別処理)。
 *
 *          未移植 (未実装扱い):
 *          - ドラッグ/ドロップ、シェルアイコンのプレビュー、コマンドファイルの
 *            参照 UI、エディタ起動
 *          - ExtMenu/ExtTool の別名・アイコン解決 (入力可能なコマンド一覧は
 *            実際の usr_cmdlist の名前から作る)
 */
#ifndef NYANFI_GUI_BTN_H
#define NYANFI_GUI_BTN_H

#include <cstddef>
#include <vector>

#include "usr_str.h"

class UsrIniFile;

namespace btn {

/// 3 項目の CSV を表すボタン
struct Item {
	UnicodeString caption;
	UnicodeString command;
	UnicodeString icon;
};

/// 一覧が対象とする画面モード
enum class Mode { FileList = 0, TextView, ImageView };

/// CSV レコードから読む。項目数が不足해도空文字で補う。
Item ParseItem(const UnicodeString &record);

/// 3 項目 CSV に整形する
UnicodeString FormatItem(const Item &item);

/// 表示名が "-" のセパレータか
bool IsSeparator(const Item &item);

/// 追加/変更ボタンの有効条件 (VCL AddBtnActionUpdate/ChgBtnActionUpdate)
bool CanAdd(const Item &item);
bool CanChange(const std::vector<Item> &items, int index, const Item &item);

/// コマンド一覧の重複を除去し、エイリアス (先頭 $) を後ろへ足す。
/// all_commands には usr_cmdlist の実在名だけを渡す。
std::vector<UnicodeString> CommandChoices(Mode mode,
                                         const std::vector<UnicodeString> &all_commands,
                                         const std::vector<UnicodeString> &aliases = {});

/// ファイル/ディレクトリ選択後の VCL と同じ `Cmd_"path"` 形式。
UnicodeString WithPathParameter(const UnicodeString &command, const UnicodeString &path);

/// 1 始まりの実行番号を解決する。範囲外/不正なら -1。
int ResolveIndex(const UnicodeString &param, int count);

/// 項目を上/下へ動かす。動かせなければ false。
bool Move(std::vector<Item> &items, int index, int delta);

/// wx 専用 ini の保存対象
class Store {
public:
	const std::vector<Item> &Items() const { return items_; }
	std::vector<Item> &MutableItems() { return items_; }
	void Clear() { items_.clear(); }
	void LoadFromIni(UsrIniFile &ini);
	void SaveToIni(UsrIniFile &ini) const;

private:
	std::vector<Item> items_;
};

}  // namespace btn

#endif  // NYANFI_GUI_BTN_H
