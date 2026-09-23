/**
 * @file gui/gen_info.h
 * @brief 汎用一覧ダイアログの wx 非依存な判断・整形 (TGeneralInfoDlg 移植)
 *
 * @details VCL 版の該当は `src/GenInfDlg.cpp` の `TGeneralInfoDlg`、
 *          UI 定義は `src/GenInfDlg.h` / `src/GenInfDlg.dfm`。判断だけを
 *          ここへ分離し、wx の画面と入力は `gui/gen_info_dialog.h/.cpp` が持つ。
 *
 *          `grep -n GeneralInfoDlg src/*.cpp` で実測した主な呼び出し元:
 *          - `src/NyanFi.cpp:83,216`: フォーム登録・生成
 *          - `src/MainFrm.cpp:12699-12725`: 外部ツール等の結果表示
 *          - `src/MainFrm.cpp:13091-13094`: スクリプト出力表示
 *          - `src/MainFrm.cpp:14440-14443`: CmdHistory (コマンド履歴)
 *          - `src/MainFrm.cpp:17093-17095,19271-19274`: 実行結果・差分詳細
 *          - `src/MainFrm.cpp:12691-12725`: ClipSaveList の LS 出力
 *          - `src/MainFrm.cpp:20947-21127`: ListNyanFi (NyanFi 情報)
 *          - `src/MainFrm.cpp:20880-20886`: ListClipboard (クリップボード)
 *          - `src/MainFrm.cpp:20912-20926`: ListFileName (ファイル名一覧)
 *          - `src/MainFrm.cpp:20936-20941`: ListLog (ログ)
 *          - `src/MainFrm.cpp:21143-21213`: ListText/ListTail/ViewTail
 *          - `src/MainFrm.cpp:21296-21299`: ListTree (ツリー表示)
 *          - `src/MainFrm.cpp:23697-23700`: PlayList
 *          - `src/usr_excmd.cpp:1778-1782`: 変数一覧
 *          - `src/GitView.cpp:1563-1568,1606-1612`: Git 差分・ファイル内容
 *          - `src/GitGrep.cpp:515-521`: Git grep 結果
 *          - `src/FindTxtDlg.cpp:68,218,231,297`: 検索語・正規表現の往復
 *
 *          移植済みの判断:
 *          - FormShow の Git / 「名前=値」自動判定 (lines 113-130)
 *          - UpdateList のエラー部分抽出・通常/AND/OR フィルタ (lines 353-447)
 *          - SortGenList の先頭数値優先の自然順 (lines 1490-1522)
 *          - 重複行の除去、元順序への復帰 (lines 1527-1569)
 *          - タブ前後の表示・値・複製の文字列化 (lines 451-462, 1174-1198)
 *          - ステータス文と次/前検索 (lines 467-567)
 *
 *          未移植 (未実装扱い):
 *          - Migemo 辞書検索、TSV/ツリー固有のフィルタ正規表現
 *          - 強調表示、Git グラフ、ログ色、変数名/拡張子などの専用描画
 *          - ファイル/ディレクトリ/プレイリスト等のモード固有操作
 *          - FindTxtDlg 連携、2 ストロークキー、独自スクロールパネル
 *          - ウィンドウ位置と各表示オプションの ini 永続化
 */
#ifndef NYANFI_GUI_GEN_INFO_H
#define NYANFI_GUI_GEN_INFO_H

#include <vector>

namespace gen_info {

/// VCL の isVarList/isLog/isGit 等の排他的な表示モード。
enum class Kind {
	Generic,
	Variable,
	Git,
	Log,
	FileList,
	Tree,
	CommandHistory,
	DirectoryNames,
	Playlist,
};

/// TGeneralInfoDlg::SortMode (1 / -1 / 0)
enum class SortMode { Original, Ascending, Descending };

/// 元一覧の行番号を保持した表示項目。
struct Entry {
	UnicodeString text;
	int source_index = -1;
};

/// UpdateList に渡す絞り込み条件。
struct FilterOptions {
	UnicodeString keyword;
	bool case_sensitive = false;
	bool any_term = false;  //!< true=AND/OR ('|'  区切りの OR グループ)
	bool errors_only = false;//!< ログの " >E " 行と四空白継続行だけ
};

/**
 * @brief VCL の FormShow と同じ自動判定を行う。
 * @param lines 元一覧
 * @param requested VCL 側のフラグ。Generic なら Git/変数一覧を自動判定する
 */
Kind DetectKind(const std::vector<UnicodeString> &lines, Kind requested = Kind::Generic);

/**
 * @brief 元一覧から現在の表示項目を作る。
 * @details エラー抽出は VCL `UpdateList` と同じ「E 行の後は四空白で始まる
 *          間だけ継続行扱い」。通常は検索語全体、部分一致で照合する。
 *          any_term=true は `filter_List(soAndOr)` と同様に、`|` 区切りの
 *          いずれかのグループに一致し、グループ内では空白区切りの全語一致とする。
 */
std::vector<Entry> BuildEntries(const std::vector<UnicodeString> &lines, Kind kind,
                                const FilterOptions &options);

/// comp_AscendOrder / comp_DescendOrder / comp_ObjectsOrder 相当で並べ替える。
void SortEntries(std::vector<Entry> &entries, SortMode mode);

/// SameStr (大文字小文字を区別する全文字一致) で重複を除く。
std::vector<Entry> RemoveDuplicates(const std::vector<Entry> &entries);

/// ファイル一覧・ツリーはタブ前、それ以外は元の行を表示する。
UnicodeString DisplayText(const UnicodeString &line, Kind kind);

/// ファイル一覧・ツリーのタブ付き行から実ファイル名を取り出す。
UnicodeString TargetText(const UnicodeString &line, Kind kind);

/// 「名前=値」の最初の '=' より右を返す (VCL get_tkn_r 相当)。
UnicodeString ValueText(const UnicodeString &line);

/// 選択した行を表示文字列として CRLF 区切りで連結する。
UnicodeString JoinEntries(const std::vector<Entry> &entries, const std::vector<int> &indices,
                          Kind kind = Kind::Generic);

/// 現在位置自身を除外し、前/後方向の最初の検索一致を返す。
int FindNext(const std::vector<Entry> &entries, int current, const UnicodeString &keyword,
             bool forward, bool case_sensitive);

/// TGeneralInfoDlg::SetStatusBar の先頭パネル相当。
UnicodeString StatusText(int visible_count, int total_count, int selected_count, int cursor,
                         bool filtered, bool errors_only);

/// AddCmdHistory が作る VCL 履歴行から、時刻・画面モードを除いた部分。
UnicodeString CommandText(const UnicodeString &line);

}  // namespace gen_info

#endif
