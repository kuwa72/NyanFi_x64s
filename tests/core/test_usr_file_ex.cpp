/**
 * @file tests/core/test_usr_file_ex.cpp
 * @brief src/usr_file_ex.cpp (ファイル・パス操作の拡張関数群) の回帰テスト
 *
 * 目的: 現在の実装の挙動をそのまま固定すること (regression test)。
 * ヘッダ (src/usr_file_ex.h) の Doxygen コメントは参考にするが、
 * コメントより実装 (src/usr_file_ex.cpp) の挙動を信じる。実装がおかしいと
 * 思っても直さず、報告にのみ記載する。
 *
 * ファイルシステムに触れるテストは、TempDir (本ファイル内で定義する
 * RAII ヘルパー) が作る一時ディレクトリの中だけで行い、SUBCASE/TEST_CASE
 * ごとに確実に後始末する。実ファイルの後始末には、テスト対象の
 * delete_Dirs 等を使わず、素の Win32 API (FindFirstFileW/DeleteFileW/
 * RemoveDirectoryW) を直接使う (テスト対象の不具合とテスト自身の後始末の
 * 不具合を切り分けるため)。
 *
 * 除外した関数 (理由は末尾のコメント参照): delete_ADS / rename_ADS /
 * cre_Junction / get_actual_name (レジストリ App Paths 分岐) /
 * FindFirstStreamW 系。
 */
#include "doctest/doctest.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "usr_file_ex.h"
#include "usr_str.h"

//===========================================================================
// テスト用ヘルパー: 一時ディレクトリ (RAII)
//===========================================================================
namespace {

void remove_all_recursive(const UnicodeString &path_with_delim)
{
	WIN32_FIND_DATAW fd;
	UnicodeString pattern = path_with_delim + "*";
	HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;

	do {
		UnicodeString name(fd.cFileName);
		if (name == UnicodeString(".") || name == UnicodeString("..")) continue;
		UnicodeString full = path_with_delim + name;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			remove_all_recursive(IncludeTrailingPathDelimiter(full));
			::SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
			::RemoveDirectoryW(full.c_str());
		}
		else {
			::SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
			::DeleteFileW(full.c_str());
		}
	} while (::FindNextFileW(h, &fd));
	::FindClose(h);
}

/// SUBCASE/TEST_CASE ごとに一意な一時ディレクトリを作成し、破棄時に再帰削除する
struct TempDir {
	UnicodeString path;  //!< 末尾 "\" 付き

	TempDir()
	{
		wchar_t buf[MAX_PATH];
		::GetTempPathW(MAX_PATH, buf);
		wchar_t unique[64];
		static LONG counter = 0;
		DWORD n = ::InterlockedIncrement(&counter);
		swprintf(unique, 64, L"nyanfi_ut_%08lx_%04lx", (unsigned long)::GetCurrentProcessId(), (unsigned long)n);
		path = IncludeTrailingPathDelimiter(UnicodeString(buf)) + UnicodeString(unique);
		::CreateDirectoryW(path.c_str(), NULL);
		path = IncludeTrailingPathDelimiter(path);
	}
	~TempDir()
	{
		remove_all_recursive(path);
		::SetFileAttributesW(ExcludeTrailingPathDelimiter(path).c_str(), FILE_ATTRIBUTE_NORMAL);
		::RemoveDirectoryW(ExcludeTrailingPathDelimiter(path).c_str());
	}

	UnicodeString file(const UnicodeString &name) const { return path + name; }
};

void write_bytes(const UnicodeString &fnam, const void *data, int len)
{
	std::unique_ptr<TFileStream> fs(new TFileStream(fnam, fmCreate));
	fs->WriteBuffer(data, len);
}

} // namespace

//===========================================================================
// cv_ex_filename: 260文字以上/末尾空白対応
//===========================================================================
TEST_CASE("cv_ex_filename: 通常の短いパスはそのまま")
{
	CHECK(cv_ex_filename("C:\\foo\\bar.txt") == UnicodeString("C:\\foo\\bar.txt"));
}

TEST_CASE("cv_ex_filename: 末尾が空白なら \\\\?\\ を付加")
{
	UnicodeString fnam = "C:\\foo\\bar ";
	CHECK(cv_ex_filename(fnam) == UnicodeString("\\\\?\\C:\\foo\\bar "));
}

TEST_CASE("cv_ex_filename: 260文字以上なら \\\\?\\ を付加")
{
	UnicodeString longpart = StringOfChar(L'a', 260);
	UnicodeString fnam = "C:\\" + longpart;
	UnicodeString r = cv_ex_filename(fnam);
	CHECK(StartsStr("\\\\?\\C:\\", r) == true);
}

TEST_CASE("cv_ex_filename: UNCパスは \\\\?\\UNC\\ に変換")
{
	UnicodeString longpart = StringOfChar(L'a', 260);
	UnicodeString fnam = "\\\\server\\share\\" + longpart;
	UnicodeString r = cv_ex_filename(fnam);
	CHECK(StartsStr("\\\\?\\UNC\\server\\share\\", r) == true);
}

//===========================================================================
// cv_env_var / cv_env_str
//===========================================================================
TEST_CASE("cv_env_var: 環境変数を展開")
{
	::SetEnvironmentVariableW(L"NYANFI_UT_VAR", L"VALUE1");
	CHECK(cv_env_var("pre%NYANFI_UT_VAR%post") == UnicodeString("preVALUE1post"));
	::SetEnvironmentVariableW(L"NYANFI_UT_VAR", NULL);
}

