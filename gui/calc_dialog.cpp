/**
 * @file gui/calc_dialog.cpp
 * @brief gui/calc_dialog.h の実装
 */
#include "gui/calc_dialog.h"

#include <algorithm>
#include <cmath>

#include <wx/statline.h>
#include <wx/stattext.h>

namespace calc_dialog {
namespace {

inline wxString to_wx(const UnicodeString &value)
{
	return wxString(value.c_str(), static_cast<size_t>(value.Length()));
}

inline UnicodeString to_us(const wxString &value)
{
	return UnicodeString(value.wc_str());
}

class CalculatorDialog final : public wxDialog {
public:
	CalculatorDialog(wxWindow *parent, Context &context)
		: wxDialog(parent, wxID_ANY, to_wx(_T("電卓")), wxDefaultPosition, wxSize(520, 170),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  context_(context)
	{
		wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

		line_ = new wxTextCtrl(this, wxID_ANY, to_wx(context.initial_line), wxDefaultPosition,
		                        wxDefaultSize, wxTE_PROCESS_ENTER);
		top->Add(line_, wxSizerFlags(1).Expand().Border(wxALL, 8));

		wxArrayString history_items;
		for (const UnicodeString &entry : context.history) history_items.Add(to_wx(entry));
		history_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		                          history_items, wxCB_DROPDOWN);
		top->Add(history_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));

		wxBoxSizer *tools = new wxBoxSizer(wxHORIZONTAL);
		angle_ = new wxButton(this, wxID_ANY, to_wx(AngleName(context.angle_mode)));
		now_ = new wxButton(this, wxID_ANY, _T("NOW"));
		hex_ = new wxButton(this, wxID_ANY, _T("HEX/DEC"));
		not_ = new wxButton(this, wxID_ANY, _T("NOT"));
		clear_ = new wxButton(this, wxID_ANY, _T("AC"));
		tools->Add(angle_, wxSizerFlags().Border(wxRIGHT, 4));
		tools->Add(now_, wxSizerFlags().Border(wxRIGHT, 4));
		tools->Add(hex_, wxSizerFlags().Border(wxRIGHT, 4));
		tools->Add(not_, wxSizerFlags().Border(wxRIGHT, 4));
		tools->Add(clear_);
		top->Add(tools, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));

		status_ = new wxStaticText(this, wxID_ANY,
		                           to_wx(_T("式を入力して Enter。式評価は gui/calc.* が担当")));
		top->Add(status_, wxSizerFlags().Expand().Border(wxALL, 8));
		top->Add(new wxStaticLine(this), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 8));
		top->Add(CreateButtonSizer(wxOK), wxSizerFlags().Expand().Border(wxALL, 8));

		SetSizerAndFit(top);
		CentreOnParent();
		SetEscapeId(wxID_CANCEL);

		line_->Bind(wxEVT_TEXT_ENTER, &CalculatorDialog::OnCalculate, this);
		history_->Bind(wxEVT_COMBOBOX, &CalculatorDialog::OnHistory, this);
		angle_->Bind(wxEVT_BUTTON, &CalculatorDialog::OnAngle, this);
		now_->Bind(wxEVT_BUTTON, &CalculatorDialog::OnNow, this);
		hex_->Bind(wxEVT_BUTTON, &CalculatorDialog::OnHex, this);
		not_->Bind(wxEVT_BUTTON, &CalculatorDialog::OnNot, this);
		clear_->Bind(wxEVT_BUTTON, &CalculatorDialog::OnClear, this);
		Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
		line_->SetFocus();
		line_->SelectAll();
	}

	Context &ResultContext() { return context_; }

