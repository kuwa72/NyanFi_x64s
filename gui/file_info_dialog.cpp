/**
 * @file gui/file_info_dialog.cpp
 * @brief gui/file_info_dialog.h の実装
 */
#include "gui/file_info_dialog.h"

#include <functional>
#include <memory>
#include <vector>

#include <wx/clipbrd.h>
#include <wx/menu.h>

#include "usr_file_ex.h"
#include "usr_str.h"

namespace file_info_dialog {

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

void SetClipboard(const UnicodeString &text)
{
	if (!wxTheClipboard->Open()) return;
	wxTheClipboard->SetData(new wxTextDataObject(to_wx(text)));
	wxTheClipboard->Close();
}

std::vector<UnicodeString> ReadFileInfo(const UnicodeString &full_path, const FileItem &item)
{
	std::vector<UnicodeString> lines;
	std::unique_ptr<TStringList> list(new TStringList());
	try {
		BuildFileInfoLines(full_path, item, list.get());
	}
	catch (const Exception &e) {
		list->Add(_T("エラー: ") + UnicodeString(e.Message));
	}
	catch (...) {
		list->Add(_T("エラー: 情報の取得中に例外が発生しました"));
	}
	for (int i = 0; i < list->Count; ++i) lines.push_back(list->Strings[i]);
	return lines;
}

class InfoDialog final : public wxDialog {
public:
	InfoDialog(wxWindow *parent, const UnicodeString &title, const std::vector<UnicodeString> &lines,
	           const UnicodeString &full_path, const FileItem *file_item)
		: wxDialog(parent, wxID_ANY, to_wx(title), wxDefaultPosition, wxSize(620, 500),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  full_path_(full_path),
		  file_item_(file_item)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
		list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
		                      wxLB_EXTENDED | wxLB_HSCROLL);
		for (const UnicodeString &line : lines) list_->Append(to_wx(line));
		top->Add(list_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxBoxSizer *tools = new wxBoxSizer(wxHORIZONTAL);
		AddTool(this, tools, _T("情報をコピー(&I)"), [this] { CopyAll(); });
		AddTool(this, tools, _T("項目の値をコピー(&V)"), [this] { CopyValue(); });
		AddTool(this, tools, _T("すべて選択(&A)"), [this] { SelectAll(); });
		hash_btn_ = AddTool(this, tools, _T("ハッシュを計算(&H)"), [this] { AppendHash(); });
		location_btn_ = AddTool(this, tools, _T("場所を開く(&L)"), [this] { OpenLocation(); });
		hash_btn_->Enable(file_item_ != nullptr && !file_item_->is_dir);
		location_btn_->Enable(!location_.IsEmpty());
		tools->AddStretchSpacer();
		tools->Add(CreateButtonSizer(wxOK), wxSizerFlags());
		top->Add(tools, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		SetSizer(top);
		CentreOnParent();

		list_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) { OpenLocation(); });
		list_->Bind(wxEVT_CONTEXT_MENU, [this](wxCommandEvent &) { ShowContextMenu(); });
		Bind(wxEVT_BUTTON, [this](wxCommandEvent &event) {
			result_ = file_info_dialog::Result{Outcome::Closed, EmptyStr};
			event.Skip();
		}, wxID_OK);

		if (!full_path_.IsEmpty()) {
			location_ = file_item_ != nullptr && file_item_->is_dir
				? ExcludeTrailingPathDelimiter(full_path_)
				: ExtractFilePath(full_path_);
		}
		location_btn_->Enable(!location_.IsEmpty());
	}

	file_info_dialog::Result GetResult() const { return result_; }

private:
	static wxButton *AddTool(wxWindow *parent, wxSizer *sizer, const UnicodeString &label,
	                         std::function<void()> fn)
	{
		wxButton *button = new wxButton(parent, wxID_ANY, to_wx(label));
		button->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
		sizer->Add(button, wxSizerFlags().Border(wxRIGHT, 6));
		return button;
	}

