/**
 * @file tests/core/test_gui_view_settings.cpp
 * @brief gui/view_settings.cpp (表示切り替えの引数解釈・計算) のテスト
 */
#include "doctest/doctest.h"

#include "gui/view_settings.h"

using namespace view_settings;

//===========================================================================
// トグル系
//===========================================================================

TEST_CASE("ParseToggle: ON/OFF トークンを認識する")
{
	CHECK(ParseToggle("ON")  == Toggle::On);
	CHECK(ParseToggle("OFF") == Toggle::Off);
	CHECK(ParseToggle("on")  == Toggle::On);	// 大小文字を区別しない (SameText)
	CHECK(ParseToggle("off") == Toggle::Off);
}

TEST_CASE("ParseToggle: 指定が無ければ Flip")
{
	CHECK(ParseToggle("") == Toggle::Flip);
	CHECK(ParseToggle("FD") == Toggle::Flip);	// ShowIcon 独自の "FD" はここでは無関係
}

TEST_CASE("ParseToggle: ON と OFF が両方あれば ON が優先される")
{
	// TestActionParam("ON") が先に評価される (MainFrm.cpp:12653 の三項式)
	CHECK(ParseToggle("ON;OFF") == Toggle::On);
}

TEST_CASE("ApplyToggle: On/Off は現在値に関わらず固定、Flip は反転する")
{
	CHECK(ApplyToggle(true,  Toggle::On)  == true);
	CHECK(ApplyToggle(false, Toggle::On)  == true);
	CHECK(ApplyToggle(true,  Toggle::Off) == false);
	CHECK(ApplyToggle(false, Toggle::Off) == false);
	CHECK(ApplyToggle(true,  Toggle::Flip)  == false);
	CHECK(ApplyToggle(false, Toggle::Flip)  == true);
}

//===========================================================================
// フォントサイズ
//===========================================================================

TEST_CASE("ClampFontSize: 範囲外は端で止まる")
{
	CHECK(ClampFontSize(10) == 10);
	CHECK(ClampFontSize(kMinFontSize) == kMinFontSize);
	CHECK(ClampFontSize(kMaxFontSize) == kMaxFontSize);
	CHECK(ClampFontSize(0) == kMinFontSize);
	CHECK(ClampFontSize(-100) == kMinFontSize);
	CHECK(ClampFontSize(1000) == kMaxFontSize);
}

TEST_CASE("AdjustFontSize: 1段ずつ動き、delta 自体も±12に丸められる")
{
	CHECK(AdjustFontSize(10, 1) == 11);
	CHECK(AdjustFontSize(10, -1) == 9);
	CHECK(AdjustFontSize(10, 100) == std::min(10 + 12, kMaxFontSize));	// delta は±12まで
	CHECK(AdjustFontSize(10, -100) == std::max(10 - 12, kMinFontSize));
}

TEST_CASE("AdjustFontSize: 端まで行っても潰れない")
{
	int sz = kDefaultFontSize;
	for (int i=0; i<50; i++) sz = AdjustFontSize(sz, -12);
	CHECK(sz == kMinFontSize);
	for (int i=0; i<50; i++) sz = AdjustFontSize(sz, 12);
	CHECK(sz == kMaxFontSize);
}

TEST_CASE("ParseFontSize: 空文字列は変更しない (false を返す)")
{
	int out = -1;
	CHECK_FALSE(ParseFontSize("", 10, kDefaultFontSize, out));
	CHECK(out == -1);	// 変更されていない
}

TEST_CASE("ParseFontSize: 絶対値を範囲内に丸めて適用する")
{
	int out = 0;
	CHECK(ParseFontSize("20", 10, kDefaultFontSize, out));
	CHECK(out == 20);

	CHECK(ParseFontSize("9999", 10, kDefaultFontSize, out));
	CHECK(out == kMaxFontSize);

	CHECK(ParseFontSize("-50", 10, kDefaultFontSize, out));
	CHECK(out == kMinFontSize);
}

TEST_CASE("ParseFontSize: 数値でなければ base にフォールバックする")
{
	int out = 0;
	CHECK(ParseFontSize("abc", 10, kDefaultFontSize, out));
	CHECK(out == kDefaultFontSize);
}

TEST_CASE("ParseFontSize: '^' 指定は指定サイズにする")
{
	int out = 0;
	CHECK(ParseFontSize("^20", 10, kDefaultFontSize, out));
	CHECK(out == 20);	// 現在値(10)と異なるのでそのまま適用
}

