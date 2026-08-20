/**
 * @file gui/main_frame.cpp
 * @brief メインウィンドウの実装
 */
#include "gui/main_frame.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/textdlg.h>

#include "gui/file_info_panel.h"
#include "gui/file_open.h"
#include "gui/file_ops.h"
#include "usr_cmdlist.h"
#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_str.h"

namespace {

inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/// wxString → UnicodeString (wxTextEntryDialog の戻り値用。MSW では両方 UTF-16)
inline UnicodeString to_us(const wxString &s)
{
	return UnicodeString(s.wc_str());
}

/// 起動時に開くディレクトリ
UnicodeString initial_path()
{
	const UnicodeString cur = IncludeTrailingPathDelimiter(GetCurrentDir());
	return dir_exists(cur) ? cur : UnicodeString(_T("C:\\"));
}

/// 破壊的操作 (コピー・移動・削除) の前に出す確認ダイアログ
/// @details 件数と、対象の先頭数件の名前、コピー/移動先 (削除なら空) を明示する
bool ConfirmItems(wxWindow *parent, const UnicodeString &title, const UnicodeString &verb,
                   const std::vector<UnicodeString> &names, const UnicodeString &dest)
{
	UnicodeString text;
	text.sprintf(_T("%d 件を%sします。よろしいですか?\n\n"), static_cast<int>(names.size()), verb.c_str());

	const std::size_t show = std::min<std::size_t>(names.size(), 8);
	for (std::size_t i = 0; i < show; ++i) text += _T("・") + names[i] + _T("\n");
	if (names.size() > show) text.cat_sprintf(_T("...ほか %d 件\n"), static_cast<int>(names.size() - show));

	if (!dest.IsEmpty()) text += _T("\n宛先: ") + dest;

	return wxMessageBox(to_wx(text), to_wx(title), wxYES_NO | wxICON_QUESTION, parent) == wxYES;
}

/// 実行可能ファイルを開く前の確認 (誤って起動してしまう事故を防ぐ)
bool ConfirmExecute(wxWindow *parent, const UnicodeString &full_path)
{
	const UnicodeString text = _T("実行可能ファイルです。開いてもよろしいですか?\n\n") + full_path;
	return wxMessageBox(to_wx(text), to_wx(_T("実行の確認")), wxYES_NO | wxICON_WARNING, parent) == wxYES;
}

}  // namespace

