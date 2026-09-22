/**
 * @file tests/core/test_gui_f_misc_ops.cpp
 * @brief gui/f_misc_ops.h/.cpp (Fモード残コマンド batch4) の回帰テスト
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の以下 (src 必読済み):
 *          Suspend (26516行) / PauseAllTask (23570行) / CancelAllTask (14070行) /
 *          TaskMan (26801行) / Backup (13645行) / CompressDir (14987行) /
 *          CompareDlg NC分岐 (14464行) / FindHardLink (18868行) /
 *          LinkToOpp (20622行) / SetFolderIcon (25594行) /
 *          FindFolderIcon (18152行) / JumpTo (20433行) / DriveGraph (16832行)。
 *          判断・変換だけを wx 非依存の純関数にしてここで固定する (規約8)。
 */
#include "doctest/doctest.h"

#include "gui/f_misc_ops.h"

using namespace f_misc_ops;

//===========================================================================
// ToggleFlagValue: VCL SetToggleAction (MainFrm.cpp:12651)
//   sw = TestActionParam("ON")? true : TestActionParam("OFF")? false : !sw
//===========================================================================

TEST_CASE("ToggleFlagValue: ON/OFF/反転")
{
	CHECK(ToggleFlagValue(false, _T("")) == true);
	CHECK(ToggleFlagValue(true, _T("")) == false);
	CHECK(ToggleFlagValue(false, _T("ON")) == true);
	CHECK(ToggleFlagValue(true, _T("ON")) == true);
	CHECK(ToggleFlagValue(false, _T("OFF")) == false);
	CHECK(ToggleFlagValue(true, _T("OFF")) == false);
	// 大小無視・トークン内一致 (VCL TestActionParam と同じ ';' 区切り完全一致)
	CHECK(ToggleFlagValue(false, _T("on")) == true);
	CHECK(ToggleFlagValue(false, _T("XX;OFF")) == false);
}

//===========================================================================
// DecidePauseAllTarget: PauseAllTaskExecute (23570行)
//   paused = any(TaskPause); SetToggleAction(paused) → 反転が既定
//===========================================================================

TEST_CASE("DecidePauseAllTarget: いずれか停止中なら再開へ、無ければ停止へ")
{
	CHECK(DecidePauseAllTarget(false, _T("")) == true);
	CHECK(DecidePauseAllTarget(true, _T("")) == false);
	CHECK(DecidePauseAllTarget(false, _T("OFF")) == false);
	CHECK(DecidePauseAllTarget(true, _T("ON")) == true);
}

TEST_CASE("AnyPaused: 一覧に1つでも停止中があれば true")
{
	CHECK(AnyPaused({}) == false);
	CHECK(AnyPaused({false, false}) == false);
	CHECK(AnyPaused({false, true}) == true);
}

//===========================================================================
// Caption 分岐 (Update 実測)
//===========================================================================

TEST_CASE("PauseAllCaption: 停止中があれば「すべて再開」")
{
	CHECK(PauseAllCaption(0) != UnicodeString(_T("すべて再開")));
	CHECK(PauseAllCaption(0) == UnicodeString(_T("すべて一旦停止")));
	CHECK(PauseAllCaption(2) == UnicodeString(_T("すべて再開")));
}

TEST_CASE("SuspendCaption: 保留中なら「解除」")
{
	CHECK(SuspendCaption(false) == UnicodeString(_T("保留")));
	CHECK(SuspendCaption(true) == UnicodeString(_T("解除")));
}

TEST_CASE("CanCancelTasks: 実行中が1つでもあれば中断できる")
{
	CHECK(CanCancelTasks(0) == false);
	CHECK(CanCancelTasks(1) == true);
	CHECK(CanCancelTasks(8) == true);
}

//===========================================================================
// ValidateBackupPaths: BackupExecute (13645行)
//   IsCurFList/IsOppFList 必須、EqualDirLR (同一) は不可
//===========================================================================

TEST_CASE("ValidateBackupPaths: 空・同一は不可")
{
	UnicodeString err;
	CHECK(ValidateBackupPaths(_T("C:\\a"), _T("D:\\b"), err) == true);
	CHECK(ValidateBackupPaths(_T(""), _T("D:\\b"), err) == false);
	CHECK(ValidateBackupPaths(_T("C:\\a"), _T(""), err) == false);
	CHECK(ValidateBackupPaths(_T("C:\\a"), _T("C:\\a"), err) == false);
	CHECK(ValidateBackupPaths(_T("C:\\A"), _T("c:\\a"), err) == false);  // 大小無視
}

//===========================================================================
// CompressTargetIndices: CompressDirExecute (14987行)
//   選択ディレクトリ優先、無ければカーソル位置 (ディレクトリのみ)
//===========================================================================

TEST_CASE("CompressTargetIndices: 選択ディレクトリだけを拾う")
{
	// selected / is_dir の組
	CHECK(CompressTargetIndices({true, false}, {false, true}).empty());      // ファイル選択は対象外
	CHECK(CompressTargetIndices({true, true}, {true, false}) == std::vector<int>{0});
	CHECK(CompressTargetIndices({true, true}, {true, true}) == std::vector<int>{0, 1});
	CHECK(CompressTargetIndices({false, false}, {true, true}).empty());      // 選択無し→カーソル分岐
}

TEST_CASE("CursorCompressTarget: カーソル位置はディレクトリのみ")
{
	CHECK(CursorCompressTarget(true) == true);
	CHECK(CursorCompressTarget(false) == false);
}

