/**
 * @file gui/regdir.h
 * @brief 登録ディレクトリの判断ロジック (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/DirDlg.cpp` (`TRegDirDlg` の通常モード) と
 *          `src/Global.cpp` (`get_RegDirItem` / `move_top_RegDirItem`)。
 *          登録ディレクトリ一覧 (`RegDirList`) の1行は `REGDIR_CSVITMCNT = 4`
 *          の CSV (`key,title,path,user`、`src/Global.h`)。
 *            - キー (`[0]`) は `ChangeRegDir` のパラメータの1文字目
 *              (`ActionParam[1]`。`MainFrm.cpp::ChangeRegDirActionExecute`
 *              を実測) で、大文字小文字を区別せず探す (`SameText` を実測)
 *            - タイトル (`[1]`) が `"-"` の行はセパレータ (`is_separator`
 *              を実測)。選択できずパスは空扱い (`GetCurDirItem` を実測)
 *            - 使用後に先頭へ動かすときセパレータをまたがない
 *              (`move_top_RegDirItem` を実測)
 *            - 一覧のキー押下は一致が1件だけのときに確定し、複数あるときは
 *              カーソル移動だけ (`RegDirListBoxKeyPress` の f_cnt 分岐を実測)
 *            - フィルタ欄は特殊フォルダ側 (`filter_List` に `soAndOr` /
 *              `contains_upper` なら `soCaseSens` を付けて渡すのを実測) と
 *              同じ規則 (空白区切り・AND/OR・大文字混じりで大小文字区別) で
 *              タイトル・パス・キーに当てる。Migemo は未移植のため扱わない
 *              (未実装扱い)
 *
 *          未移植 (未実装扱い。落とさない):
 *          - 項目の追加・編集・削除・上下移動の UI (`AddItem`/`EditItem`/
 *            `DelItem`/`ChangeItemActionExecute`)
 *          - 環境変数表示 (`UseEnvVarAction`。`%VAR%` への置換)
 *          - 特殊フォルダ (`IsSpecial`) の EXE/PATH 合成一覧
 *            (`UpdateSpDirList` の `SpDirList` 構築部)。特殊フォルダの選択
 *            自体は `SpecialDirList` コマンドとして移植済み
 *          - ダイアログ位置・チェック状態の ini 永続化
 */
#ifndef NYANFI_GUI_REGDIR_H
#define NYANFI_GUI_REGDIR_H

#include <vector>

#include "usr_str.h"

class UsrIniFile;

namespace regdir {

// RegDirList の CSV 項目数 (src/Global.h の REGDIR_CSVITMCNT と同じ)。
// src/Global.h は VCL の巨大ヘッダのため直接は読まず値を複写する
constexpr int kCsvItemCount = 4;

//---------------------------------------------------------------------------
// 登録ディレクトリの1項目 (REGDIR_CSVITMCNT = 4: key,title,path,user)
//---------------------------------------------------------------------------
struct RegDirItem {
	UnicodeString key;    //!< アクセスキー ([0]。ChangeRegDir のパラメータ)
	UnicodeString title;  //!< 登録名 ([1]。"-" ならセパレータ)
	UnicodeString path;   //!< パス ([2])
	UnicodeString user;   //!< 接続ユーザ名 ([3])
};

//---------------------------------------------------------------------------
// CSV レコードとの変換
//---------------------------------------------------------------------------

/// CSV の1行を読む
RegDirItem ParseRecord(const UnicodeString &record);

/// CSV の1行に書く
UnicodeString FormatRecord(const RegDirItem &item);

//---------------------------------------------------------------------------
// セパレータと選択 (is_separator / GetCurDirItem を実測)
//---------------------------------------------------------------------------

/// タイトルが "-" の行か
bool IsSeparator(const RegDirItem &item);

/// カーソル位置の項目として選べるパス。セパレータなら空
UnicodeString SelectablePath(const RegDirItem &item);

//---------------------------------------------------------------------------
// 一覧の操作 (move_top_RegDirItem / RegDirListBoxKeyPress を実測)
//---------------------------------------------------------------------------

/// idx 番目の項目を、属するグループ (セパレータ区切り) の先頭へ動かす。
/// idx が先頭・範囲外なら何もせず false
bool MoveTop(std::vector<RegDirItem> &items, int idx);

/// キーに一致する (大文字小文字を区別しない) 項目の添字を全部返す。
/// 空キーなら空。VCL は1件のとき確定・複数のときカーソル移動だけする
std::vector<int> KeyMatches(const std::vector<RegDirItem> &items, const UnicodeString &key);

//---------------------------------------------------------------------------
// フィルタ (TRegDirDlg のフィルタ欄の絞り込み相当)
//---------------------------------------------------------------------------

/// フィルタ (空白区切り) に合うか。and_mode が真なら全語、偽ならいずれか
/// 1語の一致で通す。大文字を含むフィルタは大小文字を区別する
bool MatchesFilter(const RegDirItem &item, const UnicodeString &filter, bool and_mode);

//---------------------------------------------------------------------------
// 登録ディレクトリ一覧の ini 永続化 (gui/tabs.h の WxGuiTabs と同じ考え方。
// VCL 版の本物の ini を書き換えないよう "<exe名>_wx.ini" 専用の新規セクション)
//---------------------------------------------------------------------------
class RegDirStore {
public:
	RegDirStore() = default;

	const std::vector<RegDirItem> &Items() const { return items_; }
	std::vector<RegDirItem> &MutableItems() { return items_; }

	void SaveToIni(UsrIniFile &ini) const;
	void LoadFromIni(UsrIniFile &ini);

private:
	std::vector<RegDirItem> items_;
};

}  // namespace regdir

#endif  // NYANFI_GUI_REGDIR_H
