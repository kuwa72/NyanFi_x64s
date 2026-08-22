/**
 * @file gui/named_state.h
 * @brief 名前を付けた状態 (タブグループ / 結果リスト / 検索設定) の保存と読み込み (機能群21)
 *
 * @details 対象コマンドは VCL 側で次の7つ (すべて `src/MainFrm.cpp`):
 *          - `SaveTabGroupActionExecute` (24632行) / `SaveAsTabGroupActionExecute` (24609行) /
 *            `LoadTabGroupActionExecute` (21540行)
 *          - `SaveAsResultListActionExecute` (24552行) / `LoadResultListActionExecute` (21440行)
 *          - `SaveAsFindSetActionExecute` (24523行) / `LoadFindSetActionExecute` (21402行)
 *
 *          ここが担当するのは**ファイル書式の読み書きだけ**。どのファイルを選ぶかの
 *          ダイアログと、実際のタブ・一覧への反映は `gui/main_frame.cpp` (wx 依存) の仕事。
 *          wx に依存しない (規約8)。
 *
 *          実測して分かった3種類の書式:
 *
 *          | 種類 | 拡張子 | 文字コード | 書式 |
 *          |---|---|---|---|
 *          | タブグループ | `.ini` (`F_FILTER_INI`) | (下記参照) | このモジュール専用の新規設計 |
 *          | 結果リスト | `.txt` (`F_FILTER_TXT`) | **UTF-8 + BOM** (`saveto_TextUTF8`) | `;[ResultList]` ヘッダ + `;Key=Value` の検索情報 + `パス<TAB>別名` の項目行 (`.nwl` と同じ行書式、`gui/work_list.h` 参照) |
 *          | 検索設定 | `.ini` (`F_FILTER_INI`) | UsrIniFile 既定 (ANSI) | `FindSettings` セクションの Key=Value。`save_FindSettings`/`load_FindSettings` (Global.cpp:5192/5360) とほぼ同じキー名 |
 *
 *          **タブグループについて**: VCL の実体 (`TabList` の CSV + `tab_info`、
 *          `src/Global.h` 実測) は `gui/tabs.h` の `TabState` とはフィールドが違う
 *          (選択状態・ホームパス・階層同期・カスタムキャプションを Phase 2 骨格が
 *          持たない。詳細は `gui/tabs.h` 冒頭のコメント)。そのため VCL の
 *          タブグループファイル (TabList の CSV) をそのまま読むことはできず、
 *          **このモジュール専用の新規 ini 書式**にした。`gui/tabs.cpp` の
 *          `TabManager::SaveToIni`/`LoadFromIni` と考え方・キー名は似せてあるが、
 *          別ファイル・別セクション名 (あちらはアプリ再起動時のセッション復元、
 *          こちらはユーザーが名前を付けて保存し他人と共有できるファイル)。
 *          **VCL 版が保存したタブグループファイルはここでは読めない** (非互換。
 *          推測ではなく Phase 2 向けの新規設計として決めた)。
 */
#ifndef NYANFI_GUI_NAMED_STATE_H
#define NYANFI_GUI_NAMED_STATE_H

#include <vector>

#include "gui/file_item.h"
#include "gui/tabs.h"

class UsrIniFile;

