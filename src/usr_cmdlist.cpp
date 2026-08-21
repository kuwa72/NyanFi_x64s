//----------------------------------------------------------------------//
//コマンド一覧															//
//																		//
//----------------------------------------------------------------------//
#include "usr_file_ex.h"
#include "usr_tag.h"
#include "usr_cmdlist.h"

//---------------------------------------------------------------------------
UnicodeString ScrModeIdStr = "FSVIL";

//---------------------------------------------------------------------------
//コマンド一覧リストを設定
//---------------------------------------------------------------------------
void set_CmdList(
	TStringList *c_list,	//[o] コマンド名=説明
	TStringList *s_list)	//[o] 識別文字:コマンド名=説明
{
	c_list->Text =
		_T("F:AboutNyanFi=バージョン情報\n")
		_T("F:AddTab=タブを追加\n")
		_T("F:BackDirHist=ディレクトリ履歴を戻る\n")
		_T("F:Backup=反対パスにバックアップ\n")
		_T("F:BgImgMode=背景画像の表示モード設定\n")
		_T("F:BorderCenter=ファイルリストの境界を中央に\n")
		_T("F:BorderLeft=ファイルリストの境界を左に移動\n")
		_T("F:BorderRight=ファイルリストの境界を右に移動\n")
		_T("F:CalcDirSize=ディレクトリ容量を計算\n")
		_T("F:CalcDirSizeAll=全ディレクトリの容量を計算\n")
		_T("F:ChangeDir=ディレクトリを変更\n")
		_T("F:ChangeDrive=ドライブの変更\n")
		_T("F:ChangeOppDir=反対側のディレクトリを変更\n")
		_T("F:ChangeOppRegDir=反対側で登録ディレクトリを開く\n")
		_T("F:ChangeRegDir=登録ディレクトリを開く\n")
		_T("F:CheckUpdate=更新の確認\n")
		_T("F:ClearMask=選択マスク/パスマスクを解除\n")
		_T("F:Clone=反対パスにクローン作成\n")
		_T("F:CloneToCurr=カレントにクローン作成\n")
		_T("F:CommandPrompt=コマンドプロンプト\n")
		_T("F:CompareDlg=同名ファイルの比較ダイアログ\n")
		_T("F:CompareHash=ハッシュ値の比較\n")
		_T("F:CompleteDelete=完全削除\n")
		_T("F:CompressDir=ディレクトリのNTFS圧縮\n")
		_T("F:ContextMenu=コンテキストメニューを表示\n")
		_T("F:ConvertDoc2Txt=バイナリ文書→テキスト変換\n")
		_T("F:ConvertHtm2Txt=HTML→テキスト変換\n")
		_T("F:ConvertImage=画像ファイルの変換\n")
		_T("F:ConvertTextEnc=文字コードの変換\n")
		_T("F:Copy=コピー\n")
		_T("F:CopyCmdName=コマンド名をクリップボードにコピー\n")
		_T("F:CopyDir=ディレクトリ構造のコピー\n")
		_T("F:CopyFileName=ファイル名をクリップボードにコピー\n")
		_T("F:CopyTo=指定ディレクトリにコピー\n")
		_T("F:CopyToClip=クリップボードにコピー\n")
		_T("F:CountLines=テキストファイルの行数をカウント\n")
		_T("F:CreateDir=ディレクトリの作成\n")
		_T("F:CreateDirsDlg=ディレクトリ一括作成ダイアログ\n")
		_T("F:CreateHardLink=ハードリンクの作成\n")
		_T("F:CreateJunction=ジャンクションの作成\n")
		_T("F:CreateShortcut=ショートカットの作成\n")
		_T("F:CreateSymLink=シンボリックリンクの作成\n")
		_T("F:CreateTestFile=テストファイルの作成\n")
		_T("F:CsrDirToOpp=カーソル位置のディレクトリを反対側に開く\n")
		_T("F:CurrFromOpp=カレントパスを反対パスに\n")
		_T("F:CurrToOpp=反対パスをカレントパスに\n")
		_T("F:CursorEnd=カーソルを最下行に移動\n")
		_T("F:CursorEndSel=選択しながらカーソルを最下行に移動\n")
		_T("F:CursorTop=カーソルを最上行に移動\n")
		_T("F:CursorTopSel=選択しながらカーソルを最上行に移動\n")
		_T("F:CutToClip=クリップボードに切り取り\n")
		_T("F:DateSelect=指定した日付条件に合うファイルを選択\n")
		_T("F:DeleteADS=ファイルの代替データストリームを削除\n")
		_T("F:DelJpgExif=Exif情報の削除\n")
		_T("F:DelSelMask=選択マスクから項目を削除\n")
		_T("F:DelTab=タブを削除\n")
		_T("F:DiffDir=ディレクトリの比較\n")
		_T("F:DirHistory=ディレクトリ履歴\n")
		_T("F:DirStack=ディレクトリ・スタック\n")
		_T("F:DistributionDlg=振り分けダイアログ\n")
		_T("F:DotNyanDlg=.nyanfi ファイルの設定\n")
		_T("F:DriveList=ドライブ一覧\n")
		_T("F:DriveGraph=ドライブ使用率推移\n")
		_T("F:EditIniFile=INIファイルの編集\n")
		_T("F:Eject=CD/DVDドライブのトレイを開く\n")
		_T("F:EjectDrive=ドライブの取り外し\n")
		_T("F:EmptyTrash=ごみ箱を空にする\n")
		_T("F:EqualListWidth=左右のリスト幅を均等に\n")
		_T("F:ExeCommandLine=コマンドラインの実行\n")
		_T("F:ExeExtMenu=追加メニューの実行\n")
		_T("F:ExeExtTool=外部ツールの実行\n")
		_T("F:Exit=NyanFiの終了\n")
		_T("F:ExitDupl=二重起動されたNyanFiを終了\n")
		_T("F:ExPopupMenu=拡張ポップアップメニュー\n")
		_T("F:ExtractChmSrc=CHMからソースを抽出\n")
		_T("F:ExtractGifBmp=アニメGIFからビットマップを抽出\n")
		_T("F:ExtractIcon=アイコンを抽出\n")
		_T("F:ExtractImage=埋め込み画像を抽出\n")
		_T("F:ExtractMp3Img=MP3の埋め込み画像を抽出\n")
		_T("F:FileExtList=拡張子別一覧\n")
		_T("F:FileListOnly=ファイルリストのみを表示\n")
		_T("F:FileRun=ファイル名を指定して実行\n")
		_T("F:Filter=ファイルリストの絞り込み\n")
		_T("F:FindDuplDlg=重複ファイルの検索ダイアログ\n")
		_T("F:FindDirDlg=ディレクトリ名検索ダイアログ\n")
		_T("F:FindFileDirDlg=ファイル/ディレクトリ名検索ダイアログ\n")
		_T("F:FindFileDlg=ファイル名検索ダイアログ\n")
		_T("F:FindFolderIcon=フォルダアイコン検索\n")
		_T("F:FindHardLink=ハードリンクを列挙\n")
		_T("F:FindMark=栞マーク項目を検索\n")
		_T("F:FindTag=タグ検索\n")
		_T("F:FixTabPath=タブ変更に対してカレントパスを一時固定/解除\n")
		_T("F:FTPChmod=パーミッションの設定\n")
		_T("F:FTPConnect=FTPホストに接続\n")
		_T("F:FTPDisconnect=FTPホストから切断\n")
		_T("F:ForwardDirHist=ディレクトリ履歴を進む\n")
		_T("F:GetHash=ファイルのハッシュ値を取得\n")
		_T("F:GitDiff=カーソル位置ファイルの差分を表示\n")
		_T("F:GitGrep=Git GREP を開く\n")
		_T("F:GitViewer=Gitビューア\n")
		_T("F:HideSizeTime=サイズと更新日時を隠す\n")
		_T("F:ImageViewer=イメージビューアで開く\n")
		_T("F:InputDir=入力したディレクトリに変更\n")
		_T("F:InputPathMask=パスマスクを入力\n")
		_T("F:InsSeparator=ワークリストにセパレータを挿入\n")
		_T("F:ItemTmpDown=項目を一時的に1つ下に移動\n")
		_T("F:ItemTmpMove=選択項目を一時的にカーソル位置に移動\n")
		_T("F:ItemTmpUp=項目を一時的に1つ上に移動\n")
		_T("F:JoinText=テキストファイルの結合\n")
		_T("F:JsonViewer=JSONビューア\n")
		_T("F:JumpTo=指定したファイル位置へ\n")
		_T("F:Library=ライブラリへ\n")
		_T("F:LinkToOpp=リンク先を反対側に開く\n")
		_T("F:ListArchive=アーカイブの内容一覧\n")
		_T("F:ListClipboard=クリップボードを一覧で表示\n")
		_T("F:ListDuration=ファイル再生時間の一覧\n")
		_T("F:ListExpFunc=エクスポート関数一覧\n")
		_T("F:ListFileName=ファイル名を一覧で表示\n")
		_T("F:ListNyanFi=NyanFi についての情報をログに表示\n")
		_T("F:ListText=テキストファイルを一覧で表示\n")
		_T("F:ListTail=テキストファイルの末尾を一覧で表示\n")
		_T("F:ListTree=ディレクトリ構造のツリー表示\n")
		_T("F:LoadResultList=結果リストをファイルから読み込む\n")
		_T("F:LoadFindSet=検索設定をファイルから読み込む\n")
		_T("F:LoadTabGroup=タブグループをファイルから読み込む\n")
		_T("F:LockComputer=コンピュータのロック\n")
		_T("F:LockKeyMouse=キーボード/マウスのロック\n")
		_T("F:LockTextPreview=テキストプレビューのロック/解除\n")
		_T("F:LogFileInfo=ファイル情報をログに出力\n")
		_T("F:MarkMask=栞マーク項目だけを残して他を隠す\n")
		_T("F:MaskFind=指定マスクにマッチする項目を検索\n")
		_T("F:MatchSelect=指定文字列を含むファイルを選択\n")
		_T("F:MonitorOff=ディスプレイの電源を切る\n")
		_T("F:Move=移動\n")
		_T("F:MoveTab=タブの位置を移動\n")
		_T("F:MoveTo=指定ディレクトリへ移動\n")
		_T("F:MuteVolume=音量ミュート\n")
		_T("F:NameFromClip=ファイル名をクリップボードの内容に\n")
		_T("F:NameToLower=ファイル名の小文字化\n")
		_T("F:NameToUpper=ファイル名の大文字化\n")
		_T("F:NetConnect=ネットワークドライブの割り当て\n")
		_T("F:NetDisconnect=ネットワークドライブの切断\n")
		_T("F:NewFile=新規ファイルの作成\n")
		_T("F:NewTextFile=新規テキストファイルの作成\n")
		_T("F:NextDrive=次のドライブへ\n")
		_T("F:NextSelItem=次の選択項目にカーソル移動\n")
		_T("F:NextTab=次のタブへ\n")
		_T("F:OpenADS=代替データストリームを仮想リストとして開く\n")
		_T("F:OpenByExp=エクスプローラで開く\n")
		_T("F:OpenCtrlPanel=コントロールパネルを開く\n")
		_T("F:OpenGitURL=リモートリポジトリURLを開く\n")
		_T("F:OpenStandard=標準の Enter キー動作\n")
		_T("F:OpenTrash=ごみ箱を開く\n")
		_T("F:Pack=反対パスにアーカイブ作成\n")
		_T("F:PackToCurr=カレントパスにアーカイブ作成\n")
		_T("F:Paste=貼り付け\n")
		_T("F:PathMaskDlg=パスマスクダイアログ\n")
		_T("F:PlayList=プレイリストを作って再生\n")
		_T("F:PopDir=ディレクトリをポップ\n")
		_T("F:PopupTab=タブ選択メニューを表示\n")
		_T("F:PowerOff=Windowsを終了\n")
		_T("F:PowerShell=PowerShell を起動\n")
		_T("F:PrevDrive=前のドライブへ\n")
		_T("F:PrevSelItem=前の選択項目にカーソル移動\n")
		_T("F:PrevTab=前のタブへ\n")
		_T("F:PushDir=ディレクトリをプッシュ\n")
		_T("F:Reboot=Windowsを再起動\n")
		_T("F:RecentList=最近使ったファイル一覧\n")
		_T("F:RegDirDlg=登録ディレクトリダイアログ\n")
		_T("F:RegDirPopup=登録ディレクトリ・ポップアップメニュー\n")
		_T("F:RegSyncDlg=同期コピーの設定\n")
		_T("F:ReloadList=ファイルリストを最新の情報に更新\n")
		_T("F:RenameDlg=名前等の変更\n")
		_T("F:RepositoryList=リポジトリ一覧\n")
		_T("F:Restart=NyanFiの再起動\n")
		_T("F:ReturnList=ファイルリスト表示に戻る\n")
		_T("F:SaveAsResultList=結果リストに名前を付けて保存\n")
		_T("F:SaveAsFindSet=検索設定に名前を付けて保存\n")
		_T("F:SaveAsTabGroup=タブグループに名前を付けて保存\n")
		_T("F:SaveTabGroup=タブグループを上書き保存\n")
		_T("F:ScrollDownLog=ログを下にスクロール\n")
		_T("F:ScrollDownText=テキストプレビューを下にスクロール\n")
		_T("F:ScrollUpLog=ログを上にスクロール\n")
		_T("F:ScrollUpText=テキストプレビューを上にスクロール\n")
		_T("F:SelAllItem=すべての項目を選択/解除\n")
		_T("F:SelByList=リストによって項目を選択\n")
		_T("F:SelectUp=選択/解除後、カーソルを上に移動\n")
		_T("F:SelEmptyDir=空のディレクトリだけを選択\n")
		_T("F:SelGitChanged=Git作業ディレクトリで変更ファイルを選択\n")
		_T("F:SelMask=選択中の項目だけを残して他を隠す\n")
		_T("F:SelOnlyCur=カレント側だけにあるファイルを選択\n")
		_T("F:SelReverseAll=すべての項目の選択状態を反転\n")
		_T("F:SelSameDir=結果リストで同じディレクトリの項目を選択\n")
		_T("F:SetAlias=ワークリストの項目にエイリアスを設定\n")
		_T("F:SetArcTime=アーカイブのタイムスタンプを最新に合わせる\n")
		_T("F:SetDirTime=ディレクトリのタイムスタンプを最新に合わせる\n")
		_T("F:SetExifTime=タイムスタンプをExif撮影日時に設定\n")
		_T("F:SetFolderIcon=フォルダアイコンの設定\n")
		_T("F:SetPathMask=パスマスクを設定\n")
		_T("F:SetSttBarFmt=ステータスバー書式を設定\n")
		_T("F:SetSubSize=サブウィンドウのサイズを設定\n")
		_T("F:ShareList=共有フォルダ一覧\n")
		_T("F:ShowByteSize=ファイルサイズをバイト単位で表示\n")
		_T("F:ShowHideAtr=隠しファイルを表示\n")
		_T("F:ShowIcon=アイコンの表示\n")
		_T("F:ShowLogWin=ログウィンドウーの表示\n")
		_T("F:ShowPreview=イメージプレビューの表示\n")
		_T("F:ShowProperty=ファイル情報の表示\n")
		_T("F:ShowSystemAtr=システムファイルを表示\n")
		_T("F:ShowTabBar=タブバーの表示\n")
		_T("F:SimilarSort=名前の類似性によるソート\n")
		_T("F:SoloTab=他のタブをすべて削除\n")
		_T("F:SpecialDirList=特殊フォルダ一覧\n")
		_T("F:SubDirList=サブディレクトリ一覧\n")
		_T("F:SwapLR=左右のファイルリストを入れ替える\n")
		_T("F:SwapName=名前を入れ替える\n")
		_T("F:SyncLR=左右ディレクトリの同期変更を有効/解除\n")
		_T("F:TabDlg=タブの設定ダイアログ\n")
		_T("F:TabHome=タブのホームへ\n")
		_T("F:TestArchive=アーカイブの正当性を検査\n")
		_T("F:TextViewer=テキストビューアで開く\n")
		_T("F:ToExViewer=別ウィンドウのテキストビューアへ\n")
		_T("F:ToLog=ログウィンドウへ\n")
		_T("F:ToNextOnRight=右ファイルリストで次のNyanFiへ\n")
		_T("F:ToOpposite=反対側のファイルリストへ\n")
		_T("F:ToOppSameItem=カーソル位置と同名の反対側項目へ\n")
		_T("F:ToOppSameHash=カーソル位置と同ハッシュ値の反対側項目へ\n")
		_T("F:ToParent=親ディレクトリへ\n")
		_T("F:ToParentOnLeft=左ファイルリストで親ディレクトリへ\n")
		_T("F:ToParentOnRight=右ファイルリストで親ディレクトリへ\n")
		_T("F:ToPrevOnLeft=左ファイルリストで前のNyanFiへ\n")
		_T("F:ToRoot=ルートディレクトリへ\n")
		_T("F:ToTab=指定番号/キャプションのタブへ\n")
		_T("F:ToText=テキストプレビューへ\n")
		_T("F:TrimTagData=タグデータの整理\n")
		_T("F:UndoRename=直前の改名を元に戻す\n")
		_T("F:UnPack=反対パスに解凍\n")
		_T("F:UnPackToCurr=カレントパスに解凍\n")
		_T("F:UpdateFromArc=アーカイブから更新\n")
		_T("F:ViewIniFile=INIファイルの閲覧\n")
		_T("F:ViewTail=テキストファイルの末尾を閲覧\n")
		_T("F:WatchTail=テキストファイルの追加更新を監視\n")
		_T("F:WidenCurList=カレント側リストの幅を広げる\n")
		_T("F:WinMaximize=ウィンドウの最大化\n")
		_T("F:WinMinimize=ウィンドウの最小化\n")
		_T("F:WinNormal=ウィンドウを元のサイズに戻す\n")
		_T("F:WinTerminal=Windows Terminal を起動\n")
		_T("F:WorkItemDown=ワークリストの項目を1つ下に移動\n")
		_T("F:WorkItemMove=選択ワークリスト項目をカーソル位置に移動\n")
		_T("F:WorkItemUP=ワークリストの項目を1つ上に移動\n")
		_T("F:XmlViewer=XMLビューア\n")
		_T("FI:AddTag=項目にタグを追加\n")
		_T("FI:DelTag=項目のタグを削除\n")
		_T("FI:HomeWorkList=ホームワークリストを開く\n")
		_T("FI:LoadBgImage=背景画像を読み込む\n")
		_T("FI:LoadWorkList=ワークリストをファイルから読み込む\n")
		_T("FI:MaskSelect=指定マスクにマッチするファイルを選択\n")
		_T("FI:NewWorkList=新規ワークリストの作成\n")
		_T("FI:NextMark=次の栞マーク項目へ\n")
		_T("FI:PrevMark=前の栞マーク項目へ\n")
		_T("FI:NextSameName=ファイル名主部が同じ次ファイルへ\n")
		_T("FI:OpenByApp=独自の関連付けで開く\n")
		_T("FI:OpenByWin=Windowsの関連付けで開く\n")
		_T("FI:SaveAsWorkList=ワークリストに名前を付けて保存\n")
		_T("FI:SaveWorkList=ワークリストを上書き保存\n")
		_T("FI:SelAllFile=すべてのファイルを選択/解除\n")
		_T("FI:SelMark=栞マーク項目を選択\n")
		_T("FI:SelReverse=ファイルの選択状態を反転\n")
		_T("FI:SelSameExt=拡張子が同じファイルを選択\n")
		_T("FI:SelSameName=ファイル名主部が同じファイルを選択\n")
		_T("FI:SelWorkItem=ワークリストの登録項目を選択\n")
		_T("FI:SetInterpolation=縮小・拡大アルゴリズムを設定\n")
		_T("FI:SetTag=項目にタグを設定\n")
		_T("FI:SortDlg=ソートダイアログ\n")
		_T("FI:SubViewer=サブビューアの表示\n")
		_T("FI:TagSelect=指定タグを含む項目を選択\n")
		_T("FI:UseTrash=削除にごみ箱を使う/使わない\n")
		_T("FI:WorkList=ワークリスト\n")
		_T("FL:CancelAllTask=すべてのタスクを中断\n")
		_T("FL:ClearLog=ログをクリア\n")
		_T("FL:PauseAllTask=すべてのタスクを一旦停止/再開\n")
		_T("FL:Suspend=タスク予約項目の実行を保留/解除\n")
		_T("FL:TaskMan=タスクマネージャ\n")
		_T("FL:ToLeft=左ファイルリストへ\n")
		_T("FL:ToRight=右ファイルリストへ\n")
		_T("FL:ViewLog=ログをテキストビューアで表示\n")
		_T("FS:ClearAll=すべての選択状態を解除\n")
		_T("FSI:Select=選択/解除\n")
		_T("FSVIL:HelpContents=ヘルプの目次/索引を表示\n")
		_T("FSVIL:KeyList=キー割り当て一覧\n")
		_T("FSVIL:OptionDlg=オプション設定\n")
		_T("FV:BinaryEdit=ファイルのバイナリ編集\n")
		_T("FV:CursorDownSel=選択しながらカーソルを下に移動\n")
		_T("FV:CursorUpSel=選択しながらカーソルを上に移動\n")
		_T("FV:EditHighlight=構文強調表示定義ファイルの編集\n")
		_T("FV:EditHistory=最近編集したファイル一覧\n")
		_T("FV:Grep=文字列検索(GREP)\n")
		_T("FV:Grep2=文字列検索(grep フロントエンド)\n")
		_T("FV:HtmlToText=テキストビューアでHTML→テキスト変換表示\n")
		_T("FV:IncSearch=インクリメンタルサーチ\n")
		_T("FV:FindTagName=tags からタグ名検索\n")
		_T("FV:FixedLen=テキストビューアでCSV/TSVを固定長表示\n")
		_T("FV:PageDownSel=選択しながら1ページ下に移動\n")
		_T("FV:PageUpSel=選択しながら1ページ上に移動\n")
		_T("FV:RegExChecker=正規表現チェッカー\n")
		_T("FV:SetColor=テキストビューアの配色\n")
		_T("FV:SetFontSize=フォントサイズを変更\n")
		_T("FV:SetMargin=テキストビューアの左側余白を設定\n")
		_T("FV:SetTab=テキストビューアのタブストップ幅を設定\n")
		_T("FV:SetWidth=テキストビューアの折り返し幅を設定\n")
		_T("FV:ShowCR=テキストビューアで改行を表示\n")
		_T("FV:ShowIndent=テキストビューアのインデントガイドを表示\n")
		_T("FV:ShowLineNo=テキストビューアの行番号を表示\n")
		_T("FV:ShowRuby=テキストビューアでルビを表示\n")
		_T("FV:ShowRuler=テキストビューアのルーラを表示\n")
		_T("FV:ShowTAB=テキストビューアでタブを表示\n")
		_T("FV:ViewHistory=最近閲覧したファイル一覧\n")
		_T("FV:WebSearch=Webで検索\n")
		_T("FV:ZoomReset=ズームを解除\n")
		_T("FVL:CursorDown=カーソルを下に移動\n")
		_T("FVL:CursorUp=カーソルを上に移動\n")
		_T("FVI:AlphaBlend=メインウィンドウを透過表示\n")
		_T("FVI:AppList=アプリケーション一覧\n")
		_T("FVI:Calculator=電卓\n")
		_T("FVI:ClearMark=すべての栞マークを解除\n")
		_T("FVI:Close=閉じる\n")
		_T("FVI:CopyFileInfo=ファイル情報をクリップボードにコピー\n")
		_T("FVI:CmdFileList=コマンドファイル一覧\n")
		_T("FVI:CmdHistory=コマンド履歴\n")
		_T("FVI:DebugCmdFile=コマンドファイルのデバッグ実行\n")
		_T("FVI:Delete=削除\n")
		_T("FVI:Duplicate=NyanFiの二重起動\n")
		_T("FVI:ExeCommands=指定したコマンドを実行\n")
		_T("FVI:ExeMenuFile=メニューファイルの実行\n")
		_T("FVI:ExeToolBtn=ツールボタンの実行\n")
		_T("FVI:FileEdit=ファイルの編集\n")
		_T("FVI:InputCommands=入力したコマンドを実行\n")
		_T("FVI:ListFileInfo=ファイル情報をダイアログ表示\n")
		_T("FVI:ListLog=ログを一覧で表示\n")
		_T("FVI:Mark=栞マーク/解除\n")
		_T("FVI:MarkList=栞マーク一覧\n")
		_T("FVI:MenuBar=メニューバーの表示\n")
		_T("FVI:NextNyanFi=次のNyanFiをアクティブに\n")
		_T("FVI:PopupMainMenu=メインメニューをポップアップ表示\n")
		_T("FVI:PrevNyanFi=前のNyanFiをアクティブに\n")
		_T("FVI:PropertyDlg=プロパティダイアログを表示\n")
		_T("FVI:ScrollDown=下にスクロール\n")
		_T("FVI:ScrollUp=上にスクロール\n")
		_T("FVI:ShowFileInfo=ファイル情報を強制的に表示\n")
		_T("FVI:ShowFKeyBar=ファンクションキーバーの表示\n")
		_T("FVI:ShowStatusBar=ステータスバーの表示\n")
		_T("FVI:ShowToolBar=ツールバーを表示\n")
		_T("FVI:ToolBarDlg=ツールバーの設定\n")
		_T("FVI:WebMap=画像のGPS情報や指定地点の地図を開く\n")
		_T("FVI:WinPos=ウィンドウの四辺を設定\n")
		_T("FVIL:PageDown=1ページ下に移動\n")
		_T("FVIL:PageUp=1ページ上に移動\n")
		_T("FVI:ZoomIn=ズームイン\n")
		_T("FVI:ZoomOut=ズームアウト\n")
		_T("I:ColorPicker=カラーピッカー\n")
		_T("I:DoublePage=見開き表示\n")
		_T("I:EndFile=最後のファイルに移動\n")
		_T("I:EqualSize=等倍表示\n")
		_T("I:FittedSize=画面フィット表示\n")
		_T("I:FlipHorz=左右反転\n")
		_T("I:FlipVert=上下反転\n")
		_T("I:FullScreen=全画面表示\n")
		_T("I:GrayScale=グレースケール表示\n")
		_T("I:Histogram=ヒストグラムの表示\n")
		_T("I:JumpIndex=指定したインデックスに移動\n")
		_T("I:Loupe=ルーペの表示\n")
		_T("I:NextPage=サムネイルの次ページに移動\n")
		_T("I:PageBind=見開き表示の綴じ方向を設定\n")
		_T("I:PrevPage=サムネイルの前ページに移動\n")
		_T("I:Print=画像の印刷\n")
		_T("I:RotateLeft=左に90度回転\n")
		_T("I:RotateRight=右に90度回転\n")
		_T("I:ScrollLeft=左にスクロール\n")
		_T("I:ScrollRight=右にスクロール\n")
		_T("I:SendToWorkList=ワークリストに送る\n")
		_T("I:ShowGrid=画像分割グリッドの表示\n")
		_T("I:ShowSeekBar=シークバーの表示\n")
		_T("I:Sidebar=サイドバーの表示\n")
		_T("I:SimilarImage=画像の類似性によるソート\n")
		_T("I:Thumbnail=サムネイルの表示\n")
		_T("I:ThumbnailEx=サムネイルの全面表示/通常表示\n")
		_T("I:TopFile=先頭ファイルに移動\n")
		_T("I:WarnHighlight=白飛び警告\n")
		_T("S:ClearIncKeyword=キーワードをクリア\n")
		_T("S:IncMatchSelect=マッチする項目をすべて選択\n")
		_T("S:IncSearchDown=マッチする項目を下方向検索\n")
		_T("S:IncSearchExit=インクリメンタルサーチから抜ける\n")
		_T("S:IncSearchTop=マッチする項目を先頭から再検索\n")
		_T("S:IncSearchUp=マッチする項目を上方向検索\n")
		_T("S:KeywordHistory=キーワード履歴を参照\n")
		_T("S:MigemoMode=Migemoモードの切り換え\n")
		_T("S:NormalMode=通常のサーチモードに戻る\n")
		_T("S:SelectDown=選択/解除後、下方向に検索\n")
		_T("V:BackViewHist=テキストビューアの履歴を戻る\n")
		_T("V:BitmapView=ビットマップビューの表示\n")
		_T("V:BoxSelMode=箱形選択モードの開始/解除\n")
		_T("V:ChangeCodePage=文字コード変更\n")
		_T("V:ChangeViewMode=テキスト/バイナリ表示の切り換え\n")
		_T("V:CharInfo=文字情報の表示\n")
		_T("V:CsvCalc=CSV/TSV項目の集計\n")
		_T("V:CsvGraph=CSV/TSV項目のグラフ\n")
		_T("V:CsvRecord=CSV/TSVレコードの表示\n")
		_T("V:CursorLeft=カーソルを左に移動\n")
		_T("V:CursorLeftSel=選択しながらカーソルを左に移動\n")
		_T("V:CursorRight=カーソルを右に移動\n")
		_T("V:CursorRightSel=選択しながらカーソルを右に移動\n")
		_T("V:ExportCsv=CSV/TSVエクスポート\n")
		_T("V:FindDown=下方向に再検索\n")
		_T("V:FindLinkDown=リンク先を下方向に検索\n")
		_T("V:FindLinkUp=リンク先を上方向に検索\n")
		_T("V:FindMarkDown=マーク行を下方向に検索\n")
		_T("V:FindMarkUp=マーク行を上方向に検索\n")
		_T("V:FindSelDown=選択文字列を下方向に検索\n")
		_T("V:FindSelUp=選択文字列を上方向に検索\n")
		_T("V:FindText=文字列検索\n")
		_T("V:FindUp=上方向に再検索\n")
		_T("V:FunctionList=関数一覧\n")
		_T("V:HelpCurWord=カーソル位置の単語を指定ヘルプで検索\n")
		_T("V:Highlight=マッチ語を強調表示\n")
		_T("V:Inspector=インスペクタの表示\n")
		_T("V:ImgPreview=イメージプレビュー\n")
		_T("V:JumpLine=指定行番号に移動\n")
		_T("V:LineEnd=行末に移動\n")
		_T("V:LineEndSel=選択しながら行末に移動\n")
		_T("V:LineTop=行頭に移動\n")
		_T("V:LineTopSel=選択しながら行頭に移動\n")
		_T("V:OpenURL=URLを開く\n")
		_T("V:ReloadFile=ファイルの再読み込み\n")
		_T("V:SaveDump=ダンプリストをファイルに保存\n")
		_T("V:ScrollCursorDown=スクロールしながら下に移動\n")
		_T("V:ScrollCursorUp=スクロールしながら上に移動\n")
		_T("V:SearchPair=対応する括弧を検索\n")
		_T("V:SelCurWord=カーソル位置の単語を選択\n")
		_T("V:SelLine=カーソル行を選択\n")
		_T("V:SelLineCR=カーソル行全体を改行単位で選択\n")
		_T("V:SelectAll=すべて選択\n")
		_T("V:SelectFile=表示中ファイルを選択/解除\n")
		_T("V:SelectMode=選択モードの開始/解除\n")
		_T("V:SetTopAddr=先頭アドレスを設定\n")
		_T("V:SetUserDefStr=ユーザ定義文字列を設定\n")
		_T("V:Sort=テキスト全体を改行単位でソート\n")
		_T("V:SwitchSameName=ファイル名主部が同じ次のファイルに切り換え\n")
		_T("V:SwitchSrcHdr=ヘッダ/ソースファイルの切り換え\n")
		_T("V:TagJump=エディタでタグジャンプ\n")
		_T("V:TagJumpDirect=エディタでダイレクトタグジャンプ\n")
		_T("V:TagView=ビューアでタグジャンプ\n")
		_T("V:TagViewDirect=ビューアでダイレクトタグジャンプ\n")
		_T("V:TextEnd=最後尾に移動\n")
		_T("V:TextEndSel=選択しながら最後尾に移動\n")
		_T("V:TextTop=先頭に移動\n")
		_T("V:TextTopSel=選択しながら先頭に移動\n")
		_T("V:UserDefList=ユーザ定義文字列一覧\n")
		_T("V:WordLeft=前の単語に移動\n")
		_T("V:WordRight=次の単語に移動\n")
		_T("VIL:ClipCopy=クリップボードにコピー\n")
		_T("VI:NextFile=次のファイルを表示\n")
		_T("VI:PrevFile=前のファイルを表示\n")
		_T("VL:NextErr=次のエラー位置へ\n")
		_T("VL:PrevErr=前のエラー位置へ\n")
		_T("InitialSearch=頭文字サーチ\n");

	s_list->Clear();
	UnicodeString cmd, tmp;
	for (int i=0; i<c_list->Count; i++) {
		UnicodeString lbuf = c_list->Strings[i];
		if (!ContainsStr(lbuf, ":")) continue;
		UnicodeString ct = split_tkn(lbuf, ':');
		c_list->Strings[i] = lbuf;
		cmd = get_tkn(lbuf, '=');
		for (int j=1; j<=ct.Length(); j++) {
			s_list->Add(tmp.sprintf(_T("%s:%s"), ct.SubString(j, 1).c_str(), cmd.c_str()));
		}
	}
	s_list->Sort();
}

