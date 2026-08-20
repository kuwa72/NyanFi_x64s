# NyanFi の GUI (issue #1 Phase 2 の骨格)
#
# VCL の 76 フォームを置き換えるものではなく、「移植済みのロジック層だけで
# ファイラとして最低限動く」ことを確かめるための土台。
#
# wxWidgets は同じツールチェインでビルドしたものを使う (CRT と例外モデルを
# 揃えるため)。導入は scripts/build_wx.sh。
#
#   cmake -B build -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake \
#     -DNYANFI_BUILD_GUI=ON -DWX_CONFIG=$HOME/opt/wx-3.3.3-mingw64/bin/wx-config
#
# 探索の方針: CMake の FindwxWidgets は Windows ターゲットだと wx-config を見ずに
# ディレクトリ構成を探すため、クロスビルドでは使えない。wx-config を直接叩く。

find_program(WX_CONFIG
	NAMES wx-config
	HINTS ENV WX_PREFIX
	PATH_SUFFIXES bin
	DOC "wxWidgets の wx-config"
)

if(NOT WX_CONFIG)
	message(FATAL_ERROR
		"wx-config が見つかりません。scripts/build_wx.sh で wxWidgets を用意し、"
		"-DWX_CONFIG=<prefix>/bin/wx-config を指定してください")
endif()

# wx-config は #!/bin/sh のスクリプトで、Windows ネイティブの CMake は shebang を
# 解釈できず直接実行できない (MSYS2 の CI で全ての問い合わせが空振りした)。
# シェル経由で呼び、失敗したら黙って進まずエラーにする。
find_program(NYANFI_SH NAMES sh bash)

function(nyanfi_wx_config out_var)
	if(NYANFI_SH)
		execute_process(COMMAND "${NYANFI_SH}" "${WX_CONFIG}" ${ARGN}
			OUTPUT_VARIABLE result RESULT_VARIABLE rc
			OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_VARIABLE err)
	else()
		execute_process(COMMAND "${WX_CONFIG}" ${ARGN}
			OUTPUT_VARIABLE result RESULT_VARIABLE rc
			OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_VARIABLE err)
	endif()
	if(NOT rc EQUAL 0 OR result STREQUAL "")
		message(FATAL_ERROR "wx-config ${ARGN} が失敗しました (rc=${rc}): ${err}")
	endif()
	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

nyanfi_wx_config(WX_VERSION --version)
nyanfi_wx_config(WX_CXXFLAGS --cxxflags)
nyanfi_wx_config(WX_LIBS --libs core,base)

separate_arguments(WX_CXXFLAGS_LIST NATIVE_COMMAND "${WX_CXXFLAGS}")
separate_arguments(WX_LIBS_LIST NATIVE_COMMAND "${WX_LIBS}")

# windres 用のインクルードパス (wx/msw/wx.rc を絶対パスで取り込むため)
string(REGEX MATCHALL "-I[^ ]+" WX_INCLUDE_FLAGS "${WX_CXXFLAGS}")
string(REPLACE "-I" "" WX_INCLUDE_DIRS "${WX_INCLUDE_FLAGS}")

message(STATUS "wxWidgets ${WX_VERSION} (${WX_CONFIG})")

# wx.rc を絶対パスで取り込む (windres へのインクルードパスの渡り方が環境依存のため)
find_file(WX_MSW_RC
	NAMES wx/msw/wx.rc
	PATHS ${WX_INCLUDE_DIRS}
	NO_DEFAULT_PATH
)
if(NOT WX_MSW_RC)
	message(FATAL_ERROR "wx/msw/wx.rc が見つかりません (探索先: ${WX_INCLUDE_DIRS})")
endif()
file(TO_CMAKE_PATH "${WX_MSW_RC}" WX_MSW_RC)
message(STATUS "wx リソース: ${WX_MSW_RC}")
configure_file(nyanfi.rc.in "${CMAKE_CURRENT_BINARY_DIR}/nyanfi.rc" @ONLY)

# gui/settings.cpp と gui/key_map.cpp (wx 非依存の部分) は nyanfi_gui_core
# (ルート CMakeLists.txt) で既にビルドされているのでここでは加えない。
# wx に依存する KeyMap::KeyStrOf (gui/key_map_wx.cpp) だけをここで加える
add_executable(nyanfi
	"${CMAKE_CURRENT_BINARY_DIR}/nyanfi.rc"
	nyanfi_app.cpp
	main_frame.cpp
	file_pane.cpp
	file_info_panel.cpp
	text_viewer.cpp
	grep_dialog.cpp
	key_map_wx.cpp
	vcl_gui_bridge.cpp
)

# 既定は GUI サブシステム。診断時だけコンソールにして wxLog を端末で読む
# (GUI サブシステムだと起動時の例外もエラー出力も見えない)
option(NYANFI_GUI_CONSOLE "コンソールサブシステムでビルドする (診断用)" OFF)
if(NOT NYANFI_GUI_CONSOLE)
	set_target_properties(nyanfi PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(nyanfi PRIVATE ${PROJECT_SOURCE_DIR} ${WX_INCLUDE_DIRS})
target_compile_options(nyanfi PRIVATE ${WX_CXXFLAGS_LIST})

# wxWidgets は NDEBUG 付きでビルドしてある。こちら側の wxDEBUG_LEVEL が食い違うと
# wx の構造体の定義がずれるため、ビルド種別に関わらず揃えておく。
target_compile_definitions(nyanfi PRIVATE NDEBUG)

target_link_libraries(nyanfi PRIVATE nyanfi_gui_core nyanfi_core nyanfi_legacy_flags ${WX_LIBS_LIST})
nyanfi_target_vcl_shim(nyanfi)
