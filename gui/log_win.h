/**
 * @file gui/log_win.h
 * @brief ログの蓄積・整形・書き出し (wx 非依存)
 *
 * @details 機能群19 (ClearLog / ListLog / ViewLog / LogFileInfo / ShowLogWin /
 * ListNyanFi / ScrollUpLog / ScrollDownLog / ToLog) のうち、**ログの状態と
 * 整形だけ**をここに置く。ウィンドウの表示・フォーカス移動・スクロールは
 * wx 側 (`gui/main_frame.cpp`) の仕事なので、ここには入れない。
 *
 * # 実測した VCL の挙動 (`src/Global.cpp` / `src/MainFrm.cpp`)
 *
 * ## ログ1行の先頭1文字 (状態)
 *
 * `make_LogHdr` (Global.cpp:14411) はコマンド名の手前に **1文字分の空白**を
 * 予約して返す (既定は成功=空白のまま)。呼び出し側がその1文字だけを
 * 書き換えて結果を記録している (`msg[1] = 'E';` のような形。実測箇所は多数、
 * 代表例のみ挙げる)。実際に使われている文字を全て確認した:
 *
 * | 文字 | 意味 | 実測箇所 (代表) |
 * |---|---|---|
 * | (空白) | 成功 (既定のまま、書き換えない) | 例: MainFrm.cpp:29040 の ok_cnt++ 側 |
 * | `E` | エラー・失敗 | Global.cpp:14638、MainFrm.cpp:29050 ほか多数 |
 * | `C` | 中断 (ユーザーによるキャンセル) | MainFrm.cpp:23402/28017/38037 |
 * | `S` | スキップ | MainFrm.cpp:9884/9888/28023/29053 ほか |
 * | `O` | 上書き (同名ファイルを上書き) | MainFrm.cpp:9875/28020/28027 |
 * | `N` | 最新のため上書き (CPYMD_NEW) | MainFrm.cpp:9880 |
 * | `W` | 警告 (自分自身が宛先、宛先ディレクトリ無し等) | MainFrm.cpp:16436/16443 |
 *
 * `R` (`set_RenameLog`, Global.cpp:14482) は「名前を変更して保存」を表す文字
 * だが、これは複製・自動リネームの結果表示にのみ使われ、`LogFileInfo` /
 * `ListNyanFi` / `ClearLog` / `ViewLog` の経路には出てこないため、今回は
 * `LogStatus` に含めていない (**入れなかったもの**。行の末尾に "---> 新名" を
 * 追記する専用の仕組みで、単純な状態文字とは性質が違う)。
 * 同じ理由で `+` (MainFrm.cpp:7230、同期コピー先の表示専用の文字で、ログとは
 * 無関係) も対象外。
 *
 * ## `make_LogHdr` が作る行の形
 *
 * `"  %6s "` (2文字の空白 + コマンド名を6文字幅で右詰め + 空白) + ファイル名
 * (`warn_filename_RLO`) または `"[" + ディレクトリ名 + "]"`。ここでは
 * **状態文字ぶんの1文字を呼び出し側 (`FormatLine`) が受け持つ**設計にしたため、
 * `MakeLogHeader` が返す先頭の空白は1文字だけにしてある (VCLは2文字)。
 * 視覚的な余白の数が変わるだけで意味は変わらない。
 *
 * ## `StartLog` / `EndLog` (Global.cpp:14503/14534)
 *
 * `StartLog`: 直前の行が空行でなく、かつ「時刻+開始」の形の行でなければ
 * 空行を1つ挟む。開始行そのものは `タスク番号+1` + `>` (タスク無しなら `>>`)
 * + `hh:nn:ss ` + 本文 (`\t` は `" ---> "` に変換)。
 * ここでは「直前が開始行か」を VCL の文字列パターン照合
 * (`s1.Pos(':')==5 && ContainsStr(s1,"開始")`) ではなく、`LogLine::is_start`
 * フラグで直接判定する (**推測ではなく設計上の単純化**。同じ実質判断を
 * 文字列一致に頼らず型で持てるため、"開始" という語がファイル名に
 * 含まれる場合の誤判定が起きない)。
 *
 * `EndLog`: 本文 + `"終了"` + 結果カウント文字列、時刻表示あり。
 * 「圧縮/解凍」ならバルーン通知を出す処理 (`NotifyPrimNyan`) は
 * wx 側の通知の話なのでここには入れていない。
 *
 * ## `get_ResCntStr` (Global.cpp:14563)
 *
 * 出力順は **OK → NG → ERR → SKIP** (引数の並びと出力順が違う。VCLの実装
 * そのままなので直さず踏襲した)。0件の項目は出さない。スクリプト変数
 * (`XCMD_set_Var` で `TaskOkCount` 等を設定する部分) はスクリプト実行系
 * 本体の話なのでここには入れていない (**入れなかったもの**)。
 *
 * ## ログの上限
 *
 * VCL の既定値は `MaxLogLines=1000` (Global.cpp:1583、`UpdateLogListBox`
 * (Global.cpp:14490) が `Count>MaxLogLines` の間、先頭から削る)。
 * ここでは `LogBuffer` の既定値もこれに合わせて 1000 にしてある。
 *
 * # LogFileInfo (機能群19)
 *
 * `LogFileInfoCore` (MainFrm.cpp:21724) は `"  FLINFO " + パス` を1行目に、
 * 続けてファイル情報 (`GetFileInfList` → `fp->inf_list`) を装飾なしで
 * 追加する。ここでは `GetFileInfList` の代わりに、既に移植済みの
 * `BuildFileInfoLines` (`gui/file_info.h`、拡張子別の解析関数を含む) を
 * そのまま呼ぶ (自前実装しない)。アーカイブ内ファイル (`arc_DspPath` を
 * 付加する部分) は未移植のため対象外。
 *
 * # ListNyanFi (機能群19)
 *
 * VCL 版はモニタ情報・VCLスタイル・アーカイバDLL情報・使用フォント数など
 * GUI/未移植領域に強く依存する (`ListNyanFiActionExecute`, MainFrm.cpp:20947)。
 * ここでは wx 非依存で安全に取得できる範囲 (実行パス・アーキテクチャ・
 * ビルド構成・コンパイラ・ビルド日時・サポートURL) だけを返す
 * **簡略版**にした。バージョンリソース (`VERSIONINFO`) が `gui/nyanfi.rc.in`
 * にまだ無いため、製品バージョン文字列は含めていない (**未対応**)。
 */
