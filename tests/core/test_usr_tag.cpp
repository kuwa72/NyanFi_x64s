/**
 * @file tests/core/test_usr_tag.cpp
 * @brief src/usr_tag.cpp (タグ管理ユニット) の回帰テスト
 *
 * 目的: 現在の実装の挙動をそのまま固定すること (regression test)。
 * ファイルシステムに触れるテストは tests/temp_dir.h の TempDir が作る
 * 一時ディレクトリの中だけで行う。
 */
#include "doctest/doctest.h"

#include <memory>

#include "usr_tag.h"
#include "usr_str.h"

#include "temp_dir.h"

using nyanfi_test::TempDir;

namespace {

/**
 * @brief タグを設定して保存し、ファイルから読み直した TagManager を返す
 * @details TagManager::TagNameList (IniCheckList/CheckToTags/CountTags が
 *          使う「全タグ名」の一覧) は private な MakeTagNameList() が
 *          コンストラクタと Recycle() でしか更新しない。SetTags/AddTags は
 *          TagDataList (ファイルごとのタグ) だけを更新し TagNameList には
 *          反映しないため、この一覧を使うテストは「保存してから読み直す」
 *          必要がある (実運用でもアプリ再起動/フォルダ切替時の再読込で
 *          同じ経路を通る)。
 */
std::unique_ptr<TagManager> seeded_tag_manager(
	const UnicodeString &fnam, std::initializer_list<std::pair<UnicodeString, UnicodeString>> entries)
{
	{
		TagManager seed(fnam);
		for (const auto &e : entries) seed.SetTags(e.first, e.second);
		seed.UpdateFile();
	}
	return std::unique_ptr<TagManager>(new TagManager(fnam));
}

}  // namespace

//===========================================================================
// GetTags / SetTags / AddTags / DelItem / HasTag
//===========================================================================
TEST_CASE("TagManager: SetTags/GetTags/HasTag の基本")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));	//存在しないファイルから開始 (空)

	CHECK(tag.HasTag("C:\\a.txt") == false);
	CHECK(tag.GetTags("C:\\a.txt") == UnicodeString(""));

	tag.SetTags("C:\\a.txt", "red;important");
	CHECK(tag.HasTag("C:\\a.txt") == true);
	CHECK(tag.GetTags("C:\\a.txt") == UnicodeString("red;important"));

	//空タグを設定すると項目自体が削除される
	tag.SetTags("C:\\a.txt", "");
	CHECK(tag.HasTag("C:\\a.txt") == false);
}

TEST_CASE("TagManager: AddTags は重複を追加せず既存タグへ連結する")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red");
	tag.AddTags("C:\\a.txt", "blue;red");	//red は重複なので追加されない
	CHECK(tag.GetTags("C:\\a.txt") == UnicodeString("red;blue"));

	//未登録ファイルへの AddTags は新規追加
	tag.AddTags("C:\\b.txt", "green");
	CHECK(tag.GetTags("C:\\b.txt") == UnicodeString("green"));
}

TEST_CASE("TagManager: DelItem はタグ情報を削除する")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red");
	CHECK(tag.HasTag("C:\\a.txt") == true);
	tag.DelItem("C:\\a.txt");
	CHECK(tag.HasTag("C:\\a.txt") == false);
}

//===========================================================================
// NormTags
//===========================================================================
TEST_CASE("TagManager: NormTags は既存タグの大小文字に揃え、無ければ追加する")
{
	TempDir dir;
	//TagNameList (NormTags が参照する「全タグ名」の一覧) はコンストラクタの
	//MakeTagNameList() でしか構築されないため、"Red" を登録済みにするには
	//保存してから読み直す必要がある (seeded_tag_manager 参照)
	auto tag = seeded_tag_manager(dir.file("tags.dat"), {{"C:\\a.txt", "Red"}});

	CHECK(tag->NormTags("red;Blue") == UnicodeString("Red;Blue"));	//大小文字は既存の "Red" に揃う

	//未登録タグ (green) は sw_add=false でも戻り値には含まれるが、
	//TagNameList には追加されない (第2引数は「無ければ追加するか」)
	CHECK(tag->NormTags("green", false) == UnicodeString("green"));
}

//===========================================================================
// Rename / Copy
//===========================================================================
TEST_CASE("TagManager: Rename はファイル名の変更にタグを追随させる")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\old.txt", "red");
	tag.Rename("C:\\old.txt", "C:\\new.txt");

	CHECK(tag.HasTag("C:\\old.txt") == false);
	CHECK(tag.GetTags("C:\\new.txt") == UnicodeString("red"));
}

TEST_CASE("TagManager: Copy はタグをコピーする (元は残る)")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red");
	tag.Copy("C:\\a.txt", "C:\\b.txt");

	CHECK(tag.GetTags("C:\\a.txt") == UnicodeString("red"));
	CHECK(tag.GetTags("C:\\b.txt") == UnicodeString("red"));
}

//===========================================================================
// RenTag / DelTagData
//===========================================================================
TEST_CASE("TagManager: RenTag はタグ名を一括改名する")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red;blue");
	tag.SetTags("C:\\b.txt", "red");

	int cnt = tag.RenTag("red", "crimson");
	CHECK(cnt == 2);
	CHECK(tag.GetTags("C:\\a.txt") == UnicodeString("crimson;blue"));
	CHECK(tag.GetTags("C:\\b.txt") == UnicodeString("crimson"));
}

