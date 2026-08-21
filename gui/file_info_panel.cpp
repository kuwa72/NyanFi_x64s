/**
 * @file gui/file_info_panel.cpp
 * @brief ファイル情報ダイアログの実装
 */
#include "gui/file_info_panel.h"

#include <memory>

#include "gui/file_info.h"

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// TStringList の内容をまとめて wxTextCtrl に反映する
void ApplyText(wxTextCtrl *text_ctrl, const TStringList &lst)
{
	text_ctrl->SetValue(to_wx(lst.Text));
}

}  // namespace

//---------------------------------------------------------------------------
void ShowFileInfoDialog(wxWindow *parent, const UnicodeString &full_path, const FileItem &item)
{
	std::unique_ptr<TStringList> lst(new TStringList());

	// 解析関数 (Exif/WIC など) が壊れたファイルで例外を投げても、ダイアログ
	// ごと落とさない。実際に起きた場合はエラー行として表示する
	try {
		BuildFileInfoLines(full_path, item, lst.get());
	}
	// Exception (compat/exception.h) は Message (UnicodeString) を正しく持つので
	// std::exception より先に捕まえる。what() は診断用の UTF-8 で、
	// UnicodeString(const char*) は ANSI コードページとして解釈するため
	// 文字化けする (compat/exception.h のコメント参照)
	catch (const Exception &e) {
		lst->Add(_T("エラー: ") + UnicodeString(e.Message));
	}
	catch (const std::exception &) {
		lst->Add(_T("エラー: 情報の取得中に例外が発生しました"));
	}
	catch (...) {
		lst->Add(_T("エラー: 情報の取得中に例外が発生しました"));
	}

	wxDialog dlg(parent, wxID_ANY, to_wx(_T("ファイル情報: ") + item.name), wxDefaultPosition, wxSize(560, 480),
	             wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

	wxTextCtrl *text_ctrl = new wxTextCtrl(&dlg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
	                                        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	text_ctrl->SetFont(wxFont(wxFontInfo().Family(wxFONTFAMILY_TELETYPE)));
	ApplyText(text_ctrl, *lst);

	// ハッシュ (SHA256/CRC32) は大きいファイルだと時間がかかるため、常には
	// 計算せずボタンで明示的に行う (gui/file_info.h の AppendHashLines)
	wxButton *hash_btn = new wxButton(&dlg, wxID_ANY, to_wx(_T("ハッシュを計算(&H)")));
	hash_btn->Enable(!item.is_dir);
	hash_btn->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
		hash_btn->Disable();
		wxBeginBusyCursor();
		try {
			AppendHashLines(full_path, lst.get());
		}
		catch (...) {
			lst->Add(_T("エラー: ハッシュの計算中に例外が発生しました"));
		}
		wxEndBusyCursor();
		ApplyText(text_ctrl, *lst);
	});

	wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
	top->Add(text_ctrl, wxSizerFlags(1).Expand().Border(wxALL, 8));

	wxBoxSizer *btn_row = new wxBoxSizer(wxHORIZONTAL);
	btn_row->Add(hash_btn, wxSizerFlags().Border(wxLEFT | wxBOTTOM | wxRIGHT, 8));
	btn_row->AddStretchSpacer();
	btn_row->Add(dlg.CreateButtonSizer(wxOK), wxSizerFlags().Border(wxLEFT | wxBOTTOM | wxRIGHT, 8));
	top->Add(btn_row, wxSizerFlags().Expand());

	dlg.SetSizer(top);
	dlg.CentreOnParent();
	dlg.ShowModal();
}
