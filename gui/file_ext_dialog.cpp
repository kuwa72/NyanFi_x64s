/**
 * @file gui/file_ext_dialog.cpp
 * @brief gui/file_ext_dialog.h の実装
 */
#include "gui/file_ext_dialog.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/file.h>

#include "gui/file_ext.h"
#include "gui/file_info_dialog.h"
#include "usr_file_ex.h"

namespace file_ext_dialog {

namespace {

constexpr int kMaxDisplayedFiles = 1000;

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

bool WriteUtf8(const wxString &path, const UnicodeString &text)
{
	wxFile file;
	if (!file.Create(path, true)) return false;
	const wxString value = to_wx(text);
	const wxCharBuffer utf8 = value.utf8_str();
	return file.Write(utf8.data(), utf8.length());
}

std::vector<UnicodeString> SplitReport(const UnicodeString &text)
{
	std::vector<UnicodeString> lines;
	UnicodeString rest = text;
	while (!rest.IsEmpty()) {
		UnicodeString line = split_tkn(rest, _T("\r\n"));
		if (SameStr(line, _T("\r")) || SameStr(line, _T("\n"))) continue;
		lines.push_back(line);
	}
	return lines;
}

UnicodeString DisplayExtension(const UnicodeString &ext)
{
	if (SameText(ext, _T("(none)"))) return _T(".");
	return StartsStr(_T("."), ext)? ext : _T(".") + ext;
}

FileItem ItemForPath(const UnicodeString &path)
{
	FileItem item;
	item.full_path = path;
	item.name = ExtractFileName(path);
	item.attr = file_GetAttr(path);
	item.is_dir = (item.attr & faDirectory) != 0;
	item.size = item.is_dir? -1 : get_file_size(path);
	item.stamp = get_file_age(path);
	return item;
}

class FileExtensionDialog final : public wxDialog {
public:
	FileExtensionDialog(wxWindow *parent, const UnicodeString &path, const Context &context,
	                    file_ext::Summary summary)
		: wxDialog(parent, wxID_ANY,
		           to_wx(_T("拡張子別一覧 - [") + ExcludeTrailingPathDelimiter(path) + _T("]")),
		           wxDefaultPosition, wxSize(760, 620),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  path_(path),
		  context_(context),
		  summary_(std::move(summary))
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		wxBoxSizer *sort_row = new wxBoxSizer(wxHORIZONTAL);
		sort_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("拡張子の並べ替え"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 6));
		ext_sort_ = new wxChoice(this, wxID_ANY);
		ext_sort_->Append(_T("拡張子"));
		ext_sort_->Append(_T("ファイル数"));
		ext_sort_->Append(_T("合計サイズ"));
		ext_sort_->Append(_T("平均サイズ"));
		ext_sort_->SetSelection(0);
		sort_row->Add(ext_sort_, wxSizerFlags().Border(wxRIGHT, 16));
		sort_row->Add(new wxStaticText(this, wxID_ANY, to_wx(_T("ファイルの並べ替え"))),
		              wxSizerFlags().CentreVertical().Border(wxRIGHT, 6));
		file_sort_ = new wxChoice(this, wxID_ANY);
		file_sort_->Append(_T("ファイル名"));
		file_sort_->Append(_T("場所"));
		file_sort_->SetSelection(0);
		sort_row->Add(file_sort_);
		top->Add(sort_row, wxSizerFlags().Expand().Border(wxALL, 8));

		ext_list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(720, 230));
		top->Add(ext_list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));
		file_header_ = new wxStaticText(this, wxID_ANY, to_wx(_T("ファイル")));
		top->Add(file_header_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, 8));
		file_list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(720, 210));
		top->Add(file_list_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 8));

		status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));
		if (summary_.truncated) {
			top->Add(new wxStaticText(this, wxID_ANY,
			                         to_wx(_T("※ 走査上限に達したため集計は途中までです"))),
			         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));
		}
		top->Add(new wxStaticText(this, wxID_ANY,
		                         to_wx(_T("※ クラスタ占有量・書庫/ライブラリ内・アクセス拒否件数は未移植"))),
		         wxSizerFlags().Border(wxLEFT | wxRIGHT, 8));

		wxBoxSizer *tools = new wxBoxSizer(wxHORIZONTAL);
		AddTool(this, tools, _T("コピー(&C)"), [this] { CopyFocused(); });
		AddTool(this, tools, _T("ログへ出力(&L)"), [this] { WriteLog(); });
		AddTool(this, tools, _T("一覧を保存(&A)"), [this] { Save(file_ext::OutputFormat::Text); });
		AddTool(this, tools, _T("CSV"), [this] { Save(file_ext::OutputFormat::Csv); });
		AddTool(this, tools, _T("TSV"), [this] { Save(file_ext::OutputFormat::Tsv); });
		AddTool(this, tools, _T("拡張子で検索(&M)"), [this] { FindByExtension(); });
		AddTool(this, tools, _T("ファイル情報(&I)"), [this] { ShowSelectedFileInfo(); });
		AddTool(this, tools, _T("開く"), [this] { OpenSelectedFile(); });
		tools->AddStretchSpacer();
		wxButton *close = new wxButton(this, wxID_CANCEL, to_wx(_T("閉じる")));
		tools->Add(close);
		top->Add(tools, wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizer(top);
		CentreOnParent();

		ext_list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) { PopulateFiles(); });
		file_list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &event) {
			UpdateFileStatus();
			event.Skip();
		});
		file_list_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) { OpenSelectedFile(); });
		ext_sort_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { SortExtensions(); });
		file_sort_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { PopulateFiles(); });
		Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);

		SortExtensions();
	}

	file_ext_dialog::Result GetResult() const { return result_; }

