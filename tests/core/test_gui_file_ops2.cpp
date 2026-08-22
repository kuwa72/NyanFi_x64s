/**
 * @file tests/core/test_gui_file_ops2.cpp
 * @brief gui/clone_name.cpp と gui/file_ops2.cpp のテスト
 *
 * どちらも VCL の実装 (src/Global.cpp の format_CloneName、src/MainFrm.cpp の
 * SwapName / UndoRename) を書き写したものなので、書式と手順を固定する。
 *
 * **破壊的な経路なので、失敗したときに元へ戻ることを必ず見る。**
 * 一括リネームで「2段目に失敗するとファイルが一時名のまま残る」不具合を
 * 実際に作っている (報告書 §12)。
 */
#include "doctest/doctest.h"

#include "gui/clone_name.h"
#include "gui/file_ops2.h"
#include "temp_dir.h"
#include "usr_file_ex.h"

using nyanfi_test::TempDir;

namespace {

void mkfile(const UnicodeString &path, const std::string &body = std::string())
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                         FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	if (!body.empty()) {
		DWORD written = 0;
		::WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, NULL);
	}
	::CloseHandle(h);
}

std::string read_all(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return std::string();
	std::string out;
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

Int64 size_of(const UnicodeString &path)
{
	WIN32_FILE_ATTRIBUTE_DATA fa;
	if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa)) return -1;
	LARGE_INTEGER li;
	li.HighPart = static_cast<LONG>(fa.nFileSizeHigh);
	li.LowPart = fa.nFileSizeLow;
	return li.QuadPart;
}

/// 「どの名前も空いている」と答えるもの (Expand の結果だけを見るとき用)
bool never_taken(const UnicodeString &) { return false; }

}  // namespace

//===========================================================================
// clone_name::Expand
//===========================================================================

TEST_CASE("Expand: 既定の書式は 名前_連番")
{
	CHECK(clone_name::Expand(EmptyStr, _T("memo"), 0, Now(), Now()) == UnicodeString(_T("memo_1")));
	CHECK(clone_name::Expand(EmptyStr, _T("memo"), 2, Now(), Now()) == UnicodeString(_T("memo_3")));
}

TEST_CASE("Expand: \\SN(n) は n の桁数でゼロ詰めする")
{
	// "001" は3桁なので %03u。1 + seq が入る
	CHECK(clone_name::Expand(_T("\\N-\\SN(001)"), _T("a"), 0, Now(), Now())
	      == UnicodeString(_T("a-001")));
	CHECK(clone_name::Expand(_T("\\N-\\SN(001)"), _T("a"), 11, Now(), Now())
	      == UnicodeString(_T("a-012")));
}

TEST_CASE("Expand: \\SN() は 1 と同じ")
{
	CHECK(clone_name::Expand(_T("\\N\\SN()"), _T("a"), 0, Now(), Now()) == UnicodeString(_T("a1")));
}

TEST_CASE("Expand: \\TS(...) は元のタイムスタンプ、\\DT(...) は現在時刻")
{
	const TDateTime stamp = EncodeDate(2024, 3, 9);
	const TDateTime now = EncodeDate(2030, 12, 31);

	CHECK(clone_name::Expand(_T("\\N_\\TS(yyyymmdd)"), _T("a"), 0, stamp, now)
	      == UnicodeString(_T("a_20240309")));
	CHECK(clone_name::Expand(_T("\\N_\\DT(yyyymmdd)"), _T("a"), 0, stamp, now)
	      == UnicodeString(_T("a_20301231")));
}

TEST_CASE("Expand: \\- は1回目だけ後ろを捨てる (連番なしで先に試すため)")
{
	// 1回目は "a"、2回目以降は "a_2" のように連番が付く
	CHECK(clone_name::Expand(_T("\\N\\-_\\SN(1)"), _T("a"), 0, Now(), Now()) == UnicodeString(_T("a")));
	CHECK(clone_name::Expand(_T("\\N\\-_\\SN(1)"), _T("a"), 1, Now(), Now()) == UnicodeString(_T("a_2")));
}

