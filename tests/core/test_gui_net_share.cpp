/**
 * @file tests/core/test_gui_net_share.cpp
 * @brief gui/net_share.h (ネットワーク共有ダイアログの判断) のテスト
 *
 * VCL 実測:
 * - src/usr_cmdlist.cpp:221 に F:ShareList
 * - src/MainFrm.cpp:25862-25892 がコンピュータ名を NetShareDlg に渡して表示する
 * - src/ShareDlg.cpp:85-91/172-225 が共有名列挙・接続・一覧更新
 */
#include "doctest/doctest.h"

#include "gui/net_share.h"

using namespace net_share;

namespace {

ShareItem share(const wchar_t *name, const wchar_t *path = L"", const wchar_t *remark = L"")
{
	ShareItem item;
	item.name = name;
	item.local_path = path;
	item.remark = remark;
	return item;
}

}  // namespace

TEST_CASE("NormalizeComputer: VCL と同じコンピュータ名入力をUNC先頭へ正規化する")
{
	auto local = NormalizeComputer(_T("SERVER"));
	CHECK(local.ok);
	CHECK(local.value == _T("\\\\SERVER"));

	auto unc = NormalizeComputer(_T("  \\\\server\\  "));
	CHECK(unc.ok);
	CHECK(unc.value == _T("\\\\server"));
}

TEST_CASE("NormalizeComputer: 空・UNC指定なし・混入区切りは不正")
{
	CHECK_FALSE(NormalizeComputer(_T("")).ok);
	CHECK_FALSE(NormalizeComputer(_T("   ")).ok);
	CHECK_FALSE(NormalizeComputer(_T("\\\\")).ok);
	CHECK_FALSE(NormalizeComputer(_T("\\\\server\\share")).ok);
	CHECK_FALSE(NormalizeComputer(_T("server/share")).ok);
	CHECK_FALSE(NormalizeComputer(_T("bad server")).ok);
	CHECK_FALSE(NormalizeComputer(_T("bad?name")).ok);
	CHECK(NormalizeComputer(_T("bad?name")).error == _T("コンピュータ名に無効な文字があります"));
}

TEST_CASE("NormalizeUncPath: 共有サーバーのUNCだけを受け付ける")
{
	auto path = NormalizeUncPath(_T("\\\\Server\\Public\\Docs\\"));
	CHECK(path.ok);
	CHECK(path.value == _T("\\\\Server\\Public\\Docs"));

	CHECK(NormalizeUncPath(_T("C:\\Share")).error == _T("UNC形式で入力してください"));
	CHECK(NormalizeUncPath(_T("\\\\server")).error == _T("UNC共有名を指定してください"));
	CHECK(NormalizeUncPath(_T("\\\\server\\")).error == _T("UNC共有名を指定してください"));
}

TEST_CASE("NormalizeUncPath: 区切り・予約相対名・禁止文字を弾く")
{
	CHECK(NormalizeUncPath(_T("\\\\server\\share\\")).ok);  // 末尾区切りは許可
	CHECK(NormalizeUncPath(_T("\\\\server\\share\\")).value == _T("\\\\server\\share"));
	CHECK_FALSE(NormalizeUncPath(_T("\\\\server\\share\\\\docs")).ok);
	CHECK_FALSE(NormalizeUncPath(_T("\\\\server\\share\\..")).ok);
	CHECK_FALSE(NormalizeUncPath(_T("\\\\bad server\\share")).ok);
	CHECK_FALSE(NormalizeUncPath(_T("\\\\server\\share\\bad|name")).ok);
	CHECK_FALSE(NormalizeUncPath(_T("//server/share")).ok);
}

TEST_CASE("ResolveConnectionAction: 列挙失敗時だけ接続、取消は閉じる")
{
	CHECK(ResolveConnectionAction(ShareListResult::Success, ConnectResult::NotAttempted)
	      == ConnectionAction::UseExisting);
	CHECK(ResolveConnectionAction(ShareListResult::Failure, ConnectResult::NotAttempted)
	      == ConnectionAction::Connect);
	CHECK(ResolveConnectionAction(ShareListResult::Failure, ConnectResult::Success)
	      == ConnectionAction::UseExisting);
	CHECK(ResolveConnectionAction(ShareListResult::Failure, ConnectResult::Cancelled)
	      == ConnectionAction::Cancel);
	CHECK(ResolveConnectionAction(ShareListResult::Failure, ConnectResult::Failure)
	      == ConnectionAction::ShowError);
}

TEST_CASE("SortFilterShares: 管理共有除外・大小無視検索・共有名順")
{
	const std::vector<ShareItem> input = {
		share(_T("zeta"), _T("D:\\Zeta")),
		share(_T("Admin$"), _T("C:\\Admin")),
		share(_T("Alpha"), _T("C:\\Data"), _T("公開")),
		share(_T("beta"), _T("C:\\Backup")),
	};

	const auto all = SortFilterShares(input, _T(""));
	REQUIRE(all.size() == 3);
	CHECK(all[0].name == _T("Alpha"));
	CHECK(all[1].name == _T("beta"));
	CHECK(all[2].name == _T("zeta"));

	const auto filtered = SortFilterShares(input, _T("data"));
	REQUIRE(filtered.size() == 1);
	CHECK(filtered[0].name == _T("Alpha"));
	CHECK(SortFilterShares(input, _T("公開")).size() == 1);
}

TEST_CASE("MakeUncSharePath: コンピュータ名と共有名を安全なUNCへ組み合わせる")
{
	CHECK(MakeUncSharePath(_T("server"), _T("Public")) == _T("\\\\server\\Public"));
	CHECK(MakeUncSharePath(_T("\\\\server\\"), _T("\\Public\\")) == _T("\\\\server\\Public"));
	CHECK(MakeUncSharePath(_T("bad\\server"), _T("Public")).IsEmpty());
	CHECK(MakeUncSharePath(_T("server"), _T("bad|name")).IsEmpty());
}

TEST_CASE("文字列整形: VCL表示はスラッシュ、共有行はUNCと補足情報を出す")
{
	CHECK(FormatComputerCaption(_T("\\\\Server\\")) == _T("//Server"));
	CHECK(FormatShareRow(_T("server"), share(_T("Public"), _T("C:\\Pub"), _T("公開")))
	      == _T("\\\\server\\Public  C:\\Pub  (公開)"));
	CHECK(FormatShareRow(_T("server"), share(_T("Docs"))) == _T("\\\\server\\Docs"));
}