#ifndef NYANFI_GUI_LOG_WIN_H
#define NYANFI_GUI_LOG_WIN_H

#include <vector>

namespace log_win {

/// ログ1行の状態 (先頭1文字が表すもの。実測は本ファイル冒頭コメントの表を参照)
enum class LogStatus {
	Info,       //!< 既定 (空白)。成功・見出し・付随情報など
	Overwrite,  //!< 'O' 同名ファイルを上書き
	Newer,      //!< 'N' 更新日時が新しいため上書き
	Skipped,    //!< 'S' スキップ
	Warning,    //!< 'W' 警告 (処理は続行するが注意が必要)
	Error,      //!< 'E' エラー・失敗
	Canceled,   //!< 'C' ユーザーによる中断
};

/// LogStatus に対応する先頭1文字 (Info は半角空白)
wchar_t StatusChar(LogStatus status);

/// ログ1行のデータ
struct LogLine {
	LogStatus status = LogStatus::Info;  //!< 状態
	TDateTime stamp;                     //!< 記録時刻 (Add/StartGroup 時の Now())
	UnicodeString text;                  //!< 本文。ヘッダ (MakeLogHeader) を含めてよい
	bool show_time = false;              //!< 時刻を表示するか (AddLog の with_t 相当)
	bool raw = false;                    //!< true なら装飾 (" >"・状態文字) を一切付けない
	                                      //!< (AddLogStrings/空行 相当。詳細情報の複数行に使う)
	bool is_start = false;               //!< true なら開始行 (StartLog 相当)
	int task_no = -1;                    //!< 開始行のタスク番号 (0起点。-1 ならタスク無し)
};

/**
 * @brief ログの保持。上限を超えたら古いものから捨てる (VCL の UpdateLogListBox 相当)
 */
class LogBuffer {
public:
	/// @param max_lines 保持する最大行数。0 以下なら無制限 (VCL 既定は 1000)
	explicit LogBuffer(int max_lines = 1000);