TEST_CASE("Expand: 知らない \\x は何も出さずに読み飛ばす")
{
	CHECK(clone_name::Expand(_T("\\N\\Q!"), _T("a"), 0, Now(), Now()) == UnicodeString(_T("a!")));
}

//===========================================================================
// clone_name::MakeUnique
//===========================================================================

TEST_CASE("MakeUnique: 空いている名前まで連番を進める")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("memo.txt");
	mkfile(src);
	mkfile(tmp.path + _T("memo_1.txt"));
	mkfile(tmp.path + _T("memo_2.txt"));

	const std::function<bool(const UnicodeString &)> taken = [](const UnicodeString &p) {
		return file_exists(p) || dir_exists(p);
	};
	const UnicodeString got = clone_name::MakeUnique(EmptyStr, src, tmp.path, false, taken);
	CHECK(got == UnicodeString(tmp.path + _T("memo_3.txt")));
}

TEST_CASE("MakeUnique: ファイルは元の拡張子が残る")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("photo.jpeg");
	mkfile(src);

	const UnicodeString got = clone_name::MakeUnique(EmptyStr, src, tmp.path, false, never_taken);
	CHECK(got == UnicodeString(tmp.path + _T("photo_1.jpeg")));
}

TEST_CASE("MakeUnique: ディレクトリは名前全体が主部で拡張子を付けない")
{
	TempDir tmp;
	// "v1.2" のようにドットを含むディレクトリ名でも切らない
	const UnicodeString src = tmp.path + _T("v1.2");
	::CreateDirectoryW(src.c_str(), NULL);

	const UnicodeString got = clone_name::MakeUnique(EmptyStr, src, tmp.path, true, never_taken);
	CHECK(got == UnicodeString(tmp.path + _T("v1.2_1")));
}

TEST_CASE("MakeUnique: 上限まで埋まっていたら空を返す (暴走させない)")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("a.txt");
	mkfile(src);

	// 常に「使用済み」と答えると、上限まで回って空が返る
	const std::function<bool(const UnicodeString &)> always = [](const UnicodeString &) { return true; };
	const UnicodeString got = clone_name::MakeUnique(EmptyStr, src, tmp.path, false, always, 5);
	CHECK(got.IsEmpty());
}

//===========================================================================
// CloneItems
//===========================================================================

TEST_CASE("CloneItems: 中身ごと複製して連番の名前にする")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("memo.txt");
	mkfile(src, "hello");

	std::vector<UnicodeString> paths;
	paths.push_back(src);
	const file_ops::FileOpResult r = file_ops2::CloneItems(paths, tmp.path, EmptyStr);

	CHECK(r.success_count == 1);
	CHECK(file_exists(tmp.path + _T("memo_1.txt")));
	CHECK(read_all(tmp.path + _T("memo_1.txt")) == "hello");
	CHECK(file_exists(src));  // 元は残る
}

TEST_CASE("CloneItems: 同じ呼び出しで2件クローンしても名前が衝突しない")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	std::vector<UnicodeString> paths;
	paths.push_back(a);
	paths.push_back(b);
	const file_ops::FileOpResult r = file_ops2::CloneItems(paths, tmp.path, EmptyStr);

	CHECK(r.success_count == 2);
	CHECK(file_exists(tmp.path + _T("a_1.txt")));
	CHECK(file_exists(tmp.path + _T("b_1.txt")));
}

TEST_CASE("CloneItems: 同じ元を2回渡しても別々の名前になる")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	mkfile(a);

	std::vector<UnicodeString> paths;
	paths.push_back(a);
	paths.push_back(a);
	const file_ops::FileOpResult r = file_ops2::CloneItems(paths, tmp.path, EmptyStr);

	CHECK(r.success_count == 2);
	CHECK(file_exists(tmp.path + _T("a_1.txt")));
	CHECK(file_exists(tmp.path + _T("a_2.txt")));
}

//===========================================================================
// CopyDirStructure
//===========================================================================

