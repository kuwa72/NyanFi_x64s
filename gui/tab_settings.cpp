/**
 * @file gui/tab_settings.cpp
 * @brief gui/tab_settings.h の実装 (wx 非依存)
 */
#include "gui/tab_settings.h"

namespace tab_settings {

//---------------------------------------------------------------------------
TabSettings FromCsvRecord(const UnicodeString &record)
{
	// force_size=true で足りない分は空で埋められる (VCL と同じ)
	TStringDynArray itm = get_csv_array(record, kCsvItemCount, true);

	TabSettings s;
	s.caption = itm[2];
	s.icon = itm[3];
	s.home0 = itm[4];
	s.home1 = itm[5];

	// VCL は itm_buf[6].ToIntDef(0) して switch (case 1/2、default→0)
	s.work_mode = itm[6].ToIntDef(kWorkNone);
	if (s.work_mode != kWorkCurrent && s.work_mode != kWorkNamed) s.work_mode = kWorkNone;

	// VCL の FormShow: mode==2 のときだけ WorkListEdit に itm_buf[7] を入れる。
	// それ以外は空表示のまま、確定時も空で上書きされるため値として持たない
	s.work_list = (s.work_mode == kWorkNamed) ? itm[7] : EmptyStr;
	return s;
}

//---------------------------------------------------------------------------
UnicodeString ApplyToCsvRecord(const UnicodeString &base_record, const TabSettings &settings)
{
	TStringDynArray itm = get_csv_array(base_record, kCsvItemCount, true);

	itm[2] = settings.caption;
	itm[3] = settings.icon;
	itm[4] = settings.home0;
	itm[5] = settings.home1;

	const int mode = (settings.work_mode == kWorkCurrent || settings.work_mode == kWorkNamed)
		? settings.work_mode : kWorkNone;
	itm[6].sprintf(_T("%d"), mode);
	// VCL の OkButtonClick: itm_buf[7] = Work2? WorkListEdit : 空
	itm[7] = (mode == kWorkNamed) ? settings.work_list : EmptyStr;

	return make_csv_rec_str(itm);
}

}  // namespace tab_settings
