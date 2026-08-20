# NyanFi_x64s - OSS ツールチェイン移植 (issue #1) Phase 0
#
# 目的は「誰でも無償でビルドでき、CI が回り、特定ベンダの許諾に依存しない」こと。
# 対象は Win64 のみ (クロスプラットフォーム化は目的外)。
#
# 基準ツールチェイン: MSYS2 UCRT64 (mingw-w64 GCC)
#   cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
#
# Linux ホストからのクロスビルド (同じ mingw-w64 ターゲット):
#   cmake -B build -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake
#
# clang-cl を採らない理由: clang は UTF-8 以外の実行時文字コードを扱えず
# (`-fexec-charset=CP932` が invalid value)、非 ASCII の narrow リテラル
# 1,944箇所を wide へ機械変換しない限りビルドできない。GCC-mingw なら不要。
# 詳細は docs/port/phase0-report.md §4。

cmake_minimum_required(VERSION 3.24)

project(NyanFiPort LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
	message(FATAL_ERROR "Windows ターゲット専用です。--toolchain cmake/toolchain-mingw-w64.cmake を指定してください")
endif()

option(NYANFI_BUILD_TESTS "doctest によるテストをビルドする" ON)
option(NYANFI_BUILD_GUI "wxWidgets 版の GUI をビルドする (Phase 2 の骨格)" OFF)
option(NYANFI_STATIC_LINK "ランタイム DLL に依存しない実行ファイルを作る" ON)

# 実行ファイルを単体で配れるようにする (MSYS2 のネイティブビルドでも、
# Linux からのクロスビルドでも同じ条件にする)
if(MINGW AND NYANFI_STATIC_LINK)
	add_link_options(-static -static-libgcc -static-libstdc++)
endif()
if(MINGW)
	add_link_options(-Wl,--gc-sections)
endif()

#-----------------------------------------------------------------------------
# 共通のコンパイル設定
#-----------------------------------------------------------------------------
add_library(nyanfi_common_flags INTERFACE)
target_compile_definitions(nyanfi_common_flags INTERFACE
	UNICODE
	_UNICODE
	NOMINMAX
	WINVER=0x0601        # Windows 7 以降 (README の動作対象に合わせる)
	_WIN32_WINNT=0x0601
)
# 文字コード: ソースは UTF-8 (BOM 無し)。
# narrow リテラルの実行時文字コードは CP932 に固定する。C++Builder は narrow
# リテラルを CP932 として扱い、UnicodeString(const char*) は CP_ACP で変換して
# いた。書庫 DLL が返す char バッファ (実行時の CP932 データ) も同じ経路を通る
# ため、リテラル側を CP932 に合わせるのが唯一整合する選択になる。
#
# 留意: clang-cl は UTF-8 以外の実行時文字コードを扱えない。本番ターゲットへ
# 移る前に、非 ASCII を含む narrow リテラルを wide (_T(...)) へ機械変換する
# 必要がある。件数は docs/port/phase0-report.md に記録する。
if(MSVC)
	target_compile_options(nyanfi_common_flags INTERFACE /utf-8 /permissive-)
else()
	target_compile_options(nyanfi_common_flags INTERFACE
		-finput-charset=UTF-8
		-fexec-charset=CP932
		# 関数単位でセクションを分け、リンク時に未使用分を落として実行ファイルを小さく保つ。
		# 注意: これは「未使用関数が参照する未定義シンボル」を消してはくれない。
		# GNU ld のセクション回収はシンボル解決の後に走るため、未使用の関数でも
		# 参照先が無ければリンクエラーになる (gui/vcl_gui_bridge.cpp で対処)。
		-ffunction-sections
		-fdata-sections
	)
endif()

# 既存コードは TStringList の Objects スロットに 32bit 値を詰めて `(DWORD)obj` で
# 取り出す書き方をしている (実測 16箇所)。C++ としては ill-formed で、GCC も
# clang も既定ではエラーにする。src/ を変更しない方針のため、GCC 側は
# -fpermissive で降格させる。clang-cl には同等のオプションが無く、移行前に
# `(DWORD)(DWORD_PTR)obj` へ直す必要がある (docs/port/phase0-report.md に記録)。
add_library(nyanfi_legacy_flags INTERFACE)
if(NOT MSVC)
	target_compile_options(nyanfi_legacy_flags INTERFACE -fpermissive)
endif()

#-----------------------------------------------------------------------------
# VCL 互換シム
#-----------------------------------------------------------------------------
add_library(nyanfi_compat STATIC
	compat/src/application.cpp
	compat/src/ustring.cpp
	compat/src/sysutils.cpp
	compat/src/datetime.cpp
	compat/src/math.cpp
	compat/src/classes.cpp
	compat/src/streams.cpp
	compat/src/encoding.cpp
	compat/src/ioutils.cpp
	compat/src/regex.cpp
	compat/src/json.cpp
	compat/src/graphics.cpp
	compat/src/registry.cpp
)
target_include_directories(nyanfi_compat PUBLIC compat/include)
target_link_libraries(nyanfi_compat PUBLIC
	nyanfi_common_flags
	shlwapi
	ole32
	oleaut32
	uuid
	version
	advapi32
	gdi32
	user32
	rstrtmgr
)

#-----------------------------------------------------------------------------
# C++Builder の暗黙 vcl.h を再現する強制インクルードを付与する
#-----------------------------------------------------------------------------
function(nyanfi_target_vcl_shim target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /FI vcl_shim.h)
	else()
		target_compile_options(${target} PRIVATE -include vcl_shim.h)
	endif()
endfunction()

#-----------------------------------------------------------------------------
# Phase 0 のロジック層 (フォーム非依存のファイル群)
#-----------------------------------------------------------------------------
include(cmake/phase0_sources.cmake)

if(NYANFI_PHASE0_SOURCES)
	add_library(nyanfi_core STATIC ${NYANFI_PHASE0_SOURCES})
	target_include_directories(nyanfi_core PUBLIC src)
	target_link_libraries(nyanfi_core PUBLIC nyanfi_compat nyanfi_legacy_flags)
	nyanfi_target_vcl_shim(nyanfi_core)
else()
	message(STATUS "NYANFI_PHASE0_SOURCES が空のため nyanfi_core はスキップします")
endif()

#-----------------------------------------------------------------------------
# GUI (Phase 2 の骨格)
#-----------------------------------------------------------------------------
if(NYANFI_BUILD_GUI)
	if(NOT TARGET nyanfi_core)
		message(FATAL_ERROR "GUI は nyanfi_core を必要とします")
	endif()
	add_subdirectory(gui)
endif()

#-----------------------------------------------------------------------------
# テスト
#-----------------------------------------------------------------------------
if(NYANFI_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
