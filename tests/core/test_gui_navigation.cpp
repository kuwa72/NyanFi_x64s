/**
 * @file tests/core/test_gui_navigation.cpp
 * @brief gui/navigation.cpp (インクリメンタルサーチの照合・ディレクトリ履歴・
 *        ドライブ種別表示名・パス直接入力の解決) の回帰テスト
 *
 * @details gui/navigation.h/.cpp は wx に依存しない (nyanfi_gui_core、
 * ルート CMakeLists.txt 参照) ため、GUI (wxWidgets) 無しでもここでテストできる。
 * ファイルシステムに触れるテストは tests/temp_dir.h の TempDir が作る
 * 一時ディレクトリの中だけで行う。
 */
#include "doctest/doctest.h"

#include <vector>

#include "gui/navigation.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;

//===========================================================================
// IncrementalSearch: サーチモードの状態遷移
//===========================================================================

TEST_CASE("IncrementalSearch: Start 直後はキーワードが空でアクティブ")
{
	IncrementalSearch is;
	CHECK_FALSE(is.IsActive());

	is.Start();
	CHECK(is.IsActive());
	CHECK(is.Word().IsEmpty());
}

TEST_CASE("IncrementalSearch: Append で末尾に1文字ずつ足す")
{
	IncrementalSearch is;
	is.Start();
	is.Append(L'a');
	is.Append(L'b');
	is.Append(L'c');
	CHECK(is.Word() == UnicodeString(_T("abc")));
}

TEST_CASE("IncrementalSearch: Backspace は末尾を1文字削り、空なら false を返す")
{
	IncrementalSearch is;
	is.Start();
	is.Append(L'a');
	is.Append(L'b');

	CHECK(is.Backspace());
	CHECK(is.Word() == UnicodeString(_T("a")));
	CHECK(is.Backspace());
	CHECK(is.Word().IsEmpty());
	CHECK_FALSE(is.Backspace());  // 既に空
}

TEST_CASE("IncrementalSearch: Exit はアクティブ状態とキーワードを両方クリアする")
{
	IncrementalSearch is;
	is.Start();
	is.Append(L'x');
	is.Exit();

	CHECK_FALSE(is.IsActive());
	CHECK(is.Word().IsEmpty());
}

//===========================================================================
// IncrementalSearchMatch: 名前とキーワードの照合 (移植済み contains_word_and_or を使う)
//===========================================================================

TEST_CASE("IncrementalSearchMatch: 空のキーワードは常に不一致")
{
	CHECK_FALSE(IncrementalSearchMatch(_T("readme.txt"), EmptyStr));
}

TEST_CASE("IncrementalSearchMatch: 部分一致・大小文字を区別しない")
{
	CHECK(IncrementalSearchMatch(_T("Readme.txt"), _T("read")));
	CHECK(IncrementalSearchMatch(_T("Readme.txt"), _T("README")));
	CHECK_FALSE(IncrementalSearchMatch(_T("Readme.txt"), _T("xyz")));
}

TEST_CASE("IncrementalSearchMatch: 空白区切りは AND")
{
	CHECK(IncrementalSearchMatch(_T("nyanfi_report.pdf"), _T("nyanfi report")));
	CHECK_FALSE(IncrementalSearchMatch(_T("nyanfi_report.pdf"), _T("nyanfi missing")));
}

TEST_CASE("IncrementalSearchMatch: | 区切りは OR")
{
	CHECK(IncrementalSearchMatch(_T("a.txt"), _T("a|b")));
	CHECK(IncrementalSearchMatch(_T("b.txt"), _T("a|b")));
	CHECK_FALSE(IncrementalSearchMatch(_T("c.txt"), _T("a|b")));
}

//===========================================================================
// FindIncrementalSearchMatch: 周回探索
//===========================================================================

TEST_CASE("FindIncrementalSearchMatch: 空リスト/空キーワードは常に -1")
{
	std::vector<UnicodeString> empty;
	CHECK(FindIncrementalSearchMatch(empty, _T("a"), -1, true) == -1);

	std::vector<UnicodeString> names{"apple", "banana"};
	CHECK(FindIncrementalSearchMatch(names, EmptyStr, -1, true) == -1);
}

TEST_CASE("FindIncrementalSearchMatch: 前方探索は start_index の次から始める")
{
	// "vo" を含むのは avocado (index 2) だけ。banana ではなく avocado が
	// 見つかることを確認する (部分一致なので "banana" 自体は対象にしない)
	std::vector<UnicodeString> names{"apple", "banana", "avocado", "cherry"};
	CHECK(FindIncrementalSearchMatch(names, _T("vo"), 0, true) == 2);
}

