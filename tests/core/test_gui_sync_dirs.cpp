/**
 * @file tests/core/test_gui_sync_dirs.cpp
 * @brief gui/sync_dirs.cpp (同期コピー設定の判断ロジック) のテスト
 *
 * @details VCL の該当実装は `src/SyncDlg.cpp` (`TRegSyncDlg`) と
 *          `src/Global.cpp` (`get_SyncDirList` / `has_SyncDir` / `is_SyncDir`)。
 *          同期設定一覧 (`SyncDirList`) の1行は
 *          `"タイトル","有効:1/無効:0","オプション(O,D)","dir1","dir2",...`
 *          (`SyncDlg.cpp::OkButtonClick` の正規化コメントを実測)。
 *            - 有効/無効 (`[1]`) はチェック状態 (`FormShow` で `equal_0` を
 *              見て `Checked` にするのを実測)。新規追加は無効 (`MakeRegItem`
 *              の `idx==-1` 分岐を実測)
 *            - オプション (`[2]`) は上書き `O`・同期削除 `D` の組み合わせ
 *              (`MakeRegItem` の `opt +=` を実測)
 *            - 追加はディレクトリが2件以上ないと不可 (`AddRegActionUpdate`
 *              の `DirListBox->Count>=2` を実測)
 *            - 確定時に5項目未満 (dir が2件未満) の不正行は落とし、dir には
 *              末尾区切りを付けて正規化する (`OkButtonClick` を実測)
 *            - 対象解決は `get_SyncDirList` を実測: 登録 dir の配下なら
 *              サブディレクトリ部分を付け替えた対応先を返す。無効行・
 *              対応先なし行は飛ばす。`dir_exists` は FS 依存のため純粋部では
 *              候補を返し、存在確認は呼び出し側が行う (未実装扱いではなく
 *              責務分離)
 */
#include "doctest/doctest.h"

#include "gui/sync_dirs.h"

namespace {

sync_dirs::SyncEntry entry_of(const wchar_t *title, bool enabled, bool owr, bool del,
                              std::initializer_list<const wchar_t *> dirs)
{
	sync_dirs::SyncEntry e;
	e.title = title;
	e.enabled = enabled;
	e.overwrite = owr;
	e.sync_delete = del;
	for (const wchar_t *d : dirs) e.dirs.emplace_back(d);
	return e;
}

}  // namespace

//===========================================================================
// ParseRecord / FormatRecord
//===========================================================================

TEST_CASE("SyncDirs: CSVレコードを往復できる")
{
	const sync_dirs::SyncEntry e =
		entry_of(L"作業", true, true, false, {L"C:\\A\\", L"D:\\B\\"});
	const sync_dirs::SyncEntry back = sync_dirs::ParseRecord(sync_dirs::FormatRecord(e));
	CHECK(back.title == UnicodeString(_T("作業")));
	CHECK(back.enabled);
	CHECK(back.overwrite);
	CHECK_FALSE(back.sync_delete);
	REQUIRE(back.dirs.size() == 2);
	CHECK(back.dirs[0] == UnicodeString(_T("C:\\A\\")));
	CHECK(back.dirs[1] == UnicodeString(_T("D:\\B\\")));
}

TEST_CASE("SyncDirs: 無効行とDオプションを読める")
{
	const sync_dirs::SyncEntry back =
		sync_dirs::ParseRecord(UnicodeString(_T("\"資料\",\"0\",\"D\",\"C:\\A\",\"D:\\B\"")));
	CHECK(back.title == UnicodeString(_T("資料")));
	CHECK_FALSE(back.enabled);
	CHECK_FALSE(back.overwrite);
	CHECK(back.sync_delete);
	REQUIRE(back.dirs.size() == 2);
}

TEST_CASE("SyncDirs: 新規項目の既定名は登録Nである")
{
	CHECK(sync_dirs::DefaultTitle(0) == UnicodeString(_T("登録1")));
	CHECK(sync_dirs::DefaultTitle(2) == UnicodeString(_T("登録3")));
}

//===========================================================================
// IsValid / CanAdd / NormalizeRecord (OkButtonClick / AddRegActionUpdate を実測)
//===========================================================================

TEST_CASE("SyncDirs: dirが2件未満は不正")
{
	CHECK_FALSE(sync_dirs::IsValid(entry_of(L"x", true, false, false, {})));
	CHECK_FALSE(sync_dirs::IsValid(entry_of(L"x", true, false, false, {L"C:\\A\\"})));
	CHECK(sync_dirs::IsValid(entry_of(L"x", true, false, false, {L"C:\\A\\", L"D:\\B\\"})));
	CHECK_FALSE(sync_dirs::CanAdd(1));
	CHECK(sync_dirs::CanAdd(2));
}

TEST_CASE("SyncDirs: 確定時に不正行は空になりdirは末尾区切りで正規化される")
{
	CHECK(sync_dirs::NormalizeRecord(UnicodeString(_T("\"x\",\"1\",\"\",\"C:\\A\""))).IsEmpty());
	const UnicodeString rec = sync_dirs::NormalizeRecord(
		UnicodeString(_T("\"作業\",\"1\",\"O\",\"C:\\A\",\"D:\\B\"")));
	CHECK_FALSE(rec.IsEmpty());
	const sync_dirs::SyncEntry back = sync_dirs::ParseRecord(rec);
	REQUIRE(back.dirs.size() == 2);
	CHECK(back.dirs[0] == UnicodeString(_T("C:\\A\\")));
	CHECK(back.dirs[1] == UnicodeString(_T("D:\\B\\")));
	CHECK(back.overwrite);
}

//===========================================================================
// ResolveTargets (get_SyncDirList を実測)
//===========================================================================

TEST_CASE("SyncDirs: 登録配下なら対応先とオプションを返す")
{
	const std::vector<sync_dirs::SyncEntry> v = {
		entry_of(L"作業", true, true, false, {L"C:\\A\\", L"D:\\B\\"}),
	};
	const sync_dirs::Resolved r =
		sync_dirs::ResolveTargets(UnicodeString(_T("C:\\A\\sub\\")), v, false);
	CHECK(r.option == UnicodeString(_T("O")));
	REQUIRE(r.targets.size() == 2);
	CHECK(r.targets[0] == UnicodeString(_T("C:\\A\\sub\\")));
	CHECK(r.targets[1] == UnicodeString(_T("D:\\B\\sub\\")));
}

TEST_CASE("SyncDirs: 無効行は飛ばされ未登録なら空")
{
	const std::vector<sync_dirs::SyncEntry> v = {
		entry_of(L"無効", false, true, false, {L"C:\\A\\", L"D:\\B\\"}),
		entry_of(L"作業", true, false, false, {L"C:\\A\\", L"D:\\B\\"}),
	};
	const sync_dirs::Resolved r =
		sync_dirs::ResolveTargets(UnicodeString(_T("C:\\A\\")), v, false);
	CHECK(r.option == UnicodeString(_T("")));
	REQUIRE(r.targets.size() == 2);
	CHECK(r.targets[1] == UnicodeString(_T("D:\\B\\")));

	const sync_dirs::Resolved none =
		sync_dirs::ResolveTargets(UnicodeString(_T("E:\\X\\")), v, false);
	CHECK(none.targets.size() == 1);
	CHECK(none.option.IsEmpty());
}
