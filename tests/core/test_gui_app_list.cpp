/**
 * @file tests/core/test_gui_app_list.cpp
 * @brief gui/app_list.cpp (アプリ一覧・ランチャーの判断ロジック) のテスト
 *
 * @details VCL の該当実装は `src/AppDlg.cpp` (`TAppListDlg::UpdateAppList`、
 *          `UpdateLaunchList`、`CloseSameItemClick`) と
 *          `src/MainFrm.cpp:13499` (`AppListActionExecute` の AO/LO/LI/FA/FL/
 *          FI/FZ/AS。パラメータ解決自体は `gui/f_batch7_ops.h` の
 *          `ParseAppListOpts` が既に持ち、ここでは表示構成の解決だけ扱う)。
 *            - 列挙フィルタ (`UpdateAppList` の do{...}while(false) 連鎖を実測):
 *              非表示・自己ダイアログ・Cloaked・ツールウィンドウ・
 *              親持ち非APPWINDOW・空矩形・空テキストは落とす。
 *              除外テキスト (`ExcAppText` の `;` 区切り) を含むものも落とす
 *              (`split_strings_semicolon` + `ContainsText` を実測)
 *            - モニタ番号 (`UpdateAppList` の面積半分以上で一致を実測)
 *            - ランチャー通常ソート (`SortComp_Launch` を実測:
 *              上へ・ディレクトリ優先・同拡張子は論理比較)
 *            - まとめて閉じる (`CloseSameItemClick` を実測: 同一実行
 *              ファイル名を大小無視で一致)
 *            - インクリメンタル検索の全件表示条件
 *              (`contained_wd_i("*|?| ", IncSeaWord)` を実測)
 *
 *          未移植 (未実装扱い。落とさない):
 *          - Win32 `EnumWindows`/`GetWindow` による実列挙・プロセス情報取得
 *            (実機APIのため。薄い層は `gui/app_dialog.cpp` 側に置く)
 *          - DWM サムネイル (`DwmRegisterThumbnail` 系)
 *          - ウィンドウ実操作 (最小化/最大化/閉じる/強制終了) は確認+警告の
 *            簡略版 (`gui/app_dialog.h` を参照)
 *          - OptDlg (4729行・設定全体のため対象外)、FtpDlg/GitView
 *            (外部連携のため対象外)
 */
#include "doctest/doctest.h"

#include "gui/app_list.h"