//---------------------------------------------------------------------------
//コマンドのパラメータ一覧を取得
//---------------------------------------------------------------------------
void get_PrmList(
	UnicodeString cmd,		//コマンド
	int id_idx,				//"FSVIL" モードのインデックス(0ベース)
	TStringList *p_list,	//[o] パラメータ一覧
	TComboBox *cp)			//コンボボックス	(default = NULL)
{
	if (cp) {
		cp->Clear();
		cp->Style	= csDropDownList;
		cp->Text	= EmptyStr;
		cp->Enabled = false;

		if (cp && contained_wd_i(
			_T("AddTag|AppList|CalcDirSize|CalcDirSizeAll|Calculator|ChangeDir|ChangeOppDir|Clone|CloneToCurr|")
			_T("ContextMenu|ConvertHtm2Txt|Copy|CopyTo|CountLines|CreateDir|CursorDown|CursorTop|CursorUp|DateSelect|")
			_T("DebugCmdFile|Delete|DistributionDlg|ExeCommands|ExeMenuFile|FileEdit|FileRun|Filter|FindDown|")
			_T("FindFileDirDlg|FindFileDlg|FindTag|FindUp|FTPChmod|FunctionList|GitDiff|GitViewer|Grep|Grep2|HelpCurWord|")
			_T("HtmlToText|IncSearch|InputCommands|JumpIndex|JumpLine|JumpTo|ListArchive|ListDuration|ListExpFunc|")
			_T("ListNyanFi|ListTail|ListText|ListTree|LoadBgImage|LoadResultList|LoadFindSet|LoadTabGroup|LoadWorkList|")
			_T("LockKeyMouse|Mark|MaskFind|MaskSelect|MatchSelect|MonitorOff|Move|MoveTo|NameFromClip|NewTextFile|")
			_T("OpenByApp|OpenByExp|OpenByWin|OpenStandard|OpenURL|Pack|PackToCurr|PlayList|PropertyDlg|RegExChecker|")
			_T("Restart|SaveAsTabGroup|ScrollCursorDown|ScrollCursorUp|ScrollDown|ScrollDownLog|ScrollDownText|")
			_T("ScrollUp|ScrollUpLog|ScrollUpText|SelByList|SetColor|SetDirTime|SetFontSize|SetInterpolation|")
			_T("SetMargin|SetPathMask|SetSttBarFmt|SetSubSize|SetTab|SetTag|SetUserDefStr|SetWidth|ShareList|")
			_T("SimilarImage|SimilarSort|SortDlg|SubDirList|TagJumpDirect|TagSelect|TagViewDirect|TextViewer|ToLeft|")
			_T("ToOppSameHash|ToRight|ToTab|UnPack|UnPackToCurr|ViewTail|WatchTail|WidenCurList|WinPos"),
			cmd))
		{
			cp->Style	= csDropDown;
			cp->Enabled = true;
		}
	}

	UnicodeString params;

	if (SameText(cmd, "AddTab")) {
		params = _T("\nNX : 現タブの次に挿入\nPR : 現タブの前に挿入");
	}
	else if (SameText(cmd, "AlphaBlend")) {
		if (cp) cp->Style = csDropDown;
		params = _T("\r\nIN : 透明度を入力\r\n");
	}
	else if (SameText(cmd, "AppList")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("FA : 一覧側にフォーカス\n")
			_T("FL : ランチャー側にフォーカス\n")
			_T("FI : ランチャー側にフォーカス(INC.サーチ)\n")
			_T("AO : 一覧のみ表示\n")
			_T("LO : ランチャーのみ表示\n")
			_T("LI : ランチャーのみ表示(INC.サーチ)\n")
			_T("FZ : あいまい検索(INC.サーチ)\n")
			_T("AS : スタートメニュー項目を追加(INC.サーチ)\n"));
	}
	else if (SameText(cmd, "BgImgMode")) {
		params.sprintf(_T("%s"),
			_T("OFF : 非表示\n")
			_T("1 : 2画面にわたって表示\n")
			_T("2 : それぞれに表示\n")
			_T("3 : デスクトップ背景を表示\n")
			_T("^1 : 2画面にわたって表示/非表示\n")
			_T("^2 : それぞれに表示/非表示\n")
			_T("^3 : デスクトップ背景を表示/非表示\n"));
	}
	else if (contained_wd_i(_T("CursorUp|CursorDown|ScrollCursorDown|ScrollCursorUp|ScrollDown|ScrollUp"), cmd)) {
		if (id_idx!=4) {
			params = _T("\nHP : 半ページ分\nFP : 1ページ分\n");
			if (id_idx==0 && contained_wd_i(_T("CursorUp|CursorDown"), cmd)) {
				params += _T("SL : 選択項目へ\n");
			}
		}
	}
	else if (SameText(cmd, "CursorTop")) {
		params = _T("\r\nTF : 最初のファイルに移動\r\n");
	}
	else if (contained_wd_i(_T("CalcDirSize|CalcDirSizeAll"), cmd)) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("FC : ファイル数、サブディレクトリ数を表示\n")
			_T("LO : 結果をログに出力\n")
			_T("CC : 結果をクリップボードにコピー\n")
			_T("LS : 結果を一覧表示\n")
			_T("SA : 結果をサイズの小さい順にソート\n")
			_T("SD : 結果をサイズの大きい順にソート\n"));
		if (SameText(cmd, "CalcDirSizeAll")) {
			params += _T("SG : グラフ表示(対カレント)\nDG : グラフ表示(対ドライブ)\n");
		}
	}
	else if (SameText(cmd, "Calculator")) {
		params = _T("\nCB : クリップボードを介して計算\n");
	}
	else if (SameText(cmd, "ChangeCodePage")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("932 :   Shift_JIS\n")
			_T("50220 : JIS(ISO-2022-JP)\n")
			_T("20932 : EUC-JP\n")
			_T("1252 :  Latin-1\n")
			_T("65001 : UTF-8\n")
			_T("1200 :  UTF-16\n"));
	}
	else if (contained_wd_i(_T("ChangeDrive|DriveGraph|EjectDrive"), cmd)) {
		for (int i=0; i<26; i++) params.cat_sprintf(_T("%c\n"), 'A'+i);	//英字
		if (SameText(cmd, "EjectDrive")) params += _T(". : カレントドライブ\n");
		if (SameText(cmd, "DriveGraph")) params = "\n" + params;
	}
	else if (SameText(cmd, "CheckUpdate")) {
		params = (_T("\nNC : 保存場所の選択、確認なし\n"));
	}
	else if (SameText(cmd, "ClearAll")) {
		params = _T("\nAL : 左右すべての選択状態を解除\n");
	}
	else if (SameText(cmd, "ClearMark")) {
		params = _T("\nAC : すべての場所のすべてのマークを解除\n");
	}
	else if (SameText(cmd, "ClipCopy")) {
		if (id_idx==2) {
			params = _T("\nAD : 現在の内容に追加\n");
		}
		else if (id_idx==3) {
			params = _T("\nVI : 表示されている状態でコピー\n");
		}
	}
	else if (SameText(cmd, "Close") && id_idx==2) {
		params = _T("\nAL : すべての別ウィンドウを閉じる\n");
	}
	else if (contained_wd_i(_T("CommandPrompt|PowerShell"), cmd)) {
		params = _T("\nRA : 管理者として実行(デフォルト)\nRC : 管理者として実行(カレント)\n");
	}
	else if (SameText(cmd,"WinTerminal")) {
		params = _T("\nRA : 管理者として実行\n");
	}
	else if (SameText(cmd, "CompareDlg")) {
		params = _T("\nCS : 大文字・小文字を区別\nNC : ダイアログを出さず、名前のみ比較\n");
	}
	else if (contained_wd_i(_T("CompareHash|GetHash|ToOppSameHash"), cmd)) {
		params.sprintf(_T("\n%s"), HASH_ALG_LIST);
		if (SameText(cmd, "GetHash"))		params += _T("IN : 入力文字列のハッシュ値を取得\n");
		if (SameText(cmd, "ToOppSameHash")) params += _T("NO : 反対側へ移動しない\n");
		if (SameText(cmd, "CompareHash"))   params += _T("OS : 不一致ファイルを反対側で選択\n");
	}
	else if (SameText(cmd, "CompressDir")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("UN : 圧縮属性を解除\n")
			_T("AL : すべての種類のファイルを圧縮\n"));
	}
	else if (SameText(cmd, "ConvertDoc2Txt")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("SJ : Shift_JIS で出力(デフォルト)\n")
			_T("IJ : ISO-2022-JP で出力\n")
			_T("EJ : EUC-JP で出力\n")
			_T("U8 : UTF-8 で出力\n")
			_T("UL : UTF-16 で出力\n")
			_T("UB : UTF-16BE で出力\n"));
	}
	else if (SameText(cmd, "ConvertHtm2Txt")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("HD : ヘッダ情報を挿入\n")
			_T("MD : Markdown記法に変換\n")
			_T("TX : 通常テキストに変換\n"));
	}
	else if (SameText(cmd, "ConvertImage")) {
		params = _T("\nCB : クリップボードの内容を変換・保存\n");
	}
	else if (SameText(cmd, "CopyDir")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("TO : コピー先を入力\n")
			_T("KT : タイムスタンプを維持\n")
			_T("NS : サブディレクトリを除外\n")
			_T("CC : ディレクトリ名をクリップボードにコピー\n")
			_T("LS : ディレクトリ名を一覧表示\n"));
	}
	else if (contained_wd_i(_T("CopyFileName|ListFileName"), cmd)) {
		if (cp) cp->Style = csDropDown;
		params = _T("\nFN : ファイル名部分のみ\n");
		if (SameText(cmd, "ListFileName")) params += _T("CH : クリップボードへのコピー履歴を表示\n");
	}
	else if (SameText(cmd, "CreateDir")) {
		params = _T("\r\nIN : デフォルト名を指定して入力\r\n");
	}
	else if (contained_wd_i(_T("CsrDirToOpp|LinkToOpp|SwapLR"), cmd)) {
		params = _T("\nTO : 反対側へ移動\n");
		if (SameText(cmd, "CsrDirToOpp")) params += _T("LK : 反対側への反映動作を維持/解除\n");
	}
	else if (contained_wd_i(_T("CurrFromOpp|CurrToOpp"), cmd)) {
		params = _T("\nSL : 選択状態を反映\n");
		if (SameText(cmd, "CurrToOpp")) params += _T("TO : 反対側へ移動\n");
	}
	else if (SameText(cmd, "CursorEnd")) {
		params = _T("\nAO : 有効なワークリスト項目に移動\n");
	}
	else if (SameText(cmd, "DateSelect")) {
		params = _T("\nTD : 今日付のファイル\nCP : カーソル位置と同じ日付のファイル\n");
	}
	else if (SameText(cmd, "DeleteADS")) {
		params = _T("\nZI : Zone.Identifier のみ削除\nTC : サムネイルキャッシュのみ削除\n");
	}
	else if (SameText(cmd, "DelJpgExif")) {
		params = _T("\nKT : タイムスタンプを維持\n");
	}
	else if (SameText(cmd, "DiffDir")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("CS : 大文字・小文字を区別\n")
			_T("AL : マスク *.*、サブディレクトリも対象として直ちに比較実行\n")
			_T("DL : 前回の条件で直ちに比較実行\n"));
	}
	else if (SameText(cmd, "DirHistory")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("GA : 全体履歴を表示\n")
			_T("GS : 全体履歴を表示(ソート/重複削除)\n")
			_T("FM : 全体履歴をフィルタ検索\n")
			_T("AC : カレント側の履歴をすべて削除\n")
			_T("GC : 全体履歴をすべて削除\n")
			_T("RD : 最近使ったディレクトリを表示\n"));
	}
	else if (SameText(cmd, "DistributionDlg")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("XC : 確認なしで直ちにコピー\n")
			_T("XM : 確認なしで直ちに移動\n")
			_T("SN : ファイルリストからマスクと振分先を設定\n"));
	}
	else if (SameText(cmd, "DotNyanDlg")) {
		params = _T("\nRS : カレント側に .nyanfi を再適用\n");
	}
	else if (SameText(cmd, "Duplicate")) {
		params = _T("\nRA : 管理者として二重起動\nDM : 一般ユーザに降格して二重起動\n");
	}
	else if (contained_wd_i(_T("EditHistory|ViewHistory"), cmd)) {
		params = _T("\nFF : フィルタ欄にフォーカス\nAC : 履歴をすべて消去\n");
	}
	else if (SameText(cmd, "ExeCommandLine")) {
		params = _T("\nFN : カーソル位置のファイル名を入力\nLC : 前回のコマンドを初期表示\n");
	}
	else if (SameText(cmd, "ExtractIcon")) {
		if (cp) cp->Style = csDropDown;
		params = _T("\nSI : スモールアイコンを抽出\n");
	}
	else if (SameText(cmd, "FileEdit") && id_idx==0) {
		params = _T("\nOS : 反対側で選択中のファイルも開く\n");
	}
	else if (SameText(cmd, "FileExtList")) {
		params = _T("\nCP : カーソル位置のディレクトリが対象\n");
	}
	else if (SameText(cmd, "Filter")) {
		params = _T("\nCS : 大小文字を区別\nCA : 実行前に選択マスクを解除\nFZ : あいまい検索\n");
	}
	else if (SameText(cmd, "FindDuplDlg")) {
		params = _T("\nLR : 左右で検索して重複ファイルを選択\n");
	}
	else if (contained_wd_i(_T("FindFileDlg|FindFileDirDlg"), cmd)) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("NM : マスク欄を非表示\n")
			_T("FK : 検索語欄にフォーカス\n")
			_T("R0 : 「サブディレクトリも検索」オフ\n")
			_T("R1 : 「サブディレクトリも検索」オン\n")
			_T("NT : ごみ箱内は検索しない\n"));
		if (SameText(cmd, "FindFileDlg")) {
			params.cat_sprintf(_T("%s"),
				_T("X0 : 「拡張検索」オフ\n")
				_T("X1 : 「拡張検索」オン\n")
				_T("A0 : 「アーカイブ内も検索」オフ\n")
				_T("A1 : 「アーカイブ内も検索」オン\n")
				_T("* : リストファイルの選択ダイアログを表示\n")
				_T("$STARTMENU : スタートメニューを検索\n")
				_T("$STARTUP : スタートアップを検索\n")
				_T("$DESKTOP : デスクトップを検索\n")
				_T("$DOCUMENT : ドキュメントを検索\n")
				_T("$PICTURE : ピクチャを検索\n")
				_T("$VIDEO : ビデオを検索\n")
				_T("$MUSIC : ミュージックを検索\n"));
		}
	}
	else if (SameText(cmd, "FindHardLink")) {
		params = _T("\nOP : 結果リストから反対側へ反映\n");
	}
	else if (SameText(cmd, "FindMark")) {
		params = _T("\nAL : すべてのマーク項目を検索\n");
	}
	else if (contained_wd_i(_T("FindTag|AddTag|SetTag|TagSelect"), cmd)) {
		params = _T("\n; : 入力ボックスでタグを指定\n");
		params += usr_TAG->TagNameList->Text;
	}
	else if (SameText(cmd, "FindTagName")) {
		params = _T("\nEJ : テキストエディタでダイレクトタグジャンプ\n");
		if (id_idx==2) params += _T("CO : 現在のファイルのみを検索\n");
	}
	else if (contained_wd_i(_T("FindSelDown|FindSelUp"), cmd)) {
		params = _T("\nEM : マッチ語を強調表示\n");
	}
	else if (SameText(cmd, "FixTabPath")) {
		params = _T("\nON : カレント側を固定\nOFF : 固定解除\n");
	}
	else if (SameText(cmd, "FunctionList")) {
		params = _T("\nFF : フィルタ欄にフォーカス\nFZ : あいまい検索\n");
	}
	else if (SameText(cmd, "GitDiff")) {
		params = _T("\nHD : HEAD から差分を表示\nXT : 外部diffツールで表示\n");
	}
	else if (SameText(cmd, "GitViewer")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("N0 : コミット履歴数の制限を外す\n")
			_T("N30 : コミット履歴数を制限する\n")
			_T("CP : カーソル位置ファイルのコミット履歴を表示\n"));
	}
	else if (SameText(cmd, "HelpContents")) {
		params = _T("\nCI : コマンドの索引\nFI : コマンドの機能別索引\nCH : 変更履歴\n");
	}
	else if (SameText(cmd, "IncSearch")) {
		params = _T("\nMM : Migemoモード\nNM : 通常モード\n");
		if (id_idx==0) params += _T("FM : フィルタマスク・モード\nCA : 実行前に選択マスクを解除\n");
	}
	else if (SameText(cmd, "InputDir")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("ND : ダイアログを表示しないで入力\n")
			_T("ND2 : ND でドロップダウンを開いて表示\n")
			_T("SD : フォルダ参照ダイアログを表示\n")
			_T("CB : クリップボード内容のディレクトリに移動\n"));
	}
	else if (SameText(cmd, "JumpLine")) {
		params = _T("\nST : スティッキー行に移動\n");
	}
	else if (SameText(cmd, "JsonViewer")) {
		params = _T("\nCB : クリップボード内容を表示\n");
	}
	else if (SameText(cmd, "DriveList")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("ND : ポップアップメニューで選択\n")
			_T("NS : ポップアップメニューで選択(空き容量非表示)\n"));
	}
	else if (SameText(cmd, "InputCommands")) {
		params = _T("\nFZ : あいまい検索\nEL : エコー、コマンドファイルの行番号表示\n");
	}
	else if (contained_wd_i(
		_T("CmdFileList|CmdHistory|ListClipboard|MarkList|KeyList|RepositoryList|SpecialDirList|UserDefList"),
		cmd))
	{
		params = _T("\nFF : フィルタ欄にフォーカス\n");
	}
	else if (contained_wd_i(_T("Exit|Close"), cmd)) {
		params = _T("\nNS : INIファイルを保存しない\nNX : 他のNyanFiを終了させない\n");
	}
	else if (SameText(cmd, "ExPopupMenu")) {
		params = _T("\nMN : 追加メニューのみ表示\nTL : 外部ツールのみ表示\n");
	}
	else if (SameText(cmd, "Library")) {
		params = _T("\nSD : 選択ダイアログを表示\n* : 選択メニューを表示\n");
		UnicodeString pnam = cv_env_str("%APPDATA%\\Microsoft\\Windows\\Libraries\\");
		std::unique_ptr<TStringList> lst(new TStringList());
		get_files(pnam, "*.library-ms", lst.get());
		for (int i=0; i<lst->Count; i++)
			params.cat_sprintf(_T("%s\n"), get_base_name(lst->Strings[i]).c_str());
	}
	else if (contained_wd_i(_T("CountLines|ListArchive|ListDuration|ListExpFunc"), cmd)) {
		params = _T("\nCC : 結果をクリップボードにコピー\nLS : 結果を一覧表示\n");
		if (SameText(cmd, "ListExpFunc"))
			params += _T("SN : 名前順にソート\nSI : インデックス順にソート\nSR : RVA順にソート\n");
	}
	else if (SameText(cmd, "ListNyanFi")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("CC : 結果をクリップボードにコピー\n")
			_T("LS : 結果を一覧表示\n")
			_T("ED : エディタ情報を追加\n")
			_T("XT : 外部ツール情報を追加\n"));
	}
	else if (contained_wd_i(_T("ListLog|ListText"), cmd)) {
		params = _T("\nFF : フィルタ欄にフォーカス\nEO : エラー箇所の絞り込み表示\n");
	}
	else if (SameText(cmd, "ListTail")) {
		params = _T("\nTE : 最後尾に移動r\nFF : フィルタ欄にフォーカス\n");
	}
	else if (SameText(cmd, "LoadFindSet")) {
		params = _T("\n* : 検索設定ファイルをポップアップメニューで選択\n");
	}
	else if (SameText(cmd, "LockComputer")) {
		params = _T("\nMO : ディスプレイの電源を切る\n");
	}
	else if (SameText(cmd, "Mark") && (id_idx==0 || id_idx==3)) {
		params = _T("\nND : カーソルを移動しない\nIM : メモを入力\nSL : 選択項目に一括適用\n");
	}
	else if (SameText(cmd, "MonitorOff")) {
		params = _T("\nLK : コンピュータをロックする\nKM : キーボード/マウスをロックする\n");
	}
	else if (SameText(cmd, "MoveTab")) {
		params = _T("\nTP : 先頭に先頭\nED : 最後に移動\nPR : １つ前に移動\n");
	}
	else if (SameText(cmd, "NameFromClip")) {
		params = _T("\nRC : ファイル名主部の文字置換を適用\n");
	}
	else if (contained_wd_i(_T("NextFile|PrevFile"), cmd)) {
		if (id_idx==3) params = _T("\nF1 : 見開き表示でも1ファイルずつ移動\n");
	}
	else if (SameText(cmd, "NextNyanFi")) {
		params = _T("\nDN : なければ二重起動\n");
	}
	else if (contained_wd_i(_T("OpenByApp|OpenByWin"), cmd)) {
		params = _T("\nDM : 一般ユーザに降格して開く");
	}
	else if (SameText(cmd, "OpenCtrlPanel")) {
		params = _T("\nGM : GodModeで開く\n");
	}
	else if (SameText(cmd, "PageBind")) {
		params = _T("\nR : 右綴じ\nL : 左綴じ\n");
	}
	else if (SameText(cmd, "Paste")) {
		params = _T("\nCL : 同名時にクローン化\nEX : テキスト/画像なら新規保存\n");
	}
	else if (SameText(cmd, "PlayList")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("RP : リピート再生\n")
			_T("SF : シャッフル再生\n")
			_T("SR : シャッフル・リピート再生\n")
			_T("NX : 次の曲へ\n")
			_T("PR : 前の曲へ\n")
			_T("PS : 一時停止\n")
			_T("RS : 再開\n")
			_T("PP : 再生/一時停止\n")
			_T("FI : ファイル情報を表示\n")
			_T("LS : プレイリストを表示\n")
			_T("CA : 停止してプレイリストをクリア\n"));
	}
	else if (SameText(cmd, "PopupMainMenu")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("F : ファイル\n")
			_T("E : 編集\n")
			_T("S : 検索\n")
			_T("V : 表示\n")
			_T("L : 一覧\n")
			_T("T : ツール\n")
			_T("O : 設定\n")
			_T("H : ヘルプ\n"));
	}
	else if (SameText(cmd, "RecentList")) {
		params = _T("\nAC : 最近使ったすべての項目を削除\nBC : リンク切れ項目を整理\n");
	}
	else if (contained_wd_i(_T("RegDirDlg|PathMaskDlg"), cmd)) {
		params = _T("\nND : ポップアップメニューで選択\n");
		if (SameText(cmd, "RegDirDlg")) params += _T("AD : 追加モード\n");
	}
	else if (contained_wd_i(_T("RegDirPopup|PushDir|PopDir"), cmd)) {
		params = _T("\nOP : 反対側で実行\n");
	}
	else if (SameText(cmd, "ReloadList")) {
		params = _T("\nCO : カレントのみ更新\nHL : ハードリンクのタイムスタンプ更新\nOFF : 更新禁止\n");
	}
	else if (SameText(cmd, "RenameDlg")) {
		params = _T("\nED : リストの編集による改名\n");
	}
	else if (SameText(cmd, "Restart")) {
		params = _T("\nNS : INIファイルを保存しない\nRA : 管理者として再起動\nDM : 管理者から降格して再起動\n");
	}
	else if (SameText(cmd, "SaveAsWorkList")) {
		params = _T("\nFL : カレントの内容をワークリストとして保存\n");
	}
	else if (contained_wd_i(_T("ScrollUpLog|ScrollUpText|ScrollDownLog|ScrollDownText"), cmd)) {
		params = _T("\nHP : 半ページ分\nFP : 1ページ分\n");
		params.cat_sprintf(_T("%s\n"), ContainsText(cmd, "Down")? _T("ED : 最後へ") : _T("TP : 先頭へ"));
	}
	else if (SameText(cmd, "SelByList")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("LR : カレント/反対側の両方で選択\n")
			_T("SM : 選択された項目だけを残す(選択マスク)\n")
			_T("CP : カーソル位置のリストファイルでカレント側を選択\n")
			_T("OP : カーソル位置のリストファイルで反対側を選択\n"));
	}
	else if (SameText(cmd, "SelCurWord")) {
		params = _T("\nEX : 選択範囲を拡張\n");
	}
	else if (SameText(cmd, "Select") && (id_idx==0 || id_idx==3)) {
		if (cp) cp->Style = csDropDown;
		params = "\n";
		if (id_idx==0) params += _T("IN : 繰り返し回数を入力\nRG : 前/後の選択項目まで範囲選択\n");
		params += _T("ND : カーソルを移動しない\n");
	}
	else if (SameText(cmd, "SelectFile")) {
		params = _T("\nNX : 次のファイルを表示\n");
	}
	else if (SameText(cmd, "SelEmptyDir")) {
		params = _T("\nNF : ファイルが含まれていなければ選択\n");
	}
	else if (SameText(cmd, "SelOnlyCur")) {
		params = _T("\nOD : ディレクトリだけを選択\nFD : ファイルとディレクトリの両方を選択\n");
	}
	else if (contained_wd_i(_T("MarkMask|SelMask"), cmd)) {
		params = _T("\nCA : マスクを解除\n");
	}
	else if (SameText(cmd, "SetColor")) {
		params = _T("\nRS : 配色をリセット\n");
	}
	else if (SameText(cmd, "SetFolderIcon")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("ND : ポップアップメニューで選択\n")
			_T("RS : デフォルトアイコンに戻す\n"));
	}
	else if (SameText(cmd, "SetInterpolation")) {
		params.sprintf(_T("%s"),
			_T("N : ニアレストネイバー\n")
			_T("L : バイリニア\n")
			_T("C : バイキュービック\n")
			_T("F : ファントリサンプリング\n")
			_T("H : 高品質バイキュービック\n")
			_T("X : 補間しない\n"));
	}
	else if (SameText(cmd, "SetPathMask")) {
		params = _T("\nEX : カーソル位置の拡張子でマスク\n");
	}
	else if (SameText(cmd, "SetTopAddr")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("TP : 先頭アドレスを先頭に\n")
			_T("NX : 先頭アドレスを後続部に\n")
			_T("PR : 先頭アドレスを先行部に\n")
			_T("ED : 終端アドレスを最後に\n"));
	}
	else if (SameText(cmd, "ShowFileInfo") && id_idx!=2) {
		params = _T("\nSD : ダイアログで表示\n");
	}
	else if (SameText(cmd, "SimilarImage")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("DH : dHash でソート\n")
			_T("AH : aHash でソート\n")
			_T("PH : pHash でソート\n")
			_T("HG : カラーヒストグラムでソート(DH|AH|PH と併用可)\n")
			_T("CC : 中央部をクロップして比較\n")
			_T("CB : クリップボード内容との類似性でソート\n"));
	}
	else if (SameText(cmd, "SimilarSort")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("IX : 拡張子を無視\n")
			_T("IC : 大小文字を無視\n")
			_T("IN : 数字部分を無視\n")
			_T("IF : 全角/半角を無視\n")
			_T("IA : IX、IC、IN、IFをすべて適用\n"));
	}
	else if (SameText(cmd, "Sort")) {
		params = _T("\nAO : 昇順\nDO : 降順\n");
	}
	else if (SameText(cmd, "SortDlg")) {
		params.sprintf(_T("%s"),
		 	_T("\n")
			_T("F : 名前順\n")
			_T("E : 拡張子順\n")
			_T("D : 更新日時順\n")
			_T("S : サイズ順\n")
			_T("A : 属性順\n")
			_T("U : なし\n")
			_T("L : 場所順(結果リスト)\n")
			_T("FE : 名前/拡張子順 トグル切り替え\n")
			_T("FD : 名前/更新日時順 トグル切り替え\n")
			_T("FS : 名前/サイズ順 トグル切り替え\n")
			_T("ED : 拡張子/更新日時順 トグル切り替え\n")
			_T("ES : 拡張子/サイズ順 トグル切り替え\n")
			_T("DS : 更新日時順/サイズ順 トグル切り替え\n")
			_T("IV : 現在のソート方法を逆順に\n")
			_T("IA : すべてのソート方法を逆順に\n")
			_T("XN : ディレクトリ - ファイルと同じ\n")
			_T("XF : ディレクトリ - 名前\n")
			_T("XD : ディレクトリ - 更新日時\n")
			_T("XS : ディレクトリ - サイズ\n")
			_T("XA : ディレクトリ - 属性\n")
			_T("XX : ディレクトリを区別しない\n")
			_T("XI : アイコン(ファイルが名前/拡張子の場合)\n")
			_T("XNX : ファイルと同じ/ディレクトリを区別しない トグル切り替え\n")
			_T("XNI : ファイルと同じ/アイコン トグル切り替え\n"));
	}
	else if (SameText(cmd, "SubDirList")) {
		params = _T("\nND : ポップアップメニューで選択\n");
	}
	else if (SameText(cmd, "SwapName")) {
		params = _T("\nLR : 左右で入れ替え\n");
	}
	else if (SameText(cmd, "TabHome")) {
		params = _T("\nAL : すべてのタブに適用\nCO : カレントのみに適用\n");
	}
	else if (contained_wd_i("TagJump|TagView", cmd)) {
		params = _T("\nDJ : 見つからなければダイレクトタグジャンプ\n");
	}
	else if (contained_wd_i(_T("TextViewer|ImageViewer"), cmd)) {
		params = _T("\nCB : クリップボードの内容を表示\nNN : 次のNyanFiで表示\n");
		if (SameText(cmd, "TextViewer")) params += _T("XW : 別ウィンドウで表示\n");
	}
	else if (SameText(cmd, "ToLeft")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("RP : 左側なら親ディレクトリへ\n")
			_T("DL : ルートならドライブ/共有フォルダ一覧を表示\n")
			_T("DP : ルートならドライブ選択メニューを表示\n"));
	}
	else if (SameText(cmd, "ToRight")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("RP : 左側なら親ディレクトリへ\n")
			_T("DL : ルートならドライブ/共有フォルダ一覧を表示\n")
			_T("DP : ルートならドライブ選択メニューを表示\n"));
	}
	else if (SameText(cmd, "ToOppSameItem")) {
		params = _T("\nNO : 反対側へ移動しない\n");
	}
	else if (contained_wd_i(_T("ToParent|ToParentOnLeft|ToParentOnRight"), cmd)) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("DL : ルートならドライブ/共有フォルダ一覧を表示\n")
			_T("DP : ルートならドライブ選択メニューを表示\n"));
	}
	else if (contained_wd_i(_T("UnPack|UnPackToCurr"), cmd)) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("CD : ディレクトリを作成してその中に\n")
			_T("CD2 : ルートに複数の対象があったらディレクトリ作成\n")
			_T("OW : 確認無しで上書き\n"));
	}
	else if (SameText(cmd, "UpdateFromArc")) {
		params = _T("\nUN : 新しいアーカイブを探して更新\n");
	}
	else if (contained_wd_i("ViewIniFile|ViewLog", cmd)) {
		params = _T("\nXW : 別ウィンドウで表示\n");
	}
	else if (SameText(cmd, "WatchTail")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("ST : 監視内容を表示\n")
			_T("CC : カーソル位置ファイルの監視を中止\n")
			_T("AC : すべての監視を中止\n"));
	}
	if (SameText(cmd, "WebMap")) {
		params = _T("\nIN : 緯度,経度を入力\nM1 : 地図の選択(1～4)\nZ16 : ズームレベル(1～18)\n");
	}
	else if (SameText(cmd, "WebSearch")) {
		params = _T("\nCB : クリップボードの内容を検索\n");
		if (id_idx==0) params += _T("FN : カーソル位置のファイル名を検索\n");
	}
	else if (SameText(cmd, "WinMaximize")) {
		params = _T("\nTN : 最大化/元に戻すのトグル切り替え\n");
	}
	else if (SameText(cmd, "WinPos")) {
		params = _T("\nL : 左位置\nT : 上位置\nR : 右位置\nB : 下位置\n");
	}
	else if (SameText(cmd, "WorkList") && id_idx==0) {
		params = _T("\nOP : 反対側で実行\nRL : 変更内容を破棄して読み込み直す\nDI : 無効な項目を一括削除\n");
	}
	else if (contained_wd_i(_T("ZoomIn|ZoomOut"), cmd)) {
		params = "\n";
		for (int i=2; i<=12; i++) params.cat_sprintf(_T("%u\n"), i);
	}
	else if (contained_wd_i(_T("Copy|Move|Delete|CompleteDelete"), cmd)) {
		params = _T("\nSO : 選択項目のみ処理\n");
		if (SameText(cmd, "Copy")) {
			params.cat_sprintf(_T("%s"),
				_T("OP : 反対側コピー先のカーソル位置を設定\nOP2 : コピー先のカーソル位置を逐次設定\n")
			 	_T("TO : コピー先を入力\nSD : コピー先を参照\n")
				_T("SS : カレントのサブディレクトリを選択\nSX : 任意のディレクトリを選択\n"));
		}
		if (SameText(cmd, "Move")) {
			params.cat_sprintf(_T("%s"),
				_T("OP : 反対側移動先のカーソル位置を設定\nOP2 : 移動先のカーソル位置を逐次設定\n")
			 	_T("TO : 移動先を入力\nSD : 移動先を参照\n")
				_T("SS : カレントのサブディレクトリを選択\nSX : 任意のディレクトリを選択\n"));
		}
		if (contained_wd_i(_T("Copy|Move"), cmd)) {
			params += _T("PR : 同名時処理を事前に指定\nKT : ディレクトリのタイムスタンプを維持\n");
		}
	}
	//トグル動作コマンド
	else if (contained_wd_i(
		_T("FileListOnly|HideSizeTime|LockTextPreview|MenuBar|MuteVolume|")
		_T("ShowByteSize|ShowFKeyBar|ShowHideAtr|ShowIcon|ShowPreview|ShowProperty|ShowStatusBar|ShowSystemAtr|ShowTabBar|")
		_T("PauseAllTask|Suspend|SyncLR|UseTrash|")
		_T("BitmapView|CharInfo|CsvRecord|Highlight|HtmlToText|Inspector|ShowCR|ShowIndent|ShowLineNo|ShowRuby|ShowRuler|ShowTAB|")
		_T("FixedLen|DoublePage|FullScreen|SubViewer|GrayScale|Histogram|Loupe|ShowGrid|ShowSeekBar|Sidebar|")
		_T("Thumbnail|ThumbnailEx|WarnHighlight"),
		cmd))
	{
		params = _T("\nON : 表示/有効\nOFF : 非表示/無効/解除\n");
		if (SameText(cmd, "ShowIcon")) {
			params += _T("FD : 全表示/フォルダアイコンのみ表示\nAC : キャッシュをすべて削除\n");
		}
		else if (SameText(cmd, "SubViewer")) {
			params += _T("CB : クリップボードの内容を表示\n");
			params += _T("LK : ロック/解除\nRL : 左に90度回転\nRR : 右に90度回転\nFH : 左右反転\nFV : 上下反転\n");
		}
		else if (SameText(cmd, "HtmlToText")) {
			params += _T("MD : Markdown記法に変換\nTX : 通常テキストに変換\n");
		}
	}
	//オプション設定
	else if (SameText(cmd, "OptionDlg")) {
		params.sprintf(_T("%s"),
			_T("\n")
			_T("GN : 一般\n")
			_T("G2 : 一般2\n")
			_T("MO : マウス操作\n")
			_T("DS : デザイン\n")
			_T("FC : フォント・配色\n")
			_T("TV : テキストビューア\n")
			_T("IV : イメージビューア\n")
			_T("ED : エディタ\n")
			_T("KY : キー設定\n")
			_T("KYO : キー設定(単独表示)\n")
			_T("AC : 関連付け\n")
			_T("XM : 追加メニュー\n")
			_T("XT : 外部ツール\n")
			_T("ST : 起動時\n")
			_T("NT : 通知・確認・ヒント\n")
			_T("CM : コマンド\n")
			_T("EV : イベント\n"));
	}

	p_list->Text = params;
	if (!cp && p_list->Count>0) {
		if (p_list->Strings[0].IsEmpty()) p_list->Delete(0);
	}
}

