/**
 * @file gui/tag.h
 * @brief タグ管理ダイアログの入力解決 (wx 非依存)
 *
 * @details VCL は `src/TagDlg.cpp` の `TTagManDlg` を `CmdStr` で
 *          AddTag / SetTag / FindTag / TagSelect / FindFolderIcon に使い分ける。
 *          その「コマンドを直接実行するかダイアログを開くか」と入力の正規化だけを
 *          ここに置いた。wxDialog は gui/tag_dialog.h 側にある。
 *
 *          VCL 呼び出し位置 (grep 実測):
 *          - AddTag:   src/MainFrm.cpp:13422-13448
 *          - FindTag:  src/MainFrm.cpp:19087-19129
 *          - SetTag:   src/MainFrm.cpp:25804-25850
 *          - TagSelect:src/MainFrm.cpp:26729-26780
 *          - FindFolderIcon: src/MainFrm.cpp:18152-18172
 */
#ifndef NYANFI_GUI_TAG_H
#define NYANFI_GUI_TAG_H

#include <vector>

#include "usr_str.h"

namespace tag {

/// TTagManDlg::CmdStr のモード
enum class Mode { Add, Set, Find, Select, FolderIcon };

/// ダイアログから返す検索・設定条件
struct Options {
	Mode mode = Mode::Set;
	UnicodeString tags;
	bool and_match = true;
	bool resolve_links = false;
	bool hide_input = false;
	bool select_mask = false;
	bool reverse_colors = false;
	bool show_count = false;
};

/// コマンドパラメーターを解決した結果
struct InputPlan {
	bool show_dialog = true;  //!< true なら wx ダイアログを起動する
	UnicodeString tags;
	bool and_match = true;
	bool match_all = false;   //!< FindTag / TagSelect の "*"
};

/**
 * @brief タグ入力欄の文字列を重複と空項目を除いて分割する
 * @details VCL TagDlg.cpp:339-347 / usr_tag.cpp:162-185 と同じ「; 区切り」。
 *          空白は落下させる。
 */
std::vector<UnicodeString> SplitTags(const UnicodeString &text);

/**
 * @brief タグ列を ; 区切りへ戻す
 * @param trailing_semicolon VCL が入力欄末尾の ';' を保持したのと同じ指定
 */
UnicodeString JoinTags(const std::vector<UnicodeString> &tags, bool trailing_semicolon = false);

/**
 * @brief VCL の ActionParam を入力計画へ変換する
 * @details 空または ";" ならダイアログを開く。FindTag / TagSelect の "*" は
 *          全タグ一致、それ以外の直接値はダイアログを通さない (VCL 実測処理)。
 */
InputPlan ResolveInput(Mode mode, const UnicodeString &param, const UnicodeString &initial_tags);

/** @brief ダイアログの表題 (FindTag は AND/OR を出す) */
UnicodeString Title(Mode mode, bool and_match);

/**
 * @brief VCL TagDlg.cpp:644-658 と同じ検索コマンドファイルを組み立てる
 * @param opposite true なら先頭に ToOpposite を入れる
 */
UnicodeString BuildSearchCommand(const UnicodeString &tags, bool and_match, bool opposite);

/**
 * @brief FolderIcon.INI の [FolderIcon] からアイコン一覧を読む
 * @details VCL Global.cpp:10723-10736 の get_FolderIconList と同じ値を返す。
 *          相対パスは既存 get_FolderIconList と同じ to_absolute_name で解決する。
 */
std::vector<UnicodeString> LoadFolderIcons(const UnicodeString &ini_path);

/**
 * @brief ダイレクトタグジャンプの入力解決 (V:TagJumpDirect/TagViewDirect)
 * @details VCL の DirectTagJumpCore (src/MainFrm.cpp:33118-33170) の簡易版。
 *          ctags フォーマット ("タグ名\tファイル名\t行番号") または
 *          ctags パターン ("タグ名\tファイル名\t/^pattern/") なら直接ジャンプ先として解釈。
 *          それ以外はタグ名として tags ファイル検索が必要 (実際の検索は wx 側の仕事)。
 * @param param コマンドパラメーター
 * @param cur_word 現在の単語 (param が空のときのタグ名)
 * @param cur_path 現在のファイルパス (基準ディレクトリ)
 * @param is_edit true なら編集 (TagJumpDirect)、false なら閲覧 (TagViewDirect)
 * @return ジャンプ先の情報。tags ファイル検索が必要な場合は file_path が空
 */
struct TagJumpTarget {
	bool is_edit = false;           //!< 編集 (TagJumpDirect) か閲覧 (TagViewDirect) か
	UnicodeString tag_name;         //!< タグ名 (tags ファイル検索用)
	UnicodeString base_dir;         //!< 基準ディレクトリ (tags ファイル検索用)
	UnicodeString file_path;        //!< ジャンプ先ファイル (ctags フォーマット時のみ)
	int line_no = 1;                //!< ジャンプ先行番号 (1ベース)
};

TagJumpTarget ResolveTagJump(const UnicodeString &param, const UnicodeString &cur_word,
                             const UnicodeString &cur_path, bool is_edit);

}  // namespace tag

#endif  // NYANFI_GUI_TAG_H
