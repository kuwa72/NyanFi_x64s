/**
 * @file tests/core/test_gui_work_list.cpp
 * @brief gui/work_list.cpp (ワークリスト) のテスト
 *
 * 並べ替えの規則は VCL の ItemTmpUp/Down/Move (src/MainFrm.cpp:19977-20080)、
 * .nwl の書式は load_WorkList/save_WorkList (src/Global.cpp:7827/7914) を
 * 実測して合わせてある。ここではその挙動を固定する。
 */
#include "doctest/doctest.h"

#include "gui/work_list.h"
#include "temp_dir.h"

using nyanfi_test::TempDir;
using work_list::WorkItem;

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

/// path だけを持つ項目 (実体は見ない)。並べ替えのテスト用
WorkItem item(const UnicodeString &path, bool marked = false)
{
	WorkItem w;
	w.path = path;
	w.marked = marked;
	return w;
}

/// 並びを "a,b,c" の形で取り出す (比較を読みやすくするため)
UnicodeString order_of(const std::vector<WorkItem> &v)
{
	UnicodeString s;
	for (std::size_t i = 0; i < v.size(); ++i) {
		if (i > 0) s += _T(",");
		s += v[i].is_separator? _T("-") : v[i].path;
	}
	return s;
}

std::string read_all(const UnicodeString &path)
{
	HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
	                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	REQUIRE(h != INVALID_HANDLE_VALUE);
	std::string out;
	char buf[4096];
	DWORD n = 0;
	while (::ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) out.append(buf, n);
	::CloseHandle(h);
	return out;
}

}  // namespace

//===========================================================================
// ParseLines (.nwl の読み)
//===========================================================================

TEST_CASE("ParseLines: パスと別名をタブで分ける")
{
	const std::vector<UnicodeString> lines = {_T("C:\\dir\\memo.txt\tメモ")};
	const std::vector<WorkItem> v = work_list::ParseLines(lines);

	REQUIRE(v.size() == 1);
	CHECK(v[0].path == UnicodeString(_T("C:\\dir\\memo.txt")));
	CHECK(v[0].alias == UnicodeString(_T("メモ")));
	CHECK(v[0].is_dir == false);
}

TEST_CASE("ParseLines: タブが無ければ別名は空")
{
	const std::vector<UnicodeString> lines = {_T("C:\\dir\\memo.txt")};
	const std::vector<WorkItem> v = work_list::ParseLines(lines);

	REQUIRE(v.size() == 1);
	CHECK(v[0].path == UnicodeString(_T("C:\\dir\\memo.txt")));
	CHECK(v[0].alias.IsEmpty());
}

TEST_CASE("ParseLines: 末尾が区切り文字ならディレクトリ (末尾は落とす)")
{
	const std::vector<UnicodeString> lines = {_T("C:\\dir\\sub\\\tフォルダ")};
	const std::vector<WorkItem> v = work_list::ParseLines(lines);

	REQUIRE(v.size() == 1);
	CHECK(v[0].is_dir == true);
	CHECK(v[0].path == UnicodeString(_T("C:\\dir\\sub")));
}

TEST_CASE("ParseLines: パスが空で別名が - なら区切り行")
{
	const std::vector<UnicodeString> lines = {_T("\t-")};
	const std::vector<WorkItem> v = work_list::ParseLines(lines);

	REQUIRE(v.size() == 1);
	CHECK(v[0].is_separator == true);
	CHECK(v[0].path.IsEmpty());
}

TEST_CASE("ParseLines: 空行と ; で始まる行は読み飛ばす")
{
	const std::vector<UnicodeString> lines = {
		_T("; これはコメント"), EmptyStr, _T("C:\\a.txt"), _T(";もう1行")
	};
	const std::vector<WorkItem> v = work_list::ParseLines(lines);

	REQUIRE(v.size() == 1);
	CHECK(v[0].path == UnicodeString(_T("C:\\a.txt")));
}

TEST_CASE("ParseLines: 別名だけの行は捨てる (パスが決まらないため)")
{
	// VCL も file_rec を作らずに落とす (is_separator でもないので)
	const std::vector<UnicodeString> lines = {_T("\tラベルだけ")};
	CHECK(work_list::ParseLines(lines).empty());
}

//===========================================================================
// FormatLines (.nwl の書き)
//===========================================================================