	void SelectAll() const
	{
		for (unsigned int i = 0; i < list_->GetCount(); ++i) list_->SetSelection(i, true);
	}

	void CopyAll() const
	{
		UnicodeString text;
		wxArrayInt selected;
		list_->GetSelections(selected);
		if (selected.empty()) {
			for (unsigned int i = 0; i < list_->GetCount(); ++i) {
				text += to_us(list_->GetString(i)) + _T("\r\n");
			}
		}
		else {
			for (int index : selected) {
				if (index >= 0 && index < static_cast<int>(list_->GetCount())) {
					text += to_us(list_->GetString(index)) + _T("\r\n");
				}
			}
		}
		SetClipboard(text);
	}

	void CopyValue() const
	{
		const int index = list_->GetSelection();
		if (index == wxNOT_FOUND) return;
		const UnicodeString line = to_us(list_->GetString(index));
		SetClipboard(Trim(get_tkn_r(line, _T(": "))));
	}

	void AppendHash()
	{
		if (full_path_.IsEmpty() || file_item_ == nullptr || file_item_->is_dir) return;
		hash_btn_->Disable();
		wxBeginBusyCursor();
		std::unique_ptr<TStringList> extra(new TStringList());
		try {
			AppendHashLines(full_path_, extra.get());
		}
		catch (...) {
			extra->Add(_T("エラー: ハッシュの計算中に例外が発生しました"));
		}
		wxEndBusyCursor();
		for (int i = 0; i < extra->Count; ++i) list_->Append(to_wx(extra->Strings[i]));
		hash_btn_->Enable(true);
	}

	void OpenLocation()
	{
		if (location_.IsEmpty()) return;
		result_ = file_info_dialog::Result{Outcome::OpenLocation, location_};
		EndModal(wxID_OK);
	}

	void ShowContextMenu()
	{
		wxMenu menu;
		enum { ID_COPY_ALL = wxID_HIGHEST + 1, ID_COPY_VALUE, ID_LOCATION };
		menu.Append(ID_COPY_ALL, to_wx(_T("ファイル情報をコピー(&I)")));
		menu.Append(ID_COPY_VALUE, to_wx(_T("項目の値をコピー(&V)")));
		if (!location_.IsEmpty()) menu.Append(ID_LOCATION, to_wx(_T("場所を開く(&L)")));
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { CopyAll(); }, ID_COPY_ALL);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { CopyValue(); }, ID_COPY_VALUE);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent &) { OpenLocation(); }, ID_LOCATION);
		PopupMenu(&menu);
	}

	wxListBox *list_ = nullptr;
	wxButton *hash_btn_ = nullptr;
	wxButton *location_btn_ = nullptr;
	UnicodeString full_path_;
	UnicodeString location_;
	const FileItem *file_item_ = nullptr;
	file_info_dialog::Result result_;
};

}  // namespace

//---------------------------------------------------------------------------
Result Run(wxWindow *parent, const UnicodeString &full_path, const FileItem &item)
{
	const std::vector<UnicodeString> lines = ReadFileInfo(full_path, item);
	const UnicodeString title = _T("ファイル情報: ") + item.name;
	InfoDialog dlg(parent, title, lines, full_path, &item);
	if (dlg.ShowModal() != wxID_OK) return Result{};
	return dlg.GetResult();
}

//---------------------------------------------------------------------------
Result Run(wxWindow *parent, const file_info::ColumnStats &stats)
{
	const std::vector<UnicodeString> lines = file_info::BuildColumnStatLines(stats);
	const UnicodeString title = stats.format == file_info::TableFormat::Csv
		? _T("CSV項目の集計")
		: _T("TSV項目の集計");
	InfoDialog dlg(parent, title, lines, EmptyStr, nullptr);
	if (dlg.ShowModal() != wxID_OK) return Result{};
	return dlg.GetResult();
}

}  // namespace file_info_dialog