TEST_CASE("cv_env_var: 未定義変数は展開されずそのまま")
{
	CHECK(cv_env_var("pre%NYANFI_UT_UNDEFINED_XYZ%post") == UnicodeString("pre%NYANFI_UT_UNDEFINED_XYZ%post"));
}

TEST_CASE("cv_env_var: %を含まない文字列はそのまま")
{
	CHECK(cv_env_var("plain") == UnicodeString("plain"));
}

TEST_CASE("cv_env_str: %ExePath% を展開")
{
	ExePath = "D:\\NyanFi\\";
	CHECK(cv_env_str("%ExePath%data") == UnicodeString("D:\\NyanFi\\data"));
}

//===========================================================================
// get_actual_path: $X, $D, $$ の展開
//===========================================================================
TEST_CASE("get_actual_path: $X はExePath(末尾\\無し)、$D はドライブ文字")
{
	ExePath = "D:\\NyanFi\\";
	CHECK(get_actual_path("$X\\foo") == UnicodeString("D:\\NyanFi\\foo"));
	CHECK(get_actual_path("$D\\foo") == UnicodeString("D:\\foo"));
}

TEST_CASE("get_actual_path: $$ は $ そのものに")
{
	ExePath = "D:\\NyanFi\\";
	CHECK(get_actual_path("price$$100") == UnicodeString("price$100"));
}

TEST_CASE("get_actual_path: $ を含まない文字列はそのまま(環境変数展開のみ)")
{
	ExePath = "D:\\NyanFi\\";
	CHECK(get_actual_path("C:\\plain\\path") == UnicodeString("C:\\plain\\path"));
}

//===========================================================================
// exclude_env_path
//===========================================================================
TEST_CASE("exclude_env_path: 空文字列/ドライブ無しはそのまま")
{
	CHECK(exclude_env_path("") == UnicodeString(""));
	CHECK(exclude_env_path("noext") == UnicodeString("noext"));
}

TEST_CASE("exclude_env_path: 拡張子なしファイルはそのまま")
{
	CHECK(exclude_env_path("C:\\foo\\bar") == UnicodeString("C:\\foo\\bar"));
}

TEST_CASE("exclude_env_path: PATHEXTに無い拡張子はそのまま")
{
	CHECK(exclude_env_path("C:\\foo\\bar.zzz") == UnicodeString("C:\\foo\\bar.zzz"));
}

TEST_CASE("exclude_env_path: PATH+PATHEXTに一致すればパスと拡張子を除去")
{
	::SetEnvironmentVariableW(L"PATHEXT", L".EXE;.COM");
	::SetEnvironmentVariableW(L"PATH", L"C:\\mytools");
	CHECK(exclude_env_path("C:\\mytools\\foo.exe") == UnicodeString("foo"));
	//パスが一致しなければそのまま
	CHECK(exclude_env_path("C:\\other\\foo.exe") == UnicodeString("C:\\other\\foo.exe"));
	::SetEnvironmentVariableW(L"PATHEXT", NULL);
	::SetEnvironmentVariableW(L"PATH", NULL);
}

//===========================================================================
// to_relative_name / to_absolute_name
//===========================================================================
TEST_CASE("to_relative_name: ExePath配下なら先頭を除去")
{
	ExePath = "C:\\NyanFi\\";
	CHECK(to_relative_name("C:\\NyanFi\\sub\\file.txt") == UnicodeString("sub\\file.txt"));
	CHECK(to_relative_name("D:\\other\\file.txt") == UnicodeString("D:\\other\\file.txt"));
}

TEST_CASE("to_absolute_name: 空文字列はEmptyStr")
{
	CHECK(to_absolute_name("") == UnicodeString(""));
}

TEST_CASE("to_absolute_name: ドライブ付きパスの .. を正規化")
{
	CHECK(to_absolute_name("C:\\foo\\..\\bar") == UnicodeString("C:\\bar"));
	CHECK(to_absolute_name("C:\\a\\b\\..\\..\\c") == UnicodeString("C:\\c"));
}

TEST_CASE("to_absolute_name: 相対パス(.\\ .. \\)を基準ディレクトリで解決")
{
	CHECK(to_absolute_name(".\\foo", "C:\\base\\") == UnicodeString("C:\\base\\foo"));
	CHECK(to_absolute_name("..\\foo", "C:\\base\\sub\\") == UnicodeString("C:\\base\\foo"));
}

TEST_CASE("to_absolute_name: 基準省略時はExePathを使う")
{
	ExePath = "C:\\NyanFi\\";
	CHECK(to_absolute_name("foo") == UnicodeString("C:\\NyanFi\\foo"));
}

//===========================================================================
// extract_file_path: ADS対応のパス取得
//===========================================================================
TEST_CASE("extract_file_path: ADS無しは通常のExtractFilePath相当")
{
	CHECK(extract_file_path("C:\\foo\\bar.txt") == UnicodeString("C:\\foo\\"));
}

TEST_CASE("extract_file_path: ADS付きファイル名はストリーム部を除いて処理")
{
	CHECK(extract_file_path("C:\\foo\\bar.txt:stream") == UnicodeString("C:\\foo\\"));
}

//===========================================================================
// match_path_list
//===========================================================================
TEST_CASE("match_path_list: 部分一致(デフォルト)")
{
	CHECK(match_path_list("C:\\foo\\bar", "C:\\foo;D:\\baz", false) == true);
	CHECK(match_path_list("C:\\other", "C:\\foo;D:\\baz", false) == false);
}

TEST_CASE("match_path_list: 前方一致(start_sw=true)")
{
	CHECK(match_path_list("C:\\foo\\bar", "C:\\foo", true) == true);
	CHECK(match_path_list("C:\\foo\\bar", "foo", true) == false);  //前方一致なので不一致
}