TEST_CASE("ParseFontSize: '^' 指定で現在値と一致するなら base に戻す")
{
	int out = 0;
	CHECK(ParseFontSize("^20", 20, kDefaultFontSize, out));
	CHECK(out == kDefaultFontSize);	// 現在値と同じなのでトグルして戻る
}

TEST_CASE("ParseFontSize: 単独の '^' は空指定と同じ扱い")
{
	int out = -1;
	CHECK_FALSE(ParseFontSize("^", 10, kDefaultFontSize, out));
	CHECK(out == -1);
}

//===========================================================================
// 透過度
//===========================================================================

TEST_CASE("ParseAlpha: 空文字列は Disable")
{
	int out = -1;
	CHECK(ParseAlpha("", 200, true, out) == AlphaAction::Disable);
	CHECK(out == -1);	// out は変更しない
}

TEST_CASE("ParseAlpha: IN は NeedsDialog (対話入力は wx 側の仕事)")
{
	int out = -1;
	CHECK(ParseAlpha("IN", 200, true, out) == AlphaAction::NeedsDialog);
	CHECK(ParseAlpha("in", 200, true, out) == AlphaAction::NeedsDialog);	// 大小文字を区別しない
	CHECK(out == -1);
}

TEST_CASE("ParseAlpha: 絶対値を指定範囲に丸めて適用する")
{
	int out = 0;
	CHECK(ParseAlpha("200", 255, false, out) == AlphaAction::Apply);
	CHECK(out == 200);

	CHECK(ParseAlpha("10", 255, false, out) == AlphaAction::Apply);
	CHECK(out == kMinAlpha);	// 64未満は64に丸める

	CHECK(ParseAlpha("999", 255, false, out) == AlphaAction::Apply);
	CHECK(out == kMaxAlpha);
}

TEST_CASE("ParseAlpha: '+'/'-' は現在値からの相対指定")
{
	int out = 0;
	CHECK(ParseAlpha("+10", 200, false, out) == AlphaAction::Apply);
	CHECK(out == 210);

	CHECK(ParseAlpha("-10", 200, false, out) == AlphaAction::Apply);
	CHECK(out == 190);
}

TEST_CASE("ParseAlpha: 相対指定も範囲外は端で止まる")
{
	int out = 0;
	CHECK(ParseAlpha("+1000", 200, false, out) == AlphaAction::Apply);
	CHECK(out == kMaxAlpha);

	CHECK(ParseAlpha("-1000", 200, false, out) == AlphaAction::Apply);
	CHECK(out == kMinAlpha);
}

TEST_CASE("ParseAlpha: '^' は指定値にするが、既に有効なら不透明(255)に戻す")
{
	int out = 0;
	// 現在無効 (current_enabled=false) -> '^' の後の数値をそのまま使う
	CHECK(ParseAlpha("^180", 255, false, out) == AlphaAction::Apply);
	CHECK(out == 180);

	// 現在有効 (current_enabled=true) -> 指定値を無視して不透明(255)に戻す
	CHECK(ParseAlpha("^180", 180, true, out) == AlphaAction::Apply);
	CHECK(out == kMaxAlpha);
}

//===========================================================================
// ウィンドウ位置
//===========================================================================

TEST_CASE("ParseWindowPos: 空文字列は false (起動時設定に戻す、はこの関数の範囲外)")
{
	WindowEdges edges;
	CHECK_FALSE(ParseWindowPos("", edges));
}

TEST_CASE("ParseWindowPos: 単一の絶対値指定")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("L100", edges));
	CHECK(edges.left.set);
	CHECK_FALSE(edges.left.relative);
	CHECK(edges.left.value == 100);
	CHECK_FALSE(edges.top.set);
}

TEST_CASE("ParseWindowPos: 複数指定と大小文字混在")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("l100;t50;r800;b600", edges));
	CHECK(edges.left.value == 100);
	CHECK(edges.top.value == 50);
	CHECK(edges.right.value == 800);
	CHECK(edges.bottom.value == 600);
	CHECK_FALSE(edges.left.relative);
}

TEST_CASE("ParseWindowPos: '+'/'-' は相対指定")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("R+20;B-10", edges));
	CHECK(edges.right.relative);
	CHECK(edges.right.value == 20);
	CHECK(edges.bottom.relative);
	CHECK(edges.bottom.value == -10);
}

