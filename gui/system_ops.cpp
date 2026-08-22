/**
 * @file gui/system_ops.cpp
 * @brief system_ops の実装 (設計は gui/system_ops.h)
 */
#include "gui/system_ops.h"

#include "usr_str.h"
#include "compat/netencoding.h"

#include <algorithm>
#include <mmsystem.h>	//MCI (EjectTray)

// IMMDeviceEnumerator / IAudioEndpointVolume (ToggleMute) の GUID を実体化する。
// mingw の libuuid.a には MMDevice/EndpointVolume 系の GUID が入っていないため、
// INITGUID を立てて DEFINE_GUID にこの翻訳単位で実体を持たせる (CMakeLists.txt に
// ライブラリを足さずに済ませるための、この .cpp 内だけの工夫)。
#define INITGUID
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include "compat/cominterface.h"

namespace system_ops {

//-----------------------------------------------------------------------
// URL の組み立て
//-----------------------------------------------------------------------

//---------------------------------------------------------------------------
UnicodeString UrlEncode(const UnicodeString &s)
{
	return System::Netencoding::TURLEncoding::URL->Encode(s);
}

//---------------------------------------------------------------------------
UnicodeString BuildSearchUrl(const UnicodeString &engine_template, const UnicodeString &keyword)
{
	// 実測 (UserFunc.cpp:1631 exe_WebSearch): keyword が空なら置換すらしない
	if (keyword.IsEmpty()) return EmptyStr;
	return ReplaceStr(engine_template, _T("\\S"), UrlEncode(keyword));
}

//---------------------------------------------------------------------------
UnicodeString BuildMapUrl(const UnicodeString &map_template, double lat, double lon, int zoom)
{
	UnicodeString s = map_template;
	s = ReplaceStr(s, _T("$Latitude$"),  UnicodeString().sprintf(_T("%.8f"), lat));
	s = ReplaceStr(s, _T("$Longitude$"), UnicodeString().sprintf(_T("%.8f"), lon));
	const int z = std::min(std::max(zoom, 1), 18);
	s = ReplaceStr(s, _T("$Zoom$"), IntToStr(z));
	return s;
}

//-----------------------------------------------------------------------
// 代替データストリーム (ADS)
//-----------------------------------------------------------------------

//---------------------------------------------------------------------------
std::vector<AdsEntry> ListStreams(const UnicodeString &path)
{
	std::vector<AdsEntry> out;

	WIN32_FIND_STREAM_DATA sd;
	HANDLE hFS = ::FindFirstStreamW(path.c_str(), FindStreamInfoStandard, &sd, 0);
	if (hFS == INVALID_HANDLE_VALUE) return out;

	do {
		// "":名前:$DATA" の形なので、末尾の ":$DATA" を落として先頭の ":" も落とす。
		// 既定のデータストリーム ("::$DATA") はこれで空文字列になり除外される。
		UnicodeString name = get_tkn(UnicodeString(sd.cStreamName), _T(":$DATA"));
		if (remove_top_s(name, _T(":")) && !name.IsEmpty()) {
			AdsEntry e;
			e.name = name;
			e.size = (static_cast<Int64>(sd.StreamSize.HighPart) << 32)
			       +  static_cast<Int64>(static_cast<DWORD>(sd.StreamSize.LowPart));
			out.push_back(e);
		}
	} while (::FindNextStreamW(hFS, &sd));
	::FindClose(hFS);

	return out;
}

//---------------------------------------------------------------------------
bool DeleteStream(const UnicodeString &path, const UnicodeString &stream_name, UnicodeString &error_out)
{
	error_out = EmptyStr;

	// ストリーム名が空だと "path:" になり本体を巻き込みかねないので必ず弾く
	if (Trim(stream_name).IsEmpty()) {
		error_out = _T("ストリーム名が指定されていません");
		return false;
	}

	// 実測 (usr_file_ex.cpp:876 delete_ADS): ":$DATA" を付けなくても DeleteFile は通る
	const UnicodeString target = path + _T(":") + stream_name;
	if (!::DeleteFileW(target.c_str())) {
		error_out = SysErrorMessage(::GetLastError());
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
UnicodeString StreamPath(const UnicodeString &path, const UnicodeString &stream_name)
{
	return path + _T(":") + stream_name + _T(":$DATA");
}

//-----------------------------------------------------------------------
// ドライブ
//-----------------------------------------------------------------------

//---------------------------------------------------------------------------
bool IsRemovableDrive(const UnicodeString &drive_root)
{
	const UnicodeString root = IncludeTrailingPathDelimiter(drive_root);
	return ::GetDriveTypeW(root.c_str()) == DRIVE_REMOVABLE;
}

//---------------------------------------------------------------------------
bool EjectTray(const UnicodeString &drive_root, UnicodeString &error_out)
{
	error_out = EmptyStr;

	MCI_OPEN_PARMS mci_prm;
	::ZeroMemory(&mci_prm, sizeof(mci_prm));
	mci_prm.lpstrDeviceType = (LPCTSTR)MCI_DEVTYPE_CD_AUDIO;
	DWORD flag = MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE | MCI_WAIT;

	// 実測 (MainFrm.cpp:16947 EjectActionExecute): ドライブ指定は "D:" の形
	const UnicodeString dstr = ExtractFileDrive(drive_root);
	if (!dstr.IsEmpty()) {
		mci_prm.lpstrElementName = dstr.c_str();
		flag |= MCI_OPEN_ELEMENT;
	}

	if (::mciSendCommand(0, MCI_OPEN, flag, (DWORD_PTR)&mci_prm) != 0) {
		error_out = _T("CD/DVD ドライブを開けません");
		return false;
	}
	::mciSendCommand(mci_prm.wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, 0);
	::mciSendCommand(mci_prm.wDeviceID, MCI_CLOSE, MCI_WAIT, 0);
	return true;
}

//---------------------------------------------------------------------------
bool EjectDrive(const UnicodeString &drive_root, UnicodeString &error_out)
{
	error_out = EmptyStr;

	const UnicodeString drv = ExtractFileDrive(drive_root);
	if (drv.IsEmpty()) {
		error_out = _T("ドライブ文字を取得できません");
		return false;
	}

	const UINT typ = ::GetDriveTypeW((drv + _T("\\")).c_str());
	DWORD access = 0;
	switch (typ) {
	case DRIVE_REMOVABLE: access = GENERIC_READ | GENERIC_WRITE; break;
	case DRIVE_CDROM:     access = GENERIC_READ; break;
	default:
		error_out = _T("このドライブは取り外せません");
		return false;
	}

	const UnicodeString dev = _T("\\\\.\\") + drv;
	HANDLE hDrive = ::CreateFileW(dev.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDrive == INVALID_HANDLE_VALUE) {
		error_out = SysErrorMessage(::GetLastError());
		return false;
	}

	bool ok = false;
	bool locked = false;
	DWORD dummy = 0;
	// 実測 (UserFunc.cpp:1427 EjectDrive): ボリュームをロックできるまで最大20回リトライ
	for (int i = 0; i < 20 && !locked; i++) {
		if (::DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dummy, NULL)) {
			locked = true;
		}
		else {
			::Sleep(500);
		}
	}
	if (locked) {
		if (::DeviceIoControl(hDrive, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dummy, NULL)) {
			PREVENT_MEDIA_REMOVAL pmr;
			pmr.PreventMediaRemoval = FALSE;
			if (::DeviceIoControl(hDrive, IOCTL_STORAGE_MEDIA_REMOVAL,
			                       &pmr, sizeof(pmr), NULL, 0, &dummy, NULL)) {
				::DeviceIoControl(hDrive, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &dummy, NULL);
				ok = true;
			}
		}
	}
	::CloseHandle(hDrive);

	if (!ok) error_out = _T("ドライブをロックできず取り外せませんでした");
	return ok;
}

//-----------------------------------------------------------------------
// そのほか (薄い包み)
//-----------------------------------------------------------------------

//---------------------------------------------------------------------------
bool EmptyRecycleBin(UnicodeString &error_out, HWND owner)
{
	error_out = EmptyStr;
	// 実測 (MainFrm.cpp:17028 EmptyTrashActionExecute): 確認要否は呼び出し元の判断
	const HRESULT hr = ::SHEmptyRecycleBinW(owner, NULL, SHERB_NOCONFIRMATION);
	if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) /* 既に空 */) {
		error_out = SysErrorMessage(hr);
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool LockComputer(UnicodeString &error_out)
{
	error_out = EmptyStr;
	if (!::LockWorkStation()) {
		error_out = SysErrorMessage(::GetLastError());
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
bool TurnOffMonitor()
{
	// 実測 (MainFrm.cpp:22065 MonitorOffActionExecute) の中核部分だけ移植
	::SendNotifyMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
	return true;
}

//---------------------------------------------------------------------------
bool ToggleMute()
{
	// 実測 (Global.cpp:11207 mute_Volume) と同じ WASAPI の手順
	TComInterface<IMMDeviceEnumerator> dev_enum;
	TComInterface<IMMDevice> device;
	TComInterface<IAudioEndpointVolume> endp_vol;

	if (FAILED(::CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
	                               __uuidof(IMMDeviceEnumerator), (void**)&dev_enum))) {
		return false;
	}
	if (FAILED(dev_enum->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))) return false;
	if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
	                            NULL, (void**)&endp_vol))) {
		return false;
	}

	BOOL mute = FALSE;
	if (FAILED(endp_vol->GetMute(&mute))) return false;
	return SUCCEEDED(endp_vol->SetMute(!mute, NULL));
}

}  // namespace system_ops