TEST_CASE("CopyDirStructure: ディレクトリだけを作り、ファイルは作らない")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("src");
	const UnicodeString dst = tmp.path + _T("dst");
	::CreateDirectoryW(src.c_str(), NULL);
	::CreateDirectoryW((src + _T("\\sub")).c_str(), NULL);
	mkfile(src + _T("\\a.txt"));
	mkfile(src + _T("\\sub\\b.txt"));
	::CreateDirectoryW(dst.c_str(), NULL);

	std::vector<UnicodeString> dirs;
	dirs.push_back(src);
	const file_ops::FileOpResult r = file_ops2::CopyDirStructure(dirs, dst, /*recursive=*/true);

	CHECK(r.success_count == 2);  // src と src\sub
	CHECK(dir_exists(dst + _T("\\src")));
	CHECK(dir_exists(dst + _T("\\src\\sub")));
	// **ファイルは作らない**
	CHECK(file_exists(dst + _T("\\src\\a.txt")) == false);
	CHECK(file_exists(dst + _T("\\src\\sub\\b.txt")) == false);
}

TEST_CASE("CopyDirStructure: recursive=false なら直下だけ")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("src");
	const UnicodeString dst = tmp.path + _T("dst");
	::CreateDirectoryW(src.c_str(), NULL);
	::CreateDirectoryW((src + _T("\\sub")).c_str(), NULL);
	::CreateDirectoryW(dst.c_str(), NULL);

	std::vector<UnicodeString> dirs;
	dirs.push_back(src);
	const file_ops::FileOpResult r = file_ops2::CopyDirStructure(dirs, dst, /*recursive=*/false);

	CHECK(r.success_count == 1);
	CHECK(dir_exists(dst + _T("\\src\\sub")) == false);
}

TEST_CASE("CopyDirStructure: 自分自身の配下へは作らせない")
{
	TempDir tmp;
	const UnicodeString src = tmp.path + _T("src");
	::CreateDirectoryW(src.c_str(), NULL);

	std::vector<UnicodeString> dirs;
	dirs.push_back(src);
	const file_ops::FileOpResult r = file_ops2::CopyDirStructure(dirs, src, true);

	CHECK(r.success_count == 0);
	CHECK(r.failures.size() == 1);
}

//===========================================================================
// CreateDirs
//===========================================================================

TEST_CASE("CreateDirs: 多段の名前でも途中まで作る")
{
	TempDir tmp;
	std::vector<UnicodeString> names;
	names.push_back(_T("a\\b\\c"));

	const file_ops::FileOpResult r = file_ops2::CreateDirs(names, tmp.path);
	CHECK(r.success_count == 1);
	CHECK(dir_exists(tmp.path + _T("a\\b\\c")));
}

TEST_CASE("CreateDirs: 既にあるものは skipped_existing に数える")
{
	TempDir tmp;
	::CreateDirectoryW((tmp.path + _T("exists")).c_str(), NULL);

	std::vector<UnicodeString> names;
	names.push_back(_T("exists"));
	names.push_back(_T("fresh"));
	names.push_back(EmptyStr);  // 空行は無視する

	const file_ops::FileOpResult r = file_ops2::CreateDirs(names, tmp.path);
	CHECK(r.success_count == 1);
	CHECK(r.skipped_existing == 1);
}

TEST_CASE("CreateDirs: 同名のファイルがあれば失敗として報告する")
{
	TempDir tmp;
	mkfile(tmp.path + _T("taken"));

	std::vector<UnicodeString> names;
	names.push_back(_T("taken"));

	const file_ops::FileOpResult r = file_ops2::CreateDirs(names, tmp.path);
	CHECK(r.success_count == 0);
	CHECK(r.failures.size() == 1);
}

//===========================================================================
// SwapNames
//===========================================================================

TEST_CASE("SwapNames: 名前の主部だけを入れ替え、拡張子は元のまま")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("alpha.txt");
	const UnicodeString b = tmp.path + _T("beta.md");
	mkfile(a, "A");
	mkfile(b, "B");

	UnicodeString error;
	REQUIRE(file_ops2::SwapNames(a, b, error));

	// alpha.txt の中身が beta.txt に、beta.md の中身が alpha.md になる
	CHECK(read_all(tmp.path + _T("beta.txt")) == "A");
	CHECK(read_all(tmp.path + _T("alpha.md")) == "B");
	CHECK(file_exists(a) == false);
	CHECK(file_exists(b) == false);
}

