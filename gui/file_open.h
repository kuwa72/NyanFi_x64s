/**
 * @file gui/file_open.h
 * @brief ファイルを開く (関連付け実行 / アプリケーションから開く)
 *
 * @details Phase 2 の骨格に「ファイルを開く」を追加する部分。gui/file_ops.h と
 * 同様に wxWidgets には依存しない (`gui/main_frame.cpp` から呼ばれる) ので
 * `nyanfi_gui_core` (ルート CMakeLists.txt) に入れ、`tests/core/` から
 * 直接テストできる範囲だけテストする (実際に ShellExecuteExW を叩く部分は
 * 実行環境に依存するため対象外)。
 *
 * # 実装上の判断 (推測・簡略化した点)
 *
 * - **OpenStandard (関連付けで開く)**: `ShellExecuteExW` (`lpVerb = L"open"`)
 *   をそのまま呼ぶ。関連付けが無いファイルの場合、`ShellExecuteExW` 自身が
 *   「このファイルを開く方法を選んでください」ダイアログを出すため、
 *   `SEE_MASK_FLAG_NO_UI` は付けない (フォールバック UI をあえて活かす)。
 * - **OpenByApp (アプリケーションから開く)**: `SHOpenWithDialog`
 *   (`OAIF_EXEC | OAIF_ALLOW_REGISTRATION`) を使う。issue の指示にある
 *   `rundll32 shell32,OpenAs_RunDLL` 相当だが、Vista 以降で使える
 *   `SHOpenWithDialog` の方が `CreateProcess` 経由の外部プロセス起動より
 *   確実にエラーを拾えるためこちらを採用した。`CoInitializeEx` は
 *   呼び出し側 (wxWidgets の MSW 初期化) が既に済ませている前提だが、
 *   念のため関数内でも試み、`RPC_E_CHANGED_MODE` (既に別モードで初期化済み)
 *   は無視する。
 * - **実行可能ファイルの確認**: `usr_file_inf.h` の `test_ExeExt()` を
 *   呼び出し側 (`gui/main_frame.cpp`) がそのまま使う。ここに専用の
 *   ラッパーは用意しない (1行で済むため)。
 */
#ifndef NYANFI_GUI_FILE_OPEN_H
#define NYANFI_GUI_FILE_OPEN_H

namespace file_open {

/**
 * @brief 関連付けで開く (Enter キーの標準動作。VCL 版の "OpenStandard" に相当)
 * @param full_path 開くファイルのフルパス
 * @param error_out [o] 失敗時の理由 (成功時は変更しない)
 * @param owner 親ウィンドウ (エラーダイアログ等の親。無くてもよい)
 * @return true 起動できた (ShellExecuteExW が成功した)
 */
bool OpenStandard(const UnicodeString &full_path, UnicodeString &error_out, HWND owner = NULL);

/**
 * @brief 「アプリケーションから開く」ダイアログを表示する (Ctrl+Enter。VCL 版の "OpenByApp" に相当)
 * @param full_path 対象ファイルのフルパス
 * @param error_out [o] 失敗時の理由 (利用者がダイアログをキャンセルした場合は
 *        設定しない。false だが error_out が空ならキャンセル扱い)
 * @param owner 親ウィンドウ
 * @return true 選択したアプリケーションで起動できた
 */
bool OpenWithDialog(const UnicodeString &full_path, UnicodeString &error_out, HWND owner = NULL);

}  // namespace file_open

#endif  // NYANFI_GUI_FILE_OPEN_H
