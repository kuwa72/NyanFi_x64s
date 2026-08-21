# doctest によるテスト
#
# tests/compat/test_*.cpp  シム自体のテスト
# tests/core/test_*.cpp    src/ の既存ロジックの回帰テスト
#
# ファイルを追加したら再 configure するだけでよいように GLOB を使う。

# テストの実行ファイルは単体で配れるように完全静的にする (wx を使わないので
# `-static` で問題が出ない)。GUI 側は共有ライブラリの wx もあり得るため付けない。
if(MINGW AND NYANFI_STATIC_LINK)
	add_link_options(-static)
endif()

add_library(doctest_main STATIC doctest_main.cpp)
target_include_directories(doctest_main PUBLIC ${PROJECT_SOURCE_DIR}/third_party ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(doctest_main PUBLIC nyanfi_common_flags)

file(GLOB NYANFI_COMPAT_TESTS CONFIGURE_DEPENDS compat/test_*.cpp)
if(NYANFI_COMPAT_TESTS)
	add_executable(compat_tests ${NYANFI_COMPAT_TESTS})
	target_link_libraries(compat_tests PRIVATE nyanfi_compat doctest_main)
	add_test(NAME compat_tests COMMAND compat_tests)
endif()

file(GLOB NYANFI_CORE_TESTS CONFIGURE_DEPENDS core/test_*.cpp)
if(NYANFI_CORE_TESTS AND TARGET nyanfi_core)
	add_executable(core_tests ${NYANFI_CORE_TESTS})
	target_link_libraries(core_tests PRIVATE nyanfi_core doctest_main)
	# gui/settings.cpp・gui/key_map.cpp (wx 非依存の ini 永続化/キー割り当て解析)
	# のテスト (tests/core/test_gui_settings.cpp) のため。NYANFI_BUILD_GUI=OFF
	# でも nyanfi_gui_core は常にビルドされる (ルート CMakeLists.txt 参照)
	if(TARGET nyanfi_gui_core)
		target_link_libraries(core_tests PRIVATE nyanfi_gui_core)
	endif()
	nyanfi_target_vcl_shim(core_tests)
	add_test(NAME core_tests COMMAND core_tests)
endif()