namespace named_state {

/// 保存/読み込みの対象の種類
enum class Kind { TabGroup, ResultList, FindSet };

/// 種類ごとの既定の拡張子 (実測した VCL のダイアログフィルタに合わせる。
/// `F_FILTER_INI` = ".ini"、`F_FILTER_TXT` = ".txt"。src/UserMdl.h:46-47)
UnicodeString ExtensionOf(Kind kind);

//---------------------------------------------------------------------------
// タブグループ
//---------------------------------------------------------------------------
/**
 * @brief タブグループを ini へ書き出す (書式を組み立てるだけの純関数)
 * @param ini 書き込み先 (呼び出し側が `UsrIniFile` を用意する。ファイルへの
 *        実際の反映は呼び出し側の `ini.UpdateFile()` で行う)
 * @param tabs 対象のタブ一覧
 * @param current 現在選択中のタブの添字
 * @details セクション "TabGroup" に `Count` / `Current` と、
 *          `Tab%02d_Dir%d` / `Tab%02d_SortKey%d` / `Tab%02d_SortDesc%d` /
 *          `Tab%02d_DirsFirst%d` (`%d` はペイン番号 0/1) を書く。
 *          `gui/tabs.cpp` の `TabManager::SaveToIni` と同じキーの並びだが、
 *          セクション名を変えて別ファイルに書けるようにしてある
 */
void WriteTabGroupIni(UsrIniFile &ini, const std::vector<TabState> &tabs, int current);

/**
 * @brief ini からタブグループを読み込む (純関数)
 * @param ini 読み込み元
 * @param tabs_out 読み込んだタブ一覧 (空のまま返らない。1件も読めなければ変更しない)
 * @param current_out 現在選択中のタブの添字 (範囲外なら 0 に補正する)
 * @return セクションが無い/1件も読めなければ false (`tabs_out` は変更しない)
 */
bool ReadTabGroupIni(UsrIniFile &ini, std::vector<TabState> &tabs_out, int &current_out);

/// タブグループをファイルへ保存する (`SaveTabGroupActionExecute` / `SaveAsTabGroupActionExecute` 相当)
bool SaveTabGroup(const UnicodeString &path, const std::vector<TabState> &tabs, int current,
                   UnicodeString &error_out);

/// タブグループをファイルから読み込む (`LoadTabGroupActionExecute` 相当)
bool LoadTabGroup(const UnicodeString &path, std::vector<TabState> &tabs_out, int &current_out,
                   UnicodeString &error_out);

//---------------------------------------------------------------------------
// 結果リスト (別々のディレクトリの項目が混ざった一覧)
//---------------------------------------------------------------------------
/**
 * @brief 結果リストの各行を解釈する (実体の存在確認はしない。純関数)
 * @param lines ファイルの各行 (改行コードを含まない)
 * @param title_out 読み取った検索パス (`;Find_Path=...`。VCL が再オープン時の
 *        カレントパス表示に使う値。空のこともある)
 * @return 解釈できた項目。**先頭行が `;[ResultList]` でなければ空を返す**
 *         (VCL の `LoadResultListActionExecute` が `USTR_IllegalFormat` で
 *         中断する条件と同じ)
 * @details `;` で始まる行 (先頭の `;[ResultList]` を含む) は検索情報として
 *          読み飛ばす (`Find_Path` だけ拾う)。項目行は `.nwl` と同じ
 *          `パス <TAB> 別名` で、末尾が区切り文字ならディレクトリ、パスが空で
 *          別名が `-` なら区切り行 (`FileItem::is_separator`)。
 *          **`FileItem::full_path` を必ず埋める** (結果リストの項目は一覧の
 *          ディレクトリと無関係な場所にあるため。報告書 §21 の事故と同じ穴に
 *          落ちないように)
 */
std::vector<FileItem> ParseResultListLines(const std::vector<UnicodeString> &lines, UnicodeString &title_out);

/**
 * @brief 結果リストの各行を組み立てる (純関数)
 * @param title 検索パス (`;Find_Path=` として書く。空でもよい)
 * @param items 対象。`is_parent` (VCL の `is_up`) の項目は書き出さない
 *        (`SaveAsResultListActionExecute` の `fp->is_up` 除外と同じ)
 * @return 書き出す行 (`;[ResultList]` ヘッダを含む)
 * @details `Find_DirList` / `Find_Mask` / `Find_Keywd` など VCL が持つ他の
 *          検索条件は書かない (Phase 2 に検索機能そのものが無いため。
 *          「VCL にあるが入れなかったもの」として報告する)
 */
std::vector<UnicodeString> FormatResultListLines(const UnicodeString &title, const std::vector<FileItem> &items);

/// 結果リストをファイルへ保存する (`SaveAsResultListActionExecute` 相当。UTF-8 + BOM)
bool SaveResultList(const UnicodeString &path, const UnicodeString &title, const std::vector<FileItem> &items,
                     UnicodeString &error_out);

/**
 * @brief 結果リストをファイルから読み込む (`LoadResultListActionExecute` 相当)
 * @details VCL と同じく、**実体が見つからない項目は黙って読み飛ばす**
 *          (MainFrm.cpp:21485-21489。`gui/work_list.h` の「missing のまま残す」
 *          方針とは違う。既存の VCL の挙動としてそのまま踏襲する。規約6)
 */
bool LoadResultList(const UnicodeString &path, UnicodeString &title_out, std::vector<FileItem> &items_out,
                     UnicodeString &error_out);

//---------------------------------------------------------------------------
// 検索設定
//---------------------------------------------------------------------------
/**
 * @brief 検索設定 (実測して絞った主要項目のみ。詳細は報告を参照)
 * @details VCL の `flist_stt` (実測) のうち、パス検索の主要な項目だけを持つ。
 *          EXIF・GPS・タグ検索・本文検索・画像サイズ・ハッシュなどの拡張検索は
 *          Phase 2 に検索機能そのものが無く確かめようがないため持たない
 *          (「VCL にあるが入れなかったもの」として報告する)
 */
struct FindSet {
	//-- 検索の種類 (排他。すべて false なら通常のパス検索) --------------------
	bool is_tag = false;       //!< VCL: FindType="TAG" (タグ検索)
	bool tag_all = false;      //!< VCL: TAG_all (is_tag のときだけ意味を持つ。AND検索)
	bool is_mark = false;      //!< VCL: FindType="MARK" (栞検索)
	bool is_dup_icon = false;  //!< VCL: FindType="DICON" (アイコン重複検索)
	UnicodeString icons;       //!< is_dup_icon のときの対象アイコン一覧 (VCL: Icons。"\r\n" 区切りのまま保持)
	bool is_hard_link = false; //!< VCL: FindType="HLINK" (ハードリンク検索)
	UnicodeString link_name;   //!< is_hard_link のときの対象名 (VCL: Name)

