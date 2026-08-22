/**
 * @file gui/history.h
 * @brief 履歴 (最近使ったファイル・コマンドの一覧) の状態と ini への保存
 *
 * @details 対象は VCL 版の4コマンド:
 *  - `EditHistory` (最近編集したファイル一覧) `src/MainFrm.cpp:16908`
 *    `EditHistoryActionExecute`
 *  - `ViewHistory` (最近閲覧したファイル一覧) `src/MainFrm.cpp:27571`
 *    (中身は `EditHistoryActionExecute` を `isView=true` で呼ぶだけ)
 *  - `RecentList`  (最近使ったファイル一覧)   `src/MainFrm.cpp:24125`
 *    `RecentListActionExecute`
 *  - `CmdHistory`  (コマンド履歴)             `src/MainFrm.cpp:14438`
 *    `CmdHistoryActionExecute`
 *
 * 持つのは「履歴リストの状態」と「ini への読み書き」だけ。一覧ダイアログの
 * 表示は wx 側 (`gui/main_frame.cpp`、未着手) の仕事なので入れない (規約8)。
 *
 * ## 実測して分かったこと
 *
 * - `EditHistory`/`ViewHistory` は VCL では `TextEditHistory`/
 *   `TextViewHistory` という2本の `TStringList` (`src/Global.cpp:693-694`)。
 *   追加は `add_TextEditHistory` (`src/Global.cpp:11093-11104`) で、
 *   **既にあれば大文字小文字を区別せず (`SameText`) 削除してから先頭へ
 *   挿入する** (`Insert(0, fnam)`)。重複して積まれることはない
 * - ini への保存は `L:TextEditHistory=50,true` / `L:TextViewHistory=50,true`
 *   (`src/Global.cpp:2005-2006`)。この `50,true` の意味は
 *   `LoadOptions`/`SaveOptions` (`src/Global.cpp:2394-2397`,`2520-2521`) が
 *   `UsrIniFile::LoadListItems`/`SaveListItems`
 *   (`src/UIniFile.h:106-107`、実装は `src/UIniFile.cpp:664-684`) に
 *   そのまま渡す引数そのもの: **`50` = `max_items` (保持する最大件数)、
 *   `true` = `del_quot` (`ReadString` で値の引用符を外すかどうか)**。
 *   「重複を許すか」のような MRU フラグでは *ない*
 * - `TextViewHistory` は実際には `パス,行番号,マーク一覧` の CSV
 *   (`add_ViewHistory`、`src/TxtViewer.cpp:4830-4844`)。このモジュールは
 *   「リストの状態」だけを持つ方針なので、行番号・マークは持たずパスだけを
 *   扱う (末尾の「入れなかったもの」参照)
 * - `RecentList` は VCL では **`TStringList` でも ini でもない**。
 *   `EditHistDlg.cpp:403-434` が Windows の「最近使ったファイル」フォルダ
 *   (`FOLDERID_Recent` の `*.lnk`) をダイアログを開くたびその場で列挙して
 *   いるだけで、一覧からの削除も `.lnk` を消す (`EditHistDlg.cpp:693-696`)。
 *   アプリ側で持つ「状態」は無い。ini キーも存在しない (`opt_def_list` に
 *   `RecentList` は無い)
 * - `CmdHistory` (`CmdHistoryActionExecute` が開く一覧) は VCL では
 *   `CommandHistory` という別の `TStringList` (`src/Global.cpp:713`)。
 *   `AddCmdHistory` (`src/Global.cpp:15995-16034`) は**末尾に追加するだけで
 *   重複を排除しない**。上限は `MAX_CMD_HISTORY=1000` (`src/Global.h:140`)
 *   で、超えたら**先頭 (=一番古い) から削除**
 *   (`while (Count>MAX_CMD_HISTORY) Delete(0)`)。**ini には一切保存されない**
 *   (`opt_def_list` に `CommandHistory` は無い。実行中だけのログで、
 *   `GeneralInfoDlg` がその場で見せるだけ)
 *
 * ## 推測で決めた点 (VCL にそのまま対応するものが無い)
 *
 * - `Kind::Recent` と `Kind::Command` は ini キー・重複排除・先頭移動の
 *   すべてが新規設計。`Kind::Edit`/`Kind::View` と同じ `HistoryList` を
 *   使い回すため、**`Kind::Command` も重複排除+先頭移動の MRU 方式にした**
 *   (VCL の `CommandHistory` は重複を許す末尾追加ログだが、そのまま持ち込むと
 *   「最近使ったコマンドを選び直す」という Phase 2 の用途に向かないと判断した)
 * - 上限超過時に切り捨てるのは**常に一番古い項目**という点は VCL の
 *   `EditHistory`/`ViewHistory`(保存時のみ切り詰め)/`CmdHistory`
 *   (追加のたびに先頭=最古を削除) のどちらとも矛盾しないが、
 *   **ここでは `Add` のたびに切り詰める** (VCL の Edit/View はメモリ上は
 *   保存するまで上限を超えたままにできる)。最終的に ini に書かれる内容は
 *   VCL と同じになる
 * - 大文字小文字は `SameText` 相当 (区別しない) に統一。`RecentList` は
 *   VCL 側に重複排除自体が無いので確認しようがなく、Windows のパスは
 *   大文字小文字を区別しないという前提で合わせた
 * - 実体が消えた項目を外す `DropMissingFiles` は VCL の
 *   `EditHistory`/`ViewHistory`/`CmdHistory` には無い (手動削除のみ、
 *   `del_HistItem`、`src/EditHistDlg.cpp:685-717`)。**`WorkListHistory` だけ
 *   起動時に同様の処理をしている** (`src/MainFrm.cpp:556-559`)。
 *   その考え方を流用した便利機能として追加した
 *
 * ## VCL にあるが入れなかったもの
 *
 * - `TextViewHistory` の行番号・マーク一覧 (CSV の2列目以降)。一覧の表示は
 *   wx 側の仕事で、このモジュールは「リストの状態」だけを持つ方針のため
 * - `RecentList` の実体 (Windows shell の `*.lnk` 列挙・削除)。VCL 版は
 *   アプリの状態を持たないので、そもそも移植すべき「状態」が無い
 * - `CommandHistory` の詳細なログ行フォーマット (時刻・画面モード・実行対象
 *   パスなどを1行にまとめたもの、`AddCmdHistory`)。ここでは単純な文字列の
 *   履歴として扱う
 *
 * ## 未検証
 *
 * - 一覧ダイアログでの見た目 (wx 側、未着手) は確認できない
 */
