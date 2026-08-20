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
# ランタイム DLL への依存を避ける。`-static` (システムライブラリまで静的) は
# ここでは付けない: wxWidgets が共有ライブラリとして提供される環境 (MSYS2 の
# パッケージ) では、インポートライブラリを使えなくなって `__imp_...` の未定義
# 参照になる。完全静的にしたいのはテスト用の実行ファイルだけなので、そちらで
# 個別に指定する (tests/CMakeLists.txt)。
if(MINGW AND NYANFI_STATIC_LINK)
	add_link_options(-static-libgcc -static-libstdc++)
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
	# gui/usr_shell_fmt_shim.cpp (add_PropLine 等、usr_shell.h の書式化関数だけの
	# 複製。詳細はそのファイルのコメントを参照) は、静的ライブラリの循環参照を
	# 避けるため NYANFI_PHASE0_SOURCES (src/ の一覧) には入れず、ここで直接
	# nyanfi_core に加える。
	# 理由: src/usr_file_inf.cpp (get_ExifInf 等) がこれらの関数を必要とするが、
	# もし nyanfi_gui_core 側に置くと「nyanfi_core → (このシム経由で)
	# nyanfi_gui_core → nyanfi_core」という2つの静的ライブラリをまたぐ循環に
	# なる。GNU ld は同一アーカイブ内なら未解決シンボルが増えるたびに再走査するが、
	# 一度通り過ぎた別アーカイブへは戻らないため、この循環は解決できずリンクに
	# 失敗する (実際に発生した。gui/CMakeLists.txt 経由の nyanfi 本体と
	# core_tests の両方で再現)。usr_file_inf.cpp.obj と同じアーカイブ
	# (nyanfi_core) に置けば、同一アーカイブ内の再走査で解決する。
	# gui/usr_file_inf_link_shim.cpp も同じ理由でここに加える。usr_file_inf.cpp
	# が (呼ばない関数を含めて) 参照する LoadUsrMsg/UserAbort/TextAbort
	# (src/usr_msg.h、GUI 依存で未移植) と UserShell::get_PropInf/get_Duration・
	# TMetafile::LoadFromFile (呼ばれたら落とす未移植スタブ) を解決する。
	# 詳細はそのファイルのコメントを参照。imagehlp は get_DllExpFunc、winmm は
	# get_duration (mciSendStringW) が使う実在の Win32 API のリンクに必要
	add_library(nyanfi_core STATIC ${NYANFI_PHASE0_SOURCES} gui/usr_shell_fmt_shim.cpp gui/usr_file_inf_link_shim.cpp)
	target_include_directories(nyanfi_core PUBLIC src)
	target_link_libraries(nyanfi_core PUBLIC nyanfi_compat nyanfi_legacy_flags imagehlp winmm)
	nyanfi_target_vcl_shim(nyanfi_core)
else()
	message(STATUS "NYANFI_PHASE0_SOURCES が空のため nyanfi_core はスキップします")
endif()

#-----------------------------------------------------------------------------
# GUI の設定永続化・キー割り当て解析・並べ替え/マスク絞り込み (wx 非依存の部分)
#
# gui/settings.cpp (ini 永続化) と gui/key_map.cpp (割り当て表・ini 読み込み)、
# gui/file_item.cpp (一覧の並べ替え比較・パスマスク照合の純粋ロジック) は
# UsrIniFile と usr_str.h 系の API しか使わず wx に依存しないため、
# NYANFI_BUILD_GUI=OFF (wxWidgets 未導入) でもビルド・テストできるよう、
# GUI 本体 (gui/CMakeLists.txt, add_subdirectory(gui) は下記) とは切り離して
# ここでライブラリ化する。tests/core/test_gui_settings.cpp・
# tests/core/test_gui_file_item.cpp がこれを使う。
# wx に依存する gui/key_map_wx.cpp (KeyMap::KeyStrOf) はここに含めない。
#
# gui/file_ops.cpp (コピー・移動・名前変更・ディレクトリ作成・ゴミ箱送り) も
# 同じ理由 (wx 非依存、tests/core/test_gui_file_ops.cpp から直接テストする)
# でここに含める。ゴミ箱送りに使う SHFileOperationW (shell32.dll) のためだけ
# shell32 をリンクする。
#
# gui/file_open.cpp (関連付けで開く/アプリケーションから開く) と
# gui/file_info.cpp (ファイル情報の組み立て) も同じ理由でここに含める。
# gui/file_info.cpp が呼ぶ src/usr_file_inf.cpp の解析関数群が必要とする
# add_PropLine 等の書式化関数 (gui/usr_shell_fmt_shim.cpp) は、静的ライブラリの
# 循環参照を避けるためここではなく nyanfi_core 側に置く (上の nyanfi_core の
# コメントを参照)。nyanfi_gui_core は nyanfi_core を PUBLIC リンクしているので
# 追加の設定なしで使える。
#-----------------------------------------------------------------------------
if(TARGET nyanfi_core)
	add_library(nyanfi_gui_core STATIC
		gui/settings.cpp
		gui/key_map.cpp
		gui/file_item.cpp
		gui/file_ops.cpp
		gui/file_open.cpp
		gui/file_info.cpp
	)
	target_include_directories(nyanfi_gui_core PUBLIC ${PROJECT_SOURCE_DIR})
	target_link_libraries(nyanfi_gui_core PUBLIC nyanfi_core shell32)
	nyanfi_target_vcl_shim(nyanfi_gui_core)
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
