/**
 * @file gui/settings.cpp
 * @brief GUI (wx 版) の設定の永続化の実装
 */
#include "gui/settings.h"

namespace {

/// このクラス専用のセクション名 (VCL 版に対応物は無い新規設計)
const UnicodeString kSection = _T("WxGuiWindow");

}  // namespace

//---------------------------------------------------------------------------
Settings::Settings(const UnicodeString &ini_path) : ini_(new UsrIniFile(ini_path))
{
	Load();
}

//---------------------------------------------------------------------------
void Settings::Load()
{
	Window.left		= ini_->ReadInteger(kSection, _T("Left"), -1);
	Window.top		= ini_->ReadInteger(kSection, _T("Top"), -1);
	Window.width	= ini_->ReadInteger(kSection, _T("Width"), Window.width);
	Window.height	= ini_->ReadInteger(kSection, _T("Height"), Window.height);
	Window.maximized = ini_->ReadBool(kSection, _T("Maximized"), false);

	LeftDir  = ini_->ReadString(kSection, _T("LeftDir"),  EmptyStr);
	RightDir = ini_->ReadString(kSection, _T("RightDir"), EmptyStr);

	WorkListName = ini_->ReadString(kSection, _T("WorkListName"), EmptyStr);
	HomeWorkList = ini_->ReadString(kSection, _T("HomeWorkList"), EmptyStr);
}

//---------------------------------------------------------------------------
bool Settings::Save()
{
	ini_->WriteInteger(kSection, _T("Left"),   Window.left);
	ini_->WriteInteger(kSection, _T("Top"),    Window.top);
	ini_->WriteInteger(kSection, _T("Width"),  Window.width);
	ini_->WriteInteger(kSection, _T("Height"), Window.height);
	ini_->WriteBool(kSection, _T("Maximized"), Window.maximized);

	ini_->WriteString(kSection, _T("LeftDir"),  LeftDir);
	ini_->WriteString(kSection, _T("RightDir"), RightDir);

	ini_->WriteString(kSection, _T("WorkListName"), WorkListName);
	ini_->WriteString(kSection, _T("HomeWorkList"), HomeWorkList);

	return ini_->UpdateFile();
}

//---------------------------------------------------------------------------
UnicodeString Settings::DefaultIniPath()
{
	// VCL 版と同じ ini (<exe名>.ini) を上書きしないよう、GUI 版専用の
	// 別ファイルにする (理由は gui/settings.h の解説を参照)
	return ChangeFileExt(Application->ExeName, _T("_wx.ini"));
}
