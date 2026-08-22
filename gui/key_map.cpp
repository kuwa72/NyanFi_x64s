/**
 * @file gui/key_map.cpp
 * @brief キー割り当て表の実装 (ini 読み込みを含む、wx 非依存の部分)
 *
 * @details wx に依存する KeyStrOf() の実装だけは gui/key_map_wx.cpp に分けてある。
 *          こちらは UsrIniFile と usr_str.h/usr_cmdlist.h/usr_key.h の
 *          UnicodeString ベースの API しか使わないため wx 無しでも単体テストできる
 *          (tests/core/test_gui_settings.cpp、CMakeLists.txt の
 *          `nyanfi_gui_core` ライブラリ)。
 */
#include "gui/key_map.h"

#include <memory>

#include "UIniFile.h"
#include "usr_cmdlist.h"
#include "usr_key.h"
#include "usr_str.h"

//---------------------------------------------------------------------------
KeyMap::KeyMap() : entries_(new TStringList())
{
	LoadDefaults();
}

//---------------------------------------------------------------------------
namespace {

/// wx/defs.h の `wxKeyCode` の値を、wx をリンクせずに書き写したもの。
/// 本物との一致は gui/key_map_wx.cpp の static_assert が確認する
/// (wx を更新して値がずれたらコンパイルが通らない)。
enum WxKeyCode {
	kWxStart    = 300,
	kWxPause    = kWxStart + 10,
	kWxEnd      = kWxStart + 12,
	kWxHome     = kWxStart + 13,
	kWxLeft     = kWxStart + 14,
	kWxUp       = kWxStart + 15,
	kWxRight    = kWxStart + 16,
	kWxDown     = kWxStart + 17,
	kWxInsert   = kWxStart + 22,
	kWxNumpad0  = kWxStart + 24,
	kWxMultiply = kWxStart + 34,
	kWxAdd      = kWxStart + 35,
	kWxSubtract = kWxStart + 37,
	kWxDecimal  = kWxStart + 38,
	kWxDivide   = kWxStart + 39,
	kWxF1       = kWxStart + 40,
	kWxPageUp   = kWxStart + 66,
	kWxPageDown = kWxStart + 67,
	kWxDelete   = 127,
};

}  // namespace

//---------------------------------------------------------------------------
WORD KeyMap::VkFromWxKeyCode(int wx_keycode)
{
	// F1〜F12 と 10キーの数字は連番なので範囲で捌く
	if (wx_keycode >= kWxF1 && wx_keycode <= kWxF1 + 11) {
		return static_cast<WORD>(VK_F1 + (wx_keycode - kWxF1));
	}
	if (wx_keycode >= kWxNumpad0 && wx_keycode <= kWxNumpad0 + 9) {
		return static_cast<WORD>(VK_NUMPAD0 + (wx_keycode - kWxNumpad0));
	}

	switch (wx_keycode) {
	case kWxLeft:		return VK_LEFT;
	case kWxRight:		return VK_RIGHT;
	case kWxUp:			return VK_UP;
	case kWxDown:		return VK_DOWN;
	case kWxPageUp:		return VK_PRIOR;
	case kWxPageDown:	return VK_NEXT;
	case kWxHome:		return VK_HOME;
	case kWxEnd:		return VK_END;
	case kWxInsert:		return VK_INSERT;
	case kWxDelete:		return VK_DELETE;
	case kWxPause:		return VK_PAUSE;
	case kWxMultiply:	return VK_MULTIPLY;
	case kWxAdd:		return VK_ADD;
	case kWxSubtract:	return VK_SUBTRACT;
	case kWxDecimal:	return VK_DECIMAL;
	case kWxDivide:		return VK_DIVIDE;
	default:			break;
	}

	// ここから下は wx の値と VK が同値の範囲。
	// BackSpace(8) / Tab(9) / Enter(13) / Esc(27) / Space(32) と、
	// 英数字 ('0'-'9' / 'A'-'Z' は VK_0-VK_9 / VK_A-VK_Z と同値)
	if (wx_keycode > 0 && wx_keycode < 128) return static_cast<WORD>(wx_keycode);

	return 0;  // WXK_SHIFT など、キー名を持たないもの
}


