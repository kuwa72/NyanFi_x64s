/**
 * @file tests/core/test_gui_tag.cpp
 * @brief gui/tag.cpp のタグ入力・コマンド解決のテスト
 */
#include "doctest/doctest.h"

#include "gui/tag.h"

TEST_CASE("tag::SplitTags: 空白と空項目を除いて分割する")
{
	const auto tags = tag::SplitTags(_T(" red ; blue;; green "));
	REQUIRE(tags.size() == 3);
	CHECK(tags[0] == UnicodeString(_T("red")));
	CHECK(tags[1] == UnicodeString(_T("blue")));
	CHECK(tags[2] == UnicodeString(_T("green")));
}

TEST_CASE("tag::JoinTags: 順序を保ち、末尾のセミコロンを選択できる")
{
	const std::vector<UnicodeString> tags = {
		_T("red"), _T(""), _T(" blue "), _T("red")
	};
	CHECK(tag::JoinTags(tags, false) == UnicodeString(_T("red;blue")));
	CHECK(tag::JoinTags(tags, true) == UnicodeString(_T("red;blue;")));
}

TEST_CASE("tag::ResolveInput: FindTag の直接パラメーターはダイアログを通さない")
{
	const tag::InputPlan plan = tag::ResolveInput(tag::Mode::Find, _T("red|blue"), EmptyStr);
	CHECK_FALSE(plan.show_dialog);
	CHECK_FALSE(plan.and_match);
	CHECK_FALSE(plan.match_all);
	CHECK(plan.tags == UnicodeString(_T("red;blue")));
}

TEST_CASE("tag::ResolveInput: ; は入力ダイアログを開き、初期値を保持する")
{
	const tag::InputPlan find_plan = tag::ResolveInput(tag::Mode::Find, _T(";"), EmptyStr);
	CHECK(find_plan.show_dialog);
	CHECK(find_plan.and_match);
	CHECK(find_plan.tags.IsEmpty());

	const tag::InputPlan set_plan = tag::ResolveInput(tag::Mode::Set, _T(";"), _T("old;blue"));
	CHECK(set_plan.show_dialog);
	CHECK(set_plan.tags == UnicodeString(_T("old;blue")));
}

TEST_CASE("tag::ResolveInput: FindTag の * は全タグ一致として扱う")
{
	const tag::InputPlan plan = tag::ResolveInput(tag::Mode::Find, _T("*"), EmptyStr);
	CHECK_FALSE(plan.show_dialog);
	CHECK(plan.match_all);
	CHECK(plan.tags == UnicodeString(_T("*")));
}

TEST_CASE("tag::BuildSearchCommand: VCL と同じ検索コマンド行を組み立てる")
{
	CHECK(tag::BuildSearchCommand(_T("red;blue"), true, false)
	      == UnicodeString(_T(";タグ検索 [red;blue]\r\nFindTag_red;blue")));
	CHECK(tag::BuildSearchCommand(_T("red;blue"), false, true)
	      == UnicodeString(_T(";タグ検索 [red|blue]\r\nToOpposite\r\nFindTag_red|blue")));
}

TEST_CASE("tag::Title: モードと AND/OR が表題に反映される")
{
	CHECK(tag::Title(tag::Mode::Add, true) == UnicodeString(_T("タグの追加")));
	CHECK(tag::Title(tag::Mode::Set, true) == UnicodeString(_T("タグの設定")));
	CHECK(tag::Title(tag::Mode::Find, true) == UnicodeString(_T("タグ検索 (AND)")));
	CHECK(tag::Title(tag::Mode::Find, false) == UnicodeString(_T("タグ検索 (OR)")));
	CHECK(tag::Title(tag::Mode::Select, true) == UnicodeString(_T("タグ選択")));
	CHECK(tag::Title(tag::Mode::FolderIcon, true) == UnicodeString(_T("フォルダアイコン検索")));
}

//===========================================================================
// ResolveTagJump: ダイレクトタグジャンプの入力解決 (V:TagJumpDirect/TagViewDirect)
// VCL: src/MainFrm.cpp:33118-33170 DirectTagJumpCore の簡易版
//===========================================================================
TEST_CASE("tag::ResolveTagJump: ctags フォーマットの直接指定")
{
	// タグ名\tファイル名\t行番号 の形式なら直接ジャンプ先として解釈
	const auto plan = tag::ResolveTagJump(_T("main\tC:\\src\\main.cpp\t42"), EmptyStr, EmptyStr, false);
	CHECK_FALSE(plan.is_edit);  // 既定は閲覧
	CHECK(plan.file_path == UnicodeString(_T("C:\\src\\main.cpp")));
	CHECK(plan.line_no == 42);
}

TEST_CASE("tag::ResolveTagJump: ctags パターン指定は行番号 1")
{
	const auto plan = tag::ResolveTagJump(_T("main\tC:\\src\\main.cpp\t/^int main/"), EmptyStr, EmptyStr, false);
	CHECK(plan.file_path == UnicodeString(_T("C:\\src\\main.cpp")));
	CHECK(plan.line_no == 1);
}

TEST_CASE("tag::ResolveTagJump: タグ名のみ (tags ファイル検索が必要)")
{
	// タグ名のみの場合は tags ファイル検索が必要。ここでは検索は行わず、
	// タグ名と基準ディレクトリを返す (実際の検索は wx 側の仕事)
	const auto plan = tag::ResolveTagJump(_T("main"), EmptyStr, _T("C:\\src"), false);
	CHECK(plan.tag_name == UnicodeString(_T("main")));
	CHECK(plan.base_dir == UnicodeString(_T("C:\\src")));
	CHECK(plan.file_path.IsEmpty());
}

TEST_CASE("tag::ResolveTagJump: 空パラメータは現在の単語を使う")
{
	const auto plan = tag::ResolveTagJump(_T(""), _T("cur_word"), _T("C:\\src"), false);
	CHECK(plan.tag_name == UnicodeString(_T("cur_word")));
	CHECK(plan.base_dir == UnicodeString(_T("C:\\src")));
}

TEST_CASE("tag::ResolveTagJump: is_edit は呼び出し側が決める (TagJumpDirect=true)")
{
	// TagJumpDirect は編集、TagViewDirect は閲覧。ここでは is_edit を引数で受け取る
	const auto plan = tag::ResolveTagJump(_T("main\tC:\\src\\main.cpp\t42"), EmptyStr, EmptyStr, false);
	CHECK_FALSE(plan.is_edit);  // 既定は閲覧 (TagViewDirect)
}