#ifndef NYANFI_GUI_HISTORY_H
#define NYANFI_GUI_HISTORY_H

#include <vector>

#include "UIniFile.h"

namespace history {

/// 履歴の種類。`IniKeyOf` で ini のセクション名と1対1に対応する
enum class Kind { Edit, View, Recent, Command };

//---------------------------------------------------------------------------
/**
 * @brief 1本の履歴リスト (MRU: 使うたびに先頭へ)
 * @details `Entries()` は常に新しい順。`Add` は既にある項目を
 *          大文字小文字を区別せず取り除いてから先頭へ挿入する
 *          (VCL の `add_TextEditHistory`、`src/Global.cpp:11093` と同じ)。
 *          上限を超えたら末尾 (一番古い項目) から切り捨てる
 */
class HistoryList {
public:
	/// @param max_items 保持する最大件数 (VCL の `L:...=N,...` の N に相当)
	explicit HistoryList(int max_items = 50);

	/**
	 * @brief 項目を追加する
	 * @param entry 追加する文字列 (ファイルパスなど。空文字は無視する)
	 * @details **既にあれば (大文字小文字を区別せず) 一旦取り除いてから
	 *          先頭へ挿入する。** 重複して積まれることはない。
	 *          追加後に件数が上限を超えていたら、末尾 (一番古い項目) から
	 *          切り捨てる
	 */
	void Add(const UnicodeString &entry);

	/// 一致する項目を取り除く (大文字小文字を区別しない。無ければ何もしない)
	void Remove(const UnicodeString &entry);

	/// すべて消す (VCL の `TextEditHistory->Clear()` に相当)
	void Clear();

	/// 新しい順の一覧
	const std::vector<UnicodeString> &Entries() const { return entries_; }

	/// 保持する最大件数
	int MaxItems() const { return max_items_; }

	/**
	 * @brief 実体が無くなったファイルを一覧から外す
	 * @return 外した件数
	 * @details ファイルとディレクトリの両方を実体として認める
	 *          (`Kind::Recent` がディレクトリを含みうるため)。
	 *          VCL の `EditHistory`/`ViewHistory`/`CmdHistory` 自体には
	 *          この自動整理は無い (上記ヘッダ解説参照)
	 */
	int DropMissingFiles();

	/**
	 * @brief ini から読み込んだ内容をそのままの順で入れる (`LoadFromIni` 専用)
	 * @details `Add` と違い、重複排除も先頭移動もしない。ini に保存されている
	 *          順序 (新しい順) をそのまま信用する。上限を超える分は末尾を切る
	 */
	void AssignLoaded(const std::vector<UnicodeString> &entries);

private:
	std::vector<UnicodeString> entries_;
	int max_items_;
};

//---------------------------------------------------------------------------
// ini とのやりとり
//---------------------------------------------------------------------------
/**
 * @brief 種類ごとの ini セクション名
 * @details `UsrIniFile::LoadListItems`/`SaveListItems` はこの名前をそのまま
 *          セクション名として使い、`Item1`,`Item2`,... の形でキーを書く
 *          (`src/UIniFile.cpp:664-684`)。
 *          `Kind::Edit`/`Kind::View` は VCL の ini キー名
 *          (`TextEditHistory`/`TextViewHistory`) にそのまま合わせてある。
 *          `Kind::Recent`/`Kind::Command` は VCL に対応する ini が無いため
 *          Phase 2 向けの新規名 (上記ヘッダ解説参照)
 */
UnicodeString IniKeyOf(Kind kind);

/// ini から読み込む (無ければ空のまま)。`out` の `MaxItems()` はそのまま使う
void LoadFromIni(UsrIniFile &ini, Kind kind, HistoryList &out);

/// ini へ書き出す
void SaveToIni(UsrIniFile &ini, Kind kind, const HistoryList &list);

}  // namespace history

#endif  // NYANFI_GUI_HISTORY_H
