/**
 * @file gui/drive_select.cpp
 * @brief gui/drive_select.h の実装
 */
#include "gui/drive_select.h"

namespace drive_select {

std::vector<DriveInfo> VisibleDrives(const std::vector<DriveInfo> &drives,
                                     const Options &options)
{
	std::vector<DriveInfo> out;
	out.reserve(drives.size());
	for (const DriveInfo &drive : drives) {
		if (options.only_accessible && !drive.accessible) continue;
		out.push_back(drive);
	}
	return out;
}

UnicodeString DriveLetter(const DriveInfo &drive)
{
	if (drive.path.Length() < 2 || drive.path[2] != _T(':')) return EmptyStr;
	return UnicodeString(drive.path[1]);
}

UnicodeString SelectionPath(const DriveInfo &drive, bool to_root)
{
	const UnicodeString letter = DriveLetter(drive);
	if (letter.IsEmpty()) return EmptyStr;
	UnicodeString out = letter + _T(":");
	// src/DriveDlg.cpp:382-389 の実測分岐を保持する。
	if (!to_root) out += _T("\\");
	return out;
}

UnicodeString DisplayLabel(const DriveInfo &drive)
{
	if (drive.virtual_drive && !drive.mount_path.IsEmpty()) {
		const UnicodeString name = ExtractFileName(drive.mount_path);
		if (!name.IsEmpty()) return _T("[") + name + _T("]");
	}
	if (drive.kind == DriveKind::Remote && !drive.unc_path.IsEmpty()) return drive.unc_path;
	if (!drive.label.IsEmpty()) return drive.label;
	return drive.path;
}

int FindByKey(const std::vector<DriveInfo> &drives, const UnicodeString &key)
{
	if (key.IsEmpty()) return -1;
	for (std::size_t i = 0; i < drives.size(); ++i) {
		if (StartsText(key, DriveLetter(drives[i]))) return static_cast<int>(i);
	}
	return -1;
}

bool CanSelect(const DriveInfo &drive)
{
	return drive.accessible && !DriveLetter(drive).IsEmpty();
}

bool CanEject(const DriveInfo &drive)
{
	return drive.ejectable || drive.virtual_drive;
}

bool CanOpenTray(const DriveInfo &drive)
{
	return drive.kind == DriveKind::CdRom;
}

Capacity CapacityFor(const DriveInfo &drive)
{
	Capacity out;
	if (drive.total_bytes <= 0 || drive.free_bytes < 0 || drive.free_bytes > drive.total_bytes)
		return out;
	out.valid = true;
	out.total = drive.total_bytes;
	out.free = drive.free_bytes;
	out.used = drive.total_bytes - drive.free_bytes;
	return out;
}

UnicodeString FormatCapacityCell(const DriveInfo &drive, int column)
{
	const Capacity cap = CapacityFor(drive);
	if (!cap.valid) return EmptyStr;

	UnicodeString text;
	if (column == 3) {
		text = get_size_str_T(static_cast<__int64>(cap.used), 1);
		text.cat_sprintf(_T(" (%4.1f%%)"), 100.0 * static_cast<double>(cap.used) /
		                                      static_cast<double>(cap.total));
	}
	else if (column == 4) {
		text = get_size_str_T(static_cast<__int64>(cap.free), 1);
		text.cat_sprintf(_T(" (%4.1f%%)"), 100.0 * static_cast<double>(cap.free) /
		                                      static_cast<double>(cap.total));
	}
	else if (column == 5) {
		text = get_size_str_T(static_cast<__int64>(cap.total), 1);
	}
	return text;
}

}  // namespace drive_select