TEST_CASE("FormatLines: 別名が空でもタブは必ず付く (VCL の cat_sprintf と同じ)")
{
	std::vector<WorkItem> v = {item(_T("C:\\a.txt"))};
	const std::vector<UnicodeString> lines = work_list::FormatLines(v);

	REQUIRE(lines.size() == 1);
	CHECK(lines[0] == UnicodeString(_T("C:\\a.txt\t")));
}

TEST_CASE("FormatLines: ディレクトリは末尾に区切り文字が付く")
{
	std::vector<WorkItem> v = {item(_T("C:\\sub"))};
	v[0].is_dir = true;
	v[0].alias = _T("さぶ");

	const std::vector<UnicodeString> lines = work_list::FormatLines(v);
	REQUIRE(lines.size() == 1);
	CHECK(lines[0] == UnicodeString(_T("C:\\sub\\\tさぶ")));
}

TEST_CASE("FormatLines: 区切り行は残る (手で編集した .nwl を壊さないため)")
{
	std::vector<WorkItem> v(1);
	v[0].is_separator = true;
	v[0].alias = _T("-");

	const std::vector<UnicodeString> lines = work_list::FormatLines(v);
	REQUIRE(lines.size() == 1);
	CHECK(lines[0] == UnicodeString(_T("\t-")));
}

TEST_CASE("ParseLines と FormatLines は往復する")
{
	const std::vector<UnicodeString> src = {
		_T("C:\\a.txt\t"), _T("C:\\sub\\\tさぶ"), _T("\t-"), _T("D:\\b.txt\tビー")
	};
	const std::vector<UnicodeString> back = work_list::FormatLines(work_list::ParseLines(src));

	REQUIRE(back.size() == src.size());
	for (std::size_t i = 0; i < src.size(); ++i) CHECK(back[i] == src[i]);
}

//===========================================================================
// Load / Save
//===========================================================================

TEST_CASE("Save: UTF-8 の BOM を付ける (VCL の TEncoding::UTF8 に合わせる)")
{
	TempDir tmp;
	const UnicodeString nwl = tmp.path + _T("w.nwl");

	std::vector<WorkItem> v = {item(_T("C:\\a.txt"))};
	UnicodeString error;
	REQUIRE(work_list::Save(nwl, v, error));

	const std::string bytes = read_all(nwl);
	REQUIRE(bytes.size() >= 3);
	CHECK(static_cast<unsigned char>(bytes[0]) == 0xEF);
	CHECK(static_cast<unsigned char>(bytes[1]) == 0xBB);
	CHECK(static_cast<unsigned char>(bytes[2]) == 0xBF);
}

TEST_CASE("Load: 実体を見てサイズと属性を埋め、無い項目は missing にする")
{
	TempDir tmp;
	const UnicodeString real = tmp.path + _T("real.txt");
	mkfile(real, "hello");

	std::vector<WorkItem> src = {item(real), item(tmp.path + _T("gone.txt"))};
	const UnicodeString nwl = tmp.path + _T("w.nwl");
	UnicodeString error;
	REQUIRE(work_list::Save(nwl, src, error));

	std::vector<WorkItem> got;
	REQUIRE(work_list::Load(nwl, /*auto_delete=*/false, got, error));

	REQUIRE(got.size() == 2);
	CHECK(got[0].missing == false);
	CHECK(got[0].size == 5);
	// **黙って捨てない。** 外付けドライブが繋がっていないだけかもしれないので、
	// 実体が無くても一覧には残す
	CHECK(got[1].missing == true);
}

TEST_CASE("Load: auto_delete なら実体の無い項目を捨てる (VCL の AutoDelWorkList)")
{
	TempDir tmp;
	const UnicodeString real = tmp.path + _T("real.txt");
	mkfile(real);

	std::vector<WorkItem> src = {item(real), item(tmp.path + _T("gone.txt"))};
	const UnicodeString nwl = tmp.path + _T("w.nwl");
	UnicodeString error;
	REQUIRE(work_list::Save(nwl, src, error));

	std::vector<WorkItem> got;
	REQUIRE(work_list::Load(nwl, /*auto_delete=*/true, got, error));
	REQUIRE(got.size() == 1);
	CHECK(got[0].missing == false);
}