//---------------------------------------------------------------------------
/**
 * @brief 既定のキー割り当て
 * @details VCL 版は ini の [Key] セクションから読む。Phase 2 の骨格では
 *          実装済みのコマンドだけを既定として持つ。コマンド名は
 *          usr_cmdlist.cpp のコマンド表と同じ綴りを使う。
 */
void KeyMap::LoadDefaults()
{
	// カーソル移動 (UP/DOWN/LEFT/RIGHT は get_CsrKeyCmd が持っているので書かない)
	Assign(_T("PGUP"), _T("PageUp"));
	Assign(_T("PGDN"), _T("PageDown"));
	Assign(_T("HOME"), _T("CursorTop"));
	Assign(_T("END"), _T("CursorEnd"));

	// 移動・実行。ENTER は src/Global.cpp の既定キー表の "F:Enter=OpenStandard"
	// と同じ綴り (カーソル位置がディレクトリなら入る、ファイルなら関連付けで
	// 開く。gui/main_frame.cpp::Execute() を参照)。Ctrl+Enter も既定キー表の
	// "F:Ctrl+Enter=OpenByApp" (アプリケーションから開く) と同じ
	Assign(_T("ENTER"), _T("OpenStandard"));
	Assign(_T("Ctrl+Enter"), _T("OpenByApp"));
	Assign(_T("BKSP"), _T("ToParent"));
	Assign(_T("TAB"), _T("ToOpposite"));
	Assign(_T("F5"), _T("ReloadList"));

	// テキストビューア。"V" は src/Global.cpp の既定キー表 ("F:V=TextViewer")
	// と同じ。ビューア自体のキー操作 (行移動・検索・折り返し切替・閉じる) は
	// TextViewer::HandleKey (gui/text_viewer.cpp) がこの KeyMap を経由せず
	// 直接処理する (V モードのキー割り当ては Phase 2 骨格では対応していない。
	// gui/key_map.h 冒頭のコメントを参照)
	Assign(_T("V"), _T("TextViewer"));

	// 画像ビューア。"G" は src/Global.cpp の既定キー表 ("F:G=ImageViewer") と
	// 同じ。ビューア自体のキー操作 (フィット/ズーム/前後移動/閉じる) は
	// ImageViewer::HandleKey (gui/image_viewer.cpp) がこの KeyMap を経由せず
	// 直接処理する (TextViewer と同じ作り)
	Assign(_T("G"), _T("ImageViewer"));

	// 文字列検索 (GREP)。usr_cmdlist.cpp のコマンド表には "FV:Grep=文字列検索(GREP)"
	// として載っているが、src/Global.cpp の既定キー表 (KeyFuncList->Text) には
	// "Grep" に対応する行が無く、メニュー専用の操作だったと見られる (実測、
	// F3=FindFileDlg というファイル名検索の割り当てはあるが内容検索は無い)。
	// そのため Ctrl+F (多くのエディタの「ファイル内検索」の慣習) を
	// Phase 2 骨格向けに新規で割り当てた (推測。要検証)
	Assign(_T("Ctrl+F"), _T("Grep"));

	// マーク
	Assign(_T("SPACE"), _T("Select"));
	Assign(_T("Ctrl+A"), _T("SelAllItem"));
	Assign(_T("Ctrl+D"), _T("ClearAll"));

	// 選択しながらカーソル移動。Shift+Up / Shift+Down は
	// src/Global.cpp:2113-2114 の既定表にある実際の割り当て
	Assign(_T("Shift+UP"), _T("CursorUpSel"));
	Assign(_T("Shift+DOWN"), _T("CursorDownSel"));
	// 以下は VCL の既定表に無いので推測 (上2つと対になるように決めた)
	Assign(_T("Shift+PGUP"), _T("PageUpSel"));
	Assign(_T("Shift+PGDN"), _T("PageDownSel"));
	Assign(_T("Shift+HOME"), _T("CursorTopSel"));
	Assign(_T("Shift+END"), _T("CursorEndSel"));

	// 表示の切り替え。**VCL の既定表 (src/Global.cpp) に1つも無い**ので
	// すべて推測。エクスプローラや他のファイラの慣習に寄せた
	Assign(_T("Ctrl+H"), _T("ShowHideAtr"));      // 隠しファイル
	Assign(_T("Ctrl+B"), _T("ShowByteSize"));     // バイト単位
	Assign(_T("Shift+LEFT"), _T("BorderLeft"));   // 境界を左へ
	Assign(_T("Shift+RIGHT"), _T("BorderRight")); // 境界を右へ
	Assign(_T("Ctrl+Shift+E"), _T("EqualListWidth"));
	Assign(_T("Ctrl+Shift+S"), _T("SwapLR"));

	// ディレクトリ移動。O / Shift+O は src/Global.cpp:2089,2112 の既定表にある
	// 実際の割り当て
	Assign(_T("O"), _T("CurrFromOpp"));
	Assign(_T("Shift+O"), _T("CurrToOpp"));
	// 以下は既定表に無いので推測
	Assign(_T("Ctrl+HOME"), _T("ToRoot"));
	Assign(_T("Ctrl+RIGHT"), _T("CsrDirToOpp"));
	Assign(_T("Ctrl+PGDN"), _T("NextDrive"));
	Assign(_T("Ctrl+PGUP"), _T("PrevDrive"));
	Assign(_T("Ctrl+Shift+D"), _T("PushDir"));
	Assign(_T("Ctrl+Shift+P"), _T("PopDir"));

	// タブ操作。VCL の既定表にタブのエントリが1件も無いので全部推測
	// (既存の Ctrl+T / Ctrl+W / Ctrl+Tab と揃えた)
	Assign(_T("Ctrl+Shift+T"), _T("SoloTab"));
	Assign(_T("Ctrl+Shift+H"), _T("TabHome"));
	Assign(_T("Ctrl+Shift+M"), _T("MoveTab"));
	Assign(_T("Ctrl+Shift+G"), _T("ToTab"));
	// ディレクトリ一覧
	Assign(_T("Ctrl+Shift+L"), _T("SubDirList"));
	Assign(_T("Ctrl+Shift+F"), _T("SpecialDirList"));

	// ファイル操作。VCL の既定表に無いので推測
	Assign(_T("Shift+C"), _T("CopyTo"));
	Assign(_T("Shift+M"), _T("MoveTo"));
	Assign(_T("Ctrl+Shift+U"), _T("NameToUpper"));
	Assign(_T("Ctrl+Shift+N"), _T("NameToLower"));
	Assign(_T("Ctrl+N"), _T("NewFile"));
	// クリップボード経由。エクスプローラと同じ Ctrl+C / Ctrl+X / Ctrl+V にした。
	// **Ctrl+C はファイル名のコピーではなくファイルのコピー**にしてある
	// (エクスプローラの慣習を優先。名前のコピーは Ctrl+Shift+C)
	Assign(_T("Ctrl+C"), _T("CopyToClip"));
	Assign(_T("Ctrl+X"), _T("CutToClip"));
	Assign(_T("Ctrl+V"), _T("Paste"));
	Assign(_T("Ctrl+Shift+C"), _T("CopyFileName"));

	// リンク・属性。VCL の既定表に無いので推測
	Assign(_T("Ctrl+L"), _T("CreateShortcut"));
	Assign(_T("Ctrl+Shift+K"), _T("CreateHardLink"));
	Assign(_T("Ctrl+Shift+Y"), _T("CreateSymLink"));
	Assign(_T("Ctrl+Shift+I"), _T("SetDirTime"));

	// 書庫。P / U は src/Global.cpp:2090,2094 の既定表にある実際の割り当て
	Assign(_T("P"), _T("Pack"));
	Assign(_T("U"), _T("UnPack"));
	// 以下は既定表に無いので推測
	Assign(_T("Shift+P"), _T("PackToCurr"));
	Assign(_T("Shift+U"), _T("UnPackToCurr"));
	Assign(_T("Ctrl+Shift+A"), _T("ListArchive"));

	// 比較・ハッシュ。VCL の既定表に無いので推測
	Assign(_T("Ctrl+Shift+X"), _T("GetHash"));
	Assign(_T("Ctrl+Shift+Z"), _T("CompareHash"));
	Assign(_T("Ctrl+Shift+O"), _T("SelOnlyCur"));
	Assign(_T("Ctrl+Shift+V"), _T("DiffDir"));

	// テキスト操作。VCL の既定表に無いので推測
	Assign(_T("Ctrl+Shift+W"), _T("CountLines"));
	Assign(_T("Ctrl+Shift+J"), _T("JoinText"));
	Assign(_T("Ctrl+Shift+R"), _T("ConvertTextEnc"));
	Assign(_T("Ctrl+Shift+B"), _T("ListFileName"));

	// 外部連携。X は src/Global.cpp:2097 の既定表にある実際の割り当て
	Assign(_T("X"), _T("ContextMenu"));
	// 以下は既定表に無いので推測
	Assign(_T("Ctrl+Shift+Q"), _T("CommandPrompt"));
	Assign(_T("Ctrl+Alt+P"), _T("PowerShell"));
	Assign(_T("Ctrl+Alt+E"), _T("OpenByExp"));

	// 情報系。I は src/Global.cpp:2084 の既定表にある実際の割り当て
	Assign(_T("I"), _T("CalcDirSize"));
	// 以下は既定表に無いので推測
	Assign(_T("Shift+I"), _T("CalcDirSizeAll"));
	Assign(_T("Ctrl+Alt+L"), _T("FileExtList"));
	Assign(_T("Ctrl+Alt+T"), _T("ListTree"));
	Assign(_T("Ctrl+Alt+A"), _T("AboutNyanFi"));

	// 設定・その他。VCL の既定表に無いので推測
	Assign(_T("Ctrl+Alt+I"), _T("EditIniFile"));
	Assign(_T("Ctrl+Alt+V"), _T("ViewIniFile"));
	Assign(_T("Ctrl+Alt+N"), _T("NameFromClip"));
	Assign(_T("Ctrl+Alt+S"), _T("ShareList"));
	Assign(_T("Ctrl+Alt+C"), _T("ListClipboard"));

	// 検索と結果リスト。F3 は src/Global.cpp:2100 の既定表にある実際の割り当て
	Assign(_T("F3"), _T("FindFileDlg"));
	// 以下は既定表に無いので推測
	Assign(_T("Shift+F3"), _T("FindDirDlg"));
	Assign(_T("Ctrl+F3"), _T("FindFileDirDlg"));
	Assign(_T("ESC"), _T("ReturnList"));
	Assign(_T("Ctrl+Alt+D"), _T("FindDuplDlg"));
	Assign(_T("Ctrl+Alt+G"), _T("SelSameDir"));

	// ワークリスト。"W" は src/Global.cpp:2096 の既定表にある実際の割り当て
	// ("F:W=WorkList")。**それ以外は既定表に記載が無いので推測**。
	// 引数付きのコマンド名 (`_` の後ろ) は VCL の ActionParam と同じ綴り
	Assign(_T("W"), _T("WorkList"));
	Assign(_T("Shift+W"), _T("WorkList_OP"));       // 反対側に出す
	Assign(_T("Ctrl+Alt+W"), _T("SendToWorkList")); // 選択項目を登録
	Assign(_T("Ctrl+Alt+X"), _T("WorkList_DI"));    // 存在しない項目を外す
	Assign(_T("Ctrl+Alt+H"), _T("HomeWorkList"));
	Assign(_T("Ctrl+Alt+O"), _T("LoadWorkList"));
	Assign(_T("Ctrl+S"), _T("SaveWorkList"));
	Assign(_T("Ctrl+Alt+U"), _T("SaveAsWorkList"));
	Assign(_T("Ctrl+Alt+F"), _T("NewWorkList"));
	Assign(_T("Ctrl+Alt+K"), _T("SelWorkItem"));
	Assign(_T("Ctrl+Alt+B"), _T("SetAlias"));
	Assign(_T("Ctrl+Alt+M"), _T("InsSeparator"));
	Assign(_T("Ctrl+Up"), _T("WorkItemUP"));
	Assign(_T("Ctrl+Down"), _T("WorkItemDown"));
	Assign(_T("Ctrl+Alt+Enter"), _T("WorkItemMove"));

	// 栞マークとタグ。**VCL の既定表に1件も無いのですべて推測**。
	// Ctrl+ と Ctrl+Shift+ の英字は埋まってしまったので Alt+ を使う
	// (このウィンドウはメニューバーを持たないのでニーモニックと衝突しない)
	Assign(_T("Alt+M"), _T("Mark"));
	Assign(_T("Alt+I"), _T("Mark_IM"));        // メモ付きで付ける
	Assign(_T("Alt+C"), _T("ClearMark"));      // 一覧の栞を外す
	Assign(_T("Alt+Q"), _T("ClearMark_AC"));   // すべての栞を外す
	Assign(_T("Alt+N"), _T("NextMark"));
	Assign(_T("Alt+P"), _T("PrevMark"));
	Assign(_T("Alt+S"), _T("SelMark"));
	Assign(_T("Alt+K"), _T("MarkMask"));
	Assign(_T("Alt+L"), _T("MarkList"));
	Assign(_T("Alt+F"), _T("FindMark"));
	Assign(_T("Alt+T"), _T("SetTag"));
	Assign(_T("Alt+A"), _T("AddTag"));
	Assign(_T("Alt+D"), _T("DelTag"));
	Assign(_T("Alt+G"), _T("TagSelect"));
	Assign(_T("Alt+R"), _T("FindTag"));
	Assign(_T("Alt+Z"), _T("TrimTagData"));

	// 選択と絞り込みの拡張。**VCL の既定表に1件も無いのですべて推測**
	Assign(_T("Alt+W"), _T("MaskSelect"));
	Assign(_T("Alt+B"), _T("SelByList"));
	Assign(_T("Alt+E"), _T("SelEmptyDir"));
	Assign(_T("Alt+Shift+E"), _T("SelEmptyDir_NF"));  // ファイルを含まないものまで
	Assign(_T("Alt+Y"), _T("DateSelect"));
	Assign(_T("Alt+X"), _T("NextSameName"));
	Assign(_T("Alt+O"), _T("SelMask"));
	Assign(_T("Alt+U"), _T("DelSelMask"));
	Assign(_T("Alt+H"), _T("MaskFind"));
	Assign(_T("Alt+J"), _T("InputPathMask"));

	// ファイル操作の続き。**VCL の既定表に1件も無いのですべて推測**
	Assign(_T("Alt+1"), _T("Clone"));
	Assign(_T("Alt+2"), _T("CloneToCurr"));
	Assign(_T("Alt+3"), _T("CopyDir"));
	Assign(_T("Alt+4"), _T("CreateDirsDlg"));
	Assign(_T("Alt+5"), _T("CreateJunction"));
	Assign(_T("Alt+6"), _T("SwapName"));
	Assign(_T("Alt+7"), _T("UndoRename"));
	Assign(_T("Alt+8"), _T("CreateTestFile"));

	// 抽出と変換。**VCL の既定表に1件も無いのですべて推測**
	Assign(_T("Alt+9"), _T("ExtractIcon"));
	Assign(_T("Alt+0"), _T("ExtractImage"));
	Assign(_T("Shift+Alt+1"), _T("ConvertDoc2Txt"));
	Assign(_T("Shift+Alt+2"), _T("ConvertHtm2Txt"));
	Assign(_T("Shift+Alt+3"), _T("ConvertHtm2Txt_MD"));
	Assign(_T("Shift+Alt+4"), _T("ConvertImage"));
	Assign(_T("Shift+Alt+5"), _T("SetExifTime"));
	Assign(_T("Shift+Alt+6"), _T("DelJpgExif"));
	Assign(_T("Shift+Alt+7"), _T("SetArcTime"));

	// ログ。**VCL の既定表に1件も無いのですべて推測**
	Assign(_T("Shift+Alt+L"), _T("ListLog"));
	Assign(_T("Shift+Alt+V"), _T("ViewLog"));
	Assign(_T("Shift+Alt+C"), _T("ClearLog"));
	Assign(_T("Shift+Alt+I"), _T("LogFileInfo"));
	Assign(_T("Shift+Alt+N"), _T("ListNyanFi"));

	// 履歴。**VCL の既定表に1件も無いのですべて推測**
	Assign(_T("Shift+Alt+H"), _T("RecentList"));
	Assign(_T("Shift+Alt+E"), _T("EditHistory"));
	Assign(_T("Shift+Alt+W"), _T("ViewHistory"));
	Assign(_T("Shift+Alt+K"), _T("CmdHistory"));

	// 名前を付けた状態の保存と読み込み。**VCL の既定表に無いのですべて推測**
	Assign(_T("Shift+Alt+T"), _T("SaveTabGroup"));
	Assign(_T("Shift+Alt+G"), _T("LoadTabGroup"));
	Assign(_T("Shift+Alt+R"), _T("SaveAsResultList"));
	Assign(_T("Shift+Alt+O"), _T("LoadResultList"));

	// 表示の切り替え。**VCL の既定表に無いのですべて推測**。
	// 拡大/縮小はブラウザやエディタの慣習に寄せた
	Assign(_T("Ctrl+SEMICOLON"), _T("ZoomIn"));
	Assign(_T("Ctrl+MINUS"), _T("ZoomOut"));
	Assign(_T("Ctrl+0"), _T("ZoomReset"));
	Assign(_T("Shift+Alt+F"), _T("SetFontSize"));
	Assign(_T("Shift+Alt+A"), _T("AlphaBlend"));
	Assign(_T("Shift+Alt+P"), _T("WinPos"));
	Assign(_T("Shift+Alt+Z"), _T("FileListOnly"));
	Assign(_T("Shift+Alt+B"), _T("SetSttBarFmt"));

	// システム操作と外部連携。**VCL の既定表に無いのですべて推測**。
	// 取り返しのつかないもの (ごみ箱を空にする・ドライブの取り外し) は
	// 押し間違えにくいよう修飾キーを重ねてある
	Assign(_T("Shift+Alt+S"), _T("WebSearch"));
	Assign(_T("Shift+Alt+D"), _T("OpenADS"));
	Assign(_T("Shift+Alt+X"), _T("DeleteADS"));
	Assign(_T("Shift+Alt+Q"), _T("EmptyTrash"));
	Assign(_T("Shift+Alt+8"), _T("Eject"));
	Assign(_T("Shift+Alt+9"), _T("EjectDrive"));
	Assign(_T("Shift+Alt+0"), _T("LockComputer"));
	Assign(_T("Shift+Alt+M"), _T("MuteVolume"));
	Assign(_T("Shift+Alt+Y"), _T("MonitorOff"));

	// 並べ替え・絞り込み。"S" は src/Global.cpp の既定キー表にある実際の割り当て
	// ("F:S=SortDlg") と同じ。Ctrl+M / Ctrl+U は既定キー表に対応する記載が無く
	// (パスマスクはコンボボックス操作が前提で単独のキー割り当てが見当たらない)、
	// Phase 2 骨格向けに新規で決めたもの (推測)
	Assign(_T("S"), _T("SortDlg"));
	Assign(_T("Ctrl+M"), _T("SetPathMask"));
	Assign(_T("Ctrl+U"), _T("ClearMask"));

	// ファイル操作。src/Global.cpp の既定キー表 ("F:C=Copy" 等) と同じ綴り・
	// 同じキーを使う (F5/F6/F7/F8 の慣習ではなく、NyanFi 本来の1文字キー)
	Assign(_T("C"), _T("Copy"));
	Assign(_T("M"), _T("Move"));
	Assign(_T("D"), _T("Delete"));
	Assign(_T("K"), _T("CreateDir"));
	Assign(_T("R"), _T("RenameDlg"));

	// インクリメンタルサーチ・ディレクトリ移動の効率化。"F"/"B"/"H"/"L" は
	// src/Global.cpp の既定キー表 ("F:F=IncSearch" / "F:B=BackDirHist" /
	// "F:H=DirHistory" / "F:L=DriveList") と同じ
	Assign(_T("F"), _T("IncSearch"));
	Assign(_T("B"), _T("BackDirHist"));
	Assign(_T("H"), _T("DirHistory"));
	Assign(_T("L"), _T("DriveList"));
	// ForwardDirHist (履歴を進む) は既定キー表に対応するキーが無い
	// (マウスの第2ボタン X2BtnCmdF の既定値も空文字列で、割り当てが無い)。
	// "B" (戻る) と対になるよう Phase 2 骨格向けに新規で決めたもの
	// (推測。要検証)
	Assign(_T("Shift+B"), _T("ForwardDirHist"));
	// InputDir (パスを直接入力して移動) も既定キー表に対応するキーが無い。
	// 多くのエディタ/ファイラの「Go to」の慣習に合わせた Phase 2 骨格独自の
	// 割り当て (推測。要検証)
	Assign(_T("Ctrl+G"), _T("InputDir"));

	// 表示・終了。"FVI:PropertyDlg" (usr_cmdlist.cpp) には既定キーが無い
	// (src/MainFrm.dfm の PropertyDlgAction にも ShortCut が無く、メニュー専用
	// らしい)。Alt+Enter は Windows のプロパティ表示の慣習に合わせた
	// Phase 2 骨格独自の割り当て (推測。要検証)
	Assign(_T("F1"), _T("KeyList"));
	Assign(_T("F12"), _T("ShowCmdList"));
	Assign(_T("Alt+Enter"), _T("PropertyDlg"));
	Assign(_T("Alt+F4"), _T("Exit"));
	Assign(_T("Ctrl+Q"), _T("Exit"));

	// タブ (複数ディレクトリの切り替え)。コマンド名は usr_cmdlist.cpp のコマンド表
	// ("F:AddTab=タブを追加" 等、実測) と同じ綴り。ただし src/Global.cpp の
	// 既定キー表 (KeyFuncList->Text) にはタブ系コマンドの既定キーが1つも無く
	// (ツールバー・タブの右クリックメニュー専用の操作だったと見られる)、
	// 以下はすべて Phase 2 骨格向けに新規で決めたもの (推測。要検証)。
	// Ctrl+T/Ctrl+W はブラウザ等でのタブ追加・閉じるの慣習に、Ctrl+Tab/
	// Shift+Ctrl+Tab は多くのタブ付きアプリでの次/前のタブ切替の慣習に合わせた
	// (修飾子の順序は get_ShiftStr() (usr_key.cpp) の実測順 "Shift+"→"Ctrl+"→
	// "Alt+" に合わせてある。TStringList の Name 比較は大小文字を区別しないため
	// 大文字/小文字自体は Lookup に影響しない)。
	// Ctrl+E (一覧から選ぶ、PopupTab) は他のキーと衝突しない空きキーから選んだ
	Assign(_T("Ctrl+T"), _T("AddTab"));
	Assign(_T("Ctrl+W"), _T("DelTab"));
	Assign(_T("Ctrl+Tab"), _T("NextTab"));
	Assign(_T("Shift+Ctrl+Tab"), _T("PrevTab"));
	Assign(_T("Ctrl+E"), _T("PopupTab"));
}