namespace {

app_list::WindowAttrs visible_attrs()
{
	app_list::WindowAttrs a;
	a.visible = true;
	return a;
}

TEST_CASE("SplitExcList: ; 区切り・空要素除去")
{
	CHECK(app_list::SplitExcList(_T("")).empty());
	CHECK(app_list::SplitExcList(_T("a;b;c")).size() == 3);
	// 空要素は落とす (split_strings_semicolon del_empty=true 相当)
	CHECK(app_list::SplitExcList(_T("a;;b")).size() == 2);
}

TEST_CASE("MatchesExc: 除外テキスト一致")
{
	const std::vector<UnicodeString> exc = { UnicodeString(_T("program")) };
	CHECK(app_list::MatchesExc(_T("Program Manager"), exc) == true);
	CHECK(app_list::MatchesExc(_T("NyanFi"), exc) == false);
	CHECK(app_list::MatchesExc(_T("NyanFi"), {}) == false);
}

TEST_CASE("ShouldListWindow: 列挙フィルタ")
{
	const std::vector<UnicodeString> no_exc;
	CHECK(app_list::ShouldListWindow(visible_attrs(), _T("NyanFi"), no_exc) == true);

	app_list::WindowAttrs a = visible_attrs();
	a.visible = false;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	a = visible_attrs();
	a.is_self_dialog = true;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	a = visible_attrs();
	a.cloaked = true;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	a = visible_attrs();
	a.tool_window = true;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	a = visible_attrs();
	a.rect_empty = true;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	a = visible_attrs();
	a.text_empty = true;
	CHECK(app_list::ShouldListWindow(a, _T("NyanFi"), no_exc) == false);

	// 除外テキスト
	CHECK(app_list::ShouldListWindow(visible_attrs(), _T("Program Manager"),
	                                 { UnicodeString(_T("program")) }) == false);
}

TEST_CASE("ResolveMonNo: 面積半分以上で一致")
{
	const app_list::Rect win{0, 0, 800, 600};
	const std::vector<app_list::Rect> mons = { {0, 0, 800, 600}, {800, 0, 800, 600} };
	CHECK(app_list::ResolveMonNo(win, mons) == 0);
	// 画面外は不一致
	CHECK(app_list::ResolveMonNo(app_list::Rect{1600, 0, 200, 100}, mons) == -1);
}

TEST_CASE("IsLaunchAllPattern: *|?|空白で全件")
{
	CHECK(app_list::IsLaunchAllPattern(_T("*")) == true);
	CHECK(app_list::IsLaunchAllPattern(_T("a?b")) == true);
	CHECK(app_list::IsLaunchAllPattern(_T("a b")) == true);
	CHECK(app_list::IsLaunchAllPattern(_T("abc")) == false);
	CHECK(app_list::IsLaunchAllPattern(_T("")) == false);
}

TEST_CASE("MatchesLaunchName: 部分一致・ファジー")
{
	CHECK(app_list::MatchesLaunchName(_T("NyanFi"), _T("nyan"), false, true) == true);
	CHECK(app_list::MatchesLaunchName(_T("NyanFi"), _T("xyz"), false, true) == false);
	// ファジー (部分列)
	CHECK(app_list::MatchesLaunchName(_T("NyanFi"), _T("nfi"), true, true) == true);
	CHECK(app_list::MatchesLaunchName(_T("NyanFi"), _T("fnx"), true, true) == false);
}

TEST_CASE("CompareLaunchNormal: 上へ・ディレクトリ優先")
{
	app_list::LaunchEntry up{ _T(".."), _T(""), true, true, _T("") };
	app_list::LaunchEntry dir{ _T("tools"), _T(""), true, false, _T("") };
	app_list::LaunchEntry lnk{ _T("nyanfi"), _T(".lnk"), false, false, _T("") };
	CHECK(app_list::CompareLaunchNormal(up, dir) < 0);
	CHECK(app_list::CompareLaunchNormal(dir, lnk) < 0);
	CHECK(app_list::CompareLaunchNormal(lnk, lnk) == 0);
	// 同拡張子なら名前順
	app_list::LaunchEntry b{ _T("b"), _T(".lnk"), false, false, _T("") };
	app_list::LaunchEntry a{ _T("a"), _T(".lnk"), false, false, _T("") };
	CHECK(app_list::CompareLaunchNormal(a, b) < 0);
}

TEST_CASE("CloseTargetsWithSameFile: 同一実行ファイル")
{
	std::vector<UnicodeString> files = { _T("C:\\a\\x.exe"), _T("C:\\A\\X.EXE"),
	                                     _T("C:\\c\\y.exe") };
	CHECK(app_list::CloseTargetsWithSameFile(files, 0) == std::vector<int>{0, 1});
	CHECK(app_list::CloseTargetsWithSameFile(files, 2) == std::vector<int>{2});
}

TEST_CASE("ResolveAppView: AO/LO による表示構成")
{
	app_list::AppView v = app_list::ResolveAppView(true, false);
	CHECK(v.show_app == true);
	CHECK(v.show_launcher == false);
	v = app_list::ResolveAppView(false, true);
	CHECK(v.show_app == false);
	CHECK(v.show_launcher == true);
	v = app_list::ResolveAppView(false, false);
	CHECK(v.show_app == true);
	CHECK(v.show_launcher == true);
}

TEST_CASE("FormatStatus: PID・サイズを含む")
{
	app_list::AppEntry e;
	e.pid = 1234;
	e.mem_ws = 1024;
	e.mem_pws = 2048;
	e.win_wd = 800;
	e.win_hi = 600;
	const UnicodeString s = app_list::FormatStatus(e);
	CHECK(ContainsText(s, _T("1234")) == true);
	CHECK(ContainsText(s, _T("800")) == true);
}

}  // namespace
