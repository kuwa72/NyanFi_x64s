/**
 * @file gui/pack_settings.h
 * @brief アーカイブ作成ダイアログの設定解決 (wx 非依存)
 *
 * @details VCL 版の `src/PackDlg.cpp` (`TPackArcDlg`) の
 *          `FormatRadioGroupClick` / `FormClose` と、
 *          `src/MainFrm.cpp:23339-23361` の Pack ダイアログ接続から
 *          入力値の正規化だけを切り出したもの。実際の圧縮・パスワード・
 *          追加スイッチの実行は `gui/pack_dialog.h` と MainFrame 側の
 *          警告に委ねる。
 *
 *          未移植 (未実装扱い):
 *          - `UserArcUnit::Pack` の圧縮レベル/パスワード/自己解凍/追加スイッチ
 *          - 既存書庫への追加・削除後の再作成
 *          - ディレクトリごとの書庫作成と include-top-dir の実処理
 */
#ifndef NYANFI_GUI_PACK_SETTINGS_H
#define NYANFI_GUI_PACK_SETTINGS_H

#include "gui/archive.h"

namespace pack_settings {

/// VCL の FormatRadioGroup の並び
enum class Format { Zip = 0, SevenZip = 1, Lha = 2, Cab = 3, Tar = 4 };

/// 同名書庫の扱い (VCL SameRadioGroup)
enum class ExistingMode { Append = 0, Recreate = 1 };

/// 利用可否 (VCL は usr_ARC->IsAvailable で各ボタンを無効化する)
struct Availability {
	bool zip = true;
	bool seven_zip = true;
	bool lha = true;
	bool cab = true;
	bool tar = true;
};

/// ダイアログの入力値
struct Options {
	UnicodeString name;             //!< 拡張子を除いた書庫名
	Format format = Format::Zip;
	int compression = 5;            //!< VCL の ZipPrm_x/SevenPrm_x/CabPrm_z/TarPrm_z
	UnicodeString extra_switches;
	UnicodeString password;
	bool self_extract = false;
	bool per_directory = false;
	bool include_top_directory = false;
	bool confirm_existing = true;
	ExistingMode existing_mode = ExistingMode::Append;
};

/// 設定を解決した結果
struct Resolved {
	Options options;
	UnicodeString extension;
	UnicodeString file_name;
	UnicodeString archive_path;   //!< dst_dir が空なら空
	bool available = false;
	bool core_compatible = false;
	UnicodeString unsupported_reason;
};

int ToIndex(Format format);
Format FromIndex(int index);
UnicodeString Extension(Format format);
bool TryFormatFromName(const UnicodeString &name, Format &format_out);
bool IsAvailable(const Availability &availability, Format format);

/// VCL の ParamComboBox の選択位置と内部圧縮値を変換する
int CompressionFromUiIndex(Format format, int index);
int UiIndexFromCompression(Format format, int compression);

/// 値域・利用可否を正規化する
Options Normalize(const Options &options, const Availability &availability);

/// 書庫名と形式から設定を解決する。dst_dir が空なら archive_path は空
Resolved Resolve(const Options &options, const Availability &availability,
                 const UnicodeString &dst_dir = EmptyStr);

/// 現在の core (`archive::Create`) でそのまま実行できる設定か
bool IsCoreCompatible(const Options &options, const Availability &availability);

}  // namespace pack_settings

#endif  // NYANFI_GUI_PACK_SETTINGS_H
