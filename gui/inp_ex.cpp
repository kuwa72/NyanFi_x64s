/**
 * @file gui/inp_ex.cpp
 * @brief gui/inp_ex.h の実装
 */
#include "gui/inp_ex.h"

namespace inp_ex {

//---------------------------------------------------------------------------
bool UsesCombo(Mode mode)
{
	switch (mode) {
	case Mode::CreateDir:
	case Mode::NewTextFile:
	case Mode::ClipPaste:
	case Mode::FindTag:
	case Mode::AddTag:
	case Mode::SetTag:
	case Mode::TagSelect:
		return true;
	default:
		return false;
	}
}

//---------------------------------------------------------------------------
UnicodeString Prompt(Mode mode)
{
	switch (mode) {
	case Mode::CreateDir:
	case Mode::NewTextFile:
	case Mode::ClipPaste:
		return _T("名前");
	case Mode::Clone:
		return _T("名前/書式");
	case Mode::FunctionKey:
		return _T("ファンクションキー");
	case Mode::CreateTestFile:
		return _T("ファイル名");
	case Mode::JumpLine:
		return _T("行番号");
	case Mode::JumpAddress:
		return _T("アドレス");
	case Mode::SetTopAddress:
		return _T("先頭アドレス");
	case Mode::FindTag:
	case Mode::AddTag:
	case Mode::SetTag:
	case Mode::TagSelect:
		return _T("タグ");
	case Mode::Simple:
		return _T("値");
	}
	return _T("値");
}

//---------------------------------------------------------------------------
UnicodeString HistorySection(Mode mode)
{
	switch (mode) {
	case Mode::CreateDir:
		return _T("CreateDirHistory");
	case Mode::NewTextFile:
	case Mode::ClipPaste:
		return _T("NewTextHistory");
	default:
		return EmptyStr;
	}
}

//---------------------------------------------------------------------------
UnicodeString Hint(Mode mode)
{
	switch (mode) {
	case Mode::FindTag:
	case Mode::TagSelect:
		return _T("; 区切りでAND検索、｜区切りでOR検索");
	case Mode::AddTag:
	case Mode::SetTag:
		return _T("; で区切って複数指定可能");
	default:
		return EmptyStr;
	}
}

//---------------------------------------------------------------------------
UnicodeString Title(Mode mode, const UnicodeString &path_name)
{
	switch (mode) {
	case Mode::CreateDir:
		return _T("ディレクトリの作成 - ") + path_name;
	case Mode::NewTextFile:
		return _T("新規テキストの作成");
	case Mode::Clone:
		return _T("クローン作成");
	case Mode::FunctionKey:
		return _T("ファンクションキー");
	case Mode::CreateTestFile:
		return _T("テストファイルの作成");
	case Mode::JumpLine:
		return _T("指定行番号に移動");
	case Mode::JumpAddress:
		return _T("指定アドレスに移動");
	case Mode::SetTopAddress:
		return _T("先頭アドレスを設定");
	case Mode::FindTag:
		return _T("タグ検索");
	case Mode::AddTag:
		return _T("タグの追加");
	case Mode::SetTag:
		return _T("タグの設定");
	case Mode::TagSelect:
		return _T("タグ選択");
	case Mode::ClipPaste:
		return _T("クリップボードから新規テキスト作成");
	case Mode::Simple:
		return EmptyStr;
	}
	return EmptyStr;
}

//---------------------------------------------------------------------------
void NormalizeOnClose(Values &values)
{
	if (values.mode != Mode::JumpAddress && values.mode != Mode::SetTopAddress) return;

	UnicodeString value = values.value.Trim();
	if (values.hexadecimal && !value.IsEmpty()) {
		const int prefix_pos = (value[1] == _T('+') || value[1] == _T('-')) ? 2 : 1;
		if (!StartsText(_T("0x"), value.SubString(prefix_pos))) {
			value.Insert(_T("0x"), prefix_pos);
		}
	}
	values.value = value;
}

//---------------------------------------------------------------------------
LengthStatus MeasureCreateDir(const UnicodeString &path_name, const UnicodeString &name)
{
	LengthStatus status;
	status.path_length = path_name.Length() + name.Length();
	status.name_length = name.Length();
	// VCL は FTP の '/' を含む場合だけ 248 文字の制限を迂回する
	// (InpExDlg.cpp:347-350)。
	status.path_ok = status.path_length < 248 || path_name.Pos(_T('/')) > 0;
	status.name_ok = status.name_length < 256;
	return status;
}

//---------------------------------------------------------------------------
bool Validate(const Values &values, UnicodeString &error_out)
{
	error_out = EmptyStr;
	const UnicodeString value = values.value.Trim();
	switch (values.mode) {
	case Mode::CreateDir: {
		if (value.IsEmpty()) {
			error_out = _T("ディレクトリ名を入力してください");
			return false;
		}
		const LengthStatus length = MeasureCreateDir(values.path_name, value);
		if (!length.path_ok) {
			error_out = _T("フルパス名が長すぎます");
			return false;
		}
		if (!length.name_ok) {
			error_out = _T("ディレクトリ名が長すぎます");
			return false;
		}
		return true;
	}
	case Mode::CreateTestFile:
		if (value.IsEmpty()) {
			error_out = _T("ファイル名を入力してください");
			return false;
		}
		if (values.test_size.Trim().IsEmpty()) {
			error_out = _T("サイズを入力してください");
			return false;
		}
		if (values.test_count <= 0) {
			error_out = _T("個数は1以上で入力してください");
			return false;
		}
		return true;
	case Mode::AddTag:
		if (value.IsEmpty()) {
			error_out = _T("タグを入力してください");
			return false;
		}
		return true;
	case Mode::SetTag:
		// VCL は空文字で既存タグを全消去する。
		return true;
	case Mode::JumpLine:
	case Mode::JumpAddress:
	case Mode::SetTopAddress:
		if (value.IsEmpty()) {
			error_out = _T("値を入力してください");
			return false;
		}
		return true;
	case Mode::NewTextFile:
	case Mode::ClipPaste:
	case Mode::Clone:
	case Mode::FunctionKey:
	case Mode::FindTag:
	case Mode::TagSelect:
		return !value.IsEmpty();
	case Mode::Simple:
		return true;
	}
	return true;
}

//---------------------------------------------------------------------------
std::vector<UnicodeString> CodePageNames()
{
	return {_T("Shift_JIS"), _T("UTF-8"), _T("UTF-8N"), _T("UTF-16"),
	        _T("UTF-16(BE)"), _T("EUC-JP"), _T("ISO-2022-JP")};
}

}  // namespace inp_ex
