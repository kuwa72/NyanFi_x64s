/**
 * @file tests/core/test_gui_tabs.cpp
 * @brief gui/tabs.cpp (タブの追加・削除・切り替えと ini 永続化) の回帰テスト
 *
 * @details gui/ 配下のうち wx に依存しない部分だけをここでテストする
 * (`nyanfi_gui_core`、ルート CMakeLists.txt 参照)。VCL 版の tab_info
 * (src/Global.h、実測) が左右ペイン共有の1本のタブバーだったことに合わせた
 * 設計であることは gui/tabs.h 冒頭のコメントを参照。
 *
 * ini の読み書きは tests/temp_dir.h の TempDir が作る一時ディレクトリの
 * 中だけで行う。
 */
#include "doctest/doctest.h"

#include "gui/tabs.h"

#include "UIniFile.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

TabState MakeTab(const UnicodeString &dir0, const UnicodeString &dir1, SortKey key = SortKey::Name,
                  bool descending = false, bool dirs_first = true)
{
	TabState state;
	state.panes[0].directory = dir0;
	state.panes[0].sort_key = key;
	state.panes[0].sort_descending = descending;
	state.panes[0].dirs_first = dirs_first;
	state.panes[1].directory = dir1;
	return state;
}

}  // namespace

//===========================================================================
// 既定の状態
//===========================================================================

TEST_CASE("TabManager: 最初は1本のタブを持つ")
{
	TabManager tabs;
	CHECK(tabs.Count() == 1);
	CHECK(tabs.CurrentIndex() == 0);
	CHECK(tabs.Current().panes[0].directory.IsEmpty());
	CHECK(tabs.Current().panes[1].directory.IsEmpty());
}

//===========================================================================
// AddTab
//===========================================================================

TEST_CASE("AddTab: 末尾に追加し、追加したタブへ切り替える")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\B\\"));

	const int idx = tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\D\\")));

	CHECK(tabs.Count() == 2);
	CHECK(idx == 1);
	CHECK(tabs.CurrentIndex() == 1);
	CHECK(tabs.Current().panes[0].directory == UnicodeString(_T("C:\\C\\")));
	// 元のタブの内容は変わらず残っている
	CHECK(tabs.At(0).panes[0].directory == UnicodeString(_T("C:\\A\\")));
}

//===========================================================================
// CloseCurrentTab / CloseTabAt
//===========================================================================

TEST_CASE("CloseCurrentTab: 最後の1枚は閉じられない")
{
	TabManager tabs;
	CHECK_FALSE(tabs.CloseCurrentTab());
	CHECK(tabs.Count() == 1);
}

TEST_CASE("CloseCurrentTab: 2枚目を閉じると1枚目に戻る")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\A\\"));
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));  // カレントは idx=1

	CHECK(tabs.CloseCurrentTab());
	CHECK(tabs.Count() == 1);
	CHECK(tabs.CurrentIndex() == 0);
	CHECK(tabs.Current().panes[0].directory == UnicodeString(_T("C:\\A\\")));
}

TEST_CASE("CloseCurrentTab: 末尾のタブを閉じると1つ手前へ移る (VCL 版 UpdateTabBar(idx) と同じ)")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\A\\"));
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\C\\")));  // カレントは idx=2 (末尾)

	CHECK(tabs.CloseCurrentTab());
	CHECK(tabs.Count() == 2);
	CHECK(tabs.CurrentIndex() == 1);  // 末尾 (元の idx=2) が消えたので新しい末尾 (idx=1) へ
	CHECK(tabs.Current().panes[0].directory == UnicodeString(_T("C:\\B\\")));
}

TEST_CASE("CloseTabAt: 表示中でないタブを閉じても表示中のタブは変わらない (index が現在より後ろ)")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\A\\"));
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\C\\")));
	CHECK(tabs.SelectAt(0));  // 表示中を idx=0 (C:\A) に戻す

	CHECK(tabs.CloseTabAt(2));  // 背後の idx=2 (C:\C) を閉じる
	CHECK(tabs.Count() == 2);
	CHECK(tabs.CurrentIndex() == 0);  // 表示中のタブは変わらない
	CHECK(tabs.Current().panes[0].directory == UnicodeString(_T("C:\\A\\")));
}

TEST_CASE("CloseTabAt: 表示中より前のタブを閉じると添字だけ詰める (表示中の内容は変わらない)")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\A\\"));
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\C\\")));  // 表示中は idx=2 (C:\C)

	CHECK(tabs.CloseTabAt(0));  // 手前の idx=0 (C:\A) を閉じる
	CHECK(tabs.Count() == 2);
	CHECK(tabs.CurrentIndex() == 1);  // 1つ詰まった
	CHECK(tabs.Current().panes[0].directory == UnicodeString(_T("C:\\C\\")));  // 内容は表示中のまま
}

TEST_CASE("CloseTabAt: 範囲外の index は何もせず false")
{
	TabManager tabs;
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	CHECK_FALSE(tabs.CloseTabAt(-1));
	CHECK_FALSE(tabs.CloseTabAt(2));
	CHECK(tabs.Count() == 2);
}

//===========================================================================
// NextTab / PrevTab / SelectAt
//===========================================================================

TEST_CASE("NextTab/PrevTab: タブが1枚なら何もしない")
{
	TabManager tabs;
	tabs.NextTab();
	CHECK(tabs.CurrentIndex() == 0);
	tabs.PrevTab();
	CHECK(tabs.CurrentIndex() == 0);
}