//---------------------------------------------------------------------------
MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "NyanFi (wxWidgets port)", wxDefaultPosition, wxSize(1000, 640))
{
	wxPanel *root = new wxPanel(this);
	wxBoxSizer *columns = new wxBoxSizer(wxHORIZONTAL);

	for (int i = 0; i < 2; ++i) {
		wxBoxSizer *column = new wxBoxSizer(wxVERTICAL);

		headers_[i] = new wxStaticText(root, wxID_ANY, wxEmptyString);
		panes_[i] = new FilePane(root, wxID_ANY);

		column->Add(headers_[i], wxSizerFlags().Expand().Border(wxLEFT | wxTOP, 4));
		column->Add(panes_[i], wxSizerFlags(1).Expand().Border(wxALL, 2));
		columns->Add(column, wxSizerFlags(1).Expand());
	}

	root->SetSizer(columns);

	CreateStatusBar(2);
	SetStatusWidths(2, std::array<int, 2>{-3, -1}.data());

	// キー割り当て: VCL 版と同じ ini (<exe名>.ini) に "KeyFuncList" セクションが
	// あれば、既定の割り当て (key_map.cpp の LoadDefaults) を上書きする。
	// 読み込みのみで書き込みは行わない (settings_ とは別ファイル。理由は
	// gui/settings.h と gui/key_map.h のコメントを参照)
	keymap_.LoadFromIni(ChangeFileExt(Application->ExeName, _T(".ini")));

	// ウィンドウ位置・ペインのディレクトリを復元する (無ければ既定)
	LoadSettings();
	SetActivePane(0);

	Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
	Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

//---------------------------------------------------------------------------
/// 起動時: settings_ (nyanfi_wx.ini) からウィンドウ位置・ペインのディレクトリを復元する
void MainFrame::LoadSettings()
{
	const UnicodeString start = initial_path();

	const UnicodeString left_dir  = settings_.LeftDir;
	const UnicodeString right_dir = settings_.RightDir;
	panes_[0]->SetPath(!left_dir.IsEmpty()  && dir_exists(left_dir)  ? left_dir  : start);
	panes_[1]->SetPath(!right_dir.IsEmpty() && dir_exists(right_dir) ? right_dir : start);

	const WindowState &w = settings_.Window;
	SetSize(wxSize(w.width, w.height));
	if (w.left != -1 && w.top != -1) SetPosition(wxPoint(w.left, w.top));
	if (w.maximized) Maximize(true);
}

//---------------------------------------------------------------------------
/// 終了時: 現在のウィンドウ位置・ペインのディレクトリを settings_ 経由で保存する
void MainFrame::SaveSettings()
{
	settings_.LeftDir  = panes_[0]->GetPath();
	settings_.RightDir = panes_[1]->GetPath();

	settings_.Window.maximized = IsMaximized();
	// 最大化中は座標・サイズを更新しない (次回、通常サイズに戻したときの
	// 位置・サイズが復元できるようにするため)
	if (!settings_.Window.maximized) {
		const wxPoint pos  = GetPosition();
		const wxSize  size = GetSize();
		settings_.Window.left	 = pos.x;
		settings_.Window.top	 = pos.y;
		settings_.Window.width	 = size.x;
		settings_.Window.height = size.y;
	}

	settings_.Save();
}

//---------------------------------------------------------------------------
void MainFrame::SetActivePane(int index)
{
	active_ = (index == 0) ? 0 : 1;
	panes_[active_]->SetActive(true);
	panes_[1 - active_]->SetActive(false);
	panes_[active_]->SetFocus();
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::UpdateStatus()
{
	for (int i = 0; i < 2; ++i) {
		const UnicodeString mark = (i == active_) ? _T("● ") : _T("　 ");

		// パス + 並べ替えキー/方向 + (絞り込み中なら) マスクをヘッダに出す。
		// マスク中は絞り込み状態が見えないと危険なので必ず表示する
		UnicodeString label = mark + panes_[i]->GetPath() + _T("  [") + panes_[i]->GetSortSummary() + _T("]");
		if (panes_[i]->HasMask()) label += _T("  マスク: ") + panes_[i]->GetMask();
		headers_[i]->SetLabel(to_wx(label));
	}

	FilePane *pane = ActivePane();
	SetStatusText(to_wx(pane->GetSummary()), 0);

	const FileItem *itm = pane->GetCurrentItem();
	SetStatusText(itm != nullptr ? to_wx(itm->name) : wxString(), 1);
}

//---------------------------------------------------------------------------
void MainFrame::OnCharHook(wxKeyEvent &event)
{
	const UnicodeString key_str = KeyMap::KeyStrOf(event);
	const UnicodeString command = keymap_.Lookup(key_str);

	if (command.IsEmpty() || !Execute(command)) {
		event.Skip();  // 割り当ての無いキーは通常処理へ
		return;
	}
	UpdateStatus();
}

//---------------------------------------------------------------------------
void MainFrame::OnClose(wxCloseEvent &event)
{
	SaveSettings();
	event.Skip();
}

//---------------------------------------------------------------------------
/**
 * @brief コマンド名で処理を実行する
 * @details コマンド名の綴りは usr_cmdlist.cpp のコマンド表に合わせている。
 *          未実装のコマンドは false を返し、キーは通常処理に流す。
 */
bool MainFrame::Execute(const UnicodeString &command)
{
	FilePane *pane = ActivePane();

	if (SameStr(command, _T("CursorUp"))) {
		pane->MoveCursor(-1);
	}
	else if (SameStr(command, _T("CursorDown"))) {
		pane->MoveCursor(1);
	}
	else if (SameStr(command, _T("PageUp"))) {
		pane->PageMove(-1);
	}
	else if (SameStr(command, _T("PageDown"))) {
		pane->PageMove(1);
	}
	else if (SameStr(command, _T("CursorTop"))) {
		pane->CursorTop();
	}
	else if (SameStr(command, _T("CursorEnd"))) {
		pane->CursorEnd();
	}
	else if (SameStr(command, _T("OpenStandard"))) {
		if (!pane->EnterCurrent()) CmdOpenStandard();  // ディレクトリでなければ関連付けで開く
	}
	else if (SameStr(command, _T("OpenByApp"))) {
		CmdOpenByApp();
	}
	else if (SameStr(command, _T("PropertyDlg"))) {
		CmdPropertyDlg();
	}
	else if (SameStr(command, _T("UpDir")) || SameStr(command, _T("ToLeft"))) {
		pane->GoParent();
	}
	else if (SameStr(command, _T("ToRight"))) {
		pane->EnterCurrent();
	}
	else if (SameStr(command, _T("ChangePane"))) {
		SetActivePane(1 - active_);
	}
	else if (SameStr(command, _T("Refresh"))) {
		pane->Reload();
	}
	else if (SameStr(command, _T("MarkItem"))) {
		pane->ToggleMark();
	}
	else if (SameStr(command, _T("MarkAll"))) {
		pane->MarkAll(true);
	}
	else if (SameStr(command, _T("UnMarkAll"))) {
		pane->MarkAll(false);
	}
	else if (SameStr(command, _T("ShowKeyList"))) {
		ShowKeyList();
	}
	else if (SameStr(command, _T("ShowCmdList"))) {
		ShowCmdList();
	}
	else if (SameStr(command, _T("SortDlg"))) {
		ShowSortDialog();
	}
	else if (SameStr(command, _T("SetPathMask"))) {
		ShowMaskDialog();
	}
	else if (SameStr(command, _T("ClearMask"))) {
		pane->SetMask(EmptyStr);
	}
	else if (SameStr(command, _T("Copy"))) {
		CmdCopy();
	}
	else if (SameStr(command, _T("Move"))) {
		CmdMove();
	}
	else if (SameStr(command, _T("Delete"))) {
		CmdDelete();
	}
	else if (SameStr(command, _T("CreateDir"))) {
		CmdCreateDir();
	}
	else if (SameStr(command, _T("RenameDlg"))) {
		CmdRenameDlg();
	}
	else if (SameStr(command, _T("Exit"))) {
		Close(true);
	}
	else {
		return false;  // 未実装
	}
	return true;
}

//---------------------------------------------------------------------------
/// 現在のキー割り当てを一覧表示する (F1)
void MainFrame::ShowKeyList()
{
	const TStringList *entries = keymap_.Entries();

	UnicodeString text = _T("カーソルキー (UP/DOWN/LEFT/RIGHT) は get_CsrKeyCmd() の割り当てを使用\n\n");
	for (int i = 0; i < entries->Count; ++i) {
		text += entries->Strings[i] + _T("\n");
	}

	wxMessageBox(to_wx(text), to_wx(_T("キー割り当て")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/// 移植済みのコマンド表の規模と、いま実装済みのコマンドを表示する (F12)
void MainFrame::ShowCmdList()
{
	// set_CmdList は VCL 版と同じコマンド表を作る。GUI が未実装でも表は生きている
	std::unique_ptr<TStringList> cmds(new TStringList());
	std::unique_ptr<TStringList> ids(new TStringList());
	set_CmdList(cmds.get(), ids.get());

	UnicodeString sample;
	for (int i = 0; i < cmds->Count && i < 12; ++i) {
		sample += cmds->Strings[i] + _T("\n");
	}

	// 実装済みのコマンドは keymap の割り当て先から拾う (ここで Execute は呼ばない)
	const TStringList *keys = keymap_.Entries();
	UnicodeString impl;
	for (int i = 0; i < keys->Count; ++i) {
		impl += keys->Strings[i] + _T("\n");
	}

	UnicodeString text;
	text.sprintf(_T("コマンド表: %d 件 (usr_cmdlist.cpp より)\n\n先頭 12 件:\n%s\n")
	             _T("Phase 2 骨格で実装済みのキー割り当て:\n%s"),
	             static_cast<int>(cmds->Count), sample.c_str(), impl.c_str());
	wxMessageBox(to_wx(text), to_wx(_T("コマンド一覧")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief ソートダイアログ (S)
 * @details SrtModDlg.cpp (ソートダイアログ) の簡略版。並べ替えキー (名前/拡張子/
 *          日時/サイズ/属性。SortModeRadioGroup と同じ並び)・降順・ディレクトリを
 *          先に集めるか、の3点だけを選ぶ。SrtModDlg.cpp にある「両側に適用」
 *          「拡張子リストの優先順」「記号順」などの拡張オプションは対象外
 *          (Phase 2 骨格のスコープ外)
 */
void MainFrame::ShowSortDialog()
{
	FilePane *pane = ActivePane();

	wxDialog dlg(this, wxID_ANY, to_wx(_T("ソート")));

	wxArrayString choices;
	choices.Add(to_wx(_T("名前(&F)")));
	choices.Add(to_wx(_T("拡張子(&E)")));
	choices.Add(to_wx(_T("日時(&D)")));
	choices.Add(to_wx(_T("サイズ(&S)")));
	choices.Add(to_wx(_T("属性(&A)")));

	wxRadioBox *key_box = new wxRadioBox(&dlg, wxID_ANY, to_wx(_T("並べ替えキー")), wxDefaultPosition,
	                                      wxDefaultSize, choices, 1, wxRA_SPECIFY_COLS);
	key_box->SetSelection(static_cast<int>(pane->GetSortKey()));

	wxCheckBox *desc_box = new wxCheckBox(&dlg, wxID_ANY, to_wx(_T("降順")));
	desc_box->SetValue(pane->IsSortDescending());

	wxCheckBox *dirs_box = new wxCheckBox(&dlg, wxID_ANY, to_wx(_T("ディレクトリを先に集める")));
	dirs_box->SetValue(pane->IsDirsFirst());

	wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
	top->Add(key_box, wxSizerFlags().Expand().Border(wxALL, 8));
	top->Add(desc_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	top->Add(dirs_box, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 8));
	top->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Expand().Border(wxALL, 8));
	dlg.SetSizerAndFit(top);
	dlg.CentreOnParent();

	if (dlg.ShowModal() != wxID_OK) return;

	pane->SetSortSettings(static_cast<SortKey>(key_box->GetSelection()), desc_box->GetValue(), dirs_box->GetValue());
}

//---------------------------------------------------------------------------
/**
 * @brief パスマスクの入力 (Ctrl+M)
 * @details MainFrm.cpp の ApplyPathMask/SplitMasksFD と同じ書式
 *          (";" 区切り、末尾 "\\" でディレクトリ用、先頭 "!" で除外) を
 *          gui/file_item.cpp::MatchPathMask がそのまま解釈する。
 *          空文字列で OK すると絞り込みを解除する (ClearMask と同じ効果)
 */
void MainFrame::ShowMaskDialog()
{
	FilePane *pane = ActivePane();

	wxTextEntryDialog dlg(this, to_wx(_T("ファイル名マスク (例: *.txt;*.doc;!*.tmp)\n空にすると解除")),
	                       to_wx(_T("パスマスクを設定")), to_wx(pane->GetMask()));
	if (dlg.ShowModal() != wxID_OK) return;

	pane->SetMask(to_us(dlg.GetValue()));
}

//---------------------------------------------------------------------------
/**
 * @brief コピー (C)
 * @details 対象はアクティブペインの選択項目 (マーク済み、無ければカーソル位置の
 * 1件)。宛先は反対側のペインの現在のディレクトリ固定 (指定ディレクトリへの
 * コピー(CopyTo)は対象外)。実行前に必ず確認ダイアログを出し、実行後は必ず
 * 結果 (成功/スキップ/失敗の件数) を表示する。ディレクトリの再帰コピー・
 * 上書き回避の判断は gui/file_ops.h を参照
 */
void MainFrame::CmdCopy()
{
	FilePane *pane = ActivePane();
	FilePane *dst_pane = OppositePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("コピー対象がありません")), to_wx(_T("コピー")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	const UnicodeString dst_dir = dst_pane->GetPath();
	if (!ConfirmItems(this, _T("コピー"), _T("コピー"), names, dst_dir)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = file_ops::CopyItems(paths, dst_dir);

	pane->Reload();
	dst_pane->Reload();

	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(_T("コピーの結果")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief 移動 (M)
 * @details Copy と同じ対象の決め方・確認・結果表示。ディレクトリの移動が
 * ボリュームを跨ぐ場合は失敗として報告する (gui/file_ops.h を参照)
 */
void MainFrame::CmdMove()
{
	FilePane *pane = ActivePane();
	FilePane *dst_pane = OppositePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("移動対象がありません")), to_wx(_T("移動")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	const UnicodeString dst_dir = dst_pane->GetPath();
	if (!ConfirmItems(this, _T("移動"), _T("移動"), names, dst_dir)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	const file_ops::FileOpResult result = file_ops::MoveItems(paths, dst_dir);

	pane->Reload();
	dst_pane->Reload();

	wxMessageBox(to_wx(file_ops::Summarize(result)), to_wx(_T("移動の結果")), wxOK | wxICON_INFORMATION, this);
}

//---------------------------------------------------------------------------
/**
 * @brief 削除 (D)
 * @details 完全削除ではなく、SHFileOperationW (FOF_ALLOWUNDO) でゴミ箱へ送る
 * だけ。delete_Dir/delete_Dirs (完全削除、しかも delete_Dirs はファイルを
 * 削除しない既知の不具合がある) は使わない
 */
void MainFrame::CmdDelete()
{
	FilePane *pane = ActivePane();

	const std::vector<UnicodeString> names = pane->GetSelectedNames();
	if (names.empty()) {
		wxMessageBox(to_wx(_T("削除対象がありません")), to_wx(_T("削除")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	if (!ConfirmItems(this, _T("削除"), _T("ゴミ箱へ移動"), names, EmptyStr)) return;

	std::vector<UnicodeString> paths;
	for (const UnicodeString &name : names) paths.push_back(pane->GetPath() + name);

	UnicodeString error;
	const bool ok = file_ops::SendToTrash(paths, error, static_cast<HWND>(GetHandle()));

	pane->Reload();

	if (ok) {
		UnicodeString msg;
		msg.sprintf(_T("%d 件をゴミ箱へ送りました"), static_cast<int>(paths.size()));
		wxMessageBox(to_wx(msg), to_wx(_T("削除の結果")), wxOK | wxICON_INFORMATION, this);
	}
	else {
		wxMessageBox(to_wx(error), to_wx(_T("削除に失敗しました")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/// ディレクトリの作成 (K)
void MainFrame::CmdCreateDir()
{
	FilePane *pane = ActivePane();

	wxTextEntryDialog dlg(this, to_wx(_T("作成するディレクトリ名")), to_wx(_T("ディレクトリの作成")));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString name = to_us(dlg.GetValue());
	UnicodeString error;
	if (!file_ops::MakeDirectory(pane->GetPath(), name, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("ディレクトリを作成できません")), wxOK | wxICON_ERROR, this);
		return;
	}

	pane->Reload();
}

//---------------------------------------------------------------------------
/// 名前等の変更 (R)。カーソル位置の1件のみ (複数選択には対応しない)
void MainFrame::CmdRenameDlg()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) {
		wxMessageBox(to_wx(_T("対象がありません")), to_wx(_T("名前の変更")), wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxTextEntryDialog dlg(this, to_wx(_T("新しい名前")), to_wx(_T("名前等の変更")), to_wx(itm->name));
	if (dlg.ShowModal() != wxID_OK) return;

	const UnicodeString new_name = to_us(dlg.GetValue());
	UnicodeString error;
	if (!file_ops::RenameItem(pane->GetPath(), itm->name, new_name, error)) {
		wxMessageBox(to_wx(error), to_wx(_T("名前を変更できません")), wxOK | wxICON_ERROR, this);
		return;
	}

	pane->Reload();
}

//---------------------------------------------------------------------------
/**
 * @brief 関連付けで開く (ENTER。カーソル位置がディレクトリでなかった場合のみ呼ばれる)
 * @details 実行可能ファイル (test_ExeExt、FEXT_EXECUTE) は誤って起動する事故を
 * 防ぐため必ず確認ダイアログを出す。関連付けが無い拡張子の場合は
 * ShellExecuteExW 自身が Windows 標準の「開く方法を選んでください」
 * ダイアログを出す (gui/file_open.h を参照)
 */
void MainFrame::CmdOpenStandard()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent || itm->is_dir) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;
	if (test_ExeExt(get_extension(full_path)) && !ConfirmExecute(this, full_path)) return;

	UnicodeString error;
	if (!file_open::OpenStandard(full_path, error, static_cast<HWND>(GetHandle())) && !error.IsEmpty()) {
		wxMessageBox(to_wx(error), to_wx(_T("開けませんでした")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/// アプリケーションから開く (Ctrl+Enter)。ディレクトリでも呼べる (関連付けダイアログ側の判断に任せる)
void MainFrame::CmdOpenByApp()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;

	UnicodeString error;
	if (!file_open::OpenWithDialog(full_path, error, static_cast<HWND>(GetHandle())) && !error.IsEmpty()) {
		wxMessageBox(to_wx(error), to_wx(_T("アプリケーションから開く")), wxOK | wxICON_ERROR, this);
	}
}

//---------------------------------------------------------------------------
/**
 * @brief ファイル情報ダイアログ (Alt+Enter、推測のキー。gui/key_map.cpp 参照)
 * @details ディレクトリでもファイルでも表示できる (usr_cmdlist.cpp の
 * "FVI:PropertyDlg" は種別を問わない)
 */
void MainFrame::CmdPropertyDlg()
{
	FilePane *pane = ActivePane();
	const FileItem *itm = pane->GetCurrentItem();
	if (itm == nullptr || itm->is_parent) return;

	const UnicodeString full_path = pane->GetPath() + itm->name;
	ShowFileInfoDialog(this, full_path, *itm);
}
