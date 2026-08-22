/**
 * @file gui/system_ops.h
 * @brief システム操作と外部連携 (wx 非依存, 機能群23)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の
 *          `EmptyTrashActionExecute` (17028行) / `EjectActionExecute` (16947行) /
 *          `EjectDriveActionExecute` (16975行) / `LockComputerActionExecute` (21654行) /
 *          `MonitorOffActionExecute` (22065行) / `MuteVolumeActionExecute` (22112行) /
 *          `WebSearchActionExecute` (34360行) / `WebMapActionExecute` (34302行) /
 *          `OpenADSActionExecute` (22431行) / `DeleteADSActionExecute` (29010行)。
 *
 *          `gui/external.h` と同じ方針で、**「何を起動するか/何を作るかを決める
 *          純関数」と「実際に起動する処理」を分ける** (規約8)。前者だけをテストする。
 *
 *          `PowerOff` / `Reboot` はメンテナの方針で対象外 (未実装)。
 *          `Calculator` (MainFrm.cpp:14039) は Windows の電卓を起動するのではなく
 *          **内部の `TCalculator` モーダルダイアログ**を開くだけで、外部連携でも
 *          システム操作でもないため、ここには入れていない。
 */
#ifndef NYANFI_GUI_SYSTEM_OPS_H
#define NYANFI_GUI_SYSTEM_OPS_H

#include <vector>