TEST_CASE("NextTab: 末尾から先頭へ周回する")
{
	TabManager tabs;
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\C\\")));  // idx=2
	CHECK(tabs.CurrentIndex() == 2);

	tabs.NextTab();
	CHECK(tabs.CurrentIndex() == 0);  // 周回
}

TEST_CASE("PrevTab: 先頭から末尾へ周回する")
{
	TabManager tabs;
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	tabs.AddTab(MakeTab(_T("C:\\C\\"), _T("C:\\C\\")));
	CHECK(tabs.SelectAt(0));

	tabs.PrevTab();
	CHECK(tabs.CurrentIndex() == 2);  // 周回
}

TEST_CASE("SelectAt: 範囲外なら何もせず false")
{
	TabManager tabs;
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));
	CHECK_FALSE(tabs.SelectAt(-1));
	CHECK_FALSE(tabs.SelectAt(2));
	CHECK(tabs.CurrentIndex() == 1);  // 変わらない
}

//===========================================================================
// CaptionAt / Captions
//===========================================================================

TEST_CASE("CaptionAt: ディレクトリの末尾要素名を使う")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\work\\project\\"), _T("C:\\B\\"));
	CHECK(tabs.CaptionAt(0) == UnicodeString(_T("project")));
}

TEST_CASE("CaptionAt: ディレクトリが空なら「(無題)」")
{
	TabManager tabs;
	CHECK(tabs.CaptionAt(0) == UnicodeString(_T("(無題)")));
}

TEST_CASE("CaptionAt: ドライブのルートは末尾要素名が取れないためパスそのものを使う")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\"), _T("C:\\"));
	CHECK(tabs.CaptionAt(0) == UnicodeString(_T("C:\\")));
}

TEST_CASE("Captions: 全タブ分を順番に返す")
{
	TabManager tabs;
	tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("C:\\A\\"));
	tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("C:\\B\\")));

	const std::vector<UnicodeString> caps = tabs.Captions();
	REQUIRE(caps.size() == 2);
	CHECK(caps[0] == UnicodeString(_T("A")));
	CHECK(caps[1] == UnicodeString(_T("B")));
}

//===========================================================================
// SaveToIni / LoadFromIni
//===========================================================================

TEST_CASE("SaveToIni/LoadFromIni: 別インスタンスで読み直すと同じ内容になる")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi_wx.ini"));

	{
		TabManager tabs;
		tabs.MutableCurrent() = MakeTab(_T("C:\\A\\"), _T("D:\\A\\"), SortKey::Size, true, false);
		tabs.AddTab(MakeTab(_T("C:\\B\\"), _T("D:\\B\\"), SortKey::Ext, false, true));
		CHECK(tabs.SelectAt(0));

		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		tabs.SaveToIni(*ini);
		CHECK(ini->UpdateFile());
	}

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	TabManager reloaded;
	reloaded.LoadFromIni(*ini);

	CHECK(reloaded.Count() == 2);
	CHECK(reloaded.CurrentIndex() == 0);
	CHECK(reloaded.At(0).panes[0].directory == UnicodeString(_T("C:\\A\\")));
	CHECK(reloaded.At(0).panes[1].directory == UnicodeString(_T("D:\\A\\")));
	CHECK(reloaded.At(0).panes[0].sort_key == SortKey::Size);
	CHECK(reloaded.At(0).panes[0].sort_descending);
	CHECK_FALSE(reloaded.At(0).panes[0].dirs_first);

	CHECK(reloaded.At(1).panes[0].directory == UnicodeString(_T("C:\\B\\")));
	CHECK(reloaded.At(1).panes[0].sort_key == SortKey::Ext);
	CHECK_FALSE(reloaded.At(1).panes[0].sort_descending);
	CHECK(reloaded.At(1).panes[0].dirs_first);
}

TEST_CASE("LoadFromIni: セクションが無い場合は何もしない (既定の1タブのまま)")
{
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("does_not_exist.ini"));

	std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
	TabManager tabs;
	tabs.LoadFromIni(*ini);

	CHECK(tabs.Count() == 1);
	CHECK(tabs.CurrentIndex() == 0);
}

TEST_CASE("SaveToIni: このクラスのセクション以外を書き換えない")
{
	// UsrIniFile::UpdateFile はコンストラクタ時に読み込んだ全セクションを
	// まるごと書き戻す実装 (src/UIniFile.cpp) なので、他セクションの値が
	// Save 後も保たれることを確認する (gui/settings.h の Settings と同じ観点)
	TempDir dir;
	const UnicodeString ini_path = dir.file(_T("nyanfi_wx.ini"));

	{
		std::unique_ptr<TStringList> buf(new TStringList());
		buf->Text = _T("[General]\r\nSomeOtherKey=SomeOtherValue\r\n");
		buf->SaveToFile(ini_path);
	}

	{
		std::unique_ptr<UsrIniFile> ini(new UsrIniFile(ini_path));
		TabManager tabs;
		tabs.SaveToIni(*ini);
		CHECK(ini->UpdateFile());
	}

	std::unique_ptr<UsrIniFile> check(new UsrIniFile(ini_path));
	CHECK(check->ReadString(_T("General"), _T("SomeOtherKey")) == UnicodeString(_T("SomeOtherValue")));
}