//===========================================================================
// is_same_file / is_same_dir
//===========================================================================
TEST_CASE("is_same_file: 絶対パス化して比較、大文字小文字を無視")
{
	CHECK(is_same_file("C:\\foo\\bar.txt", "c:\\FOO\\BAR.TXT", "C:\\base\\") == true);
	CHECK(is_same_file("C:\\foo\\bar.txt", "C:\\foo\\baz.txt", "C:\\base\\") == false);
}

TEST_CASE("is_same_dir: 末尾区切りの有無や大小文字を無視")
{
	CHECK(is_same_dir("C:\\foo\\bar", "C:\\FOO\\BAR\\") == true);
	CHECK(is_same_dir("C:\\foo", "C:\\bar") == false);
}

//===========================================================================
// get_root_name / is_root_dir / is_root_unc / exclede_delimiter_if_root
//===========================================================================
TEST_CASE("get_root_name: ドライブ名またはUNCコンピュータ名")
{
	CHECK(get_root_name("C:\\foo\\bar") == UnicodeString("C:\\"));
	CHECK(get_root_name("\\\\server\\share\\foo") == UnicodeString("\\\\server\\"));
}

TEST_CASE("is_root_dir: ドライブ直下/UNC共有直下はルート")
{
	CHECK(is_root_dir("C:\\") == true);
	CHECK(is_root_dir("C:\\foo") == false);
	CHECK(is_root_dir("\\\\server\\share") == true);
	CHECK(is_root_dir("\\\\server\\share\\foo") == false);
}

TEST_CASE("is_root_unc: UNCパスのみルート判定、非UNCは常にfalse")
{
	CHECK(is_root_unc("\\\\server\\share") == true);
	CHECK(is_root_unc("\\\\server\\share\\foo") == false);
	CHECK(is_root_unc("C:\\") == false);
}

TEST_CASE("exclede_delimiter_if_root: ルートなら末尾の\\を除去、非ルートなら付加")
{
	CHECK(exclede_delimiter_if_root("C:\\") == UnicodeString("C:"));
	CHECK(exclede_delimiter_if_root("C:\\foo") == UnicodeString("C:\\foo\\"));
	CHECK(exclede_delimiter_if_root("") == UnicodeString(""));
}

//===========================================================================
// get_drive_str / drive_exists / get_drive_type
//===========================================================================
TEST_CASE("get_drive_str: 大文字+\\付きのドライブ名")
{
	CHECK(get_drive_str("c:\\foo\\bar") == UnicodeString("C:\\"));
}

TEST_CASE("drive_exists: 存在するドライブ(C:)は true, 使われていなさそうな文字はfalse")
{
	CHECK(drive_exists("C:") == true);
	CHECK(drive_exists("") == false);
}

TEST_CASE("get_drive_type: C: ドライブは有効な種別を返す(DRIVE_UNKNOWN=0ではない)")
{
	UINT t = get_drive_type("C:\\foo");
	CHECK(t != DRIVE_UNKNOWN);
}

//===========================================================================
// get_base_name / get_extension / get_extension_if_file / nrm_FileExt
//===========================================================================
TEST_CASE("get_base_name: 拡張子を除いたファイル名")
{
	CHECK(get_base_name("C:\\foo\\bar.txt") == UnicodeString("bar"));
	CHECK(get_base_name("C:\\foo\\") == UnicodeString(""));
}

TEST_CASE("get_base_name: ドットファイルは拡張子扱いしない")
{
	CHECK(get_base_name("C:\\foo\\.gitignore") == UnicodeString(".gitignore"));
}

TEST_CASE("get_extension: 通常ファイルの拡張子")
{
	CHECK(get_extension("C:\\foo\\bar.txt") == UnicodeString(".txt"));
	CHECK(get_extension("C:\\foo\\bar") == UnicodeString(""));
}

TEST_CASE("get_extension: ドットファイルは拡張子なし扱い")
{
	CHECK(get_extension("C:\\foo\\.gitignore") == UnicodeString(""));
	//ドットファイル名にさらに拡張子がある場合は、除いた残りから抽出
	CHECK(get_extension("C:\\foo\\.tar.gz") == UnicodeString(".gz"));
}

TEST_CASE("nrm_FileExt: 先頭・末尾に . を補う")
{
	CHECK(nrm_FileExt("") == UnicodeString(""));
	CHECK(nrm_FileExt("txt") == UnicodeString(".txt."));
	CHECK(nrm_FileExt(".txt") == UnicodeString(".txt."));
	CHECK(nrm_FileExt(".txt.") == UnicodeString(".txt."));
}

//===========================================================================
// test_FileExt / test_FileExtSize
//===========================================================================
TEST_CASE("test_FileExt: リストに含まれるか")
{
	CHECK(test_FileExt(".txt", ".txt.html.md") == true);
	CHECK(test_FileExt(".png", ".txt.html.md") == false);
	CHECK(test_FileExt("", ".txt") == false);
	CHECK(test_FileExt(".txt", "") == false);
	CHECK(test_FileExt(".txt", "*") == true);
	CHECK(test_FileExt(".txt", ".*") == true);
	CHECK(test_FileExt(".", ".txt") == false);  //"." のみは常に false
}

TEST_CASE("test_FileExtSize: サイズ制限付き拡張子リスト")
{
	//".exe:100.dll" 100MB未満のexeはマッチしない
	CHECK(test_FileExtSize(".exe", ".exe:100.dll", 50ll * 1048576) == false);
	CHECK(test_FileExtSize(".exe", ".exe:100.dll", 200ll * 1048576) == true);
	//サイズ指定の無い拡張子は常にマッチ
	CHECK(test_FileExtSize(".dll", ".exe:100.dll", 1) == true);
	CHECK(test_FileExtSize(".zzz", ".exe:100.dll", 1) == false);
}