TEST_CASE("ParseWindowPos: L/T/R/B 以外で始まる指定は不正")
{
	WindowEdges edges;
	CHECK_FALSE(ParseWindowPos("X100", edges));
}

TEST_CASE("ParseWindowPos: 数値が読めない指定は不正")
{
	WindowEdges edges;
	CHECK_FALSE(ParseWindowPos("Labc", edges));
	CHECK_FALSE(ParseWindowPos("L+", edges));	// 符号だけで数値が無い
	CHECK_FALSE(ParseWindowPos("L", edges));	// 数値そのものが無い
}

TEST_CASE("ApplyWindowPos: 絶対指定は置換、相対指定は加算")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("L100;T50;R+20;B-10", edges));

	int l, t, r, b;
	CHECK(ApplyWindowPos(edges, 0, 0, 800, 600, l, t, r, b));
	CHECK(l == 100);
	CHECK(t == 50);
	CHECK(r == 820);	// 800+20
	CHECK(b == 590);	// 600-10
}

TEST_CASE("ApplyWindowPos: 指定の無い辺は変えない")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("L100", edges));

	int l, t, r, b;
	CHECK(ApplyWindowPos(edges, 0, 0, 800, 600, l, t, r, b));
	CHECK(l == 100);
	CHECK(t == 0);
	CHECK(r == 800);
	CHECK(b == 600);
}

TEST_CASE("ApplyWindowPos: left>=right や top>=bottom になる指定は拒否する")
{
	WindowEdges edges;
	CHECK(ParseWindowPos("L900", edges));	// 現在の right(800) を超える

	int l, t, r, b;
	CHECK_FALSE(ApplyWindowPos(edges, 0, 0, 800, 600, l, t, r, b));
}

//===========================================================================
// サブウィンドウのサイズ
//===========================================================================

TEST_CASE("ParseSubSize: 絶対値と相対値を判別する")
{
	int out = 0; bool rel = true;
	CHECK(ParseSubSize("120", out, rel));
	CHECK(out == 120);
	CHECK_FALSE(rel);

	CHECK(ParseSubSize("+30", out, rel));
	CHECK(out == 30);
	CHECK(rel);

	CHECK(ParseSubSize("-30", out, rel));
	CHECK(out == -30);
	CHECK(rel);
}

TEST_CASE("ParseSubSize: 空や0は変更なし (false)")
{
	int out = -1; bool rel = true;
	CHECK_FALSE(ParseSubSize("", out, rel));
	CHECK_FALSE(ParseSubSize("0", out, rel));
	CHECK_FALSE(ParseSubSize("abc", out, rel));	// ToIntDef(0) と同じ扱い
}

TEST_CASE("ResolveSubSize: 絶対値・相対値を適用する")
{
	CHECK(ResolveSubSize(200, false, 150, 1000, 100) == 200);
	CHECK(ResolveSubSize(30, true, 150, 1000, 100) == 180);
	CHECK(ResolveSubSize(-30, true, 150, 1000, 100) == 120);
}

TEST_CASE("ResolveSubSize: 一覧側の最小サイズを侵すなら変更を拒否する")
{
	// container=1000, min=100 のとき、新サイズが 900 を超えると拒否
	CHECK(ResolveSubSize(950, false, 150, 1000, 100) == 150);	// current のまま
	CHECK(ResolveSubSize(900, false, 150, 1000, 100) == 900);	// ちょうど境界はOK
}

//===========================================================================
// ステータスバーの書式
//===========================================================================

TEST_CASE("ReplaceDirDelimiter: バックスラッシュを置換する")
{
	CHECK(ReplaceDirDelimiter("C:\\foo\\bar", "/") == "C:/foo/bar");
	CHECK(ReplaceDirDelimiter("C:\\foo", "\\") == "C:\\foo");	// 既定は無変換
}

TEST_CASE("ExpandStatusFormat: パスとファイル名")
{
	StatusFormatValues v;
	v.has_file = true;
	v.path = "C:\\dir\\";
	v.base_name = "file.txt";

	CHECK(ExpandStatusFormat("$P", v) == "C:\\dir\\");
	CHECK(ExpandStatusFormat("$B", v) == "file.txt");
	CHECK(ExpandStatusFormat("$F", v) == "C:\\dir\\file.txt");
}