TEST_CASE("FindIncrementalSearchMatch: 一致が無ければ1周して -1")
{
	std::vector<UnicodeString> names{"apple", "banana"};
	CHECK(FindIncrementalSearchMatch(names, _T("zzz"), 0, true) == -1);
}

TEST_CASE("FindIncrementalSearchMatch: 前方探索は末尾から先頭へ周回する")
{
	// "app" を含むのは apple (index 0) だけ
	std::vector<UnicodeString> names{"apple", "banana", "avocado"};
	CHECK(FindIncrementalSearchMatch(names, _T("app"), 2, true) == 0);
}

TEST_CASE("FindIncrementalSearchMatch: 後方探索は先頭から末尾へ周回する")
{
	// "avo" を含むのは avocado (index 2) だけ
	std::vector<UnicodeString> names{"apple", "banana", "avocado"};
	CHECK(FindIncrementalSearchMatch(names, _T("avo"), 0, false) == 2);
}

TEST_CASE("FindIncrementalSearchMatch: start_index が範囲外 (-1) でも動く")
{
	// "app" を含むのは apple (index 0) だけ
	std::vector<UnicodeString> names{"apple", "banana"};
	CHECK(FindIncrementalSearchMatch(names, _T("app"), -1, true) == 0);
}

//===========================================================================
// DirHistory: 戻る/進む/一覧
//===========================================================================

TEST_CASE("DirHistory: 初期状態は戻る/進むともにできない")
{
	DirHistory h;
	CHECK_FALSE(h.CanBack());
	CHECK_FALSE(h.CanForward());
	CHECK(h.CurrentIndex() == -1);
}

TEST_CASE("DirHistory: Navigate で履歴が積まれ、戻る/進むで行き来できる")
{
	DirHistory h;
	h.Navigate(_T("C:\\A\\"));
	h.Navigate(_T("C:\\B\\"));
	h.Navigate(_T("C:\\C\\"));

	CHECK(h.CanBack());
	CHECK_FALSE(h.CanForward());

	CHECK(h.Back() == UnicodeString(_T("C:\\B\\")));
	CHECK(h.CanForward());
	CHECK(h.Back() == UnicodeString(_T("C:\\A\\")));
	CHECK_FALSE(h.CanBack());

	CHECK(h.Forward() == UnicodeString(_T("C:\\B\\")));
	CHECK(h.Forward() == UnicodeString(_T("C:\\C\\")));
	CHECK_FALSE(h.CanForward());
}

TEST_CASE("DirHistory: これ以上戻る/進むができない場合は空文字列を返し状態を変えない")
{
	DirHistory h;
	h.Navigate(_T("C:\\A\\"));

	CHECK(h.Back().IsEmpty());
	CHECK(h.CurrentIndex() == 0);
	CHECK(h.Forward().IsEmpty());
	CHECK(h.CurrentIndex() == 0);
}

TEST_CASE("DirHistory: 直前と同じディレクトリへの Navigate は重複追加しない")
{
	DirHistory h;
	h.Navigate(_T("C:\\A\\"));
	h.Navigate(_T("C:\\A\\"));  // Reload 等での重複
	h.Navigate(_T("C:\\B\\"));

	CHECK(h.Entries().size() == 2);
}

TEST_CASE("DirHistory: 履歴の途中から新しいディレクトリへ移動すると、その先の進む履歴を捨てる")
{
	DirHistory h;
	h.Navigate(_T("C:\\A\\"));
	h.Navigate(_T("C:\\B\\"));
	h.Navigate(_T("C:\\C\\"));

	h.Back();  // C:\B へ
	h.Navigate(_T("C:\\D\\"));  // ここで C:\C への「進む」履歴が捨てられる

	CHECK_FALSE(h.CanForward());
	CHECK(h.Entries().size() == 3);  // A, B, D
	CHECK(h.Entries()[2] == UnicodeString(_T("C:\\D\\")));
}

