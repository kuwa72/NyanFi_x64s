//----------------------------------------------------------------------//
// メッセージボックス表示														//
//																		//
//----------------------------------------------------------------------//
// src/usr_msg.cpp から分離した (port/phase2)。
//
// 分離の理由: msgbox_ERR/msgbox_WARN/msgbox_OK/msgbox_Y_N/msgbox_Y_N_C/
// msgbox_Retry/msgbox_Sure/msgbox_SureAll は、いずれも VCL の
// CreateMessageDialog() / TForm::ShowModal() / TCheckBox の実インスタンス化
// に依存しており、これらはヘッドレスな compat シムでは「宣言のみ」
// (呼ばれたらリンクエラー) にしかできない (呼び出し側がユーザ入力を待つ
// 実際の動作を要求するため、gui_stubs.h の設計方針上 no-op 実装はできない)。
//
// 一方 LoadUsrMsg / UserAbort 等 (src/usr_msg.cpp に残した部分) は GUI に
// 依存しないため wx 版に移植済み。もしこのファイルの関数群を usr_msg.cpp と
// 同じ翻訳単位に置いたままにすると、GNU ld はオブジェクトファイル単位で
// static ライブラリから取り込むため、LoadUsrMsg 目的で usr_msg.cpp.obj が
// 取り込まれた瞬間にこのファイルの CreateMessageDialog 等の未解決参照も
// 一緒にリンクされ、実際には誰も呼んでいなくてもビルドが失敗する
// (CLAUDE.md 規約5 の `--gc-sections` の注意と同じ事情)。ファイルを分けて
// cmake/phase0_sources.cmake に載せないことで、この問題を避けている。
//
// 現状 (2026-08-21) src/ 内でこれらの関数を呼んでいる箇所は無い
// (Phase 3 で MainFrm.cpp 等を移植する際に必要になる)。呼び出しが必要に
// なった時点で、wx (wxMessageDialog 等) を使った実装を gui/ 側に置くか、
// このファイルごと移植するかを判断すること。
//
// src/NyanFi.cbproj には CppCompile として追加済み (C++Builder 側は
// 元の usr_msg.cpp と合わせて全関数がビルドされる。BCC64 未検証)。
//---------------------------------------------------------------------------
#include "usr_str.h"
#include "usr_msg.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
//エラーメッセージ
//---------------------------------------------------------------------------
void msgbox_ERR(UnicodeString msg)
{
	if (msg.IsEmpty()) return;

	Screen->MessageFont->Assign(Application->DefaultFont);
	TForm *MsgDlg = CreateMessageDialog(msg, mtError, TMsgDlgButtons() << mbOK, mbOK);
	MsgDlg->ShowModal();
	delete MsgDlg;
}
//---------------------------------------------------------------------------
void msgbox_ERR(unsigned id)
{
	msgbox_ERR(LoadUsrMsg(id));
}

//---------------------------------------------------------------------------
//警告メッセージ
//---------------------------------------------------------------------------
void msgbox_WARN(UnicodeString msg)
{
	if (msg.IsEmpty()) return;

	Screen->MessageFont->Assign(Application->DefaultFont);
	TForm *MsgDlg = CreateMessageDialog(msg, mtWarning, TMsgDlgButtons() << mbOK, mbOK);
	MsgDlg->ShowModal();
	delete MsgDlg;
}
//---------------------------------------------------------------------------
void msgbox_WARN(unsigned id)
{
	msgbox_WARN(LoadUsrMsg(id));
}