TEST_CASE("SwapNames: 一時ファイルを残さない")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a);
	mkfile(b);

	UnicodeString error;
	REQUIRE(file_ops2::SwapNames(a, b, error));

	CHECK(file_exists(tmp.path + _T("$~NF0000.~TMP")) == false);
	CHECK(file_exists(tmp.path + _T("$~NF0001.~TMP")) == false);
}

TEST_CASE("SwapNames: 一時ファイルが残っていたら手を付けずに断る")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	const UnicodeString b = tmp.path + _T("b.txt");
	mkfile(a, "A");
	mkfile(b, "B");
	mkfile(tmp.path + _T("$~NF0000.~TMP"));

	UnicodeString error;
	CHECK(file_ops2::SwapNames(a, b, error) == false);
	CHECK(!error.IsEmpty());
	// **何も動いていないこと**
	CHECK(read_all(a) == "A");
	CHECK(read_all(b) == "B");
}

TEST_CASE("SwapNames: 同じ項目は断る")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("a.txt");
	mkfile(a);

	UnicodeString error;
	CHECK(file_ops2::SwapNames(a, a, error) == false);
}

TEST_CASE("SwapNames: 親子関係にあるものは断る (VCL も非対応)")
{
	TempDir tmp;
	const UnicodeString parent = tmp.path + _T("outer");
	::CreateDirectoryW(parent.c_str(), NULL);
	const UnicodeString child = parent + _T("\\inner");
	::CreateDirectoryW(child.c_str(), NULL);

	UnicodeString error;
	CHECK(file_ops2::SwapNames(parent, child, error) == false);
	CHECK(dir_exists(parent));
	CHECK(dir_exists(child));
}

TEST_CASE("SwapNames: 拡張子が同じなら名前がそのまま入れ替わる")
{
	TempDir tmp;
	const UnicodeString a = tmp.path + _T("one.txt");
	const UnicodeString b = tmp.path + _T("two.txt");
	mkfile(a, "1");
	mkfile(b, "2");

	UnicodeString error;
	REQUIRE(file_ops2::SwapNames(a, b, error));
	CHECK(read_all(a) == "2");
	CHECK(read_all(b) == "1");
}

//===========================================================================
// NeedsTempStep / UndoRenames
//===========================================================================

TEST_CASE("NeedsTempStep: 新旧が交差していれば true")
{
	std::vector<file_ops2::RenameRecord> v(2);
	v[0].old_path = _T("C:\\a");  v[0].new_path = _T("C:\\b");
	v[1].old_path = _T("C:\\b");  v[1].new_path = _T("C:\\a");
	CHECK(file_ops2::NeedsTempStep(v) == true);
}

TEST_CASE("NeedsTempStep: 交差していなければ false")
{
	std::vector<file_ops2::RenameRecord> v(2);
	v[0].old_path = _T("C:\\a");  v[0].new_path = _T("C:\\x");
	v[1].old_path = _T("C:\\b");  v[1].new_path = _T("C:\\y");
	CHECK(file_ops2::NeedsTempStep(v) == false);
}

TEST_CASE("UndoRenames: 単純な改名を元に戻す")
{
	TempDir tmp;
	const UnicodeString newp = tmp.path + _T("after.txt");
	mkfile(newp, "X");

	std::vector<file_ops2::RenameRecord> v(1);
	v[0].old_path = tmp.path + _T("before.txt");
	v[0].new_path = newp;

	const file_ops::FileOpResult r = file_ops2::UndoRenames(v);
	CHECK(r.success_count == 1);
	CHECK(read_all(tmp.path + _T("before.txt")) == "X");
	CHECK(file_exists(newp) == false);
}