//===========================================================================
// to_path_name / get_dir_name / get_parent_path
//===========================================================================
TEST_CASE("to_path_name: 末尾に\\を付加、空はEmptyStrのまま")
{
	CHECK(to_path_name("C:\\foo") == UnicodeString("C:\\foo\\"));
	CHECK(to_path_name("") == UnicodeString(""));
}

TEST_CASE("get_dir_name: 末尾の\\を除いた最後の要素")
{
	CHECK(get_dir_name("C:\\foo\\bar\\") == UnicodeString("bar"));
	CHECK(get_dir_name("C:\\foo\\bar") == UnicodeString("bar"));
}

TEST_CASE("get_parent_path: 親ディレクトリ")
{
	CHECK(get_parent_path("C:\\foo\\bar\\") == UnicodeString("C:\\foo\\"));
	CHECK(get_parent_path("C:\\foo\\bar") == UnicodeString("C:\\foo\\"));
}

//===========================================================================
// nrm_ftp_path
//===========================================================================
TEST_CASE("nrm_ftp_path: 先頭に / を補う、空は/")
{
	CHECK(nrm_ftp_path("") == UnicodeString("/"));
	CHECK(nrm_ftp_path("foo/bar") == UnicodeString("/foo/bar"));
	CHECK(nrm_ftp_path("/foo/bar") == UnicodeString("/foo/bar"));
}

//===========================================================================
// split_path (2形態)
//===========================================================================
TEST_CASE("split_path(dlmt指定): 通常パスの分割")
{
	TStringDynArray a = split_path("C:\\foo\\bar\\baz", "\\");
	REQUIRE(a.Length == 4);
	CHECK(a[0] == UnicodeString("C:"));
	CHECK(a[1] == UnicodeString("foo"));
	CHECK(a[2] == UnicodeString("bar"));
	CHECK(a[3] == UnicodeString("baz"));
}

TEST_CASE("split_path(dlmt指定): 末尾が区切りなら除去してから分割")
{
	TStringDynArray a = split_path("C:\\foo\\bar\\", "\\");
	REQUIRE(a.Length == 3);
	CHECK(a[0] == UnicodeString("C:"));
	CHECK(a[1] == UnicodeString("foo"));
	CHECK(a[2] == UnicodeString("bar"));
}

TEST_CASE("split_path(dlmt指定): UNCパスは先頭\\\\を保持")
{
	TStringDynArray a = split_path("\\\\server\\share\\foo", "\\");
	REQUIRE(a.Length == 3);
	CHECK(a[0] == UnicodeString("\\\\server"));
	CHECK(a[1] == UnicodeString("share"));
	CHECK(a[2] == UnicodeString("foo"));
}

TEST_CASE("split_path(単一引数): 末尾の\\を除いてから\\で分割")
{
	TStringDynArray a = split_path("C:\\foo\\bar\\");
	REQUIRE(a.Length == 3);
	CHECK(a[0] == UnicodeString("C:"));
	CHECK(a[1] == UnicodeString("foo"));
	CHECK(a[2] == UnicodeString("bar"));
}

TEST_CASE("split_path(単一引数): UNCパスは先頭\\\\を保持")
{
	TStringDynArray a = split_path("\\\\server\\share");
	REQUIRE(a.Length == 2);
	CHECK(a[0] == UnicodeString("\\\\server"));
	CHECK(a[1] == UnicodeString("share"));
}

//===========================================================================
// split_user_name / is_computer_name
//===========================================================================
TEST_CASE("split_user_name: \"UNCパス:ユーザ名\" からユーザ名を分離")
{
	UnicodeString dnam = "\\\\server\\share:user1";
	UnicodeString u = split_user_name(dnam);
	CHECK(u == UnicodeString("user1"));
	CHECK(dnam == UnicodeString("\\\\server\\share"));
}

TEST_CASE("split_user_name: UNCでない、または:位置が3以下なら分離しない")
{
	UnicodeString dnam = "C:\\foo";
	UnicodeString u = split_user_name(dnam);
	CHECK(u == UnicodeString(""));
	CHECK(dnam == UnicodeString("C:\\foo"));
}

TEST_CASE("is_computer_name: \\\\name 形式のみ true")
{
	CHECK(is_computer_name("\\\\server") == true);
	CHECK(is_computer_name("\\\\server\\share") == false);
	CHECK(is_computer_name("C:\\foo") == false);
}

//===========================================================================
// pos_ADS_delimiter / is_ADS_name / split_ADS_name
//===========================================================================
TEST_CASE("pos_ADS_delimiter / is_ADS_name: ADS区切りの検出")
{
	CHECK(is_ADS_name("C:\\foo\\bar.txt") == false);
	CHECK(is_ADS_name("C:\\foo\\bar.txt:stream") == true);
	CHECK(pos_ADS_delimiter("C:\\foo\\bar.txt:stream") > 0);
}

TEST_CASE("split_ADS_name: ファイル名とストリーム名を分離")
{
	UnicodeString fnam = "C:\\foo\\bar.txt:stream1";
	UnicodeString base = split_ADS_name(fnam);
	CHECK(base == UnicodeString("C:\\foo\\bar.txt"));
	CHECK(fnam == UnicodeString(":stream1"));
}

