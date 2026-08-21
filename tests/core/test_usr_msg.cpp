/**
 * @file tests/core/test_usr_msg.cpp
 * @brief src/usr_msg.cpp (メッセージ文字列テーブル・Abort系) の回帰テスト
 *
 * @details msgbox_ERR 等のメッセージボックス表示そのものは src/usr_msg_dlg.cpp
 * に分離されており (GUI/VCL 依存で未移植)、本テストの対象外。ここでは
 * LoadUsrMsg (%s 展開・全角スラッシュ(／)の改行変換を含む) と
 * UserAbort/TextAbort が投げる例外のメッセージを検証する。
 *
 * @note メッセージテーブルの各行末の `\n` は、TStringList::Text の行区切り
 * としてのみ使われ、実際のメッセージ文字列には含まれない (TStrings::SetText
 * が `\r`/`\n` を区切り文字として消費し、各行の値には残さないため。
 * compat/src/classes.cpp の実装のとおりで、これは実 VCL の TStrings と同じ
 * 挙動)。そのため以下の期待値には (％の置換で挿入される `\r\n` を除き)
 * 末尾の改行を含めていない。期待文言は実行結果を照合しながら
 * src/usr_msg.cpp のメッセージテーブルの実装から書き写した (推測ではない)。
 */
#include "doctest/doctest.h"

#include "usr_msg.h"

//===========================================================================
// LoadUsrMsg: %s を含まないメッセージ
//===========================================================================
TEST_CASE("LoadUsrMsg: %sを含まないメッセージはそのまま返る")
{
	CHECK(LoadUsrMsg(USTR_CantOperate) == UnicodeString(_T("この操作はできません。")));
	CHECK(LoadUsrMsg(USTR_Canceled) == UnicodeString(_T("中断しました。")));
	CHECK(LoadUsrMsg(USTR_HintMltFExt) == UnicodeString(_T(". で区切って複数指定可能")));
	CHECK(LoadUsrMsg(USTR_SelectedItem) == UnicodeString(_T("選択項目")));
}

//===========================================================================
// LoadUsrMsg: %s を置換文字列無しで呼ぶと %s とそれに続く助詞が取り除かれる
//===========================================================================
TEST_CASE("LoadUsrMsg: 置換文字列省略時は%sと後続の助詞(のをがに)を除去")
{
	//"%sが見つかりません。" -> "%sが" が丸ごと除去される
	CHECK(LoadUsrMsg(USTR_NotFound) == UnicodeString(_T("見つかりません。")));
	//"%sをコピーしますか?" -> "%sを" が除去される
	CHECK(LoadUsrMsg(USTR_CopyQ) == UnicodeString(_T("コピーしますか?")));
	//"%s コマンドで変更可能" -> 直後が空白 (助詞ではない) なので %s だけ除去され
	//後続の空白はそのまま残る
	CHECK(LoadUsrMsg(USTR_HintOptCmd) == UnicodeString(_T(" コマンドで変更可能")));
	//"削除開始  %s" -> 末尾の %s のみ除去される
	CHECK(LoadUsrMsg(USTR_BeginDelete) == UnicodeString(_T("削除開始  ")));
}

//===========================================================================
// LoadUsrMsg: %s に文字列を渡すと置換される
//===========================================================================
TEST_CASE("LoadUsrMsg: 置換文字列を渡すと%sがそのまま置き換わる")
{
	CHECK(LoadUsrMsg(USTR_NotFound, UnicodeString(_T("ファイル"))) ==
		UnicodeString(_T("ファイルが見つかりません。")));
	CHECK(LoadUsrMsg(USTR_DeleteQ, UnicodeString(_T("この項目"))) ==
		UnicodeString(_T("この項目を削除しますか?")));
}

//===========================================================================
// LoadUsrMsg: const _TCHAR* オーバーロード
//===========================================================================
TEST_CASE("LoadUsrMsg: const _TCHAR*版はUnicodeString版と同じ結果")
{
	CHECK(LoadUsrMsg(USTR_NotFound, _T("ファイル")) == UnicodeString(_T("ファイルが見つかりません。")));
}

//===========================================================================
// LoadUsrMsg: id_s オーバーロード (置換文字列を別のメッセージIDから引く)
//===========================================================================
TEST_CASE("LoadUsrMsg: 置換元をメッセージIDで指定するオーバーロード")
{
	//USTR_CantGetInfo = "%s情報が取得できません。"、USTR_SelectedItem = "選択項目"
	//(いずれも末尾の改行は行区切りとして消費されるため含まれない)
	CHECK(LoadUsrMsg(USTR_CantGetInfo, USTR_SelectedItem) ==
		UnicodeString(_T("選択項目情報が取得できません。")));
}

//===========================================================================
// LoadUsrMsg: 全角スラッシュ(／)は\r\nに変換される
//===========================================================================
TEST_CASE("LoadUsrMsg: ／ は \\r\\n に変換される")
{
	CHECK(LoadUsrMsg(USTR_SearchingESC) ==
		UnicodeString(_T("　検索中...\r\n　ESCキーで中断します。")));
}

//===========================================================================
// UserAbort: メッセージIDから EAbort を送出する
//===========================================================================
TEST_CASE("UserAbort: メッセージIDに対応する文言でEAbortを送出")
{
	bool caught = false;
	try {
		UserAbort(USTR_CantOperate);
	} catch (EAbort &e) {
		caught = true;
		CHECK(e.Message == UnicodeString(_T("この操作はできません。")));
	}
	CHECK(caught);
}

//===========================================================================
// TextAbort: 任意の文字列で EAbort を送出する
//===========================================================================
TEST_CASE("TextAbort: 渡した文字列そのままでEAbortを送出")
{
	bool caught = false;
	try {
		TextAbort(_T("不正なチャンネル数です。"));
	} catch (EAbort &e) {
		caught = true;
		CHECK(e.Message == UnicodeString(_T("不正なチャンネル数です。")));
	}
	CHECK(caught);
}

//===========================================================================
// SkipAbort / CancelAbort / EmptyAbort: 固定文言でEAbortを送出する
//===========================================================================
TEST_CASE("SkipAbort/CancelAbort/EmptyAbort: 固定文言のEAbort")
{
	CHECK_THROWS_AS(SkipAbort(), EAbort);
	CHECK_THROWS_AS(CancelAbort(), EAbort);
	CHECK_THROWS_AS(EmptyAbort(), EAbort);
}
