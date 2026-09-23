/**
 * @file gui/print_image.h
 * @brief 画像印刷ダイアログの設定解決 (wx 非依存)
 *
 * @details VCL 実測:
 * - `src/usr_cmdlist.cpp:394` に `I:Print`
 * - `src/MainFrm.cpp:35689-35697` が `TPrintImgDlg` を現在の画像で表示する
 * - `src/PrnImgDlg.cpp:30-84/99-108/181-221/347-355` が設定、描画、実行
 *
 * wx の入力は `gui/print_image_dialog.h`、実印刷は Windows プリンタに
 * 依存するためここでは扱わない。
 *
 * 未移植 (未実装扱い):
 * - プリンタ選択/設定、描画、`BeginDoc`/`EndDoc` による実印刷
 * - 設定の ini 永続化、フォント選択、EXIF 日時を使った文字の描画
 * - VCL の前/次/先頭/末尾ボタンによる画像一覧の移動
 */
#ifndef NYANFI_GUI_PRINT_IMAGE_H
#define NYANFI_GUI_PRINT_IMAGE_H

#include <vector>

#include "usr_str.h"

namespace print_image {

/// 用紙方向。
enum class Orientation {
	Portrait,
	Landscape,
};

/// 画像の位置・大きさ。
enum class ImageFit {
	FitPage,   //!< 用紙に合わせる
	FillPage,  //!< 用紙サイズでトリミング
	Center,    //!< 中央
	TopLeft,   //!< 左上
};

/// 印刷範囲。1枚/全枚/選択範囲。
enum class PrintRange {
	Current,
	All,
	Selection,
};

/// 文字の上下位置。
enum class TextPosition { Top, Bottom };

/// 文字の左右位置。
enum class TextAlignment { Left, Center, Right };

/// ダイアログで受け付ける入力。
struct PrintOptions {
	Orientation orientation = Orientation::Portrait;
	int copies = 1;
	int scale_percent = 100;
	ImageFit fit = ImageFit::FitPage;
	PrintRange print_range = PrintRange::Current;
	int offset_x_percent = 0;
	int offset_y_percent = 0;
	bool grayscale = false;
	bool print_text = false;
	UnicodeString text_format = _T("$XT(yy/mm/dd)");
	TextPosition text_position = TextPosition::Bottom;
	TextAlignment text_alignment = TextAlignment::Center;
	int text_margin_percent = 5;
};

/// 正規化と印刷範囲の解決結果。
struct ResolvedSettings {
	bool valid = false;
	UnicodeString error;
	Orientation orientation = Orientation::Portrait;
	int copies = 1;
	int scale_percent = 100;
	int effective_scale_percent = 100;
	ImageFit fit = ImageFit::FitPage;
	PrintRange range = PrintRange::Current;
	int first_page = 1;
	int last_page = 1;
	int offset_x_percent = 0;
	int offset_y_percent = 0;
	bool grayscale = false;
	bool print_text = false;
	UnicodeString text_format;
	TextPosition text_position = TextPosition::Bottom;
	TextAlignment text_alignment = TextAlignment::Center;
	int text_margin_percent = 5;
};

/**
 * @brief 入力値を VCL の UpDown 範囲へ正規化し、印刷範囲を解決する
 * @param options ダイアログの入力
 * @param page_count 対象ページ数
 * @param current_page 現在ページ (1始まり)
 * @param selected_pages 選択されたページ (1始まり)
 */
ResolvedSettings ResolvePrintSettings(const PrintOptions &options, int page_count,
                                      int current_page,
                                      const std::vector<int> &selected_pages);

/// 解決済み設定を1行へ整形する (プレビュー/ログ用)。
UnicodeString FormatPrintSettings(const ResolvedSettings &settings);

}  // namespace print_image

#endif  // NYANFI_GUI_PRINT_IMAGE_H