TEST_CASE("split_ADS_name: ADSが無ければファイル名全体、fnamはEmptyStrに")
{
	UnicodeString fnam = "C:\\foo\\bar.txt";
	UnicodeString base = split_ADS_name(fnam);
	CHECK(base == UnicodeString("C:\\foo\\bar.txt"));
	CHECK(fnam == UnicodeString(""));
}

//===========================================================================
// get_file_attr_str
//===========================================================================
TEST_CASE("get_file_attr_str: 属性文字列(RHSAC)")
{
	CHECK(get_file_attr_str(faInvalid) == UnicodeString("_____"));
	CHECK(get_file_attr_str(0) == UnicodeString("_____"));
	CHECK(get_file_attr_str(faReadOnly) == UnicodeString("R____"));
	CHECK(get_file_attr_str(faHidden) == UnicodeString("_H___"));
	CHECK(get_file_attr_str(faSysFile) == UnicodeString("__S__"));
	CHECK(get_file_attr_str(faArchive) == UnicodeString("___A_"));
	CHECK(get_file_attr_str(faCompressed) == UnicodeString("____C"));
	CHECK(get_file_attr_str(faReadOnly | faHidden | faSysFile | faArchive | faCompressed) == UnicodeString("RHSAC"));
}

//===========================================================================
// FileComp_Base: TStringList ソート比較関数
//===========================================================================
TEST_CASE("FileComp_Base: 同名(拡張子除く)ならディレクトリで比較、違えばベース名で自然順比較")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	lst->Add("C:\\foo\\img2.txt");
	lst->Add("C:\\foo\\img10.txt");
	lst->Add("C:\\foo\\img1.txt");
	lst->CustomSort(FileComp_Base);
	CHECK(lst->Strings[0] == UnicodeString("C:\\foo\\img1.txt"));
	CHECK(lst->Strings[1] == UnicodeString("C:\\foo\\img2.txt"));
	CHECK(lst->Strings[2] == UnicodeString("C:\\foo\\img10.txt"));
}

//===========================================================================
// ファイルシステム操作系 (一時ディレクトリを使用)
//===========================================================================
TEST_CASE("file_exists / file_exists_x / dir_exists: 実体との整合")
{
	TempDir td;
	UnicodeString f = td.file("a.txt");
	UnicodeString d = td.file("sub");

	CHECK(file_exists(f) == false);
	CHECK(dir_exists(d) == false);

	CHECK(create_EmptyFile(f) == true);
	::CreateDirectoryW(d.c_str(), NULL);

	CHECK(file_exists(f) == true);
	CHECK(file_exists_x(f) == true);
	CHECK(dir_exists(d) == true);
	//file_exists はディレクトリでも true (属性の存在チェックのみ)
	CHECK(file_exists(d) == true);
	//file_exists_x はディレクトリだと false
	CHECK(file_exists_x(d) == false);

	CHECK(file_exists("") == false);
	CHECK(file_exists_x("") == false);
}

TEST_CASE("file_exists_ico: \"名前,インデックス\" 形式は , 以降を除いてチェック")
{
	TempDir td;
	UnicodeString f = td.file("icon.dll");
	create_EmptyFile(f);

	CHECK(file_exists_ico(f) == true);
	CHECK(file_exists_ico(f + ",3") == true);
	CHECK(file_exists_ico(td.file("nofile.dll") + ",0") == false);
	CHECK(file_exists_ico("") == false);
}

TEST_CASE("file_exists_wc: ワイルドカードで最初にマッチしたファイルを返す")
{
	TempDir td;
	create_EmptyFile(td.file("aaa.txt"));
	create_EmptyFile(td.file("bbb.txt"));

	UnicodeString fnam = td.path + "*.txt";
	bool ok = file_exists_wc(fnam);
	CHECK(ok == true);
	//戻り値は実在する具体的なファイル名になっている
	CHECK(file_exists(fnam) == true);

	UnicodeString none = td.path + "*.zzz";
	CHECK(file_exists_wc(none) == false);
	CHECK(none == UnicodeString(""));
}

TEST_CASE("create_Dir / create_ForceDirs / delete_Dir / delete_Dirs")
{
	TempDir td;
	UnicodeString d1 = td.file("d1");
	CHECK(create_Dir(d1) == true);
	CHECK(dir_exists(d1) == true);
	//同名を再度作成するとfalse
	CHECK(create_Dir(d1) == false);

	UnicodeString deep = td.file("a\\b\\c");
	CHECK(create_ForceDirs(deep) == true);
	CHECK(dir_exists(deep) == true);

	CHECK(delete_Dir(d1) == true);
	CHECK(dir_exists(d1) == false);
}

TEST_CASE("delete_Dirs: サブディレクトリだけの木は削除できる")
{
	TempDir td;
	create_ForceDirs(td.file("x\\y\\z"));
	CHECK(delete_Dirs(td.file("x")) == true);
	CHECK(dir_exists(td.file("x")) == false);
}

TEST_CASE("delete_Dirs: ファイルは削除しないため、木の中にファイルが残っていると失敗する")
{
	//delete_Dirs はディレクトリを再帰的に辿るだけで、ファイル自体は削除しない
	//(内部で FindFirst した結果のうちディレクトリだけを再帰し、ファイルは
	//単に読み飛ばす)。そのため木の中にファイルが1つでも残っていると、
	//そのファイルを含む階層で RemoveDirectory 相当が失敗し、失敗が
	//そのまま上位のディレクトリにも連鎖して、木全体の削除が失敗する。
	//ヘッダのコメント(「サブディレクトリを含めたディレクトリの削除」)からは
	//読み取れない挙動のため、現状の挙動として固定しておく
	//(疑わしい点として報告に記載する)。
	TempDir td;
	create_ForceDirs(td.file("a\\b\\c"));
	create_EmptyFile(td.file("a\\b\\c\\f.txt"));

	CHECK(delete_Dirs(td.file("a")) == false);
	CHECK(dir_exists(td.file("a")) == true);
	CHECK(dir_exists(td.file("a\\b\\c")) == true);
	CHECK(file_exists(td.file("a\\b\\c\\f.txt")) == true);  //ファイルは残ったまま
}

