/**
 * @file tests/compat/test_json.cpp
 * @brief System.JSON (TJSONValue 系) 互換シムの単体テスト
 */
#include "doctest/doctest.h"

#include <memory>

#include "compat/json.h"

//===========================================================================
// パース: プリミティブ値
//===========================================================================
TEST_CASE("ParseJSONValue: true/false/null")
{
	std::unique_ptr<TJSONValue> t(TJSONValue::ParseJSONValue("true"));
	REQUIRE(t);
	CHECK(t->ClassNameIs("TJSONTrue"));

	std::unique_ptr<TJSONValue> f(TJSONValue::ParseJSONValue("false"));
	REQUIRE(f);
	CHECK(f->ClassNameIs("TJSONFalse"));

	std::unique_ptr<TJSONValue> n(TJSONValue::ParseJSONValue("null"));
	REQUIRE(n);
	CHECK(n->ClassNameIs("TJSONNull"));
}

TEST_CASE("ParseJSONValue: 数値")
{
	std::unique_ptr<TJSONValue> v(TJSONValue::ParseJSONValue("-12.5e3"));
	REQUIRE(v);
	CHECK(v->ClassNameIs("TJSONNumber"));
	CHECK(UnicodeString(v->Value()) == UnicodeString("-12.5e3"));
}

TEST_CASE("ParseJSONValue: 文字列とエスケープ (サロゲートペアの \\u を含む)")
{
	//U+1F600 (😀) はサロゲートペア 😀 で表現される
	std::unique_ptr<TJSONValue> v(TJSONValue::ParseJSONValue(L"\"a\\tb\\ud83d\\ude00c\""));
	REQUIRE(v);
	CHECK(v->ClassNameIs("TJSONString"));
	UnicodeString expect;
	expect += L'a';
	expect += L'\t';
	expect += L'b';
	expect += static_cast<wchar_t>(0xD83D);
	expect += static_cast<wchar_t>(0xDE00);
	expect += L'c';
	CHECK(v->Value() == expect);
}

//===========================================================================
// パース: オブジェクト / 配列
//===========================================================================
TEST_CASE("ParseJSONValue: オブジェクト (Pairs[]/Count/GetValue)")
{
	std::unique_ptr<TJSONValue> root(TJSONValue::ParseJSONValue(L"{\"name\":\"nyanfi\",\"ver\":16}"));
	REQUIRE(root);
	REQUIRE(root->ClassNameIs("TJSONObject"));

	TJSONObject *obj = dynamic_cast<TJSONObject *>(root.get());
	REQUIRE(obj);
	CHECK(obj->Count == 2);
	CHECK(UnicodeString(obj->Pairs[0]->JsonString->Value()) == UnicodeString("name"));
	CHECK(UnicodeString(obj->Pairs[0]->JsonValue->Value()) == UnicodeString("nyanfi"));

	TJSONValue *v = obj->GetValue("ver");
	REQUIRE(v);
	CHECK(UnicodeString(v->Value()) == UnicodeString("16"));
	CHECK_FALSE(obj->GetValue("missing"));
}

TEST_CASE("ParseJSONValue: 配列 (Items[]/Count/Size)")
{
	std::unique_ptr<TJSONValue> root(TJSONValue::ParseJSONValue("[1,2,3]"));
	REQUIRE(root);
	REQUIRE(root->ClassNameIs("TJSONArray"));

	TJSONArray *ary = dynamic_cast<TJSONArray *>(root.get());
	REQUIRE(ary);
	CHECK(ary->Count == 3);
	CHECK(ary->Size() == 3);
	CHECK(UnicodeString(ary->Items[1]->Value()) == UnicodeString("2"));
}

TEST_CASE("ParseJSONValue: ネストしたオブジェクト・配列")
{
	std::unique_ptr<TJSONValue> root(
		TJSONValue::ParseJSONValue(L"{\"assets\":[{\"name\":\"a.zip\",\"browser_download_url\":\"http://x/a.zip\"}]}"));
	REQUIRE(root);
	TJSONObject *obj = dynamic_cast<TJSONObject *>(root.get());
	REQUIRE(obj);

	TJSONArray *assets = dynamic_cast<TJSONArray *>(obj->GetValue("assets"));
	REQUIRE(assets);
	CHECK(assets->Size() == 1);

	TJSONObject *v0 = dynamic_cast<TJSONObject *>(assets->Items[0]);
	REQUIRE(v0);
	TJSONValue *v_nam = v0->GetValue("name");
	REQUIRE(v_nam);
	CHECK(UnicodeString(v_nam->Value()) == UnicodeString("a.zip"));
}

//===========================================================================
// エラー処理
//===========================================================================
TEST_CASE("ParseJSONValue: RaiseExc=false (既定) は失敗時に nullptr を返す")
{
	TJSONValue *v = TJSONValue::ParseJSONValue("{invalid");
	CHECK(v == nullptr);
}

TEST_CASE("ParseJSONValue: RaiseExc=true は EJSONParseException を送出する")
{
	bool thrown = false;
	try {
		TJSONValue::ParseJSONValue("{invalid", false, true);
	}
	catch (EJSONParseException &e) {
		thrown = true;
		CHECK(e.Line >= 1);
		CHECK(e.Position >= 1);
	}
	CHECK(thrown);
}

//===========================================================================
// ToJSON (直列化)
//===========================================================================
TEST_CASE("ToJSON: パースした値を再度 JSON テキストへ直列化できる")
{
	std::unique_ptr<TJSONValue> root(TJSONValue::ParseJSONValue(L"{\"a\":1,\"b\":[true,false,null,\"x\"]}"));
	REQUIRE(root);
	UnicodeString out = root->ToJSON();
	//往復させても同じ構造として再パースできることを確認する (整形の細かい違いは問わない)
	std::unique_ptr<TJSONValue> reparsed(TJSONValue::ParseJSONValue(out));
	REQUIRE(reparsed);
	CHECK(reparsed->ClassNameIs("TJSONObject"));
}

//===========================================================================
// format_Json / get_JsonValStr が使う ClassNameIs の分岐を模した確認
//===========================================================================
TEST_CASE("ClassNameIs: get_JsonValStr 相当の分岐が全パターンで判別できる")
{
	auto class_of = [](TJSONValue *v) { return UnicodeString(v->ClassName()); };

	std::unique_ptr<TJSONValue> t(TJSONValue::ParseJSONValue("true"));
	std::unique_ptr<TJSONValue> f(TJSONValue::ParseJSONValue("false"));
	std::unique_ptr<TJSONValue> n(TJSONValue::ParseJSONValue("null"));
	std::unique_ptr<TJSONValue> s(TJSONValue::ParseJSONValue("\"str\""));
	std::unique_ptr<TJSONValue> num(TJSONValue::ParseJSONValue("42"));

	CHECK(class_of(t.get()) == UnicodeString("TJSONTrue"));
	CHECK(class_of(f.get()) == UnicodeString("TJSONFalse"));
	CHECK(class_of(n.get()) == UnicodeString("TJSONNull"));
	CHECK(class_of(s.get()) == UnicodeString("TJSONString"));
	CHECK(class_of(num.get()) == UnicodeString("TJSONNumber"));
}