//---------------------------------------------------------------------------
//確認メッセージ
//---------------------------------------------------------------------------
void msgbox_OK(UnicodeString msg, UnicodeString tit)
{
	Screen->MessageFont->Assign(Application->DefaultFont);
	TForm *MsgDlg = CreateMessageDialog(msg, mtConfirmation, TMsgDlgButtons() << mbOK, mbOK);
	if (!tit.IsEmpty()) MsgDlg->Caption = tit;
	MsgDlg->ShowModal();
	delete MsgDlg;
}
//---------------------------------------------------------------------------
bool msgbox_Y_N(UnicodeString msg, UnicodeString tit)
{
	Screen->MessageFont->Assign(Application->DefaultFont);
	TMsgDlgButtons opt = TMsgDlgButtons() << mbYes << mbNo;
	TForm *MsgDlg = CreateMessageDialog(msg, mtConfirmation, opt, SureDefNo? mbNo : mbYes);
	if (!tit.IsEmpty()) MsgDlg->Caption = tit;
	int res = MsgDlg->ShowModal();
	delete MsgDlg;
	return (res==mrYes);
}
//---------------------------------------------------------------------------
TModalResult msgbox_Y_N_C(UnicodeString msg, UnicodeString tit)
{
	Screen->MessageFont->Assign(Application->DefaultFont);
	TMsgDlgButtons opt = TMsgDlgButtons() << mbYes << mbNo << mbCancel;
	TForm *MsgDlg = CreateMessageDialog(msg, mtConfirmation, opt, SureDefNo? mbNo : mbYes);
	if (!tit.IsEmpty()) MsgDlg->Caption = tit;
	TModalResult res = MsgDlg->ShowModal();
	delete MsgDlg;
	return res;
}
//---------------------------------------------------------------------------
TModalResult msgbox_Retry(UnicodeString msg, UnicodeString tit)
{
	Screen->MessageFont->Assign(Application->DefaultFont);
	TMsgDlgButtons opt = TMsgDlgButtons() << mbRetry  << mbCancel;
	TForm *MsgDlg = CreateMessageDialog(msg, mtError, opt, mbRetry);
	if (!tit.IsEmpty()) MsgDlg->Caption = tit;
	TModalResult res = MsgDlg->ShowModal();
	delete MsgDlg;
	return res;
}

//---------------------------------------------------------------------------
bool msgbox_Sure(UnicodeString msg, bool ask, bool center)
{
	if (!ask) return true;

	Screen->MessageFont->Assign(Application->DefaultFont);
	TMsgDlgButtons opt = TMsgDlgButtons() << mbYes << mbNo;
	if (SureCancel) opt << mbCancel;
	TForm *MsgDlg = CreateMessageDialog(msg, mtConfirmation, opt, SureDefNo? mbNo : mbYes);

	MsgPosCenter = center;
	int res = MsgDlg->ShowModal();
	MsgPosCenter = false;
	delete MsgDlg;

	return (res==mrYes);
}
//---------------------------------------------------------------------------
bool msgbox_Sure(const _TCHAR *msg, bool ask, bool center)
{
	return msgbox_Sure(UnicodeString(msg), ask, center);
}
//---------------------------------------------------------------------------
bool msgbox_Sure(unsigned id, bool ask, bool center)
{
	return msgbox_Sure(LoadUsrMsg(id), ask, center);
}

//---------------------------------------------------------------------------
//「すべてに適用」チェックボックス付き確認メッセージ
//---------------------------------------------------------------------------
int msgbox_SureAll(UnicodeString msg, bool &app_chk, bool center)
{
	Screen->MessageFont->Assign(Application->DefaultFont);
	TForm *MsgDlg = CreateMessageDialog(msg, mtConfirmation,
						TMsgDlgButtons() << mbYes << mbNo << mbCancel,
						SureDefNo? mbNo : mbYes);

	//「すべてに適用」チェックボックスを追加
	TCheckBox *cp = new TCheckBox(MsgDlg);
	MsgDlg->ClientHeight = MsgDlg->ClientHeight + cp->Height + 12;

	cp->Caption = "すべてに適用(&A)";
	cp->Parent	= MsgDlg;
	cp->Left	= 20;
	cp->Top		= MsgDlg->ClientHeight - cp->Height - 12;
	cp->Width	= MsgDlg->ClientWidth - cp->Left;

	MsgPosCenter = center;
	int res = MsgDlg->ShowModal();
	MsgPosCenter = false;

	app_chk = cp->Checked;
	delete MsgDlg;

	return res;
}
//---------------------------------------------------------------------------
