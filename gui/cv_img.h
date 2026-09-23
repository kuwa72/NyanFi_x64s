/**
 * @file gui/cv_img.h
 * @brief 画像変換ダイアログの判定ロジック (wx 非依存)
 *
 * @details VCL の実測元は `src/CvImgDlg.cpp:22-212`、
 *          `src/CvImgDlg.dfm:35-367`、`src/MainFrm.cpp:29289-29373`、
 *          コマンド表は `src/usr_cmdlist.cpp:47,668` です。
 *          画像の実読み書きは既存の `convert_ops::ConvertImages` に委ね、
 *          この層は形式・スケールの入力状態と表示可否だけを扱う。
 *
 *          未移植 (未実装扱い):
 *          - WIC の grayscale/chroma/compression/resize/余白/補間/時刻保持
 *          - クリップボード画像の取得と保存、名前変更/自動連番
 *          - VCL のプレビュー、ini の位置/履歴、SpecialKeyProc 相当のキー処理
 */
#ifndef NYANFI_GUI_CV_IMG_H
#define NYANFI_GUI_CV_IMG_H

#include "usr_str.h"

namespace cv_img {

/// src/CvImgDlg.dfm:43-49 の 6 形式
enum class Format {
	Bmp = 0,
	Jpg = 1,
	Png = 2,
	Gif = 3,
	Tif = 4,
	Hdp = 5,
};

/// src/CvImgDlg.cpp:24-33 の ScaleModeComboBox
enum class ScaleMode {
	None = 0,
	Percent = 1,
	LongSide = 2,
	Width = 3,
	Height = 4,
	Fit = 5,
	Stretch = 6,
	FitPad = 7,
	Crop = 8,
};

/// src/CvImgDlg.dfm:261-267 の ChgNameComboBox
enum class NameMode {
	Prefix = 0,
	Suffix = 1,
};

/// ダイアログから受け渡して既存 core へ渡す条件
struct Options {
	Format format = Format::Png;
	int quality = 80;             //!< 0〜100
	int ycrcb = 1;                //!< 0=既定, 1=4:2:0, 2=4:2:2, 3=4:4:4
	int compression = 0;          //!< TIFF の圧縮モード
	bool grayscale = false;
	ScaleMode scale_mode = ScaleMode::None;
	int scale_param1 = 100;
	int scale_param2 = 100;
	int interpolation = 0;
	unsigned int margin_color = 0;
	NameMode name_mode = NameMode::Suffix;
	UnicodeString name_text;
	bool keep_time = false;
	bool not_use_preview = false;

	// VCL の fromClip 用。クリップボード実処理は未移植だが、入力条件は保持する。
	bool from_clipboard = false;
	UnicodeString clipboard_name;
	bool clipboard_overwrite = false;
};

/// 形式インデックスと拡張子の変換
int FormatIndex(Format format);
Format FormatFromIndex(int index);
UnicodeString Extension(Format format);

/// 値域を正規化する (VCL の ItemIndex/TrackBar と同じ範囲)
Options Normalize(const Options &in);

/// 出力形式・スケール変更時に表示する部品の状態
struct FormatVisibility {
	bool quality = false;
	bool ycrcb = false;
	bool compression = false;
};

FormatVisibility ResolveVisibility(const Options &opt);

/// スケール欄のラベルと有効状態 (CvImgDlg.cpp:179-202)
struct ScaleState {
	bool option = false;
	bool param1 = false;
	bool param2 = false;
	UnicodeString label1;
	UnicodeString label2;
};
ScaleState ResolveScaleState(ScaleMode mode);

/// 既存の ConvertImages が反映しない指定があれば true (UI で明示するため)
bool HasUnsupportedRuntimeOptions(const Options &opt);

}  // namespace cv_img

#endif  // NYANFI_GUI_CV_IMG_H
