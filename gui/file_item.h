/**
 * @file gui/file_item.h
 * @brief 一覧の1行のデータ、並べ替え・マスク絞り込みの純粋ロジック
 *
 * wxWidgets に依存しない (wx/wx.h を include しない)。file_pane.h から使われる
 * ほか、tests/core/ の doctest からも直接 include できるようにするための分離。
 *
 * 並べ替え・マスク絞り込みの実際の判断根拠は Global.cpp / MainFrm.cpp の
 * 該当ロジックだが、そちらは GUI グローバル (SortMode[] / PathMask[] など) に
 * 強く依存しており Phase 2 の対象外 (未移植)。ここでは実装を直接呼ぶのではなく、
 * 同じ考え方を FileItem 単位の純粋関数として書き起こしている。
 * (詳細な対応関係は gui/file_item.cpp の各関数のコメントを参照)
 */
#ifndef NYANFI_GUI_FILE_ITEM_H
#define NYANFI_GUI_FILE_ITEM_H

/// 一覧の1行
struct FileItem {
	UnicodeString name;    //!< ファイル名 (パスを含まない)
	UnicodeString full_path;//!< フルパス。**結果リストのときだけ**入る
	                       //!< (通常の一覧はペインのディレクトリと name で決まるので空)
	Int64 size = 0;        //!< サイズ (ディレクトリは -1)
	TDateTime stamp;       //!< 最終更新日時
	int attr = 0;          //!< 属性 (faXXX)
	bool is_dir = false;   //!< ディレクトリか
	bool is_parent = false;//!< ".." か
	bool marked = false;   //!< マーク済みか
	bool matched = false;  //!< インクリメンタルサーチのキーワードに一致しているか
	                       //!< (表示のハイライトにのみ使う一時的な状態。gui/navigation.h を参照)

	//-- ワークリスト専用 (gui/work_list.h)。通常の一覧では既定値のまま ---------
	UnicodeString alias;      //!< 別名。空でなければ name の代わりに表示する
	                          //!< (MainFrm.cpp:10500 と同じ)
	bool is_separator = false;//!< 区切り行。名前も日時も持たず、横線として描く
	bool missing = false;     //!< 登録されているが実体が見つからない (VCL の faInvalid)
};

/**
 * @brief 項目のフルパスを組み立てる
 * @param dir 一覧が開いているディレクトリ (末尾の区切り文字は有っても無くてもよい)
 * @param item 対象
 * @return フルパス
 * @details `FileItem::full_path` が入っていれば**そちらを使う**。
 *          結果リスト (検索結果・grep 結果・ワークリスト) の項目は
 *          一覧のディレクトリとは別の場所にあるので、`dir + name` で
 *          組み立てると**別のファイルを指してしまう**
 */
UnicodeString FullPathOfItem(const UnicodeString &dir, const FileItem &item);

/// 並べ替えキー。SrtModDlg (ソートダイアログ) の並び (名前/拡張子/更新日時/サイズ/属性) に合わせてある
enum class SortKey { Name, Ext, Date, Size, Attr };

/**
 * @brief 2件の FileItem を並べ替え設定に従って比較する
 * @param a 比較対象1
 * @param b 比較対象2
 * @param key 並べ替えキー
 * @param descending true なら降順
 * @param dirs_first true ならディレクトリを先に集める (NyanFi の DirSortMode の
 *        「ディレクトリを区別しない」(5) 以外の全モードに相当。区別する場合の
 *        ディレクトリ同士の順序は常に自然順の名前で扱う簡略版)
 * @return a が先なら負、b が先なら正、同順なら 0
 * @details ".." は dirs_first / キーに関わらず常に先頭に来る
 */
int CompareFileItems(const FileItem &a, const FileItem &b, SortKey key, bool descending, bool dirs_first);

/**
 * @brief パスマスクに一致するか判定する (MainFrm.cpp::ApplyPathMask 相当)
 * @param mask セミコロン区切りのマスク文字列。空または "*" のみなら常に true。
 *        各要素の末尾が '\\' ならディレクトリ専用マスク (末尾の '\\' は除いて照合)、
 *        それ以外はファイル専用マスク。先頭が '!' なら除外指定
 * @param name 判定対象のファイル名 (パスを含まない)
 * @param is_dir 判定対象がディレクトリか
 * @return true 表示する (一致する)
 */
bool MatchPathMask(const UnicodeString &mask, const UnicodeString &name, bool is_dir);

#endif  // NYANFI_GUI_FILE_ITEM_H