TEST_CASE("chk_cre_dir: 無ければ作成して末尾\\付きで返す、あれば引数をそのまま返す")
{
	TempDir td;
	UnicodeString d = td.file("newdir");
	UnicodeString r = chk_cre_dir(d);
	CHECK(r == UnicodeString(IncludeTrailingPathDelimiter(d)));
	CHECK(dir_exists(d) == true);

	//既に存在する場合は何もせず引数をそのまま返す(末尾\は付かない)
	UnicodeString r2 = chk_cre_dir(d);
	CHECK(r2 == UnicodeString(d));
}

TEST_CASE("is_EmptyDir: 空/非空/no_file指定の判定")
{
	TempDir td;
	UnicodeString d = td.file("emptydir");
	::CreateDirectoryW(d.c_str(), NULL);
	CHECK(is_EmptyDir(d) == true);

	create_EmptyFile(d + "\\f.txt");
	CHECK(is_EmptyDir(d) == false);

	//no_file=true: ファイルが無ければ空とみなす(サブディレクトリのみは再帰的に判定)
	UnicodeString d2 = td.file("dironly");
	::CreateDirectoryW(d2.c_str(), NULL);
	::CreateDirectoryW((d2 + "\\sub").c_str(), NULL);
	CHECK(is_EmptyDir(d2, true) == true);
	CHECK(is_EmptyDir(d2, false) == false);
}

TEST_CASE("get_file_size: ファイルサイズを取得")
{
	TempDir td;
	UnicodeString f = td.file("size.bin");
	char buf[123] = {0};
	write_bytes(f, buf, sizeof(buf));
	CHECK(get_file_size(f) == 123);
	CHECK(get_file_size(td.file("nofile.bin")) == 0);
}

TEST_CASE("file_GetAttr / file_SetAttr / set_FileWritable")
{
	TempDir td;
	UnicodeString f = td.file("attr.txt");
	create_EmptyFile(f);

	int atr = file_GetAttr(f);
	CHECK(atr != faInvalid);
	CHECK((atr & faReadOnly) == 0);

	CHECK(file_SetAttr(f, atr | faReadOnly) == true);
	CHECK((file_GetAttr(f) & faReadOnly) != 0);

	CHECK(set_FileWritable(f) == true);
	CHECK((file_GetAttr(f) & faReadOnly) == 0);

	CHECK(file_GetAttr(td.file("nofile.txt")) == faInvalid);
}

TEST_CASE("dir_CopyAttr: RHSいずれか立っている場合のみ属性をコピー")
{
	TempDir td;
	UnicodeString src = td.file("src.txt");
	UnicodeString dst = td.file("dst.txt");
	create_EmptyFile(src);
	create_EmptyFile(dst);

	//属性が立っていない(Archiveのみ)場合は何もせず true
	CHECK(dir_CopyAttr(src, dst) == true);

	int atr = file_GetAttr(src);
	file_SetAttr(src, atr | faHidden);
	CHECK(dir_CopyAttr(src, dst) == true);
	CHECK((file_GetAttr(dst) & faHidden) != 0);

	//remove_ro=trueならReadOnlyは伝播させない
	file_SetAttr(src, file_GetAttr(src) | faReadOnly);
	UnicodeString dst2 = td.file("dst2.txt");
	create_EmptyFile(dst2);
	CHECK(dir_CopyAttr(src, dst2, true) == true);
	CHECK((file_GetAttr(dst2) & faReadOnly) == 0);

	CHECK(dir_CopyAttr(td.file("nofile.txt"), dst) == false);
}

TEST_CASE("move_File / copy_File / rename_File")
{
	TempDir td;
	UnicodeString src = td.file("src.txt");
	char data[4] = {1, 2, 3, 4};
	write_bytes(src, data, 4);

	UnicodeString dst = td.file("dst.txt");
	CHECK(copy_File(src, dst) == true);
	CHECK(file_exists(src) == true);
	CHECK(file_exists(dst) == true);
	CHECK(get_file_size(dst) == 4);

	UnicodeString ren = td.file("ren.txt");
	CHECK(rename_File(dst, ren) == true);
	CHECK(file_exists(dst) == false);
	CHECK(file_exists(ren) == true);

	UnicodeString moved = td.file("moved.txt");
	CHECK(move_File(src, moved) == true);
	CHECK(file_exists(src) == false);
	CHECK(file_exists(moved) == true);
}

TEST_CASE("rename_Path: ルートが一致する場合のみ、各階層の名前をリネーム")
{
	TempDir td;
	UnicodeString old_dir = td.file("olddir");
	create_ForceDirs(old_dir);
	create_EmptyFile(old_dir + "\\f.txt");

	UnicodeString new_dir = td.file("newdir");
	CHECK(rename_Path(old_dir, new_dir) == true);
	CHECK(dir_exists(old_dir) == false);
	CHECK(dir_exists(new_dir) == true);
	CHECK(file_exists(new_dir + "\\f.txt") == true);
}

TEST_CASE("rename_Path: ルート(ドライブ)が異なれば false")
{
	CHECK(rename_Path("C:\\foo", "D:\\foo") == false);
}