TEST_CASE("Load: 日本語の別名が UTF-8 で往復する")
{
	TempDir tmp;
	const UnicodeString real = tmp.path + _T("real.txt");
	mkfile(real);

	std::vector<WorkItem> src = {item(real)};
	src[0].alias = _T("日本語の別名");

	const UnicodeString nwl = tmp.path + _T("w.nwl");
	UnicodeString error;
	REQUIRE(work_list::Save(nwl, src, error));

	std::vector<WorkItem> got;
	REQUIRE(work_list::Load(nwl, false, got, error));
	REQUIRE(got.size() == 1);
	CHECK(got[0].alias == UnicodeString(_T("日本語の別名")));
}

TEST_CASE("Load: 無いファイルは理由を返して false")
{
	TempDir tmp;
	std::vector<WorkItem> got;
	UnicodeString error;
	CHECK(work_list::Load(tmp.path + _T("nothere.nwl"), false, got, error) == false);
	CHECK(!error.IsEmpty());
}

//===========================================================================
// IndexOfPath / Add / InsertSeparator / RemoveMissing
//===========================================================================

TEST_CASE("IndexOfPath: 大文字小文字を区別しない (VCL の TStringList::IndexOf と同じ)")
{
	const std::vector<WorkItem> v = {item(_T("C:\\Dir\\Memo.TXT"))};
	CHECK(work_list::IndexOfPath(v, _T("c:\\dir\\memo.txt")) == 0);
	CHECK(work_list::IndexOfPath(v, _T("c:\\dir\\other.txt")) == -1);
}

TEST_CASE("Add: 既に登録済みなら足さない")
{
	TempDir tmp;
	const UnicodeString f = tmp.path + _T("a.txt");
	mkfile(f);

	std::vector<WorkItem> v;
	CHECK(work_list::Add(v, f) == true);
	CHECK(work_list::Add(v, f) == false);
	CHECK(v.size() == 1);
}

TEST_CASE("Add: 実体が無ければ足さない")
{
	TempDir tmp;
	std::vector<WorkItem> v;
	CHECK(work_list::Add(v, tmp.path + _T("nothere.txt")) == false);
	CHECK(v.empty());
}

TEST_CASE("InsertSeparator: カーソルの次に入る (VCL の idx+1)")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b")), item(_T("c"))};
	work_list::InsertSeparator(v, 0);
	CHECK(order_of(v) == UnicodeString(_T("a,-,b,c")));
}

TEST_CASE("InsertSeparator: 末尾にカーソルがあるなら末尾に足す")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"))};
	work_list::InsertSeparator(v, 1);
	CHECK(order_of(v) == UnicodeString(_T("a,b,-")));
}

TEST_CASE("RemoveMissing: 実体の無い項目だけを外し、件数を返す")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b")), item(_T("c"))};
	v[1].missing = true;
	CHECK(work_list::RemoveMissing(v) == 1);
	CHECK(order_of(v) == UnicodeString(_T("a,c")));
}

TEST_CASE("HasSeparator: 区切り行の有無を見る")
{
	std::vector<WorkItem> v = {item(_T("a"))};
	CHECK(work_list::HasSeparator(v) == false);
	work_list::InsertSeparator(v, 0);
	CHECK(work_list::HasSeparator(v) == true);
}

//===========================================================================
// MoveUp / MoveDown / MoveSelectedTo
//===========================================================================

TEST_CASE("MoveUp: 選択が無ければカーソル位置の1件が上がる")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b")), item(_T("c"))};
	int cursor = 2;
	CHECK(work_list::MoveUp(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("a,c,b")));
	CHECK(cursor == 1);
}

TEST_CASE("MoveUp: 先頭が対象なら何もしない (詰まっていて動けない)")
{
	std::vector<WorkItem> v = {item(_T("a"), true), item(_T("b"), true), item(_T("c"))};
	int cursor = 0;
	CHECK(work_list::MoveUp(v, cursor) == false);
	CHECK(order_of(v) == UnicodeString(_T("a,b,c")));
	CHECK(cursor == 0);
}

TEST_CASE("MoveUp: 離れた複数選択でもそれぞれ1つずつ上がる")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"), true), item(_T("c")), item(_T("d"), true)};
	int cursor = 1;
	CHECK(work_list::MoveUp(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("b,a,d,c")));
	CHECK(cursor == 0);
}

TEST_CASE("MoveUp: 連続した選択の塊は塊のまま上がる")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"), true), item(_T("c"), true), item(_T("d"))};
	int cursor = 1;
	CHECK(work_list::MoveUp(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("b,c,a,d")));
}

