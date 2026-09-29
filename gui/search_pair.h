/**
 * @file gui/search_pair.h
 * @brief 対応する括弧・HTMLブロック・行パターンの検索 (wx 非依存)
 *
 * @details VCL 版の該当は `src/TxtViewer.cpp:4488-4667` (SearchPairCore /
 *          SearchPair) と `src/usr_highlight.cpp:944-973` (GetSearchPairPtn)。
 *          VCL 版は TRegEx と TStringList を直接使うが、ここでは
 *          wx に依存しない純関数として切り出した。
 *
 *          未移植 (未実装扱い):
 *          - VCL の SearchPair における「括弧」モード (UpdatePairPos 連携)。
 *            VCL はカーソル位置の文字が括弧かどうかを判定してから探索を始めるが、
 *            ここでは呼び出し側が「括弧ペアのインデックス」を渡す方式に簡略化した。
 *          - VCL の SearchPair における「HTML ブロック」モード
 *            (test_HtmlExt による判定と GetCurWord によるタグ取得)。
 *            ここでは「タグ名を渡す」方式に簡略化した。
 *          - VCL の SearchPair における「行対応(パラメータ指定)」モードの
 *            詳細な書式チェック (is_regex_slash による /.../ の確認)。
 *            ここでは「/.../;.../ 形式かどうか」を判定する簡易版にした。
 */
#ifndef NYANFI_GUI_SEARCH_PAIR_H
#define NYANFI_GUI_SEARCH_PAIR_H

#include <vector>

#include "usr_str.h"

namespace search_pair {

/// 括弧ペアの定義 (VCL: br_str / kt_str の対応)
struct BracketPair {
	wchar_t open;   //!< 開き括弧
	wchar_t close;  //!< 閉じ括弧
};

/// VCL の br_str / kt_str に対応する標準の括弧ペア一覧
const std::vector<BracketPair> &DefaultPairs();

/**
 * @brief 文字が開き括弧かどうか判定する
 * @details VCL: br_str = "（〔［｛〈《「『【({[｢"
 */
bool IsOpen(wchar_t ch);

/**
 * @brief 文字が閉じ括弧かどうか判定する
 * @details VCL: kt_str = "）〕］｝〉》」』】)}]｣"
 */
bool IsClose(wchar_t ch);

/**
 * @brief 開き括弧に対応する閉じ括弧を返す
 * @details VCL: kt_str[p_b] (br_str の位置から kt_str の対応文字を取得)
 */
wchar_t Match(wchar_t open_ch);

/// 拡張子に対応する SearchPair 用パターンの種類
enum class PairPattern {
	None,    //!< 対応なし
	Pascal,  //!< .pas (begin...end)
	Cpp,     //!< .c/.h/.cpp (#if...#endif)
	Vbs,     //!< .vbs/.vb/.mac (sub...end sub など)
	Perl,    //!< .pod/.pl/.pm (=pod...=cut など)
};

/**
 * @brief 拡張子から SearchPair 用パターンの種類を判定する
 * @param ext 拡張子 (".pas" のようにドット付き。空なら None)
 * @details VCL: GetSearchPairPtn (usr_highlight.cpp:944-973)
 */
PairPattern GetPairPattern(const UnicodeString &ext);

/**
 * @brief SearchPair のパラメータを開始/終了正規表現に分解する
 * @param param "/開始/;/終了/" 形式 (VCL TxtViewer.cpp:4620-4640 の "/～/;～/" 解析)
 * @param begin_ptn 開始正規表現 (出力)
 * @param end_ptn 終了正規表現 (出力)
 * @return 分解できたら true。空・形式不正は false
 */
bool ParsePairParam(const UnicodeString &param, UnicodeString &begin_ptn, UnicodeString &end_ptn);

/**
 * @brief ペアパターンを「開始正規表現」と「終了正規表現」に分解する
 * @param pattern ペアパターンの種類
 * @param begin_ptn [o] 開始パターン
 * @param end_ptn [o] 終了パターン
 * @return 分解できたら true (pattern が None でない)
 * @details VCL: GetSearchPairPtn が返す "begin\tend" 形式の文字列を分割する。
 *          ここではハードコードしたパターンを返す。
 */
bool GetRegexPair(PairPattern pattern, UnicodeString &begin_ptn, UnicodeString &end_ptn);

/**
 * @brief 正規表現ペアで対応行を下方向に探す
 * @param lines 行一覧
 * @param cur_y カーソルの行位置 (0始まり)
 * @param begin_ptn 開始正規表現
 * @param end_ptn 終了正規表現
 * @return 見つかった行 (0始まり)。見つからないなら -1
 * @details VCL: SearchPairCore (TxtViewer.cpp:4488-4540)
 *          VCL は「現在行が begin_ptn にマッチするなら下方向に end_ptn を探し、
 *          現在行が end_ptn にマッチするなら上方向に begin_ptn を探す」という
 *          2段構えの探索を行う。ここでは下方向のみの探索とする。
 *          ネストする同じ種類のパターンはレベルカウントで扱う。
 */
int SearchPairCore(const std::vector<UnicodeString> &lines, int cur_y,
                   const UnicodeString &begin_ptn, const UnicodeString &end_ptn);

/**
 * @brief カーソル位置の括弧から対応する括弧を探す (V:SearchPair の括弧モード)
 * @param lines 行一覧
 * @param cur_x カーソル列位置 (1ベース)
 * @param cur_y カーソル行位置 (0ベース)
 * @param down true なら下方向、false なら上方向
 * @return 見つかった位置 (x=列 1ベース, y=行 0ベース)。見つからなければ x<0
 * @details VCL の UpdatePairPos (TxtViewer.cpp:4488-4540) の簡易版。
 *          ネストする同じ種類の括弧はレベルカウントで扱う。
 */
std::pair<int, int> FindBracket(const std::vector<UnicodeString> &lines, int cur_x, int cur_y, bool down);

/**
 * @brief 選択文字列を検索する (FindSelUp/FindSelDown)
 * @param lines 行一覧
 * @param sel_text 選択文字列 (空なら最後に検索した文字列を再利用)
 * @param cur_y カーソルの行位置 (0始まり)
 * @param up true なら上方向、false なら下方向
 * @return 見つかった行 (0始まり)。見つからないなら -1
 * @details VCL: SearchSel (TxtViewer.cpp:4393-4409)
 *          VCL は SearchUp/SearchDown を呼ぶが、ここでは FindNextLine を呼ぶ。
 *          em が true のときは RegExPtn/FindWord/Highlight を設定するが、
 *          ここでは純関数として検索のみ行う (強調表示は呼び出し側で行う)。
 */
int SearchSelection(const std::vector<UnicodeString> &lines,
                    const UnicodeString &sel_text, int cur_y, bool up);

/**
 * @brief リンクパターン (URL) を検索する (FindLinkUp/FindLinkDown)
 * @param lines 行一覧
 * @param cur_y カーソルの行位置 (0始まり)
 * @param up true なら上方向、false なら下方向
 * @return 見つかった行 (0始まり)。見つからないなら -1
 * @details VCL: FindLinkDown/FindLinkUp (TxtViewer.cpp:5265-5272)
 *          VCL は SearchDown/SearchUp を LINK_MATCH_PTN で呼ぶ。
 */
int SearchLink(const std::vector<UnicodeString> &lines, int cur_y, bool up);

/// VCL の LINK_MATCH_PTN (TxtViewer.h:15)
extern const wchar_t *LINK_MATCH_PTN;

}  // namespace search_pair

#endif  // NYANFI_GUI_SEARCH_PAIR_H
