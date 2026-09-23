/**
 * @file gui/drive_select_dialog.cpp
 * @brief gui/drive_select_dialog.h の実装
 */
#include "gui/drive_select_dialog.h"

#include <memory>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <windows.h>

#include "gui/navigation.h"
#include "usr_file_ex.h"
#include "usr_str.h"

namespace drive_select_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

drive_select::DriveKind kind_from_type(UINT type)
{
	switch (type) {
	case DRIVE_REMOVABLE: return drive_select::DriveKind::Removable;
	case DRIVE_FIXED:     return drive_select::DriveKind::Fixed;
	case DRIVE_REMOTE:    return drive_select::DriveKind::Remote;
	case DRIVE_CDROM:     return drive_select::DriveKind::CdRom;
	case DRIVE_RAMDISK:   return drive_select::DriveKind::RamDisk;
	case DRIVE_NO_ROOT_DIR: return drive_select::DriveKind::NoRoot;
	default:              return drive_select::DriveKind::Unknown;
	}
}

UnicodeString drive_label_for(const drive_select::DriveInfo &drive)
{
	return drive_select::DisplayLabel(drive);
}

}  // namespace

//---------------------------------------------------------------------------
std::vector<drive_select::DriveInfo> EnumerateDrives()
{
	std::vector<drive_select::DriveInfo> result;

	// 既存 core の API を先に使う。get_available_drive_list は
	// accessible なドライブの既存列挙 API で、論理ドライブの追加確認に
	// drive_exists を使う (Global.cpp の get_DriveInfoList 依存は避ける)。
	std::unique_ptr<TStringList> available(new TStringList());
	get_available_drive_list(available.get());

	for (int n = 0; n < 26; ++n) {
		UnicodeString path;
		path.sprintf(_T("%c:\\"), static_cast<int>(_T('A') + n));
		if (!drive_exists(path)) continue;

		drive_select::DriveInfo drive;
		drive.path = path;
		drive.accessible = available->IndexOf(path) >= 0 || is_drive_accessible(path);
		const UINT type = get_drive_type(path);
		drive.kind = kind_from_type(type);
		drive.type_name = DriveTypeLabel(type);

		if (drive.accessible) {
			wchar_t volume[MAX_PATH] = {};
			wchar_t filesystem[MAX_PATH] = {};
			DWORD serial = 0;
			DWORD max_component = 0;
			DWORD flags = 0;
			if (::GetVolumeInformationW(path.c_str(), volume, MAX_PATH, &serial,
			                             &max_component, &flags, filesystem, MAX_PATH)) {
				drive.label = UnicodeString(volume);
				drive.filesystem = UnicodeString(filesystem);
			}

			ULARGE_INTEGER free_available{};
			ULARGE_INTEGER total{};
			ULARGE_INTEGER free_total{};
			if (::GetDiskFreeSpaceExW(path.c_str(), &free_available, &total, &free_total) &&
			    total.QuadPart > 0) {
				drive.total_bytes = static_cast<long long>(total.QuadPart);
				drive.free_bytes = static_cast<long long>(free_total.QuadPart);
			}
		}

		// ejectable/virtual/アイコン/バス種別/UNC は Global.cpp の
		// get_DriveInfoList に依存するため、ここでは未移植 (未実装扱い) とする。
		result.push_back(drive);
	}
	return result;
}