namespace system_ops {

//-----------------------------------------------------------------------
// URL の組み立て (純関数)
//-----------------------------------------------------------------------

/**
 * @brief URL に入れる文字をエスケープする
 * @details 実測: `compat/netencoding.h` の `TURLEncoding::URL->Encode` を
 *          そのまま使う (`src/UserFunc.cpp:1635` の `exe_WebSearch` が使っている
 *          のと同じもの)。未予約文字 (`A-Za-z0-9-._~`) 以外を UTF-8 で
 *          percent-encode する。
 */
UnicodeString UrlEncode(const UnicodeString &s);

/**
 * @brief Web 検索の URL を作る
 * @param engine_template 検索エンジンの雛形。例:
 *        `"https://www.google.co.jp/search?q=\\S&ie=UTF-8"`
 * @param keyword 検索語 (未エンコード)
 * @details 実測 (`src/UserFunc.cpp:1631` `exe_WebSearch` / `src/Global.cpp:1513`
 *          既定値): プレースホルダは **`\S` (バックスラッシュ + S)**。`%s` ではない。
 *          `keyword` は `UrlEncode` してから `\S` を置換する。
 *
 *          VCL は `keyword` が空なら**置換すら行わず空文字列を返す** (呼び出し元は
 *          それを見て `ShellExecute` しない)。ここでも同じ挙動にする。
 */
UnicodeString BuildSearchUrl(const UnicodeString &engine_template, const UnicodeString &keyword);

/**
 * @brief 緯度経度をテンプレートに埋め込む
 * @param map_template 埋め込み先のテンプレート文字列
 * @param lat 緯度 / @param lon 経度 / @param zoom ズームレベル (既定 16)
 * @details 実測 (`src/Global.cpp:15569` `OpenWebMaps`): **実際に開かれるのは
 *          Google マップ等の URL ではなく、Leaflet を使った HTML を一時ファイルに
 *          書き出したもの**。プレースホルダは `$Latitude$` / `$Longitude$` /
 *          `$Zoom$` で、`ReplaceStr` で文字列置換するだけ (URL エンコードはしない)。
 *
 *          - 緯度経度は `sprintf(_T("%.8f"), 値)` (小数点以下8桁)
 *          - ズームは `IntToStr(std::min(std::max(zoom, 1), 18))` (1〜18 に丸める)
 *
 *          HTML 全体の組み立てや一時ファイルへの保存・オープンは「実際に起動する
 *          処理」側 (呼び出し元/GUI 層) の仕事であり、ここでは含めない。
 *          `$Title$` / `$FileName$` / `$PathName$` / `$ExifTime$` / `$ExifInfo$`
 *          など画像ファイル由来のプレースホルダも同様に対象外 (未検証: 呼び出し元で
 *          個別に置換する前提)。
 */
UnicodeString BuildMapUrl(const UnicodeString &map_template, double lat, double lon, int zoom = 16);

//-----------------------------------------------------------------------
// 代替データストリーム (ADS)
//-----------------------------------------------------------------------

/// 代替データストリームの1件
struct AdsEntry {
	UnicodeString name;  //!< ストリーム名 (`:$DATA` を除いた素の名前)
	Int64 size = 0;      //!< サイズ (バイト)
};

/**
 * @brief ファイルの代替データストリームを列挙する
 * @details 実測 (`src/MainFrm.cpp:9090` `ChangeAdsList` / `src/usr_file_ex.cpp:2528`
 *          / `src/usr_file_ex.cpp:876` `delete_ADS`、いずれも同じ手順):
 *          **`FindFirstStreamW` / `FindNextStreamW`** (`FindStreamInfoStandard`) を
 *          使う。`WIN32_FIND_STREAM_DATA::cStreamName` は `:名前:$DATA` の形なので
 *          `get_tkn(name, ":$DATA")` で末尾を落とし、`remove_top_s(name, ":")` で
 *          先頭の `:` を落として素の名前にする。
 *
 *          **既定のデータストリーム (`::$DATA`) はこの手順で必ず空文字列になり
 *          除外される** (先頭の `:` を落とすと "" になるため)。NTFS 以外や
 *          ADS の無いファイルでは空の一覧を返す。
 */
std::vector<AdsEntry> ListStreams(const UnicodeString &path);

/**
 * @brief 指定のストリームを削除する (本体のデータは消さない)
 * @param path 対象ファイル
 * @param stream_name 削除するストリーム名 (`ListStreams` が返す素の名前)
 * @param error_out 失敗時の理由
 * @details 実測 (`src/usr_file_ex.cpp:876` `delete_ADS`):
 *          `DeleteFile(path + ":" + stream_name)` を呼ぶだけ (`:$DATA` は付けない
 *          でも動く)。
 *
 *          @warning **`stream_name` が空だと `path + ":" + ""` = `path + ":"`
 *          になり、本体そのものを削除する経路に落ちかねない。ここで明示的に弾く**
 *          (削除前に必ずガードする、というのが本関数の存在意義)。
 */
bool DeleteStream(const UnicodeString &path, const UnicodeString &stream_name, UnicodeString &error_out);

/**
 * @brief "file.txt:name:$DATA" の形のフルパスを組み立てる
 * @details NTFS の ADS 参照構文。`CreateFile` 等にそのまま渡せる形。
 *          `DeleteStream` 自体はこの形を使わない (実測した `delete_ADS` に合わせて
 *          `:$DATA` を省いた形で `DeleteFile` する) が、別の用途 (表示・比較など)
 *          で完全な形が要る場合のための組み立て関数。
 */
UnicodeString StreamPath(const UnicodeString &path, const UnicodeString &stream_name);

//-----------------------------------------------------------------------
// ドライブ
//-----------------------------------------------------------------------

/// 取り外し可能 (リムーバブル) ドライブか。`drive_root` は "E:\\" のような形
bool IsRemovableDrive(const UnicodeString &drive_root);

/**
 * @brief CD/DVD ドライブのトレイを開く
 * @param drive_root ドライブ (例 "D:\\")。空なら既定の CD デバイスを使う
 * @param error_out 失敗時の理由
 * @details 実測 (`src/MainFrm.cpp:16947` `EjectActionExecute`): MCI
 *          (`mciSendCommand`) で `MCI_DEVTYPE_CD_AUDIO` を開き、
 *          `MCI_SET_DOOR_OPEN` → `MCI_CLOSE` する。
 *
 *          **テストしない** (実機のドライブを動かしてしまう)。
 */
bool EjectTray(const UnicodeString &drive_root, UnicodeString &error_out);

/**
 * @brief ドライブを安全に取り外す
 * @param drive_root ドライブ (例 "E:\\")
 * @param error_out 失敗時の理由
 * @details 実測 (`src/UserFunc.cpp:1427` `EjectDrive`) の一部を移植:
 *          `\\\\.\\<ドライブ文字>` を `CreateFile` で開き、
 *          `FSCTL_LOCK_VOLUME` → `FSCTL_DISMOUNT_VOLUME` →
 *          `IOCTL_STORAGE_MEDIA_REMOVAL` → `IOCTL_STORAGE_EJECT_MEDIA` の順に
 *          `DeviceIoControl` する。
 *
 *          **推測/未移植**: VCL の実装はこの前に `EjectDrive2`
 *          (SetupDi/CfgMgr32 でデバイスツリーを辿って取り外す別経路) を試し、
 *          失敗したときだけここに落ちてくる。`EjectDrive2` は `setupapi` /
 *          `cfgmgr32` の追加リンクが要り、`CMakeLists.txt` は触ってよいファイルの
 *          対象外なので**移植していない**。USB メモリ等の典型的なリムーバブル
 *          ドライブはこちらの経路だけでも取り外せることを確認した実装 (実機未検証)。
 *
 *          **テストしない** (実際にドライブを取り外してしまう)。
 */
bool EjectDrive(const UnicodeString &drive_root, UnicodeString &error_out);

//-----------------------------------------------------------------------
// そのほか (薄い包み。テストしにくいので「呼ぶだけ」に留める)
//-----------------------------------------------------------------------

/**
 * @brief ごみ箱を空にする
 * @details 実測 (`src/MainFrm.cpp:17028` `EmptyTrashActionExecute`):
 *          `SHEmptyRecycleBin(Handle, NULL, dwFlags)`。確認あり/なしは
 *          `SHERB_NOCONFIRMATION` の有無で切り替えているが、確認ダイアログの
 *          要否は呼び出し元 (GUI 層) の判断に委ねる。
 *
 *          @warning **テストを書いていない。実行すると本当にごみ箱が空になる**。
 */
bool EmptyRecycleBin(UnicodeString &error_out, HWND owner = NULL);

/**
 * @brief コンピュータをロックする
 * @details 実測 (`src/MainFrm.cpp:21654` `LockComputerActionExecute`):
 *          `::LockWorkStation()`。
 *
 *          **テストしない** (実行すると画面がロックされる)。
 */
bool LockComputer(UnicodeString &error_out);

/**
 * @brief ディスプレイの電源を切る
 * @details 実測 (`src/MainFrm.cpp:22065` `MonitorOffActionExecute`):
 *          `SendNotifyMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2)`。
 *          VCL 版は事前にコンピュータのロックやキーボード/マウスのロックを
 *          組み合わせられるが、それらは別機能 (`LockComputer` 等) の仕事なので
 *          ここには含めない。
 *
 *          **テストしない** (実行すると画面が消える)。
 */
bool TurnOffMonitor();

/**
 * @brief 既定のオーディオ出力のミュートを切り替える
 * @details 実測 (`src/Global.cpp:11207` `mute_Volume`): `IMMDeviceEnumerator` →
 *          `GetDefaultAudioEndpoint` → `IAudioEndpointVolume` (WASAPI) を使い、
 *          `GetMute` の否定を `SetMute` する。
 *
 *          **テストしない** (実行するとシステムの音量設定が変わる)。
 */
bool ToggleMute();

}  // namespace system_ops

#endif  // NYANFI_GUI_SYSTEM_OPS_H
