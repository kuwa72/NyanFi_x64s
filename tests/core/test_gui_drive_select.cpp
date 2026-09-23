/**
 * @file tests/core/test_gui_drive_select.cpp
 * @brief gui/drive_select.h の wx 非依存判断ロジックの回帰テスト
 *
 * VCL の実測元:
 *   - src/DriveDlg.cpp:135-200 (TSelDriveDlg::UpdateDriveList)
 *   - src/DriveDlg.cpp:365-409 (DriveGridKeyDown の選択)
 *   - src/DriveDlg.cpp:427-431 (OptCheckBoxClick)
 *   - src/DriveDlg.cpp:445-478 (ShowDriveMenu)
 *   - src/MainFrm.cpp:16855-16866 (DriveList の TSelDriveDlg 呼び出し)
 *   - src/usr_cmdlist.cpp:81 (F:DriveList)
 *
 * 実ドライブ列挙・空き容量は OS API を伴うため dialog 層の薄いラッパに置く。
 * ここでは表示対象/path/状態判定だけを固定する。
 */
#include "doctest/doctest.h"

#include "gui/drive_select.h"

using namespace drive_select;

namespace {

DriveInfo MakeDrive(const wchar_t *path, const wchar_t *label, bool accessible,
                    DriveKind kind = DriveKind::Fixed)
{
	DriveInfo d;
	d.path = path;
	d.label = label;
	d.accessible = accessible;
	d.kind = kind;
	return d;
}

}  // namespace

TEST_CASE("VisibleDrives: アクセシブルのみ表示する設定と全ドライブ表示")
{
	const std::vector<DriveInfo> all = {
		MakeDrive(_T("C:\\"), _T("System"), true),
		MakeDrive(_T("D:\\"), _T("Data"), false),
		MakeDrive(_T("E:\\"), _T("Media"), true, DriveKind::CdRom),
	};

	Options only;
	only.only_accessible = true;
	CHECK(VisibleDrives(all, only).size() == 2);
	CHECK(VisibleDrives(all, only)[0].path == UnicodeString(_T("C:\\")));
	CHECK(VisibleDrives(all, only)[1].path == UnicodeString(_T("E:\\")));

	only.only_accessible = false;
	CHECK(VisibleDrives(all, only).size() == 3);
	CHECK(VisibleDrives({}, only).empty());
}

TEST_CASE("DriveLetter/SelectionPath: VCL のキー入力と同じ形の選択先を返す")
{
	const DriveInfo d = MakeDrive(_T("C:\\"), _T("System"), true);
	CHECK(DriveLetter(d) == UnicodeString(_T("C")));
	CHECK(SelectionPath(d, true) == UnicodeString(_T("C:")));
	CHECK(SelectionPath(d, false) == UnicodeString(_T("C:\\")));
	CHECK(SelectionPath(MakeDrive(_T(""), _T(""), true), true).IsEmpty());
}

TEST_CASE("FindByKey: キー文字列を大文字小文字無視で前方一致させる")
{
	const std::vector<DriveInfo> all = {
		MakeDrive(_T("C:\\"), _T("System"), true),
		MakeDrive(_T("D:\\"), _T("Data"), true),
	};
	CHECK(FindByKey(all, _T("d")) == 1);
	CHECK(FindByKey(all, _T("D")) == 1);
	CHECK(FindByKey(all, _T("x")) == -1);
	CHECK(FindByKey(all, EmptyStr) == -1);
}

TEST_CASE("CapacityFor: 空き容量から使用容量を計算し、不正値は取得不能にする")
{
	DriveInfo d = MakeDrive(_T("C:\\"), _T("System"), true);
	d.total_bytes = 1000;
	d.free_bytes = 250;
	const Capacity c = CapacityFor(d);
	CHECK(c.valid);
	CHECK(c.total == 1000);
	CHECK(c.free == 250);
	CHECK(c.used == 750);

	d.free_bytes = 1001;
	CHECK_FALSE(CapacityFor(d).valid);
	d.free_bytes = -1;
	CHECK_FALSE(CapacityFor(d).valid);
	d.total_bytes = 0;
	d.free_bytes = 0;
	CHECK_FALSE(CapacityFor(d).valid);
}

TEST_CASE("ドライブ操作の有効条件は VCL の ActionUpdate 相当")
{
	DriveInfo fixed = MakeDrive(_T("C:\\"), _T("System"), true);
	DriveInfo cd = MakeDrive(_T("E:\\"), _T("CD"), true, DriveKind::CdRom);
	cd.ejectable = true;
	DriveInfo virtual_drive = MakeDrive(_T("V:\\"), _T("Virtual"), true);
	virtual_drive.virtual_drive = true;
	virtual_drive.ejectable = false;
	DriveInfo unavailable = MakeDrive(_T("X:\\"), _T(""), false);

	CHECK(CanSelect(fixed));
	CHECK_FALSE(CanSelect(unavailable));
	CHECK_FALSE(CanEject(fixed));
	CHECK(CanEject(cd));
	CHECK(CanEject(virtual_drive));
	CHECK(CanOpenTray(cd));
	CHECK_FALSE(CanOpenTray(fixed));
}

TEST_CASE("FormatCapacityCell: 容量セルは使用/空き/全体と比率を整形する")
{
	DriveInfo d = MakeDrive(_T("C:\\"), _T("System"), true);
	d.total_bytes = 1000;
	d.free_bytes = 250;
	CHECK(ContainsText(FormatCapacityCell(d, 3), _T("75.0")));
	CHECK(ContainsText(FormatCapacityCell(d, 4), _T("25.0")));
	CHECK(ContainsText(FormatCapacityCell(d, 5), _T("KB")));
	CHECK(ContainsText(FormatCapacityCell(d, 3), _T("B")));
}