//---------------------------------------------------------------------------
//ファイル/ディレクトリ参照が必要か?
//---------------------------------------------------------------------------
bool need_RefDirFile(UnicodeString cmd)
{
	return contained_wd_i(
		_T("CalcDirSize|CalcDirSizeAll|ChangeDir|ChangeOppDir|ContextMenu|CopyTo|DebugCmdFile|DistributionDlg|")
		_T("ExeCommands|ExeMenuFile|FileEdit|FileRun|FindFileDlg|JumpTo|ListArchive|ListDuration|ListExpFunc|")
		_T("ListTail|ListText|ListTree|LoadBgImage|LoadTabGroup|LoadResultList|LoadWorkList|MoveTo|OpenByApp|")
		_T("OpenByExp|OpenByWin|OpenStandard|PropertyDlg|PlayList|TextViewer|SaveAsTabGroup|SelByList|SetColor|")
		_T("SetFolderIcon|SubDirList|HelpCurWord|Restart"),
		cmd);
}

//---------------------------------------------------------------------------
//コマンドパラメータから説明文字列を削除
//---------------------------------------------------------------------------
UnicodeString del_CmdDesc(UnicodeString cmd)
{
	UnicodeString prm = get_PrmStr(cmd);
	cmd = get_CmdStr(cmd);
	split_dsc(prm);
	if (!prm.IsEmpty()) cmd.cat_sprintf(_T("_%s"), prm.c_str());
	return cmd;
}

//---------------------------------------------------------------------------
//カーソルキーをコマンドに変換
//---------------------------------------------------------------------------
UnicodeString get_CsrKeyCmd(UnicodeString key_str)
{
	switch (idx_of_word_i("DOWN|UP|LEFT|RIGHT", key_str)) {
	case  0: return "CursorDown";
	case  1: return "CursorUp";
	case  2: return "ToLeft";
	case  3: return "ToRight";
	default: return EmptyStr;
	}
}
//---------------------------------------------------------------------------