TEST_CASE("UndoRenames: 入れ替わっていた改名も一時名を経由して戻せる")
{
	TempDir tmp;
	// a→b, b→a という改名が行われた後の状態を作る
	mkfile(tmp.path + _T("a.txt"), "was_b");
	mkfile(tmp.path + _T("b.txt"), "was_a");

	std::vector<file_ops2::RenameRecord> v(2);
	v[0].old_path = tmp.path + _T("a.txt");  v[0].new_path = tmp.path + _T("b.txt");
	v[1].old_path = tmp.path + _T("b.txt");  v[1].new_path = tmp.path + _T("a.txt");

	const file_ops::FileOpResult r = file_ops2::UndoRenames(v);
	CHECK(r.success_count == 2);
	CHECK(read_all(tmp.path + _T("a.txt")) == "was_a");
	CHECK(read_all(tmp.path + _T("b.txt")) == "was_b");
	CHECK(file_exists(tmp.path + _T("$~NF0000.~TMP")) == false);
}

TEST_CASE("UndoRenames: 戻し先が塞がっていたら飛ばして元の場所に残す")
{
	TempDir tmp;
	mkfile(tmp.path + _T("after.txt"), "X");
	mkfile(tmp.path + _T("before.txt"), "OCCUPIED");

	std::vector<file_ops2::RenameRecord> v(1);
	v[0].old_path = tmp.path + _T("before.txt");
	v[0].new_path = tmp.path + _T("after.txt");

	const file_ops::FileOpResult r = file_ops2::UndoRenames(v);
	CHECK(r.skipped_existing == 1);
	// **上書きしない。**元のファイルも残っている
	CHECK(read_all(tmp.path + _T("before.txt")) == "OCCUPIED");
	CHECK(read_all(tmp.path + _T("after.txt")) == "X");
}

TEST_CASE("UndoRenames: 空の記録なら何もしない")
{
	const file_ops::FileOpResult r = file_ops2::UndoRenames(std::vector<file_ops2::RenameRecord>());
	CHECK(r.success_count == 0);
	CHECK(r.failures.empty());
}

//===========================================================================
// ParseSize / TestFileName / CreateTestFiles
//===========================================================================

TEST_CASE("ParseSize: 単位なし・K・M・G を解釈する")
{
	CHECK(file_ops2::ParseSize(_T("1024")) == 1024);
	CHECK(file_ops2::ParseSize(_T("2K")) == 2048);
	CHECK(file_ops2::ParseSize(_T("1m")) == 1024 * 1024);
	CHECK(file_ops2::ParseSize(_T("1G")) == 1024LL * 1024 * 1024);
}

TEST_CASE("ParseSize: 解釈できないものは -1")
{
	CHECK(file_ops2::ParseSize(EmptyStr) == -1);
	CHECK(file_ops2::ParseSize(_T("K")) == -1);
	CHECK(file_ops2::ParseSize(_T("12X")) == -1);
	CHECK(file_ops2::ParseSize(_T("1.5M")) == -1);
}

TEST_CASE("TestFileName: 1個なら連番を付けない")
{
	CHECK(file_ops2::TestFileName(_T("t.dat"), 0, 1) == UnicodeString(_T("t.dat")));
}

TEST_CASE("TestFileName: 総数の桁数でゼロ詰めする")
{
	CHECK(file_ops2::TestFileName(_T("t.dat"), 0, 10) == UnicodeString(_T("t01.dat")));
	CHECK(file_ops2::TestFileName(_T("t.dat"), 9, 10) == UnicodeString(_T("t10.dat")));
	CHECK(file_ops2::TestFileName(_T("t.dat"), 0, 5) == UnicodeString(_T("t1.dat")));
}

TEST_CASE("CreateTestFiles: 指定したサイズで指定した個数を作る")
{
	TempDir tmp;
	const file_ops::FileOpResult r = file_ops2::CreateTestFiles(tmp.path, _T("t.dat"), 4096, 3);

	CHECK(r.success_count == 3);
	CHECK(size_of(tmp.path + _T("t1.dat")) == 4096);
	CHECK(size_of(tmp.path + _T("t3.dat")) == 4096);
}

TEST_CASE("CreateTestFiles: 既にあるものは上書きしない")
{
	TempDir tmp;
	mkfile(tmp.path + _T("t.dat"), "keep");

	const file_ops::FileOpResult r = file_ops2::CreateTestFiles(tmp.path, _T("t.dat"), 100, 1);
	CHECK(r.success_count == 0);
	CHECK(r.skipped_existing == 1);
	CHECK(read_all(tmp.path + _T("t.dat")) == "keep");
}
