/**
 * @file tests/core/test_link_stubs.cpp
 * @brief core_tests 用のリンク専用スタブ
 *
 * @details
 * compat/include/compat/gui_stubs.h は意図的に「宣言のみ」にしてある
 * (TWinControl::LockDrawing/UnlockDrawing, TControl::Perform,
 * TDirect2DCanvas のコンストラクタ/Supported())。これらは GUI 依存の
 * パスが実際に呼ばれたときにリンクエラーで気付けるようにする安全策である。
 *
 * ところが、これらを呼び出す関数 (usr_key.cpp の assign_KeyList/
 * perform_Key、usr_str.cpp の get_WidthInPanel) は、テスト対象の
 * 純粋な文字列関数 (get_KeyStr など) と同じ翻訳単位 (同じ .cpp) に
 * 定義されている。静的ライブラリは .o (翻訳単位) 単位でリンクされる
 * ため、get_KeyStr 等を1つでも参照すると usr_key.cpp.o 全体が
 * リンクに引き込まれ、実際には呼び出していない assign_KeyList/
 * perform_Key 内の未定義参照 (TWinControl::LockDrawing 等) まで
 * 解決を要求されてしまう (-ffunction-sections / --gc-sections が
 * ビルド設定に無いため、関数単位でのデッドコード除去が効かない)。
 * このビルド設定 (CMakeLists.txt / scripts/) は担当外のため変更できない。
 *
 * そのため、テストバイナリ (core_tests) のリンクを通すためだけに、
 * ここで最小限のスタブ定義を用意する。実際にテストから呼び出すことは
 * 無い経路なので、万一呼ばれたら分かるように abort() させる
 * (「呼んでも動いてしまう」no-op にはしない、という gui_stubs.h の
 * 設計方針を踏襲する)。
 *
 * 本番の compat/ 実装 (Phase 2 で wxWidgets 版に置き換わる予定) には
 * 一切影響しない。これはテストバイナリ限定のリンク成立用スタブである。
 *
 * Phase 1 (issue #1) で追加: UIniFile.cpp::LoadPosInfo(TForm*, bool, ...) が
 * UserFunc.h::adjust_form_pos(TForm*) (GUI 依存、宣言のみ) を呼ぶ。
 * test_UIniFile.cpp はこのオーバーロードを直接は呼ばないが、同じ .o に
 * ある ReadString 等を参照すると UIniFile.cpp.o 全体がリンクに引き込まれる
 * ため、同じ理由でスタブが要る。
 */
#include <cstdlib>

#include "UserFunc.h"

void adjust_form_pos(TForm *frm)
{
	(void)frm;
	std::abort();  //テストからは呼ばれない経路 (LoadPosInfo(TForm*, bool, ...) は対象外)
}

void TWinControl::LockDrawing()
{
	std::abort();  //テストからは呼ばれない経路 (assign_KeyList は対象外)
}

void TWinControl::UnlockDrawing()
{
	std::abort();
}

NativeInt TControl::Perform(unsigned msg, NativeInt wParam, NativeInt lParam)
{
	(void)msg; (void)wParam; (void)lParam;
	std::abort();  //テストからは呼ばれない経路 (perform_Key は対象外)
}

TDirect2DCanvas::TDirect2DCanvas(HDC dc, const TRect &rect) : TCanvas()
{
	(void)dc; (void)rect;
	std::abort();  //テストからは呼ばれない経路 (get_WidthInPanel の d2d_sw=true は対象外)
}

bool TDirect2DCanvas::Supported()
{
	return false;  //get_WidthInPanel(d2d_sw=true) 以外からは参照されない
}