TEST_CASE("DirHistory: 上限件数を超えたら古いものから捨てる")
{
	DirHistory h(3);  // 上限3件
	h.Navigate(_T("C:\\A\\"));
	h.Navigate(_T("C:\\B\\"));
	h.Navigate(_T("C:\\C\\"));
	h.Navigate(_T("C:\\D\\"));  // A が捨てられる

	CHECK(h.Entries().size() == 3);
	CHECK(h.Entries()[0] == UnicodeString(_T("C:\\B\\")));
	CHECK(h.Entries()[2] == UnicodeString(_T("C:\\D\\")));
	CHECK(h.CurrentIndex() == 2);
}

TEST_CASE("DirHistory: JumpTo で一覧の任意の位置へ直接移動できる")
{
	DirHistory h;
	h.Navigate(_T("C:\\A\\"));
	h.Navigate(_T("C:\\B\\"));
	h.Navigate(_T("C:\\C\\"));

	CHECK(h.JumpTo(0) == UnicodeString(_T("C:\\A\\")));
	CHECK(h.CurrentIndex() == 0);
	CHECK(h.JumpTo(-1).IsEmpty());
	CHECK(h.JumpTo(99).IsEmpty());
}

//===========================================================================
// DriveTypeLabel
//===========================================================================

TEST_CASE("DriveTypeLabel: 既知の種別は Global.cpp と同じ文言を返す")
{
	CHECK(DriveTypeLabel(DRIVE_FIXED) == UnicodeString(_T("ハードディスク")));
	CHECK(DriveTypeLabel(DRIVE_REMOVABLE) == UnicodeString(_T("リムーバブル・メディア")));
	CHECK(DriveTypeLabel(DRIVE_REMOTE) == UnicodeString(_T("ネットワーク・ドライブ")));
	CHECK(DriveTypeLabel(DRIVE_CDROM) == UnicodeString(_T("CD-ROMドライブ")));
	CHECK(DriveTypeLabel(DRIVE_RAMDISK) == UnicodeString(_T("RAMディスク")));
}

TEST_CASE("DriveTypeLabel: 不明な種別は空文字列")
{
	CHECK(DriveTypeLabel(DRIVE_UNKNOWN).IsEmpty());
	CHECK(DriveTypeLabel(DRIVE_NO_ROOT_DIR).IsEmpty());
}

//===========================================================================
// ResolveDirectoryInput: パス直接入力の解決
//===========================================================================

TEST_CASE("ResolveDirectoryInput: 空文字列は解決できない")
{
	UnicodeString resolved;
	CHECK_FALSE(ResolveDirectoryInput(EmptyStr, _T("C:\\"), resolved));
	CHECK_FALSE(ResolveDirectoryInput(_T("   "), _T("C:\\"), resolved));
}

TEST_CASE("ResolveDirectoryInput: 実在しないディレクトリは解決できない")
{
	TempDir dir;
	UnicodeString resolved;
	CHECK_FALSE(ResolveDirectoryInput(dir.file(_T("no_such_subdir")), _T("C:\\"), resolved));
}

TEST_CASE("ResolveDirectoryInput: 絶対パスで実在するディレクトリを解決できる")
{
	TempDir dir;
	UnicodeString resolved;
	CHECK(ResolveDirectoryInput(ExcludeTrailingPathDelimiter(dir.path), _T("C:\\"), resolved));
	CHECK(SameText(resolved, dir.path));
}

TEST_CASE("ResolveDirectoryInput: 環境変数を展開して解決する (cv_env_var 経由の get_actual_path)")
{
	TempDir dir;
	const UnicodeString varname = _T("NYANFI_TEST_NAV_DIR");
	::SetEnvironmentVariableW(varname.c_str(), ExcludeTrailingPathDelimiter(dir.path).c_str());

	UnicodeString resolved;
	const bool ok = ResolveDirectoryInput(_T("%") + varname + _T("%"), _T("C:\\"), resolved);

	::SetEnvironmentVariableW(varname.c_str(), NULL);  // 後始末

	CHECK(ok);
	CHECK(SameText(resolved, dir.path));
}

TEST_CASE("ResolveDirectoryInput: 相対パス (..\\) を基準ディレクトリから解決する (to_absolute_name)")
{
	TempDir dir;
	const UnicodeString sub = dir.file(_T("sub"));
	::CreateDirectoryW(sub.c_str(), NULL);

	UnicodeString resolved;
	// to_absolute_name は ".." 単体ではなく "..\" (末尾の区切り込み) をトークンとして
	// 認識する実装 (usr_file_ex.cpp を実測)
	CHECK(ResolveDirectoryInput(_T("..\\"), IncludeTrailingPathDelimiter(sub), resolved));
	CHECK(SameText(resolved, dir.path));
}
