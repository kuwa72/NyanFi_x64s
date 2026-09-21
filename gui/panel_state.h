/**
 * @file gui/panel_state.h
 * @brief パネル表示切替・ログスクロールの判断 (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の ShowIcon (26006行)、
 *          ScrollUpLog/ScrollDownLog (24774/24784行)、SetSubSize (25729行)。
 *
 *          ShowPreview/ShowProperty/ShowFKeyBar/ShowToolBar/MenuBar の
 *          ON/OFF/反転は VCL `SetToggleAction` (MainFrm.cpp:12651) そのもので、
 *          既存の `view_settings::ParseToggle/ApplyToggle`
 *          (または `text_display::ToggleValue`) をそのまま使うため
 *          ここには置かない。SetSubSize の引数解釈・適用も既存の
 *          `view_settings::ParseSubSize/ResolveSubSize` を使う。
 *          ここに置くのは上記に無い判断だけ (規約8):
 *
 *          - `HasToken`: VCL `TestActionParam` (MainFrm.cpp:12515)。
 *            ActionParam を ';' で分割し、各トークンを `SameText`
 *            (大小文字を区別しない完全一致) で比較する。ShowIcon の
 *            "AC" (キャッシュ消去) / "FD" (表示切替) の判定に使う
 *          - `NextIconModeFD`: ShowIcon の FD 分岐 (MainFrm.cpp:26014-26016)。
 *            `IconMode = (IconMode==0)? 1 : (IconMode==1)? 2 : 1;`
 *          - `ToggleIconMode`: 通常分岐 (MainFrm.cpp:26017-26021)。
 *            `sw=(IconMode>0)` を反転して `IconMode=sw?1:0` にする。
 *            モード2 (詳細表示) も「表示中」として扱う
 *          - `ScrollLogIndex`: ログスクロール後の注目行。
 *            VCL は `ListBoxScrollUp/Down(LogListBox, N)` で一覧を動かすが、
 *            こちらにログ専用ウィンドウは無いため、ログ行への注目位置だけを
 *            動かしてステータスに表示する簡略版にした。その位置計算だけを
 *            ここに置く。範囲外は端で止める (失敗しない)
 */
#ifndef NYANFI_GUI_PANEL_STATE_H
#define NYANFI_GUI_PANEL_STATE_H

#include "gui/file_item.h"
#include "gui/view_settings.h"

namespace panel_state {

/**
 * @brief 引数に指定トークンが含まれるか (VCL TestActionParam と同じ)
 * @param param ActionParam 全体 (';' 区切り)
 * @param token 探すトークン (大小無視の完全一致)
 */
bool HasToken(const UnicodeString &param, const UnicodeString &token);

/**
 * @brief FD 指定時の次の IconMode (VCL ShowIcon の FD 分岐と同じ)
 * @param current 現在の IconMode (0=非表示、1=表示、2=詳細)
 */
int NextIconModeFD(int current);

/**
 * @brief 通常トグル後の IconMode (VCL ShowIcon の通常分岐と同じ)
 * @param current 現在の IconMode
 * @param how view_settings::ParseToggle の結果
 */
int ToggleIconMode(int current, view_settings::Toggle how);

/**
 * @brief ログスクロール後の注目行
 * @param current 現在の注目位置 (-1 なら先頭の前)
 * @param total ログ行数
 * @param lines 動かす行数 (1以上を想定。0以下は1に丸める)
 * @param down true なら下へ、false なら上へ
 * @return 注目位置。[0, total-1] に丸める。total<=0 なら -1
 */
int ScrollLogIndex(int current, int total, int lines, bool down);

}  // namespace panel_state

#endif  // NYANFI_GUI_PANEL_STATE_H