	//-- 通常のパス検索 (上のどれでもないとき) ----------------------------------
	UnicodeString path;        //!< VCL: Path (検索の起点。再オープン時のカレントパスにも使う)
	UnicodeString dir_list;    //!< VCL: DirList
	UnicodeString skip_dir;    //!< VCL: SkipDir
	bool target_dir = false;   //!< VCL: Dir (ディレクトリも対象にする)
	bool target_both = false;  //!< VCL: Both
	bool sub_dir = false;      //!< VCL: SubDir (サブディレクトリを検索)
	bool include_arc = false;  //!< VCL: Arc (アーカイブの中も検索)
	bool exclude_trash = false;//!< VCL: xTrash
	bool res_link = false;     //!< VCL: ResLink
	bool dir_link = false;     //!< VCL: DirLink
	UnicodeString mask;        //!< VCL: Mask
	UnicodeString keywd;       //!< VCL: Keywd (空なら reg_ex/match_and/match_case は無意味)
	bool reg_ex = false;       //!< VCL: RegEx
	bool match_and = false;    //!< VCL: And ("and" は C++ の代替表記キーワードなので避けた)
	bool match_case = false;   //!< VCL: Case

	//-- 日時条件 (dt_mode<=0 なら無効) -----------------------------------------
	int dt_mode = 0;           //!< VCL: DT_mode
	int dt_rel = 0;            //!< VCL: DT_rel (相対日数。0 以外なら dt_value は
	                           //!< 保存せず、読み込み時に「今日から dt_rel 日」で
	                           //!< 計算し直す。VCL の `IncDay(Today(), dt_rel)` と同じ)
	TDateTime dt_value;        //!< VCL: DT_value (dt_rel==0 のときだけ ini に書く)
	UnicodeString dt_str;      //!< VCL: DT_str (画面表示用の元の文字列。dt_rel==0 のときだけ)

	//-- サイズ条件 (sz_mode<=0 なら無効) ---------------------------------------
	int sz_mode = 0;           //!< VCL: SZ_mode
	Int64 sz_value = 0;        //!< VCL: SZ_value

	//-- 属性条件 (at_mode<=0 なら無効) -----------------------------------------
	int at_mode = 0;           //!< VCL: AT_mode
	int at_value = 0;          //!< VCL: AT_value

	bool path_sort = false;    //!< VCL: PathSort
	int sort_mode = -1;        //!< VCL: SortMode
};

/**
 * @brief 検索設定を ini へ書き出す (純関数)
 * @details VCL の `save_FindSettings` (Global.cpp:5192) と同じセクション名
 *          "FindSettings" ・同じキー名を使う (拡張検索を除く)。
 *          呼び出し前に該当セクションを消してから書く (上書き保存で古いキーが
 *          残らないように。VCL も `EraseSection` している)
 */
void WriteFindSetIni(UsrIniFile &ini, const FindSet &set);

/// ini から検索設定を読み込む (純関数)。セクションが無ければ false
bool ReadFindSetIni(UsrIniFile &ini, FindSet &out);

/// 検索設定をファイルへ保存する (`SaveAsFindSetActionExecute` 相当)
bool SaveFindSet(const UnicodeString &path, const FindSet &set, UnicodeString &error_out);

/// 検索設定をファイルから読み込む (`LoadFindSetActionExecute` 相当)
bool LoadFindSet(const UnicodeString &path, FindSet &out, UnicodeString &error_out);

}  // namespace named_state

#endif  // NYANFI_GUI_NAMED_STATE_H