//---------------------------------------------------------------------------
void KeyMap::Assign(const UnicodeString &key_str, const UnicodeString &command)
{
	const int idx = entries_->IndexOfName(key_str);
	if (idx != -1) {
		entries_->ValueFromIndex[idx] = command;
	}
	else {
		UnicodeString line;
		entries_->Add(line.sprintf(_T("%s=%s"), key_str.c_str(), command.c_str()));
	}
}

//---------------------------------------------------------------------------
UnicodeString KeyMap::Lookup(const UnicodeString &key_str) const
{
	if (key_str.IsEmpty()) return EmptyStr;

	// カーソルキーは移植済みの割り当てをそのまま使う
	const UnicodeString csr = get_CsrKeyCmd(key_str);
	if (!csr.IsEmpty()) return csr;

	return entries_->Values[key_str];
}

//---------------------------------------------------------------------------
// KeyMap::KeyStrOf() は wx に依存するため gui/key_map_wx.cpp に定義がある
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/**
 * @brief KeyFuncList の1エントリを解析する
 * @details 形式の実測結果は gui/key_map.h 冒頭のコメントを参照。
 *          "F:" (ファイルペイン) 以外のモード、SELECT+ 付き、2ストローク
 *          ('~' を含む) は Phase 2 骨格が対応する仕組みを持たないため、
 *          誤動作させるより読み飛ばす方を選び false を返す。
 */
