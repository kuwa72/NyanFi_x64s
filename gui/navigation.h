/**
 * @file gui/navigation.h
 * @brief ディレクトリ移動を速くするための wx 非依存ロジック
 *
 * @details
 * インクリメンタルサーチ・ディレクトリ履歴 (戻る/進む/一覧)・ドライブ一覧・
 * パス直接入力の4機能のうち、wxWidgets に依存しない部分をここにまとめる。
 * gui/file_pane.h/.cpp (画面表示・カーソル移動) や gui/main_frame.h/.cpp
 * (キー入力の受け口・ダイアログ表示) から使われるが、このヘッダ自体は
 * <wx/wx.h> を include しないため、tests/core/test_gui_navigation.cpp から
 * 直接テストできる (nyanfi_gui_core、ルート CMakeLists.txt を参照)。
 *
 * それぞれの VCL 版との対応 (実測結果) は各クラス・関数のコメントを参照。
 */
#ifndef NYANFI_GUI_NAVIGATION_H
#define NYANFI_GUI_NAVIGATION_H

#include <functional>
#include <vector>

//---------------------------------------------------------------------------
// インクリメンタルサーチ
//---------------------------------------------------------------------------

/**
 * @brief インクリメンタルサーチのキーワード入力の状態
 *
 * @details VCL 版 (MainFrm.cpp::FileListIncSearch / IncSearchActionExecute /
 * ExitIncSearch、Global.cpp::update_IncSeaWord) の簡略版。
 *
 * VCL 版の update_IncSeaWord は JP/US キーボードごとの Shift+記号の変換表を
 * 自前で持っている (Global.cpp 参照)。これは VCL のキー入力が仮想キーコード
 * (VK_*) しか渡さず、OS のキーボードレイアウトによる翻訳結果を得られない
 * ためだが、wxWidgets は wxKeyEvent::GetUnicodeKey() で OS が翻訳済みの
 * 文字を渡してくる。そのため Phase 2 骨格では変換表を持たず、渡された文字を
 * そのままキーワードに足す/削るだけにしている。
 *
 * Migemo (ローマ字で日本語を検索するモード) には対応しない。usr_migemo.cpp
 * (MigemoUnit) は移植済みで migemo.dll が無い環境では自動的に無効化される
 * ため技術的には利用できるが、GUI 側のモード切替・状態表示までを含めた
 * 統合は Phase 2 骨格のスコープ外として見送った (要検討事項として報告に明記)。
 */
class IncrementalSearch {
public:
	/// サーチモード中か
	bool IsActive() const { return active_; }

	/// 現在のキーワード
	const UnicodeString &Word() const { return word_; }

	/// サーチモードへ入る (VCL 版の IncSearchActionExecute 相当)
	void Start()
	{
		active_ = true;
		word_ = EmptyStr;
	}

	/// サーチモードを抜ける (VCL 版の ExitIncSearch 相当)。キーワードも捨てる
	void Exit()
	{
		active_ = false;
		word_ = EmptyStr;
	}

	/// 末尾に1文字追加する
	void Append(wchar_t ch) { word_ += UnicodeString(ch); }

	/// 末尾の1文字を削除する。@return 削除できたら true (キーワードが空でなかった)
	bool Backspace();

private:
	bool active_ = false;
	UnicodeString word_;
};

/**
 * @brief 名前がインクリメンタルサーチのキーワードに一致するか判定する
 * @details VCL 版の既定 (Migemo 無効時、contains_upper で大小文字を自動判定
 * する部分は簡略化して常に大小文字を区別しない版) と同じ、移植済みの
 * contains_word_and_or() (usr_str.h) をそのまま使う。半角スペース区切りの
 * AND、'|' 区切りの OR に対応する (contains_word_and_or 自体の仕様)
 * @param name 判定対象の名前 (ファイル名など)
 * @param keyword サーチキーワード。空文字列なら常に false (VCL 版もキーワードが
 *        空の間は matched フラグを立てない)
 */
