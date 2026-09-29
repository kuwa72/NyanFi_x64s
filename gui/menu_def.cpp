/**
 * @file gui/menu_def.cpp
 * @brief メニューバーの定義表 (wx 非依存)
 */
#include "gui/menu_def.h"

namespace menu_def {

const std::vector<Item> &Items()
{
	// 全て実装済みコマンドのみ (E2E 済みか実ダイアログ持ち)。
	// 未実装コマンドは載せない。
	static const std::vector<Item> items = {
		{_T("ファイル"), _T("新規ファイル"), _T("NewFile")},
		{{}, _T("フォルダ作成"), _T("CreateDir")},
		{{}, {}, {}},
		{{}, _T("コピー"), _T("Copy")},
		{{}, _T("移動"), _T("Move")},
		{{}, _T("名前変更"), _T("RenameDlg")},
		{{}, _T("削除"), _T("Delete")},
		{{}, {}, {}},
		{{}, _T("プロパティ"), _T("PropertyDlg")},
		{{}, {}, {}},
		{{}, _T("終了"), _T("Exit")},
		{_T("編集"), _T("コピー"), _T("CopyToClip")},
		{{}, _T("切り取り"), _T("CutToClip")},
		{{}, _T("貼り付け"), _T("Paste")},
		{{}, {}, {}},
		{{}, _T("選択"), _T("Select")},
		{{}, _T("すべて選択"), _T("SelAllItem")},
		{{}, _T("選択解除"), _T("ClearAll")},
		{_T("表示"), _T("再読み込み"), _T("ReloadList")},
		{{}, _T("隠しファイル表示切替"), _T("ShowHideAtr")},
		{{}, {}, {}},
		{{}, _T("テキストビューア"), _T("TextViewer")},
		{{}, _T("画像ビューア"), _T("ImageViewer")},
		{{}, {}, {}},
		{{}, _T("並べ替え"), _T("SortDlg")},
		{_T("検索"), _T("インクリメンタルサーチ"), _T("IncSearch")},
		{{}, _T("Grep"), _T("Grep")},
		{_T("設定"), _T("登録フォルダ"), _T("RegDirDlg")},
		{_T("設定"), _T("キー一覧"), _T("KeyList")},
		{{}, _T("コマンド一覧"), _T("ShowCmdList")},
		{_T("ヘルプ"), _T("バージョン情報"), _T("AboutNyanFi")},
	};
	return items;
}

}  // namespace menu_def