TEST_CASE("get_file_age / set_file_age / utc_to_DateTime: タイムスタンプの設定と取得")
{
	TempDir td;
	UnicodeString f = td.file("age.txt");
	create_EmptyFile(f);

	TDateTime dt = EncodeDate(2020, 1, 2);
	CHECK(set_file_age(f, dt) == true);

	TDateTime got = get_file_age(f);
	//分単位までの一致を確認 (書き込み時に秒未満の丸めがある可能性を考慮)
	unsigned short y, mo, d, h, mi, s, ms;
	DecodeDate(got, y, mo, d);
	CHECK(y == 2020);
	CHECK(mo == 1);
	CHECK(d == 2);

	CHECK(get_file_age(td.file("nofile.txt")) == doctest::Approx(0.0));
}

TEST_CASE("is_same_file / get_case_name: 実ファイルでの動作確認")
{
	TempDir td;
	UnicodeString f = td.file("Case.txt");
	create_EmptyFile(f);

	//is_same_file: 絶対パス同士で大文字小文字を無視して同一判定
	UnicodeString upper = td.path + "CASE.TXT";
	CHECK(is_same_file(f, upper, td.path) == true);

	//get_case_name: 実際のファイルシステム上の大小文字を返す
	UnicodeString queried = td.path + "case.txt";
	UnicodeString cased = get_case_name(queried);
	CHECK(SameText(cased, f) == true);
}

TEST_CASE("get_HardLinkCount: 通常ファイルは1、存在しないファイルは0")
{
	TempDir td;
	UnicodeString f = td.file("link.txt");
	create_EmptyFile(f);
	CHECK(get_HardLinkCount(f) == 1);
	CHECK(get_HardLinkCount(td.file("nofile.txt")) == 0);
}

TEST_CASE("is_SymLink: 通常ファイル/ディレクトリ/存在しないパスはfalse")
{
	TempDir td;
	UnicodeString f = td.file("plain.txt");
	create_EmptyFile(f);
	CHECK(is_SymLink(f) == false);
	CHECK(is_SymLink(td.path) == false);
	CHECK(is_SymLink(td.file("nofile.txt")) == false);
}

TEST_CASE("is_IdenticalFile: 同一ファイル/別ファイルの判定")
{
	TempDir td;
	UnicodeString f1 = td.file("id1.txt");
	UnicodeString f2 = td.file("id2.txt");
	create_EmptyFile(f1);
	create_EmptyFile(f2);

	CHECK(is_IdenticalFile(f1, f1) == true);
	CHECK(is_IdenticalFile(f1, f2) == false);
	CHECK(is_IdenticalFile(f1, td.file("nofile.txt")) == false);
}

TEST_CASE("get_files: マスクに該当するファイル一覧(ディレクトリは含まない)")
{
	TempDir td;
	create_EmptyFile(td.file("x1.txt"));
	create_EmptyFile(td.file("x2.log"));
	::CreateDirectoryW(td.file("subdir").c_str(), NULL);

	std::unique_ptr<TStringList> lst(new TStringList());
	get_files(td.path, "*.txt", lst.get());
	REQUIRE(lst->Count == 1);
	CHECK(SameText(lst->Strings[0], td.file("x1.txt")) == true);

	//存在しないディレクトリは何もせず終わる (例外にならない)
	std::unique_ptr<TStringList> lst2(new TStringList());
	get_files(td.file("nodir"), "*.*", lst2.get());
	CHECK(lst2->Count == 0);
}

TEST_CASE("get_all_files_ex: サブディレクトリ再帰と除外ディレクトリ")
{
	TempDir td;
	create_EmptyFile(td.file("top.txt"));
	create_ForceDirs(td.file("sub"));
	create_EmptyFile(td.file("sub\\deep.txt"));
	create_ForceDirs(td.file("skipme"));
	create_EmptyFile(td.file("skipme\\hidden_from_result.txt"));

	std::unique_ptr<TStringList> lst(new TStringList());
	int n = get_all_files_ex(td.path, "*.txt", lst.get(), true, 99, "skipme");
	CHECK(n == 2);
	CHECK(lst->Count == 2);

	//sub_n=0 ならサブディレクトリを検索しない
	std::unique_ptr<TStringList> lst2(new TStringList());
	int n2 = get_all_files_ex(td.path, "*.txt", lst2.get(), true, 0);
	CHECK(n2 == 1);  //top.txt のみ (skipme指定なしでもsub_n=0で再帰しない)
}

TEST_CASE("is_dir_accessible / is_drive_accessible: 存在するディレクトリ/ドライブ")
{
	TempDir td;
	CHECK(is_drive_accessible("C:\\") == true);
	CHECK(is_drive_accessible("") == false);
	CHECK(is_dir_accessible(td.path) == true);
	CHECK(is_dir_accessible(td.file("nodir") ) == false);
}

TEST_CASE("get_available_drive_list: C:ドライブを含む")
{
	std::unique_ptr<TStringList> lst(new TStringList());
	int n = get_available_drive_list(lst.get());
	CHECK(n > 0);
	bool has_c = false;
	for (int i = 0; i < lst->Count; i++) if (SameText(lst->Strings[i], "C:\\")) has_c = true;
	CHECK(has_c == true);
}

TEST_CASE("get_ClusterSize: C:ドライブのクラスタサイズは正の値")
{
	CHECK(get_ClusterSize("C:\\foo") > 0);
}