//===========================================================================
// MatchFileNames: CompareDlg NC/CS分岐 (14464行)
//   cs_sw = TestActionParam("CS")。既定は大小無視
//===========================================================================

TEST_CASE("MatchFileNames: 既定は大小無視、CS指定で区別")
{
	CHECK(MatchFileNames(_T("Foo.txt"), _T("foo.txt"), false) == true);
	CHECK(MatchFileNames(_T("Foo.txt"), _T("foo.txt"), true) == false);
	CHECK(MatchFileNames(_T("a.txt"), _T("b.txt"), false) == false);
}

//===========================================================================
// CanFindHardLink: FindHardLinkExecute (18868行)
//===========================================================================

TEST_CASE("CanFindHardLink: 書庫/ADS/FTP/UNC不可、ディレクトリ不可、2以上必須")
{
	UnicodeString err;
	CHECK(CanFindHardLink(false, false, false, false, false, 3, err) == true);
	CHECK(CanFindHardLink(true, false, false, false, false, 3, err) == false);   // 書庫
	CHECK(CanFindHardLink(false, true, false, false, false, 3, err) == false);   // ADS
	CHECK(CanFindHardLink(false, false, true, false, false, 3, err) == false);   // FTP
	CHECK(CanFindHardLink(false, false, false, true, false, 3, err) == false);   // UNC
	CHECK(CanFindHardLink(false, false, false, false, true, 3, err) == false);   // dir
	CHECK(CanFindHardLink(false, false, false, false, false, 1, err) == false);  // 単一リンク
}

//===========================================================================
// PickOppHardLink: LinkToOppExecute (20622行)
//   反対側の一覧にあるものを優先、無ければ先頭
//===========================================================================

TEST_CASE("PickOppHardLink: 反対側にあるものを優先")
{
	UnicodeString cur = _T("C:\\a\\x.txt");
	std::vector<UnicodeString> cands = {_T("D:\\b\\x.txt"), _T("E:\\c\\x.txt")};
	std::vector<UnicodeString> opp = {_T("E:\\c\\x.txt")};
	CHECK(PickOppHardLink(cur, cands, opp) == UnicodeString(_T("E:\\c\\x.txt")));
	CHECK(PickOppHardLink(cur, cands, {}) == UnicodeString(_T("D:\\b\\x.txt")));
	CHECK(PickOppHardLink(cur, {cur}, {}) == UnicodeString(_T("")));  // 自分だけ
	CHECK(PickOppHardLink(cur, {}, {}) == UnicodeString(_T("")));
}

//===========================================================================
// ParseFolderIconParam: SetFolderIconExecute (25594行)
//===========================================================================

TEST_CASE("ParseFolderIconParam: RD/SD/RS/ND の分岐")
{
	CHECK(ParseFolderIconParam(_T("RD")) == FolderIconCmd::ClearDefault);
	CHECK(ParseFolderIconParam(_T("SD")) == FolderIconCmd::SelectDefault);
	CHECK(ParseFolderIconParam(_T("RS")) == FolderIconCmd::ResetToDefault);
	CHECK(ParseFolderIconParam(_T("ND")) == FolderIconCmd::Menu);
	CHECK(ParseFolderIconParam(_T("")) == FolderIconCmd::ChooseFile);
	CHECK(ParseFolderIconParam(_T("C:\\a.ico")) == FolderIconCmd::SetFile);
}

//===========================================================================
// ValidateFolderIconSearch: FindFolderIconExecute (18152行)
//===========================================================================

TEST_CASE("ValidateFolderIconSearch: アイコン未選択は不可")
{
	UnicodeString err;
	CHECK(ValidateFolderIconSearch(_T("C:\\a"), false, err) == true);
	CHECK(ValidateFolderIconSearch(_T("C:\\a"), true, err) == false);
}

//===========================================================================
// ResolveJumpTarget: JumpToExecute (20433行)
//   fnam = def_if_empty(ActionParam, GetCurFileName())。空は不可
//===========================================================================

TEST_CASE("ResolveJumpTarget: パラメータ優先、無ければカーソル、両方空は不可")
{
	UnicodeString err;
	CHECK(ResolveJumpTarget(_T("D:\\x.txt"), _T("C:\\a.txt"), _T("C:\\"), err)
	      == UnicodeString(_T("D:\\x.txt")));
	CHECK(ResolveJumpTarget(_T(""), _T("a.txt"), _T("C:\\"), err)
	      == UnicodeString(_T("C:\\a.txt")));
	CHECK(ResolveJumpTarget(_T(""), _T(""), _T("C:\\"), err) == UnicodeString(_T("")));
}

//===========================================================================
// ResolveDriveName: DriveGraphExecute (16832行)
//   パラメータあれば "X:" 形式、無ければカレントのドライブ
//===========================================================================

TEST_CASE("ResolveDriveName: パラメータ優先")
{
	CHECK(ResolveDriveName(_T("D"), _T("C:\\a")) == UnicodeString(_T("D:")));
	CHECK(ResolveDriveName(_T(""), _T("C:\\a")) == UnicodeString(_T("C:")));
	CHECK(ResolveDriveName(_T(""), _T("\\\\srv\\sh")) == UnicodeString(_T("")));
}

//===========================================================================
// FormatTaskSummary: TaskMan ダイアログの1行
//===========================================================================

TEST_CASE("FormatTaskSummary: 件数の表示")
{
	CHECK(FormatTaskSummary(0, 0) == UnicodeString(_T("実行中のタスクはありません")));
	CHECK(FormatTaskSummary(2, 1).Pos(_T("2")) > 0);
}