TEST_CASE("TagManager: DelTagData は指定タグをすべての項目から削除する")
{
	TempDir dir;
	//DelTagData は TagNameList->IndexOf(tag) が見つからないと即 0 を返す実装
	//なので (TagNameList はコンストラクタの MakeTagNameList() でしか構築
	//されない)、保存してから読み直してタグ名一覧を確定させる
	auto tag = seeded_tag_manager(dir.file("tags.dat"), {{"C:\\a.txt", "red;blue"}, {"C:\\b.txt", "red"}});

	int cnt = tag->DelTagData("red");
	CHECK(cnt == 2);
	CHECK(tag->GetTags("C:\\a.txt") == UnicodeString("blue"));
	//red だけだった項目はタグが空になるので項目自体が消える
	CHECK(tag->HasTag("C:\\b.txt") == false);
}

//===========================================================================
// GetMatchList / GetAllList / Match
//===========================================================================
TEST_CASE("TagManager: Match は AND/OR 検索を切り替えられる")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red;blue");

	CHECK(tag.Match("C:\\a.txt", "red;blue", true) == true);	//AND: 両方一致
	CHECK(tag.Match("C:\\a.txt", "red;green", true) == false);	//AND: 片方しか無い
	CHECK(tag.Match("C:\\a.txt", "red;green", false) == true);	//OR: どちらか一致
	CHECK(tag.Match("C:\\a.txt", "green;yellow", false) == false);
}

TEST_CASE("TagManager: GetMatchList/GetAllList")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	tag.SetTags("C:\\a.txt", "red");
	tag.SetTags("C:\\b.txt", "blue");

	std::unique_ptr<TStringList> all(new TStringList());
	CHECK(tag.GetAllList(all.get()) == 2);

	std::unique_ptr<TStringList> matched(new TStringList());
	CHECK(tag.GetMatchList("red", true, matched.get()) == 1);
}

//===========================================================================
// GetColor / SetColor
//===========================================================================
TEST_CASE("TagManager: SetColor/GetColor")
{
	TempDir dir;
	TagManager tag(dir.file("tags.dat"));

	CHECK(tag.GetColor("red", clWhite) == clWhite);	//未設定は既定値

	tag.SetColor("red", (TColor)0x0000ff);
	CHECK(tag.GetColor("red", clWhite) == (TColor)0x0000ff);
}

//===========================================================================
// UpdateFile / Recycle (永続化)
//===========================================================================
TEST_CASE("TagManager: UpdateFile で保存し、別インスタンスから読み直せる")
{
	TempDir dir;
	UnicodeString fnam = dir.file("tags.dat");
	{
		TagManager tag(fnam);
		tag.SetTags("C:\\a.txt", "red");
		CHECK(tag.UpdateFile() == true);
	}
	{
		TagManager tag2(fnam);
		CHECK(tag2.GetTags("C:\\a.txt") == UnicodeString("red"));
	}
}

//===========================================================================
// IniCheckList / CheckToTags / CountTags (TCheckListBox 連携)
//===========================================================================
TEST_CASE("TagManager: IniCheckList はタグ名リストをチェックリストへ反映する")
{
	TempDir dir;
	auto tag = seeded_tag_manager(dir.file("tags.dat"), {{"C:\\a.txt", "red;blue"}});

	TCheckListBox lp;
	tag->IniCheckList(&lp, "blue");

	REQUIRE(lp.Count == 2);
	CHECK(lp.Items->Strings[0] == UnicodeString("blue"));
	CHECK(lp.Items->Strings[1] == UnicodeString("red"));
	//すべて未チェックで初期化される
	CHECK((bool)lp.Checked[0] == false);
	CHECK((bool)lp.Checked[1] == false);
	//tnam 指定で選択行が設定される
	CHECK(lp.ItemIndex == 0);	//"blue" の位置
}

TEST_CASE("TagManager: CheckToTags はチェック状態からタグ文字列を再構成する")
{
	TempDir dir;
	auto tag = seeded_tag_manager(dir.file("tags.dat"), {{"C:\\a.txt", "red;blue;green"}});

	TCheckListBox lp;
	tag->IniCheckList(&lp);	//blue, green, red の順 (Sort 済み)
	REQUIRE(lp.Count == 3);

	//blue と red だけチェックする
	for (int i = 0; i < lp.Count; i++) {
		UnicodeString name = lp.Items->Strings[i];
		lp.Checked[i] = (name == UnicodeString("blue") || name == UnicodeString("red"));
	}

	UnicodeString result = tag->CheckToTags(&lp, "");
	CHECK(result == UnicodeString("blue;red"));
}

TEST_CASE("TagManager: CountTags はタグごとの使用数を Items->Objects へ設定する")
{
	TempDir dir;
	auto tag = seeded_tag_manager(dir.file("tags.dat"),
		{{"C:\\a.txt", "red"}, {"C:\\b.txt", "red;blue"}, {"C:\\c.txt", "blue"}});

	TCheckListBox lp;
	tag->IniCheckList(&lp, EmptyStr, true);	//count_sw=true で CountTags まで実行
	REQUIRE(lp.Count == 2);

	int blue_count = 0, red_count = 0;
	for (int i = 0; i < lp.Count; i++) {
		UnicodeString name = lp.Items->Strings[i];
		int cnt = (int)(NativeInt)lp.Items->Objects[i];
		if (name == UnicodeString("blue")) blue_count = cnt;
		if (name == UnicodeString("red")) red_count = cnt;
	}
	CHECK(blue_count == 2);
	CHECK(red_count == 2);
}
