/**
 * @file gui/list_search.h
 * @brief L モードのログエラー移動と S モードのインクリメンタルサーチ操作 (wx 非依存)
 *
 * @details L モード (ログウィンドウ) の NextErr/PrevErr と、S モード
 *          (インクリメンタルサーチ) の頻度上位コマンドのうち、wxWidgets に
 *          依存しない判断部分をここにまとめる。gui/main_frame.h/.cpp
 *          (キー入力の受け口・画面表示) から使われるが、このヘッダ自体は
 *          <wx/wx.h> を include しないため、tests/core/test_gui_list_search.cpp
 *          から直接テストできる (nyanfi_gui_core、ルート CMakeLists.txt を参照)。
 *
 *          VCL 版との対応 (いずれも実測):
 *          - NextErr/PrevErr は MainFrm.cpp::ExeCommandL (13377行) の正規表現
 *            `^.>([ECW]|(     [45]\\d{2})) .*` をそのまま使う。タスク系
 *            (CancelAllTask/PauseAllTask/Suspend/TaskMan) はタスクスレッド
 *            (未移植) が前提のため対象外。ClipCopy (VIL) は画像ビューアの
 *            クリップボード転送で L モードの管轄ではないため対象外。
 *            ToLeft/ToRight・CursorUp/Down・PageUp/Down・ClearLog/ViewLog は
 *            移植済みのためここには入れない。
 *          - S モードは MainFrm.cpp::FileListIncSearch (12030行) と
 *            set_IncSeaStt (11945行) が本体。Migemo 辞書 (usr_migemo) は
 *            移植済みだが migemo.dll が無い環境では無効になるため、
 *            ここでは辞書の有無を dict_ready で受けて切り替えるだけにする
 *            (実際の辞書引きは gui/navigation.h のスコープ外とした判断を踏襲)。
 *          - 履歴の上限 50 は VCL 版の ini 既定値 `IncSeaHistory=50`
 *            (Global.cpp:2013) に合わせる。実際に上限で捨てる処理は
 *            呼び出し側 (gui/main_frame.cpp) が持つ。
 */
#ifndef NYANFI_GUI_LIST_SEARCH_H
#define NYANFI_GUI_LIST_SEARCH_H

#include <vector>

namespace list_search {

//---------------------------------------------------------------------------
// L モード: ログのエラー位置への移動 (NextErr / PrevErr)
//---------------------------------------------------------------------------

/**
 * @brief ログ行がエラー位置 (次/前のエラーへ飛ぶ対象) かを判定する
 * @param line 表示用のログ行 (gui/log_win.h の FormatLine の出力想定)
 * @return エラーなら true
 * @details VCL 版 (MainFrm.cpp:13383) のパターン `^.>([ECW]|(     [45]\\d{2})) .*`
 *          そのまま。E=エラー・C=中断・W=警告の状態文字か、HTTP 状態コード風の
 *          4xx/5xx を含む行が対象。FormatLine は " >" + 時刻 + 状態文字 + 本文の
 *          形なので、先頭の " >" が "^.>" に一致する。
 */
bool IsErrorLine(const UnicodeString &line);

/**
 * @brief 現在位置から次 (または前) のエラー行を探す
 * @param lines 表示中のログ行 (表示順)
 * @param from 現在位置 (VCL 版の ItemIndex。-1 なら先頭扱いにするのは
 *        呼び出し側の責任で、ここではそのまま from+1/from-1 から探す)
 * @param forward true なら後の方向、false なら前の方向に探す
 * @return 見つかった行の添字。VCL 版と同じく**折り返さない**ので、
 *         その方向に無ければ -1 (呼び出し側が beep する)
 */
int FindNextError(const std::vector<UnicodeString> &lines, int from, bool forward);

//---------------------------------------------------------------------------
// S モード: MigemoMode / NormalMode (サーチ方式の切り替え)
//---------------------------------------------------------------------------

/**
 * @brief Migemo モードの切り替え (S:MigemoMode)
 * @param is_migemo 現在 Migemo モードか
 * @param dict_ready Migemo 辞書が使えるか (VCL 版の usr_Migemo->DictReady)
 * @return 切り替え後の状態
 * @details VCL 版 (MainFrm.cpp:12066) と同じ:
 *          `is_Migemo = (!is_Migemo && DictReady)`。辞書が無ければ
 *          何度押しても OFF のまま (呼び出し側が警告を出す)
 */
bool ToggleMigemoMode(bool is_migemo, bool dict_ready);

/**
 * @brief 通常のサーチモードに戻す (S:NormalMode)
 * @return 常に false (VCL 版と同じく無条件で OFF)
 */
bool SetNormalMode(bool is_migemo);

//---------------------------------------------------------------------------
// S モード: KeywordHistory (キーワード履歴からの選択)
//---------------------------------------------------------------------------

/**
 * @brief 履歴のうち一覧に一致する候補だけを残す (S:KeywordHistory)
 * @param history キーワード履歴 (新しい順。VCL 版の IncSeaHistory)
 * @param names 一覧の項目名 (表示順)
 * @return 一致可能な候補 (history の順序を保つ)。1件も無ければ空
 *         (VCL 版は Abort して beep するので、呼び出し側が警告を出す)
 * @details VCL 版 (MainFrm.cpp:12097) は履歴の各語が一覧のいずれかに
 *          一致するかを1件ずつ確かめる。照合自体は自前で書かず、
 *          移植済みの IncrementalSearchMatch (gui/navigation.h) を使う。
 *          VCL 版は IncSeaFuzzy/大小文字区別の設定で照合を変えるが、
 *          ここでは既定 (AND/OR・大小文字を区別しない) の簡略版にした
 *          (**推測ではなく意図した簡略化**。設定の参照は呼び出し側の役割と
 *          判断した。報告書に明記する)。
 */
std::vector<UnicodeString> FilterKeywordHistory(const std::vector<UnicodeString> &history,
                                                 const std::vector<UnicodeString> &names);

//---------------------------------------------------------------------------
// S モード: IncMatchSelect (マッチ項目の一括選択) と IncSearchTop (先頭から再検索)
//---------------------------------------------------------------------------

/**
 * @brief 一致している位置だけを集める (S:IncMatchSelect の対象決め)
 * @param matched 項目ごとの一致状態 (表示順)
 * @return 一致している添字 (昇順)
 * @details VCL 版は set_IncSeaStt(true) で一致項目を選択・不一致項目の選択を
 *          外す。ここでは対象の添字決めだけを行い、実際のマーク付けは
 *          呼び出し側 (FilePane::VisibleItems/ApplyMarks) が行う。
 */
std::vector<int> CollectMatchedIndices(const std::vector<bool> &matched);

/**
 * @brief 先頭から探し直す (S:IncSearchTop)
 * @param names 一覧の項目名 (表示順)
 * @param keyword サーチキーワード。空なら必ず見つからない (-1)
 * @return 最初に一致した項目の添字。無ければ -1
 * @details VCL 版 (MainFrm.cpp:12160) は s_idx=0 から find_NextIncSea
 *          (ループありなら先頭にも戻る)。ここでは「先頭を含めて最初の一致」
 *          を返す単純化で、結果は等価 (ループする実装のため)。
 */
int FindFromTop(const std::vector<UnicodeString> &names, const UnicodeString &keyword);

}  // namespace list_search

#endif  // NYANFI_GUI_LIST_SEARCH_H