namespace {

//---------------------------------------------------------------------------
/**
 * @brief ドライブ一覧ダイアログ
 * @details `src/DriveDlg.dfm` の7列と `src/DriveDlg.cpp:135-200` の行生成を
 *          wxListCtrl で再構成した。VCL の ContextMenu/取り出し等は、機能を
 *          削除せず押した時点で「未移植 (未実装扱い)」を明示する。
 */
class DriveSelectDialog final : public wxDialog {
public:
	DriveSelectDialog(wxWindow *parent, const std::vector<drive_select::DriveInfo> &drives,
	                  const Context &context)
		: wxDialog(parent, wxID_ANY, to_wx(_T("ドライブ一覧")), wxDefaultPosition,
		           wxSize(760, 430), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, drives_(drives)
		, context_(context)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(740, 245),
		                        wxLC_REPORT | wxLC_SINGLE_SEL);
		list_->InsertColumn(0, to_wx(_T("キー")), wxLIST_FORMAT_CENTER, 45);
		list_->InsertColumn(1, to_wx(_T("ボリューム")), wxLIST_FORMAT_LEFT, 190);
		list_->InsertColumn(2, to_wx(_T("種類 (I/F)")), wxLIST_FORMAT_LEFT, 155);
		list_->InsertColumn(3, to_wx(_T("使用容量")), wxLIST_FORMAT_RIGHT, 125);
		list_->InsertColumn(4, to_wx(_T("空き容量")), wxLIST_FORMAT_RIGHT, 125);
		list_->InsertColumn(5, to_wx(_T("全体容量")), wxLIST_FORMAT_RIGHT, 125);
		list_->InsertColumn(6, to_wx(_T("システム")), wxLIST_FORMAT_LEFT, 90);
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));
		list_->Bind(wxEVT_LIST_ITEM_SELECTED, &DriveSelectDialog::OnSelect, this);
		list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &DriveSelectDialog::OnActivate, this);
		list_->Bind(wxEVT_CHAR, &DriveSelectDialog::OnChar, this);

		wxBoxSizer *options = new wxBoxSizer(wxHORIZONTAL);
		only_acc_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("アクセス可能のみ表示")));
		only_acc_->SetValue(context_.options.only_accessible);
		options->Add(only_acc_, wxSizerFlags().Border(wxRIGHT, 10));
		show_icon_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("アイコン表示")));
		show_icon_->SetValue(context_.options.show_icons);
		options->Add(show_icon_, wxSizerFlags().Border(wxRIGHT, 10));
		large_icon_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("大サイズ")));
		large_icon_->SetValue(context_.options.large_icons);
		large_icon_->Enable(context_.options.show_icons);
		options->Add(large_icon_, wxSizerFlags().Border(wxRIGHT, 10));
		to_root_ = new wxCheckBox(this, wxID_ANY, to_wx(_T("ルートへ移動")));
		to_root_->SetValue(context_.options.to_root);
		options->Add(to_root_);
		top->Add(options, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxStaticText *note = new wxStaticText(
			this, wxID_ANY,
			to_wx(_T("※ アイコン、バス種類、仮想ドライブ、Eject/Tray/Property、"
			         _T("ネットワーク操作、DriveGraph は未移植 (未実装扱い) です。"))));
		top->Add(note, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *actions = new wxBoxSizer(wxHORIZONTAL);
		property_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("プロパティ")));
		eject_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("取り出し")));
		tray_btn_ = new wxButton(this, wxID_ANY, to_wx(_T("トレイを開く")));
		actions->Add(property_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(eject_btn_, wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(tray_btn_);
		actions->AddStretchSpacer();
		actions->Add(new wxButton(this, wxID_OK, to_wx(_T("移動"))), wxSizerFlags().Border(wxRIGHT, 4));
		actions->Add(new wxButton(this, wxID_CANCEL, to_wx(_T("キャンセル"))));
		top->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizerAndFit(top);
		CentreOnParent();

		only_acc_->Bind(wxEVT_CHECKBOX, &DriveSelectDialog::OnOptionChanged, this);
		show_icon_->Bind(wxEVT_CHECKBOX, &DriveSelectDialog::OnOptionChanged, this);
		large_icon_->Bind(wxEVT_CHECKBOX, &DriveSelectDialog::OnOptionChanged, this);
		to_root_->Bind(wxEVT_CHECKBOX, &DriveSelectDialog::OnOptionChanged, this);
		property_btn_->Bind(wxEVT_BUTTON, &DriveSelectDialog::OnUnimplemented, this);
		eject_btn_->Bind(wxEVT_BUTTON, &DriveSelectDialog::OnUnimplemented, this);
		tray_btn_->Bind(wxEVT_BUTTON, &DriveSelectDialog::OnUnimplemented, this);
		Bind(wxEVT_BUTTON, &DriveSelectDialog::OnOk, this, wxID_OK);

		RefreshList();
		const int initial = FindInitial();
		if (initial >= 0) SelectIndex(initial);
		else if (!visible_.empty()) SelectIndex(0);
		UpdateButtons();
		list_->SetFocus();
	}

	const Result &ResultValue() const { return result_; }

private:
	void RefreshList()
	{
		const UnicodeString old = SelectedPath();
		visible_ = drive_select::VisibleDrives(drives_, CurrentOptions());
		list_->DeleteAllItems();
		for (std::size_t i = 0; i < visible_.size(); ++i) {
			const drive_select::DriveInfo &drive = visible_[i];
			const long row = list_->InsertItem(static_cast<long>(i), to_wx(drive_select::DriveLetter(drive)));
			list_->SetItem(row, 1, to_wx(drive_label_for(drive)));
			list_->SetItem(row, 2, to_wx(drive.type_name));
			list_->SetItem(row, 3, to_wx(drive_select::FormatCapacityCell(drive, 3)));
			list_->SetItem(row, 4, to_wx(drive_select::FormatCapacityCell(drive, 4)));
			list_->SetItem(row, 5, to_wx(drive_select::FormatCapacityCell(drive, 5)));
			list_->SetItem(row, 6, to_wx(drive.filesystem));
		}
		const int keep = FindByPath(old);
		if (keep >= 0) SelectIndex(keep);
		else if (!visible_.empty()) SelectIndex(0);
		UpdateButtons();
	}

	drive_select::Options CurrentOptions() const
	{
		drive_select::Options options;
		options.only_accessible = only_acc_ != nullptr && only_acc_->GetValue();
		options.show_icons = show_icon_ != nullptr && show_icon_->GetValue();
		options.large_icons = large_icon_ != nullptr && large_icon_->GetValue();
		options.to_root = to_root_ != nullptr && to_root_->GetValue();
		return options;
	}

	int SelectedIndex() const
	{
		if (!list_) return -1;
		return list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}

	UnicodeString SelectedPath() const
	{
		const int index = SelectedIndex();
		if (index < 0 || index >= static_cast<int>(visible_.size())) return EmptyStr;
		return visible_[static_cast<std::size_t>(index)].path;
	}

	int FindByPath(const UnicodeString &path) const
	{
		if (path.IsEmpty()) return -1;
		for (std::size_t i = 0; i < visible_.size(); ++i) {
			if (SameText(visible_[i].path, path)) return static_cast<int>(i);
		}
		return -1;
	}

	int FindInitial() const
	{
		const UnicodeString drive = ExtractFileDrive(context_.current_path);
		if (drive.IsEmpty()) return -1;
		for (std::size_t i = 0; i < visible_.size(); ++i) {
			if (SameText(ExtractFileDrive(visible_[i].path), drive)) return static_cast<int>(i);
		}
		return -1;
	}

	void SelectIndex(int index)
	{
		if (index < 0 || index >= static_cast<int>(visible_.size())) return;
		list_->SetItemState(index, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
		                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		UpdateButtons();
	}

	void UpdateButtons()
	{
		const int index = SelectedIndex();
		const bool has = index >= 0 && index < static_cast<int>(visible_.size());
		property_btn_->Enable(has);
		if (!has) {
			eject_btn_->Disable();
			tray_btn_->Disable();
			return;
		}
		const drive_select::DriveInfo &drive = visible_[static_cast<std::size_t>(index)];
		eject_btn_->Enable(drive_select::CanEject(drive));
		tray_btn_->Enable(drive_select::CanOpenTray(drive));
	}

	void OnOptionChanged(wxCommandEvent &event)
	{
		large_icon_->Enable(show_icon_->GetValue());
		RefreshList();
		event.Skip();
	}

	void OnSelect(wxCommandEvent &) { UpdateButtons(); }

	void OnChar(wxKeyEvent &event)
	{
		const wchar_t key = event.GetUnicodeKey();
		if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
			Finish();
			return;
		}
		if ((key >= L'a' && key <= L'z') || (key >= L'A' && key <= L'Z')) {
			const int index = drive_select::FindByKey(visible_, UnicodeString(key));
			if (index >= 0) SelectIndex(index);
			return;
		}
		event.Skip();
	}

	void OnActivate(wxCommandEvent &) { Finish(); }

	void OnUnimplemented(wxCommandEvent &event)
	{
		wxObject *source = event.GetEventObject();
		UnicodeString feature = _T("この操作");
		if (source == property_btn_) feature = _T("ドライブのプロパティ表示");
		else if (source == eject_btn_) feature = _T("ドライブの取り出し");
		else if (source == tray_btn_) feature = _T("CD/DVD トレイを開く");
		wxMessageBox(to_wx(feature + _T("は未移植 (未実装扱い) です")),
		             to_wx(_T("ドライブ一覧")), wxOK | wxICON_WARNING, this);
	}

	void OnOk(wxCommandEvent &event)
	{
		Finish();
		event.Skip();
	}

	void Finish()
	{
		const int index = SelectedIndex();
		if (index < 0 || index >= static_cast<int>(visible_.size())) {
			wxMessageBox(to_wx(_T("ドライブを選んでください")), to_wx(_T("ドライブ一覧")),
			             wxOK | wxICON_INFORMATION, this);
			return;
		}
		const drive_select::DriveInfo &drive = visible_[static_cast<std::size_t>(index)];
		if (!drive_select::CanSelect(drive)) {
			wxMessageBox(to_wx(_T("アクセスできないドライブです")), to_wx(_T("ドライブ一覧")),
			             wxOK | wxICON_WARNING, this);
			return;
		}
		result_.selected = index;
		result_.path = drive_select::SelectionPath(drive, to_root_->GetValue());
		result_.options = CurrentOptions();
		EndModal(wxID_OK);
	}

	std::vector<drive_select::DriveInfo> drives_;
	Context context_;
	std::vector<drive_select::DriveInfo> visible_;
	Result result_;
	wxListCtrl *list_ = nullptr;
	wxCheckBox *only_acc_ = nullptr;
	wxCheckBox *show_icon_ = nullptr;
	wxCheckBox *large_icon_ = nullptr;
	wxCheckBox *to_root_ = nullptr;
	wxButton *property_btn_ = nullptr;
	wxButton *eject_btn_ = nullptr;
	wxButton *tray_btn_ = nullptr;
};

}  // namespace

//---------------------------------------------------------------------------
bool Run(wxWindow *parent, const std::vector<drive_select::DriveInfo> &drives,
         const Context &context, Result &result)
{
	DriveSelectDialog dlg(parent, drives, context);
	if (dlg.ShowModal() != wxID_OK) return false;
	result = dlg.ResultValue();
	return true;
}

}  // namespace drive_select_dialog