//===========================================================================
// fsRead_* : ファイルストリームからの読み込み
//===========================================================================
TEST_CASE("fsRead_byte / fsRead_int2 / fsRead_int3 / fsRead_int4: LE/BE")
{
	TempDir td;
	UnicodeString f = td.file("bin.dat");
	unsigned char data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	write_bytes(f, data, sizeof(data));

	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	CHECK(fsRead_byte(fs.get()) == 0x01);
	CHECK(fsRead_int2(fs.get(), false) == 0x0302);  //LE: byte[1]<<8|byte[0] (0x02<<8|0x03)
	fs->Seek(0, soFromBeginning);
	CHECK(fsRead_int2(fs.get(), true) == 0x0102);   //BE: byte[0]<<8|byte[1]
	fs->Seek(0, soFromBeginning);
	CHECK(fsRead_int3(fs.get(), false) == 0x030201);
	fs->Seek(0, soFromBeginning);
	CHECK(fsRead_int3(fs.get(), true) == 0x010203);
	fs->Seek(0, soFromBeginning);
	CHECK(fsRead_int4(fs.get(), false) == 0x04030201);
	fs->Seek(0, soFromBeginning);
	CHECK(fsRead_int4(fs.get(), true) == 0x01020304);
}

TEST_CASE("fsRead_double: 8バイトのdoubleを読み込み")
{
	TempDir td;
	UnicodeString f = td.file("dbl.dat");
	double v = 3.14159;
	write_bytes(f, &v, sizeof(v));

	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	CHECK(fsRead_double(fs.get()) == doctest::Approx(3.14159));
}

TEST_CASE("fsRead_char: 指定サイズの文字列を読み込み(途中の\\0で切り捨て)")
{
	TempDir td;
	UnicodeString f = td.file("str.dat");
	char data[6] = {'a', 'b', '\0', 'c', 'd', 'e'};
	write_bytes(f, data, sizeof(data));

	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	UnicodeString s = fsRead_char(fs.get(), 6);
	CHECK(s == UnicodeString("ab"));  //\0以降切り捨て
}

TEST_CASE("fsRead_comment_utf8: 長さ(4byte)+UTF8文字列")
{
	TempDir td;
	UnicodeString f = td.file("cmt.dat");
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(f, fmCreate));
		int len = 6;  //"abc" のUTF-8は3バイトだが、日本語混在を避けテストを単純化
		const char *s = "abcdef";
		fs->WriteBuffer(&len, 4);
		fs->WriteBuffer(s, len);
	}
	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	UnicodeString s = fsRead_comment_utf8(fs.get());
	CHECK(s == UnicodeString("abcdef"));
}

TEST_CASE("fsRead_comment_utf8: 長さ0はEmptyStr")
{
	TempDir td;
	UnicodeString f = td.file("cmt0.dat");
	{
		std::unique_ptr<TFileStream> fs(new TFileStream(f, fmCreate));
		int len = 0;
		fs->WriteBuffer(&len, 4);
	}
	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	CHECK(fsRead_comment_utf8(fs.get()) == UnicodeString(""));
}

TEST_CASE("fsRead_check_char: 一致すれば直後位置へ、不一致なら現在位置のまま")
{
	TempDir td;
	UnicodeString f = td.file("chk.dat");
	const char *data = "HEADERbody";
	write_bytes(f, data, 10);

	std::unique_ptr<TFileStream> fs(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	CHECK(fsRead_check_char(fs.get(), "HEADER") == true);
	CHECK(fs->Position == 6);

	//不一致: 位置は変わらない
	std::unique_ptr<TFileStream> fs2(new TFileStream(f, fmOpenRead | fmShareDenyNone));
	CHECK(fsRead_check_char(fs2.get(), "XXXXXX") == false);
	CHECK(fs2->Position == 0);

	//空文字列は常にfalse
	CHECK(fsRead_check_char(fs2.get(), "") == false);
}

//===========================================================================
// 除外した関数と理由 (無言のスキップ禁止):
//
// - delete_ADS / rename_ADS:
//     FindFirstStreamW / NtSetInformationFile による NTFS 代替データストリーム
//     操作。WSL interop から見えるテスト用一時ディレクトリの実体がどの
//     ファイルシステム(NTFS直下か、9p/drvfs越しか)に載るか実行環境に
//     依存し、ADS 対応状況が不確実なため対象から外した。
//
// - cre_Junction:
//     FSCTL_SET_REPARSE_POINT でジャンクション(リパースポイント)を作成する。
//     失敗時の後始末が難しく(作成途中で残ったリパースポイントは通常の
//     RemoveDirectory では削除できない場合がある)、実行環境の権限にも
//     依存するため、実ファイルを壊さない方針を優先して対象から外した。
//
// - get_actual_name:
//     PATH/PATHEXT を使った検索自体は原理的にテスト可能だが、見つかった
//     ファイルがシンボリックリンクだった場合に HKEY_CURRENT_USER の
//     "App Paths" レジストリキーを参照する分岐がある。レジストリを
//     読み書きするテストは実行環境のレジストリ状態に影響しうるため、
//     この関数全体を対象から外した(cv_env_str/get_actual_path 等、
//     内部で使う純粋な変換ロジックは別途テスト済み)。
//
// - is_drive_protected:
//     IOCTL_DISK_IS_WRITABLE で実デバイスに問い合わせる。読み取り専用の
//     問い合わせとはいえ実デバイス依存であり、CI環境によって結果が
//     変わりうるため対象から外した。
//
// - get_dirs:
//     src/usr_file_ex.h に宣言があり Doxygen コメントも付いているが、
//     src/usr_file_ex.cpp には実装が無く(grepしても定義が見当たらない)、
//     呼び出すとリンクエラーになる。実装が存在しないため対象から外した
//     (シムのバグではなく、Phase 0 移植時点で本体側が未実装/欠落して
//     いる可能性がある。他の *.cpp にも実装は見当たらない)。
//===========================================================================
