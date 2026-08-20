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

execute_process(COMMAND "${WX_CONFIG}" --version
	OUTPUT_VARIABLE WX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${WX_CONFIG}" --cxxflags
	OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${WX_CONFIG}" --libs core,base
	OUTPUT_VARIABLE WX_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
# windres 用のインクルードパス (wx/msw/wx.rc を取り込むため)
execute_process(COMMAND "${WX_CONFIG}" --cflags
	OUTPUT_VARIABLE WX_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCHALL "-I[^ ]+" WX_INCLUDE_FLAGS "${WX_CFLAGS}")
string(REPLACE "-I" "" WX_INCLUDE_DIRS "${WX_INCLUDE_FLAGS}")

separate_arguments(WX_CXXFLAGS_LIST NATIVE_COMMAND "${WX_CXXFLAGS}")
separate_arguments(WX_LIBS_LIST NATIVE_COMMAND "${WX_LIBS}")

message(STATUS "wxWidgets ${WX_VERSION} (${WX_CONFIG})")

add_executable(nyanfi
	nyanfi.rc
	nyanfi_app.cpp
	main_frame.cpp
	file_pane.cpp
	key_map.cpp
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

target_link_libraries(nyanfi PRIVATE nyanfi_core nyanfi_legacy_flags ${WX_LIBS_LIST})
nyanfi_target_vcl_shim(nyanfi)
