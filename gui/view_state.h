/**
 * @file gui/view_state.h
 * @brief 表示の切り替え状態と、その計算 (wx 非依存の純粋ロジック)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の表示系コマンド。どれも
 *          グローバルなフラグを反転して再描画するだけなので、**フラグの意味と
 *          幅の計算だけをここに切り出す** (規約8)。
 *
 *          VCL の実装から実測した点:
 *          - `ShowHideAtr` / `ShowSystemAtr` は列挙のしかたを変えるので
 *            **一覧の読み直しが要る** (MainFrm.cpp:25992 が `ReloadList()`)
 *          - `ShowByteSize` / `HideSizeTime` は表示だけなので再描画で足りる
 *            (MainFrm.cpp:25901 / 19514)
 *          - `EqualListWidth` は `WidenCurList` に `"50"` と `"Left"` を
 *            渡したもの (MainFrm.cpp:17244)。独立した処理ではない
 *          - `WidenCurList` の既定は 0.75。右ペインが対象なら 1.0 - r
 *            (MainFrm.cpp:27634-27645)
 *          - 境界の移動量は ini の `BorderMoveWidth` で既定 50
 *            (Global.cpp:1611)
 */
#ifndef NYANFI_GUI_VIEW_STATE_H
#define NYANFI_GUI_VIEW_STATE_H

#include "gui/file_item.h"

namespace view_state {

/// 境界を1回動かす量 (画面上の比率)。VCL は画素数 (BorderMoveWidth=50) だが、
/// こちらは比率で持つので、1000px 前後の幅を想定して 0.05 とした
inline constexpr double kBorderStep = 0.05;
/// 左右の比率が取りうる範囲。片側が完全に潰れないようにする
inline constexpr double kMinRatio = 0.05;
inline constexpr double kMaxRatio = 0.95;

/// 左ペインの取り分 (0.0〜1.0) を範囲内に丸める
double ClampRatio(double ratio);

/**
 * @brief 境界を動かした後の左ペインの取り分を返す
 * @param ratio 現在の取り分
 * @param direction 負なら左へ、正なら右へ
 */
double MoveBorder(double ratio, int direction);

/**
 * @brief 片側を広げたときの左ペインの取り分を返す (WidenCurList)
 * @param widen_left true なら左を広げる
 * @param share 広げる側の取り分 (既定 0.75。MainFrm.cpp:27638)
 * @details 右を広げる場合は `1.0 - share` が左の取り分になる
 *          (MainFrm.cpp:27645 の `if (tag==1) r = 1.0 - r;`)
 */
double WidenSide(bool widen_left, double share = 0.75);

/**
 * @brief 一覧に出す項目か (隠し属性・システム属性の絞り込み)
 * @param attr ファイル属性 (faXXX)
 * @param show_hidden 隠しファイルを出すか (ShowHideAtr)
 * @param show_system システムファイルを出すか (ShowSystemAtr)
 * @details VCL は列挙時に弾く (だから ShowHideAtr は ReloadList を呼ぶ)。
 *          **隠し属性とシステム属性は独立**で、両方立っているファイルは
 *          どちらか一方の設定が off なら出ない
 */
bool IsListedByAttr(int attr, bool show_hidden, bool show_system);

}  // namespace view_state

#endif  // NYANFI_GUI_VIEW_STATE_H