TEST_CASE("MoveDown: 選択が無ければカーソル位置の1件が下がる")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b")), item(_T("c"))};
	int cursor = 0;
	CHECK(work_list::MoveDown(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("b,a,c")));
	CHECK(cursor == 1);
}

TEST_CASE("MoveDown: 末尾が対象なら何もしない")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"), true), item(_T("c"), true)};
	int cursor = 1;
	CHECK(work_list::MoveDown(v, cursor) == false);
	CHECK(order_of(v) == UnicodeString(_T("a,b,c")));
}

TEST_CASE("MoveDown: 連続した選択の塊は塊のまま下がる")
{
	std::vector<WorkItem> v = {item(_T("a"), true), item(_T("b"), true), item(_T("c")), item(_T("d"))};
	int cursor = 0;
	CHECK(work_list::MoveDown(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("c,a,b,d")));
}

TEST_CASE("MoveSelectedTo: 抜いた分だけ挿入位置が手前にずれる")
{
	// a(選) b c d(選) を、カーソル c (2) の位置へ寄せる。
	// a を抜くと c は 1 へずれるので、挿入先は 1
	std::vector<WorkItem> v = {item(_T("a"), true), item(_T("b")), item(_T("c")), item(_T("d"), true)};
	int cursor = 2;
	CHECK(work_list::MoveSelectedTo(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("b,a,d,c")));
	CHECK(cursor == 1);
}

TEST_CASE("MoveSelectedTo: 動かした項目の選択は解除される (VCL と同じ)")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"), true)};
	int cursor = 0;
	REQUIRE(work_list::MoveSelectedTo(v, cursor) == true);
	CHECK(v[0].marked == false);
}

TEST_CASE("MoveSelectedTo: 選択が1件も無ければ何もしない")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"))};
	int cursor = 1;
	CHECK(work_list::MoveSelectedTo(v, cursor) == false);
	CHECK(order_of(v) == UnicodeString(_T("a,b")));
}

TEST_CASE("並べ替えは区切り行も同じように動かす (VCL は区切りも選択できる)")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"))};
	work_list::InsertSeparator(v, 0);  // a,-,b
	REQUIRE(order_of(v) == UnicodeString(_T("a,-,b")));

	int cursor = 1;  // 区切り行の上
	CHECK(work_list::MoveUp(v, cursor) == true);
	CHECK(order_of(v) == UnicodeString(_T("-,a,b")));
}

//===========================================================================
// ToFileItems / ApplyMarks
//===========================================================================

TEST_CASE("ToFileItems: 並び順を保ち、full_path を必ず入れる")
{
	// full_path が空だと一覧のディレクトリと繋がれて別のファイルを指す
	// (docs/port/phase0-report.md §21)
	std::vector<WorkItem> v = {item(_T("D:\\other\\memo.txt"))};
	const std::vector<FileItem> f = work_list::ToFileItems(v);

	REQUIRE(f.size() == 1);
	CHECK(f[0].name == UnicodeString(_T("memo.txt")));
	CHECK(f[0].full_path == UnicodeString(_T("D:\\other\\memo.txt")));
}

TEST_CASE("ToFileItems: 区切り行は名前もパスも持たない")
{
	std::vector<WorkItem> v(1);
	v[0].is_separator = true;
	v[0].alias = _T("-");

	const std::vector<FileItem> f = work_list::ToFileItems(v);
	REQUIRE(f.size() == 1);
	CHECK(f[0].is_separator == true);
	CHECK(f[0].name.IsEmpty());
	CHECK(f[0].full_path.IsEmpty());
}

TEST_CASE("ToFileItems: 別名をそのまま渡す (表示側が名前の代わりに出す)")
{
	std::vector<WorkItem> v = {item(_T("C:\\a.txt"))};
	v[0].alias = _T("エー");

	const std::vector<FileItem> f = work_list::ToFileItems(v);
	REQUIRE(f.size() == 1);
	CHECK(f[0].alias == UnicodeString(_T("エー")));
}

TEST_CASE("ApplyMarks: 添字で対応付けて選択状態を書き戻す")
{
	std::vector<WorkItem> v = {item(_T("a")), item(_T("b"))};
	std::vector<FileItem> f = work_list::ToFileItems(v);
	f[1].marked = true;

	work_list::ApplyMarks(v, f);
	CHECK(v[0].marked == false);
	CHECK(v[1].marked == true);
}
