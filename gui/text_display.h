/**
 * @file gui/text_display.h
 * @brief テキスト表示設定のパラメータ解釈 (wx 非依存の純関数)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の ShowLineNo/ShowRuler/ShowTAB/
 *          ShowCR (33701行〜)、SetTab (34185行)、SetWidth、SetMargin、
 *          ScrollUpText/ScrollDownText (24808行)、ViewTail (27615行)。
 *
 *          VCL は `SetToggleAction` (MainFrm.cpp:12651) で ON/OFF/反転を
 *          決める。ここも同じ判定にする。既定値は `src/Global.cpp` の
 *          オプション表と同じ (ShowLineNo/ShowTAB/ShowCR/ShowTextRuler=true、
 *          ViewTabWidthX=2・他は8、ViewFoldWidth=80、ViewLeftMargin=10、
 *          ListWheelScrLn=2)。
 */
#ifndef NYANFI_GUI_TEXT_DISPLAY_H
#define NYANFI_GUI_TEXT_DISPLAY_H

#include "usr_str.h"

namespace text_display {

//---------------------------------------------------------------------------
// 既定値・上限 (src/Global.cpp のオプション表と同じ)
//---------------------------------------------------------------------------

constexpr int kDefaultTabWidth = 8;       //!< 通常拡張子のタブ幅
constexpr int kMaxTabWidth = 32;          //!< タブ幅の上限 (推測・要検証)
constexpr int kDefaultFoldWidth = 80;     //!< 折り返し幅の既定 (ViewFoldWidth)
constexpr int kMaxFoldWidth = 1024;       //!< 折り返し幅の上限 (推測・要検証)
constexpr int kDefaultMargin = 10;        //!< 左余白の既定 (ViewLeftMargin)
constexpr int kMaxMargin = 128;           //!< 左余白の上限 (推測・要検証)
constexpr int kDefaultScrollLines = 2;    //!< プレビュースクロール既定 (ListWheelScrLn)
constexpr int kDefaultTailLines = 100;    //!< ViewTail の既定行数

/**
 * @brief ON/OFF/反転の判定 (VCL SetToggleAction と同じ)
 * @param cur 現在値
 * @param param 空=反転、"ON"=true、"OFF"=false (大小無視)。他は反転
 */
bool ToggleValue(bool cur, const UnicodeString &param);

/**
 * @brief タブ幅の解釈 (VCL SetTabActionExecute と同じ)
 * @details `ActionParam.ToIntDef(0)` をそのまま TabLength に入れる。
 *          空は「入力ボックスを出す」ので純関数では扱わない (呼び出し側で弾く)。
 *          負は0、超過は上限に丸める
 */
int ParseTabWidth(const UnicodeString &param, int fallback = kDefaultTabWidth);

/**
 * @brief 折り返し幅の解釈 (VCL SetWidthActionExecute と同じ)
 * @details 0はウィンドウ幅追従 (ViewFoldFitWin)。超過は上限に丸める
 */
int ParseFoldWidth(const UnicodeString &param, int fallback = kDefaultFoldWidth);

/**
 * @brief 左余白の解釈 (VCL SetMarginActionExecute と同じ)
 * @details `ActionParam.ToIntDef(0)`。負は0、超過は上限に丸める
 */
int ParseMargin(const UnicodeString &param, int fallback = kDefaultMargin);

/**
 * @brief プレビュースクロール行数の解釈
 * @details VCL は ActionParam 空で ListWheelScrLn、非空でその行数
 *          (ScrollUpTextActionExecute)。0以下は1に丸める
 */
int ParseScrollLines(const UnicodeString &param, int fallback = kDefaultScrollLines);

/// ViewTail の引数 (VCL ViewTailActionExecute と同じ)
struct TailParam {
	int limit_lines = kDefaultTailLines;  //!< 末尾から何行か
	bool reverse = false;                 //!< true なら逆順表示 ("R")
};

/**
 * @brief ViewTail 引数の解釈
 * @details VCL は `remove_top_text(ActionParam, "R")` で逆順を取り、
 *          残りを `ToIntDef(100)` で行数にする
 */
TailParam ParseTailParam(const UnicodeString &param);

}  // namespace text_display

#endif  // NYANFI_GUI_TEXT_DISPLAY_H