private:
	static const wchar_t *AngleName(calc::AngleMode mode)
	{
		switch (mode) {
		case calc::AngleMode::Rad: return _T("RAD");
		case calc::AngleMode::Grad: return _T("GRAD");
		default: return _T("DEG");
		}
	}

	calc::EvalOptions Options() const
	{
		calc::EvalOptions options;
		options.angle_mode = context_.angle_mode;
		options.output_digits = context_.output_digits;
		options.comma_grouping = context_.comma_grouping;
		return options;
	}

	void SetStatus(const UnicodeString &text)
	{
		status_->SetLabel(to_wx(text));
	}

	void OnCalculate(wxCommandEvent &event)
	{
		const UnicodeString input = to_us(line_->GetValue()).Trim();
		if (input.IsEmpty()) return;
		const calc::EvalResult result = calc::Evaluate(input, Options());
		if (!result.ok) {
			SetStatus(_T("ERR: ") + result.error);
			return;
		}
		const UnicodeString entry = input + _T(" = ") + result.value;
		if (context_.history.empty() || context_.history.front() != entry) {
			context_.history.insert(context_.history.begin(), entry);
			if (context_.history.size() > 12) context_.history.resize(12);
		}
		history_->Clear();
		for (const UnicodeString &item : context_.history) history_->Append(to_wx(item));
		history_->SetSelection(0);
		line_->ChangeValue(to_wx(result.value));
		line_->SetInsertionPointEnd();
		SetStatus(_T("= ") + result.value);
		event.Skip();
	}

	void OnHistory(wxCommandEvent &event)
	{
		const int index = history_->GetSelection();
		if (index < 0 || index >= static_cast<int>(context_.history.size())) return;
		UnicodeString value = context_.history[static_cast<std::size_t>(index)];
		const int equal = value.Pos(L'=');
		if (equal > 0) value = value.SubString(1, equal - 1).Trim();
		line_->ChangeValue(to_wx(value));
		line_->SetInsertionPointEnd();
		event.Skip();
	}

	void OnAngle(wxCommandEvent &event)
	{
		switch (context_.angle_mode) {
		case calc::AngleMode::Deg: context_.angle_mode = calc::AngleMode::Rad; break;
		case calc::AngleMode::Rad: context_.angle_mode = calc::AngleMode::Grad; break;
		default: context_.angle_mode = calc::AngleMode::Deg; break;
		}
		angle_->SetLabel(to_wx(AngleName(context_.angle_mode)));
		SetStatus(UnicodeString(_T("角度: ")) + AngleName(context_.angle_mode));
		event.Skip();
	}

	void OnNow(wxCommandEvent &event)
	{
		const calc::EvalResult result = calc::Evaluate(_T("Now"), Options());
		if (result.ok) line_->ChangeValue(to_wx(result.value));
		event.Skip();
	}

	void OnHex(wxCommandEvent &event)
	{
		const UnicodeString old = to_us(line_->GetValue());
		const UnicodeString value = calc::ToggleHex(old);
		if (value == old) SetStatus(_T("16進/10進の入力ではありません"));
		else line_->ChangeValue(to_wx(value));
		event.Skip();
	}

	void OnNot(wxCommandEvent &event)
	{
		const calc::EvalResult result = calc::Evaluate(to_us(line_->GetValue()), Options());
		if (!result.ok || std::trunc(result.number) != result.number) {
			SetStatus(_T("整数を入力してください"));
			return;
		}
		line_->ChangeValue(to_wx(calc::BitwiseNot(static_cast<long long>(result.number))));
		event.Skip();
	}

	void OnClear(wxCommandEvent &event)
	{
		line_->ChangeValue(wxEmptyString);
		line_->SetFocus();
		event.Skip();
	}

	Context &context_;
	wxTextCtrl *line_ = nullptr;
	wxComboBox *history_ = nullptr;
	wxButton *angle_ = nullptr;
	wxButton *now_ = nullptr;
	wxButton *hex_ = nullptr;
	wxButton *not_ = nullptr;
	wxButton *clear_ = nullptr;
	wxStaticText *status_ = nullptr;
};

}  // namespace

bool Run(wxWindow *parent, Context &context)
{
	CalculatorDialog dialog(parent, context);
	if (dialog.ShowModal() != wxID_OK) return false;
	context = dialog.ResultContext();
	return true;
}

}  // namespace calc_dialog
