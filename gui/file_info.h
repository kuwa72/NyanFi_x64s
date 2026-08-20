/**
 * @file gui/file_info.h
 * @brief カーソル位置のファイルの情報を、移植済みの解析関数から組み立てる (wx 非依存)
 *
 * @details gui/file_ops.h と同じ理由で wxWidgets には依存しない。実際の表示
 * (wxDialog) は gui/file_info_panel.h/.cpp が行い、こちらは
 * `nyanfi_gui_core` (ルート CMakeLists.txt) に入れて `tests/core/` から
 * 一時ディレクトリ上のテストファイルで直接テストする。
 *
 * # 使っている移植済み関数 (すべて src/usr_file_inf.h / src/usr_id3.h)
 *
 * | 種別 | 関数 |
 * |---|---|
 * | Exif (JPEG/RAW/HEIC) | `get_ExifInf` (+JPEGなら `get_JpgExInf`) |
 * | PNG | `get_PngInf` |
 * | GIF | `get_GifInf` |
 * | WebP | `get_WebpInf` |
 * | ICO/CUR | `get_IconInf` |
 * | ANI | `get_AniInf` |
 * | Paint Shop Pro | `get_PspInf` |
 * | メタファイル(wmf/emf) | `get_MetafileInf` |
 * | WAV | `get_WavInf` |
 * | MP3 (ID3) | `ID3_GetInf` (src/usr_id3.h) |
 * | FLAC | `get_FlacInf` |
 * | Opus | `get_OpusInf` |
 * | CDA | `get_CdaInf` |
 * | HTML | `get_HtmlInf` |
 * | PDF | `get_PdfVer` |
 * | C++Builder プロジェクト/ソース | `get_BorlandInf` |
 * | ctags の `tags` ファイル | `get_TagsInf` |
 * | 代替データストリーム(ADS) | `get_ADS_count` / `get_ADS_Inf` |
 * | ハッシュ (任意、ボタンで計算) | `get_HashStr` (SHA256) / `get_CRC32_str` |
 *
 * これらはいずれも内部で `add_PropLine`/`get_PropTitle` (src/usr_shell.h 宣言)
 * を呼んでいるが、その実体がある src/usr_shell.cpp は `IShellFolder` 等の
 * シェル統合が大半を占め GUI 依存でビルドが通らない
 * (`scripts/probe.sh usr_shell` で確認済み)。書式化関数だけを
 * gui/usr_shell_fmt_shim.cpp に複製してリンクできるようにしてある
 * (解析ロジックの自前実装ではなく、複製元は src/usr_shell.cpp そのもの)。
 *
 * # GUI 依存で使わなかった関数 (usr_SH / UserShell が必要)
 *
 * - `get_AppInf` (実行ファイル情報): 内部で `usr_SH->get_PropInf()` を呼ぶ。
 *   `usr_SH` は `UserShell` (src/usr_shell.cpp) のグローバルインスタンスで
 *   未移植のため使えない。実行可能ファイルかどうかの判定だけは
 *   `test_ExeExt()` で行い、「実行可能ファイルです」の1行のみ表示する。
 * - `get_duration` (再生時間): 同じく `usr_SH->get_Duration()` に依存する
 *   ため未使用。
 * - `get_ProcessingInf` (使用中プロセス一覧): `RestartManager` API 自体は
 *   使えるが、GenInfDlg 相当の用途 (ファイル使用中エラー時の案内) がまだ
 *   無いため、今回は呼んでいない (使えないわけではない)。
 *
 * # 対象外にした種別 (未移植 or 判断が重い)
 *
 * - アーカイブ (zip/7z 等) の中身・圧縮率: `usr_ARC` (`UserArcUnit` の
 *   グローバルインスタンス) が `src/Global.cpp` 側にあり未移植。
 * - `.lnk` のリンク先解決: `IShellLink` (COM) が必要で未移植。
 * - フォント (`get_FontInf`)・XML (`get_xml_inf`)・xdoc2txt: いずれも
 *   `usr_file_inf.h` に公開関数が無い (Global.cpp 側の実装のみ)。
 */
#ifndef NYANFI_GUI_FILE_INFO_H
#define NYANFI_GUI_FILE_INFO_H

#include "gui/file_item.h"

/**
 * @brief カーソル位置の1件の情報行を組み立てる
 * @param full_path 対象のフルパス
 * @param item 一覧から得た基本情報 (名前・サイズ・日時・属性・ディレクトリか)
 * @param[out] lst 組み立てた行を追加する (呼び出し前にクリアはしない)
 * @details 基本情報 (パス・サイズ・日時・属性) を必ず先頭に追加したうえで、
 * 拡張子に応じた移植済みの解析関数を1つ呼ぶ。解析関数が例外を投げた場合は
 * 呼び出し側 (gui/file_info_panel.cpp) で捕捉するため、ここでは伝播させる。
 */
void BuildFileInfoLines(const UnicodeString &full_path, const FileItem &item, TStringList *lst);

/**
 * @brief ハッシュ (SHA256 / CRC32) を計算して行を追加する
 * @details 大きいファイルだと時間がかかるため、BuildFileInfoLines には含めず
 * 呼び出し側がボタン等で明示的に呼ぶ想定
 */
void AppendHashLines(const UnicodeString &full_path, TStringList *lst);

#endif  // NYANFI_GUI_FILE_INFO_H
