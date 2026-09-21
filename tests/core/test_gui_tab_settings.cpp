/**
 * @file tests/core/test_gui_tab_settings.cpp
 * @brief gui/tab_settings.cpp (タブの設定ダイアログの判断ロジック) のテスト
 *
 * @details VCL の該当実装は `src/TabDlg.cpp` (`TTabSetDlg`)。タブ一覧
 *          (`TabList`) の1行は `TABLIST_CSVITMCNT = 9` の CSV
 *          (`path0,path1,caption,icon,home0,home1,nwl_mode,nwl,sync_lr`、
 *          `src/Global.h`) で、ダイアログはそのうち [2..7]
 *          (キャプション・アイコン・左右のホーム・ワークリスト指定) だけを
 *          読み書きする (`FormShow` / `OkButtonClick` を実測)。
 *          パス ([0],[1]) と同期フラグ ([8]) は触らない。
 */
#include "doctest/doctest.h"

#include "gui/tab_settings.h"

namespace {

// path0,path1,caption,icon,home0,home1,nwl_mode,nwl,sync_lr
UnicodeString rec_of(const wchar_t *caption, const wchar_t *icon, const wchar_t *home0,
                     const wchar_t *home1, const wchar_t *mode, const wchar_t *nwl)
{
	UnicodeString r;
	r.sprintf(_T("C:\\L\\,C:\\R\\,%s,%s,%s,%s,%s,%s,0"), caption, icon, home0, home1, mode, nwl);
	return r;
}

}  // namespace

//===========================================================================
// FromCsvRecord (TTabSetDlg::FormShow の読み取り部分)
//===========================================================================

TEST_CASE("TabSettings: CSVレコードからダイアログ項目を読める")
{
	const tab_settings::TabSettings s =
		tab_settings::FromCsvRecord(rec_of(_T("作業"), _T("C:\\a.ico"), _T("D:\\h0\\"),
		                                    _T("E:\\h1\\"), _T("2"), _T("F:\\w.nwl")));

	CHECK(s.caption == UnicodeString(_T("作業")));
	CHECK(s.icon == UnicodeString(_T("C:\\a.ico")));
	CHECK(s.home0 == UnicodeString(_T("D:\\h0\\")));
	CHECK(s.home1 == UnicodeString(_T("E:\\h1\\")));
	CHECK(s.work_mode == 2);
	CHECK(s.work_list == UnicodeString(_T("F:\\w.nwl")));
}

TEST_CASE("TabSettings: ワークリストは mode==2 のときだけ有効")
{
	// VCL の FormShow: case 1 (現在のワークリスト) と default (使わない) では
	// WorkListEdit を空にする。保存時 (OkButtonClick) も mode!=2 なら [7] に
	// 空を書くため、設定値としても空に正規化する
	const tab_settings::TabSettings s1 =
		tab_settings::FromCsvRecord(rec_of(_T("c"), _T(""), _T(""), _T(""), _T("1"), _T("F:\\w.nwl")));
	CHECK(s1.work_mode == 1);
	CHECK(s1.work_list.IsEmpty());

	const tab_settings::TabSettings s0 =
		tab_settings::FromCsvRecord(rec_of(_T("c"), _T(""), _T(""), _T(""), _T("0"), _T("F:\\w.nwl")));
	CHECK(s0.work_mode == 0);
	CHECK(s0.work_list.IsEmpty());
}

TEST_CASE("TabSettings: 不正なワークリストモードは0 (使わない) になる")
{
	// VCL は itm_buf[6].ToIntDef(0) して switch の default で Work0 に倒す
	const tab_settings::TabSettings s =
		tab_settings::FromCsvRecord(rec_of(_T("c"), _T(""), _T(""), _T(""), _T("X"), _T("F:\\w.nwl")));
	CHECK(s.work_mode == 0);
	CHECK(s.work_list.IsEmpty());
}

TEST_CASE("TabSettings: 短いレコードでも既定値で読める")
{
	const tab_settings::TabSettings s = tab_settings::FromCsvRecord(UnicodeString(_T("C:\\L\\")));
	CHECK(s.caption.IsEmpty());
	CHECK(s.icon.IsEmpty());
	CHECK(s.home0.IsEmpty());
	CHECK(s.home1.IsEmpty());
	CHECK(s.work_mode == 0);
	CHECK(s.work_list.IsEmpty());
}

//===========================================================================
// ApplyToCsvRecord (TTabSetDlg::OkButtonClick の書き込み部分)
//===========================================================================

TEST_CASE("TabSettings: パスと同期フラグは変えず [2..7] だけ書く")
{
	const UnicodeString base = rec_of(_T("old"), _T("old.ico"), _T("C:\\h0\\"),
	                                  _T("C:\\h1\\"), _T("0"), _T(""));

	tab_settings::TabSettings s;
	s.caption = _T("new");
	s.icon = _T("D:\\b.ico");
	s.home0 = _T("D:\\n0\\");
	s.home1 = _T("D:\\n1\\");
	s.work_mode = 2;
	s.work_list = _T("D:\\w.nwl");

	const UnicodeString out = tab_settings::ApplyToCsvRecord(base, s);
	const tab_settings::TabSettings back = tab_settings::FromCsvRecord(out);
	CHECK(back.caption == UnicodeString(_T("new")));
	CHECK(back.icon == UnicodeString(_T("D:\\b.ico")));
	CHECK(back.home0 == UnicodeString(_T("D:\\n0\\")));
	CHECK(back.home1 == UnicodeString(_T("D:\\n1\\")));
	CHECK(back.work_mode == 2);
	CHECK(back.work_list == UnicodeString(_T("D:\\w.nwl")));

	// [0],[1],[8] は保存される (パスと同期フラグが変わっていない)
	const TStringDynArray raw = get_csv_array(out, 9, true);
	CHECK(raw[0] == UnicodeString(_T("C:\\L\\")));
	CHECK(raw[1] == UnicodeString(_T("C:\\R\\")));
	CHECK(raw[8] == UnicodeString(_T("0")));
}

TEST_CASE("TabSettings: mode!=2 で保存するとワークリストは空になる")
{
	// VCL の OkButtonClick: itm_buf[7] = Work2RadioBtn? WorkListEdit : 空
	const UnicodeString base = rec_of(_T("c"), _T(""), _T(""), _T(""), _T("2"), _T("F:\\w.nwl"));

	tab_settings::TabSettings s = tab_settings::FromCsvRecord(base);
	s.work_mode = 0;
	s.work_list = _T("F:\\w.nwl");  // ダイアログ外から混ざっても保存時は捨てる

	const tab_settings::TabSettings back =
		tab_settings::FromCsvRecord(tab_settings::ApplyToCsvRecord(base, s));
	CHECK(back.work_mode == 0);
	CHECK(back.work_list.IsEmpty());
}
