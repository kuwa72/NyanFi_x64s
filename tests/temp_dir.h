/**
 * @file tests/temp_dir.h
 * @brief テスト用の一時ディレクトリ (RAII) ヘルパー
 *
 * tests/core/test_usr_file_ex.cpp が定義しているものと同じ考え方の
 * ヘルパーを、Phase 1 (issue #1) で追加したテスト (UIniFile / usr_tag /
 * usr_highlight) から共有するために切り出したもの。
 *
 * ファイルシステムに触れるテストは、TempDir が作る一時ディレクトリの中だけで
 * 行うこと。後始末には対象コードの delete 系関数を使わず、素の Win32 API
 * (FindFirstFileW/DeleteFileW/RemoveDirectoryW) を直接使う (テスト対象の
 * 不具合とテスト自身の後始末の不具合を切り分けるため)。
 */
#ifndef NYANFI_TESTS_TEMP_DIR_H
#define NYANFI_TESTS_TEMP_DIR_H

#include "compat/config.h"
#include "usr_str.h"

namespace nyanfi_test {

inline void remove_all_recursive(const UnicodeString &path_with_delim)
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

}  // namespace nyanfi_test

#endif  // NYANFI_TESTS_TEMP_DIR_H