	/// 状態付きの1行を追加する (AddLog 相当)。
	/// text に "\r\n" を含んでいても分割しない (VCL の AddLog は
	/// TStringList::Text 経由で複数行に分割するが、ここでは常に1つの
	/// LogLine として保持する。複数行に分けたい詳細情報は AddRaw を使うこと。
	/// **推測ではなく意図した簡略化**: 呼び出し側の実測 (LogFileInfoCore 等) でも
	/// ヘッダ1行と詳細複数行は最初から別の呼び出し (AddLog / AddLogStrings) に
	/// 分かれている)
	void Add(LogStatus status, const UnicodeString &text, bool show_time = false);

	/// 装飾なしの複数行をまとめて追加する (AddLogStrings 相当。改行区切りで分割する)
	void AddRaw(const UnicodeString &text);

	/// 最後の行が空行でなければ、空行を1つ追加する (AddLogCr 相当)
	void AddBlankIfNeeded();

	/// 開始行を追加する (StartLog 相当)。text が空なら空行だけを追加する
	void StartGroup(const UnicodeString &text, int task_no = -1);

	/// 終了行を追加する (EndLog 相当)。text + "終了" + result_summary を時刻付きで追加する
	void EndGroup(const UnicodeString &text, const UnicodeString &result_summary = UnicodeString());

	/// 全て消す (ClearLog 相当)
	void Clear();

	/// 保持している行 (古い順)
	const std::vector<LogLine> &Lines() const { return lines_; }

	/// 保持している行数
	int Count() const { return static_cast<int>(lines_.size()); }

	/// 指定した状態の行数を数える
	int CountOf(LogStatus status) const;

	/// 上限 (コンストラクタで指定した値)
	int MaxLines() const { return max_lines_; }

private:
	int max_lines_;
	std::vector<LogLine> lines_;

	void Push(LogLine line);
	void Trim();
};

/// 1行を表示用の文字列にする (LogBufList->Strings[i] 相当)
UnicodeString FormatLine(const LogLine &line);

/**
 * @brief ログの見出し行を組み立てる (make_LogHdr 相当。Global.cpp:14411)
 * @param cmd コマンド識別文字列 (例: "COPY" "DELETE")。6文字幅で右詰めする
 * @param name 対象のファイル名またはディレクトリ名
 * @param is_dir true なら "[名前]" の形にする
 * @param full_path true ならフルパスをそのまま使う (RLO文字だけ警告置換)。
 *        false ならファイル名部分だけを使う。VCL は `LogFullPath` 設定と
 *        コマンド種別 (EDIT/LOAD) で自動的に決めるが、ここでは呼び出し側
 *        (main_frame) が明示的に指定する設計にした (**推測で決めた点**:
 *        設定の参照はこのモジュールの役割ではないと判断した)
 * @param width 0 より大きければ名前部分を左詰めでこの幅にする (複数行の位置合わせ用)
 */
UnicodeString MakeLogHeader(const UnicodeString &cmd, const UnicodeString &name,
                             bool is_dir = false, bool full_path = false, int width = 0);

/**
 * @brief 結果件数の要約文字列を作る (get_ResCntStr 相当。Global.cpp:14563)
 * @details 出力順は OK, NG, ERR, SKIP (VCL の実装のまま。引数の並びとは順序が違う)
 */
UnicodeString FormatResultCount(int ok_cnt, int er_cnt = 0, int sk_cnt = 0, int ng_cnt = 0);

/**
 * @brief ログをテキストファイルに書き出す (UTF-8 + BOM。VCL 側が BOM 無しだと ANSI と誤読するため)
 * @param path 書き出し先
 * @param lines 書き出す行 (FormatLine で整形してから書く)
 * @param[out] error_out 失敗時の理由
 * @return 成功したか
 */
bool SaveTo(const UnicodeString &path, const std::vector<LogLine> &lines, UnicodeString &error_out);

/**
 * @brief ファイル情報をログ用の行にする (LogFileInfo 相当。MainFrm.cpp:21724)
 * @param path 対象のフルパス
 * @return 1行目がヘッダ ("  FLINFO " + パス、または見つからない場合のエラー文)、
 *         2行目以降が詳細情報 (装飾なし。LogBuffer::AddRaw で追加する想定)。
 *         見つからない場合は要素1件 (エラー文) のみを返す
 */
std::vector<UnicodeString> FormatFileInfo(const UnicodeString &path);

/**
 * @brief NyanFi 自身の情報を行にする (ListNyanFi 相当。簡略版。詳細はファイル冒頭コメント参照)
 */
std::vector<UnicodeString> FormatAboutLines();

}  // namespace log_win

#endif  // NYANFI_GUI_LOG_WIN_H
