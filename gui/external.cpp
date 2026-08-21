/**
 * @file gui/external.cpp
 * @brief 外部プログラム連携の実装 (設計は gui/external.h)
 */
#include "gui/external.h"

#include "usr_str.h"

namespace external {

//---------------------------------------------------------------------------
LaunchSpec ShellLaunchSpec(ShellKind kind, const UnicodeString &directory)
{
	LaunchSpec spec;
	const UnicodeString dir = ExcludeTrailingPathDelimiter(directory);

	switch (kind) {
	case ShellKind::CommandPrompt:
		spec.file = _T("cmd.exe");
		spec.directory = dir;
		break;

	case ShellKind::PowerShell:
		spec.file = _T("powershell.exe");
		spec.directory = dir;
		break;

	case ShellKind::WindowsTerminal:
		// VCL の該当実装を読めていないので、これはこちらの判断。
		// wt.exe は作業ディレクトリを渡しても中のシェルには効かないので、
		// -d で明示する
		spec.file = _T("wt.exe");
		spec.parameters = _T("-d \"") + dir + _T("\"");
		break;
	}
	return spec;
}

//---------------------------------------------------------------------------
LaunchSpec ExplorerLaunchSpec(const UnicodeString &path, bool is_dir)
{
	LaunchSpec spec;
	spec.file = _T("explorer.exe");

	// 特殊フォルダの指定はそのまま渡す (MainFrm.cpp:22596)
	if (StartsStr(_T("::{"), path) || StartsStr(_T("shell:"), path) || StartsStr(_T("/"), path)) {
		spec.parameters = path;
		return spec;
	}

	// ディレクトリはそこを開く。ファイルは選択した状態で開く
	spec.parameters = is_dir? (_T("\"") + ExcludeTrailingPathDelimiter(path) + _T("\""))
	                        : (_T("/select,\"") + path + _T("\""));
	return spec;
}

}  // namespace external
