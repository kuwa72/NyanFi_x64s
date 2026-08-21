//----------------------------------------------------------------------//
// メッセージ															//
//																		//
//----------------------------------------------------------------------//
#include "usr_str.h"
#include "usr_msg.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
//メッセージ文字列の取得
//---------------------------------------------------------------------------
UnicodeString LoadUsrMsg(int id, UnicodeString s)
{
	std::unique_ptr<TStringList> msg_lst(new TStringList());
	msg_lst->Text =
		_T("1001=%sが見つかりません。\n")							//USTR_NotFound
		_T("1002=%sこの操作には対応していません。\n")				//USTR_OpeNotSuported
		_T("1003=この操作はできません。\n")							//USTR_CantOperate
		_T("1004=対応していない形式です。\n")						//USTR_FmtNotSuported
		_T("1005=一時解凍に失敗しました。\n")						//USTR_FaildTmpUnpack
		_T("1006=移動先が同じです。\n")								//USTR_SameMoveDest
		_T("1007=コピー先が同じです。\n")							//USTR_SameCopyDest
		_T("1008=ディレクトリが含まれています。\n")					//USTR_IncludeDir
		_T("1009=ディレクトリにアクセスできません。\n")				//USTR_CantAccessDir
		_T("1010=不正な日付または時刻です。\n")						//USTR_IllegalDate
		_T("1011=不正な指定です。\n")								//USTR_IllegalParam
		_T("1012=不正な形式です。\n")								//USTR_IllegalFormat
		_T("1013=不正なアドレスです\n")								//USTR_IllegalAddress
		_T("1014=起動に失敗しました。\n")							//USTR_FaildExec
		_T("1015=処理に失敗しました。\n")							//USTR_FaildProc
		_T("1016=読み込みに失敗しました。\n")						//USTR_FaildLoad
		_T("1017=%sの保存に失敗しました。\n")						//USTR_FaildSave
		_T("1018=削除に失敗しました。\n")							//USTR_FaildDelete
		_T("1019=コピーに失敗しました。\n")							//USTR_FaildCopy
		_T("1020=メニューが実行できません。\n")						//USTR_FaildMenu
		_T("1021=中断しました。\n")									//USTR_Canceled
		_T("1022=ワークリストを開けません。\n")						//USTR_WlistCantOpen
		_T("1023=処理中にこの操作はできません。\n")					//USTR_ProcBusy
		_T("1024=対象がありません。\n")								//USTR_NoObject
		_T("1025=正規表現に誤りがあります。\n")						//USTR_IllegalRegEx
		_T("1026=書式文字列の入力\n")								//USTR_InputFmtStr
		_T("1027=すでに登録されています。\n")						//USTR_Registered
		_T("1028=　検索中...／　ESCキーで中断します。\n")			//USTR_SearchingESC
		_T("1029=　計算中...／　ESCキーで中断します。\n")			//USTR_CalculatingESC
		_T("1030=　処理中...／　ESCキーで中断します。\n")			//USTR_ProcessingESC
		_T("1031=　処理中...／　しばらくお持ちください。\n")		//USTR_WaitForReady
		_T("1032=レスポンスファイルの作成に失敗しました。\n")		//USTR_FaildListFile
		_T("1033=一時ディレクトリが作成できません。\n")				//USTR_CantMakeTmpDir
		_T("1034=不正な構文です。\n")								//USTR_SyntaxError
		_T("1035=対応する制御文が見つかりません。\n")				//USTR_BadStatmet
		_T("1036=パラメータが指定されていません。\n")				//USTR_NoParameter
		_T("1037=アプリケーションが見つかりません。\n")				//USTR_AppNotFound
		_T("1038=画像の準備ができていません。\n")					//USTR_ImgNotReady
		_T("1039=ファイルを開けません。\n")							//USTR_FileNotOpen
		_T("1040=ディレクトリが見つかりません。\n")					//USTR_DirNotFound
		_T("1041=無効なコマンドです。\n")							//USTR_InvalidCmd
		_T("1042=. で区切って複数指定可能\n")						//USTR_HintMltFExt
		_T("1043=; で区切って複数指定可能\n")						//USTR_HintMltSepSC
		_T("1044=対応するエディタがありません\n")					//USTR_NoEditor
		_T("1045=再生できません。\n")								//USTR_CantPlay
		_T("1046=%sに名前を付けて保存\n")							//USTR_SaveAs
		_T("1047=ディレクトリ[%s]が作成できません。\n")				//USTR_CantCreDir
		_T("1048=%sをコピーしますか?\n")							//USTR_CopyQ
		_T("1049=%sを削除しますか?\n")								//USTR_DeleteQ
		_T("1050=%sを完全削除しますか?\n")							//USTR_CompDeleteQ
		_T("1051=%sの履歴をすべて削除しますか?\n")					//USTR_DelHistoryQ
		_T("1052=すべての場所のすべてのマークを解除しますか?\n")	//USTR_ClrAllMarkQ
		_T("1053=コマンドを中断しますか?\n")						//USTR_CancelCmdQ
		_T("1054=アーカイブを開けません。\n")						//USTR_ArcNotOpen
		_T("1055=テキストファイルではありません。\n")				//USTR_NotText
		_T("1056=%sを抽出しますか?\n")								//USTR_Extract
		_T("1057=%s情報が取得できません。\n")						//USTR_CantGetInfo
		_T("1058=名前が重複しています。\n")							//USTR_DuplicateName
		_T("1059=上書きしてもよいですか?\n")						//USTR_OverwriteQ
		_T("1060=%sをアップロードしますか?\n")						//USTR_UploadQ
		_T("1061=%sをダウンロードしますか?\n")						//USTR_DownloadQ
		_T("1062=削除開始  %s\n")									//USTR_BeginDelete
		_T("1063=同名ファイルの処理 <%s>\n")						//USTR_SameName
		_T("1064=名前の変更\n")										//USTR_Rename
		_T("1065=ディレクトリ属性の異なる同名対象が存在します。\n")	//USTR_NgSameName
		_T("1066=不正な日付条件です。\n")							//USTR_IllegalDtCond
		_T("1067=選択マスク中のワークリストは変更できません。\n")	//USTR_WorkFiltered
		_T("1068=左右が同一ディレクトリです。\n")					//USTR_SameDirLR
		_T("1069=Gitの作業ディレクトリではありません。\n")			//USTR_NotRepository
		_T("1070=設定パネルを隠す\n")								//USTR_HideOptPanel
		_T("1071=設定パネルを表示\n")								//USTR_ShowOptPanel
		_T("1072=%s コマンドで変更可能\n")							//USTR_HintOptCmd
		_T("1073=サウンド識別子でも可\n")							//USTR_HintSndID

		_T("1100=選択項目\n")										//USTR_SelectedItem
		_T("1101=一覧\n")											//USTR_List
		_T("1102=アップロード\n")									//USTR_Upload
		_T("1103=ダウンロード\n");									//USTR_Download

	UnicodeString ret_str = ReplaceStr(msg_lst->Values[IntToStr(id)], _T("／"), "\r\n");
	ret_str = s.IsEmpty()? TRegEx::Replace(ret_str, _T("%s[のをがに]?"), EmptyStr) : ReplaceStr(ret_str, "%s", s);
	return ret_str;
}
//---------------------------------------------------------------------------
UnicodeString LoadUsrMsg(int id, const _TCHAR *s)
{
	return LoadUsrMsg(id, UnicodeString(s));
}
//---------------------------------------------------------------------------
UnicodeString LoadUsrMsg(int id, int id_s)
{
	return LoadUsrMsg(id, LoadUsrMsg(id_s));
}