private:
	static wxButton *AddTool(wxWindow *parent, wxSizer *sizer, const UnicodeString &label,
	                         std::function<void()> fn)
	{
		wxButton *button = new wxButton(parent, wxID_ANY, to_wx(label));
		button->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
		sizer->Add(button, wxSizerFlags().Border(wxRIGHT, 6));
		return button;
	}

	static file_ext::ExtensionSort ChoiceToExtSort(int selection)
	{
		switch (selection) {
		case 1: return file_ext::ExtensionSort::Count;
		case 2: return file_ext::ExtensionSort::Bytes;
		case 3: return file_ext::ExtensionSort::Average;
		default: return file_ext::ExtensionSort::Extension;
		}
	}

	void SortExtensions()
	{
		const int old = ext_list_->GetSelection();
		file_ext::SortExtensions(summary_, ChoiceToExtSort(ext_sort_->GetSelection()), false);
		ext_list_->Clear();
		for (const file_ext::Entry &entry : summary_.extensions) {
			UnicodeString label;
			label.sprintf(_T("%-12s %10s %14s %14s"),
			              DisplayExtension(entry.extension).c_str(),
			              get_size_str_B(entry.count, 10).Trim().c_str(),
			              get_size_str_G(entry.bytes, 14, 2).Trim().c_str(),
			              get_size_str_G(static_cast<Int64>(entry.average), 14, 2).Trim().c_str());
			ext_list_->Append(to_wx(label));
		}
		if (!summary_.extensions.empty()) {
			const int select = old >= 0 && old < static_cast<int>(summary_.extensions.size())
				? old : 0;
			ext_list_->SetSelection(select);
			PopulateFiles();
		}
		else {
			file_list_->Clear();
			status_->SetLabel(to_wx(_T("ファイルがありません")));
		}
	}

	void PopulateFiles()
	{
		current_files_.clear();
		file_list_->Clear();
		const int index = ext_list_->GetSelection();
		if (index < 0 || index >= static_cast<int>(summary_.extensions.size())) return;

		current_files_ = summary_.extensions[index].files;
		file_ext::SortFiles(current_files_, file_sort_->GetSelection() == 1
			? file_ext::FileSort::Path : file_ext::FileSort::Name, false);
		const std::size_t shown = std::min(current_files_.size(),
		                                   static_cast<std::size_t>(kMaxDisplayedFiles));
		const UnicodeString root = IncludeTrailingPathDelimiter(path_);
		for (std::size_t i = 0; i < shown; ++i) {
			UnicodeString label = ExtractFileName(current_files_[i]);
			const UnicodeString dir = ExtractFilePath(current_files_[i]);
			if (StartsText(root, dir)) label += _T("  [") + dir.SubString(root.Length() + 1) + _T("]");
			else label += _T("  [") + dir + _T("]");
			file_list_->Append(to_wx(label));
		}
		const int count = summary_.extensions[index].count;
		if (count > kMaxDisplayedFiles) {
			file_list_->Append(to_wx(UnicodeString().sprintf(_T("…他 %d Files"), count - kMaxDisplayedFiles)));
		}
		file_header_->SetLabel(to_wx(_T("ファイル: ") + DisplayExtension(summary_.extensions[index].extension)));
		status_->SetLabel(to_wx(UnicodeString().sprintf(
			_T("ファイル数:%s  合計:%s  平均:%s"), get_size_str_B(count, 0).c_str(),
			get_size_str_G(summary_.extensions[index].bytes, 0, 2).c_str(),
			get_size_str_G(static_cast<Int64>(summary_.extensions[index].average), 0, 2).c_str())));
	}

	void UpdateFileStatus()
	{
		const UnicodeString path = SelectedFile();
		if (path.IsEmpty()) return;
		const Int64 size = get_file_size(path);
		status_->SetLabel(to_wx(UnicodeString().sprintf(
			_T("更新日時:%s  サイズ:%s"), FormatDateTime(_T("yyyy/mm/dd hh:nn:ss"), get_file_age(path)).c_str(),
			get_size_str_G(size, 0, 2).c_str())));
	}

	UnicodeString SelectedFile() const
	{
		const int index = file_list_->GetSelection();
		if (index < 0 || index >= static_cast<int>(current_files_.size())
		    || index >= kMaxDisplayedFiles) {
			return EmptyStr;
		}
		return current_files_[index];
	}

	std::vector<UnicodeString> FocusedLines(file_ext::OutputFormat format) const
	{
		if (file_list_->FindFocus() != nullptr) {
			std::vector<UnicodeString> lines;
			lines.push_back(IncludeTrailingPathDelimiter(path_));
			for (std::size_t i = 0; i < current_files_.size()
			                        && i < static_cast<std::size_t>(kMaxDisplayedFiles); ++i) {
				lines.push_back(current_files_[i]);
			}
			return lines;
		}
		(void)format;
		return SplitReport(file_ext::FormatReport(summary_, file_ext::OutputFormat::Text, path_));
	}

	void CopyFocused() const
	{
		UnicodeString text;
		const std::vector<UnicodeString> lines = FocusedLines(file_ext::OutputFormat::Text);
		for (const UnicodeString &line : lines) text += line + _T("\r\n");
		SetClipboard(text);
	}

	void WriteLog()
	{
		if (!context_.log_output) {
			wxMessageBox(to_wx(_T("ログ出力は未移植です")),
			            to_wx(_T("未実装")), wxOK | wxICON_INFORMATION, this);
			return;
		}
		context_.log_output(FocusedLines(file_ext::OutputFormat::Text));
	}

	void Save(file_ext::OutputFormat format)
	{
		const wxString wildcard = format == file_ext::OutputFormat::Csv
			? _T("CSV (*.csv)|*.csv")
			: format == file_ext::OutputFormat::Tsv? _T("TSV (*.tsv)|*.tsv")
			: _T("テキスト (*.txt)|*.txt");
		wxFileDialog dlg(this, to_wx(_T("一覧を保存")), to_wx(path_), wxEmptyString, wildcard,
		                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() != wxID_OK) return;
		const UnicodeString text = file_ext::FormatReport(summary_, format, path_);
		if (!WriteUtf8(dlg.GetPath(), text)) {
			wxMessageBox(to_wx(_T("保存できませんでした")), to_wx(_T("エラー")), wxOK | wxICON_ERROR, this);
		}
	}

	void FindByExtension()
	{
		const int index = ext_list_->GetSelection();
		if (index < 0 || index >= static_cast<int>(summary_.extensions.size())) return;
		result_ = file_ext_dialog::Result{Outcome::FindMask, EmptyStr,
		                 file_ext::BuildMask(summary_.extensions[index].extension)};
		EndModal(wxID_OK);
	}

	void ShowSelectedFileInfo()
	{
		const UnicodeString path = SelectedFile();
		if (path.IsEmpty()) return;
		const FileItem item = ItemForPath(path);
		file_info_dialog::Run(this, path, item);
	}

	void OpenSelectedFile()
	{
		const UnicodeString path = SelectedFile();
		if (path.IsEmpty()) return;
		result_ = file_ext_dialog::Result{Outcome::OpenFile, path, EmptyStr};
		EndModal(wxID_OK);
	}

	UnicodeString path_;
	Context context_;
	file_ext::Summary summary_;
	std::vector<UnicodeString> current_files_;
	wxChoice *ext_sort_ = nullptr;
	wxChoice *file_sort_ = nullptr;
	wxListBox *ext_list_ = nullptr;
	wxListBox *file_list_ = nullptr;
	wxStaticText *file_header_ = nullptr;
	wxStaticText *status_ = nullptr;
	file_ext_dialog::Result result_;
};

}  // namespace

//---------------------------------------------------------------------------
Result Run(wxWindow *parent, const UnicodeString &path, const Context &context)
{
	file_ext::Summary summary;
	bool truncated = false;
	{
		wxBusyCursor busy;
		summary = file_ext::Collect(path, true, context.show_hidden, context.show_system, truncated);
	}
	summary.truncated = truncated;
	FileExtensionDialog dlg(parent, path, context, std::move(summary));
	if (dlg.ShowModal() != wxID_OK) return Result{};
	return dlg.GetResult();
}

}  // namespace file_ext_dialog