bool IncrementalSearchMatch(const UnicodeString &name, const UnicodeString &keyword);

/**
 * @brief 現在位置から周回的に次 (または前) の一致項目を探す
 * @details VCL 版の find_NextIncSea/find_PrevIncSea (未移植、GUI グローバルの
 * TStringList を直接舐める実装) に相当する、名前の配列版
 * @param names 一覧の項目名 (表示順)
 * @param keyword サーチキーワード。空なら必ず見つからない (-1)
 * @param start_index 探索を開始する位置 (この位置自体は対象に含めない)。
 *        一覧の外 (-1 など) を渡してもよい
 * @param forward true なら添字が増える方向、false なら減る方向に探す
 * @return 見つかった項目の添字。1周しても見つからなければ -1
 */
int FindIncrementalSearchMatch(const std::vector<UnicodeString> &names, const UnicodeString &keyword,
                                int start_index, bool forward);

//---------------------------------------------------------------------------
// ディレクトリ履歴 (戻る/進む/一覧)
//---------------------------------------------------------------------------

/**
 * @brief 1ペイン分のディレクトリ履歴 (戻る/進む/一覧)
 *
 * @details VCL 版 (Global.cpp の tab_info::dir_hist/dir_hist_p、
 * get_DirHistory/get_DirHistPtr、MainFrm.cpp の MoveDirHistCore) の簡略版。
 * VCL 版はタブ・ディレクトリ比較モードごとに複数系列の履歴を持つが、
 * Phase 2 骨格はタブが無いため1ペインにつき1系列のみ。
 *
 * 上限件数の既定値 30 は VCL 版の ini 既定値 `MaxDirHistory=30`
 * (Global.cpp) に合わせた (実測)。
 *
 * 履歴への追加は Navigate() を呼んだときだけ行う。Back()/Forward()/JumpTo()
 * (履歴をたどる移動) では追加しない (ブラウザの戻る/進むと同じ考え方)。
 * どちらを呼ぶかは呼び出し側 (gui/file_pane.cpp の FilePane::SetPath) の
 * 責任で切り分ける。
 */
class DirHistory {
public:
	/// VCL 版の既定値 (ini の MaxDirHistory の既定 30、Global.cpp で実測)
	static constexpr int kDefaultMaxEntries = 30;

	explicit DirHistory(int max_entries = kDefaultMaxEntries) : max_entries_(max_entries) {}

	/// 新しいディレクトリへ移動したことを記録する
	/// @details 直前と同じディレクトリなら何もしない (Reload 等での重複防止)。
	/// 履歴の途中から移動した場合は、その先の「進む」履歴を切り捨ててから追加する
	void Navigate(const UnicodeString &path);

	bool CanBack() const { return pos_ > 0; }
	bool CanForward() const { return pos_ >= 0 && pos_ + 1 < static_cast<int>(entries_.size()); }

	/// 1つ戻る。戻れなければ空文字列 (呼び出し側の状態は変えない)
	UnicodeString Back();

	/// 1つ進む。進めなければ空文字列 (呼び出し側の状態は変えない)
	UnicodeString Forward();

	/// 履歴の一覧から index (Entries() の添字) の位置へ直接移動する
	/// @return 移動先のパス。index が範囲外なら空文字列
	UnicodeString JumpTo(int index);

	/// 履歴一覧 (古い順)
	const std::vector<UnicodeString> &Entries() const { return entries_; }

	/// 現在位置 (Entries() の添字。履歴が空なら -1)
	int CurrentIndex() const { return pos_; }

private:
	std::vector<UnicodeString> entries_;
	int pos_ = -1;
	int max_entries_;
};

//---------------------------------------------------------------------------
// ドライブ一覧
//---------------------------------------------------------------------------

/**
 * @brief ドライブ種別 (Win32 の GetDriveType() の戻り値) の表示名
 * @details Global.cpp (SetDriveInfo 相当箇所、実測) の type_str への
 * 割り当てと同じ文言を使う
 */