TEST_CASE("ExpandStatusFormat: P2/F2 は区切り文字を置換する")
{
	StatusFormatValues v;
	v.has_file = true;
	v.path = "C:\\dir\\";
	v.base_name = "file.txt";
	v.dir_delimiter = "/";

	CHECK(ExpandStatusFormat("$P2", v) == "C:/dir/");
	CHECK(ExpandStatusFormat("$F2", v) == "C:/dir/file.txt");
}

TEST_CASE("ExpandStatusFormat: '2'付きの綴りが単文字版に化けない")
{
	// "P" を先に判定すると "$P2" が "P" にマッチして末尾に "2" が残ってしまう。
	// そうならないことを確認する
	StatusFormatValues v;
	v.has_file = true;
	v.path = "C:\\dir\\";

	CHECK(ExpandStatusFormat("$P2", v) != "C:\\dir\\2");
}

TEST_CASE("ExpandStatusFormat: リテラル文字はそのまま出力される")
{
	StatusFormatValues v;
	v.has_file = true;
	v.base_name = "a.txt";

	CHECK(ExpandStatusFormat("[$B]", v) == "[a.txt]");
}

TEST_CASE("ExpandStatusFormat: ソート方法 (S/S2) を語彙表から引く")
{
	StatusFormatValues v;
	v.sort_mode = 0;
	CHECK(ExpandStatusFormat("$S", v) == UnicodeString(_T("名前")));
	CHECK(ExpandStatusFormat("$S2", v) == UnicodeString(_T("名")));

	v.sort_mode = 3;	// サイズ
	CHECK(ExpandStatusFormat("$S2", v) == UnicodeString(_T("サ")));
}

TEST_CASE("ExpandStatusFormat: 範囲外の sort_mode は空文字列")
{
	StatusFormatValues v;
	v.sort_mode = 99;
	CHECK(ExpandStatusFormat("[$S]", v) == "[]");
}

TEST_CASE("ExpandStatusFormat: HS は隠し/システム属性の表示可否")
{
	StatusFormatValues v;
	v.show_hidden = true;
	v.show_system = false;
	CHECK(ExpandStatusFormat("$HS", v) == "H_");

	v.show_hidden = false;
	v.show_system = true;
	CHECK(ExpandStatusFormat("$HS", v) == "_S");
}

TEST_CASE("ExpandStatusFormat: has_file が false なら M/Z/Y/T/PR は出さない")
{
	StatusFormatValues v;
	v.has_file = false;
	v.mark_memo = "memo";
	v.size_str = "1KB";
	v.size_str_alt = "1,024";
	v.time_str = "2026-08-22";

	CHECK(ExpandStatusFormat("$M$Z$Y$T", v) == "");
	CHECK(ExpandStatusFormat("$PR(foo)", v, [](const UnicodeString &) -> UnicodeString { return UnicodeString("x"); }) == "");
}

TEST_CASE("ExpandStatusFormat: has_file が true なら M/Z/Y/T をそのまま出す")
{
	StatusFormatValues v;
	v.has_file = true;
	v.mark_memo = "memo";
	v.size_str = "1KB";
	v.size_str_alt = "1,024";
	v.time_str = "2026-08-22";

	CHECK(ExpandStatusFormat("$M/$Z/$Y/$T", v) == "memo/1KB/1,024/2026-08-22");
}

TEST_CASE("ExpandStatusFormat: DV は末尾へのタブ挿入としてマークされる")
{
	StatusFormatValues v;
	v.has_file = true;
	v.base_name = "a.txt";

	UnicodeString result = ExpandStatusFormat("$B$DVEND", v);
	CHECK(result == "a.txt\tEND");
}

TEST_CASE("ExpandStatusFormat: PR は field_lookup の結果に前置/後置を付ける (区切りは ',')")
{
	StatusFormatValues v;
	v.has_file = true;

	auto lookup = [](const UnicodeString &field) -> UnicodeString {
		return SameText(field, "SIZE")? UnicodeString("123") : UnicodeString("");
	};

	CHECK(ExpandStatusFormat("$PR(SIZE,[,])", v, lookup) == "[123]");
	// 値が空なら前置/後置ごと出さない
	CHECK(ExpandStatusFormat("$PR(OTHER,[,])", v, lookup) == "");
}

TEST_CASE("ExpandStatusFormat: field_lookup を渡さなければ PR は常に空")
{
	StatusFormatValues v;
	v.has_file = true;
	CHECK(ExpandStatusFormat("$PR(SIZE)", v) == "");
}