//---------------------------------------------------------------------------
//ユーザ定義メッセージの EAbort を送出
//---------------------------------------------------------------------------
void UserAbort(unsigned id)
{
	throw EAbort(LoadUsrMsg(id));
}

//---------------------------------------------------------------------------
void SysErrAbort(unsigned id)
{
	throw EAbort(SysErrorMessage(id));
}
//---------------------------------------------------------------------------
void LastErrAbort()
{
	throw EAbort(SysErrorMessage(GetLastError()));
}
//---------------------------------------------------------------------------
void TextAbort(const _TCHAR *msg)
{
	throw EAbort(UnicodeString(msg));
}
//---------------------------------------------------------------------------
void SkipAbort()
{
	throw EAbort("SKIP");
}
//---------------------------------------------------------------------------
void CancelAbort()
{
	throw EAbort("CANCELED");
}
//---------------------------------------------------------------------------
void EmptyAbort()
{
	throw EAbort(EmptyStr);
}

//---------------------------------------------------------------------------
//確認メッセージ
//---------------------------------------------------------------------------
bool SureCopy;
bool SureMove;
bool SureDelete;
bool SureUnPack;
bool SureExec;
bool SureOther;
bool SureExit;
bool SureCmpDel;
bool SureWorkList;

//---------------------------------------------------------------------------
bool SureCancel;
bool SureDefNo;
bool SureAdjPos;
bool MsgPosCenter = false;

// msgbox_ERR/msgbox_WARN/msgbox_OK/msgbox_Y_N/msgbox_Y_N_C/msgbox_Retry/
// msgbox_Sure/msgbox_SureAll (メッセージボックス表示そのもの) は
// src/usr_msg_dlg.cpp に分離した。理由は同ファイル冒頭のコメントを参照。
//---------------------------------------------------------------------------