/**
 * @brief ディレクトリ・スタック (PushDir / PopDir / DirStack)
 * @details VCL の `DirStack` (TStringList) 相当。実測した点:
 *          - `PushDir` は**先頭に挿入**する (MainFrm.cpp:24100 の `Insert(0, ...)`)。
 *            末尾に足すのではないので、後入れ先出しになる
 *          - `PopDir` は**存在しなくなったディレクトリを読み飛ばす**
 *            (MainFrm.cpp:23715 の `while (...) if (dir_exists(...))`)。
 *            消えたディレクトリでスタックが詰まらないようにするため
 *          - 空のときは何もしない (DirStack の表示も出さない)
 *
 *          VCL はパスと一緒にカーソル位置と並べ替えも積むが、ここでは
 *          パスとカーソル位置だけにした (並べ替えはペインの設定として
 *          持っており、スタックで戻す対象にしていないため)。
 */
class DirStack {
public:
	struct Entry {
		UnicodeString path;
		int cursor = 0;
	};

	/// 先頭に積む
	void Push(const UnicodeString &path, int cursor);

	/**
	 * @brief 先頭から取り出す。存在しないディレクトリは読み飛ばす
	 * @param out 取り出せた項目
	 * @return 取り出せたら true。スタックが空 (または全部消えていた) なら false
	 * @param exists ディレクトリの存在判定。テストから差し替えられるようにしてある
	 */
	bool Pop(Entry &out, const std::function<bool(const UnicodeString &)> &exists);

	int Count() const { return static_cast<int>(entries_.size()); }
	bool IsEmpty() const { return entries_.empty(); }
	const std::vector<Entry> &Entries() const { return entries_; }
	void Clear() { entries_.clear(); }

private:
	std::vector<Entry> entries_;
};

/**
 * @brief 次 (または前) のドライブを選ぶ (NextDrive / PrevDrive)
 * @param drives 利用できるドライブの一覧 ("C:\\" のような文字列)
 * @param current 現在のドライブ
 * @param forward true なら次、false なら前
 * @return 選んだドライブ。一覧が空なら空文字列
 * @details VCL の実装 (MainFrm.cpp:22361) は「現在より**辞書順で大きい**最初の
 *          ドライブ。無ければ先頭」。一覧中の位置を +1 するのではないので、
 *          現在のドライブが一覧に無くても動く (取り外した直後など)
 */
UnicodeString NextDriveOf(const std::vector<UnicodeString> &drives,
                          const UnicodeString &current, bool forward);

UnicodeString DriveTypeLabel(unsigned int drive_type);

//---------------------------------------------------------------------------
// パス直接入力
//---------------------------------------------------------------------------

/**
 * @brief 入力文字列を実在するディレクトリの絶対パスに解決する
 *
 * @details 環境変数・書式文字列の展開は移植済みの get_actual_path()
 * (usr_file_ex.h。内部で cv_env_var() を経由する cv_env_str() を呼ぶため
 * %VAR% だけでなく %ExePath%/$X/$D も展開できる) を使い、相対パスの解決は
 * 移植済みの to_absolute_name() (同じく usr_file_ex.h) を使う。実装を
 * 自前で書き直さない (CLAUDE.md の規約3)。
 *
 * VCL 版 (MainFrm.cpp::InputDirActionExecute) は UNC パスの疎通確認・
 * ユーザー名指定 (`user@path` 形式) ・ファイルパスの分離などかなり複雑な
 * 処理をしているが、Phase 2 骨格では「実在するディレクトリに解決できるか」
 * だけを見る簡略版にした。
 *
 * @param input 入力文字列 (環境変数・相対パスを含んでよい)
 * @param base_dir 相対パスの基準ディレクトリ (現在開いているペインのパス)
 * @param[out] resolved_out 解決できたときの絶対パス (末尾 "\" 付き)
 * @return true 解決でき、かつ実在するディレクトリだった
 */
bool ResolveDirectoryInput(const UnicodeString &input, const UnicodeString &base_dir, UnicodeString &resolved_out);

#endif  // NYANFI_GUI_NAVIGATION_H