bool KeyMap::ParseKeyFuncListEntry(
	const UnicodeString &name, const UnicodeString &value,
	UnicodeString &key_str_out, UnicodeString &command_out)
{
	// モード文字 "F:" (ファイルペイン) 以外は非対応
	static const UnicodeString kModePrefix = _T("F:");
	if (!StartsText(kModePrefix, name)) return false;

	UnicodeString key_str = name.SubString(kModePrefix.Length() + 1);
	if (key_str.IsEmpty()) return false;

	// SELECT+ (選択操作の修飾子。src/Global.cpp の KeyStr_SELECT = "SELECT+")
	// は対応する選択モデルが無いため非対応
	if (StartsText(_T("SELECT+"), key_str)) return false;

	// 2ストロークキー ("Ctrl+K~D" 等) は非対応
	if (ContainsStr(key_str, _T("~"))) return false;

	if (value.IsEmpty()) return false;

	key_str_out = key_str;
	command_out = value;
	return true;
}

//---------------------------------------------------------------------------
/**
 * @brief ini の KeyFuncList セクションから読み込み、既定の割り当てに上書きする
 * @details 読み込みは寛容にする (ini が無い/セクションが無い/1行も解釈でき
 *          ないときは何もせず既定のまま)。書き込みは一切行わない
 *          (UsrIniFile::UpdateFile を呼ばない) ので、VCL 版の既存 ini を
 *          読むだけなら壊れる心配が無い。
 */
void KeyMap::LoadFromIni(const UnicodeString &ini_path)
{
	if (!FileExists(ini_path)) return;

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	std::unique_ptr<TStringList> lst(new TStringList());
	ini->ReadSection(_T("KeyFuncList"), lst.get());

	for (int i = 0; i < lst->Count; ++i) {
		UnicodeString key_str, command;
		if (ParseKeyFuncListEntry(lst->Names[i], lst->ValueFromIndex[i], key_str, command)) {
			Assign(key_str, command);
		}
	}
}
