/**
 * @file gui/external.h
 * @brief 外部プログラムとの連携 (wx 非依存)
 *
 * @details VCL 版の該当は `src/MainFrm.cpp` の `CommandPrompt` / `PowerShell` /
 *          `WinTerminal` / `OpenByExp` / `OpenCtrlPanel` / `FileRun` /
 *          `OpenTrash` / `ContextMenu`。
 *
 *          起動そのものは `ShellExecute` を呼ぶだけなので、
 *          **「何をどう起動するか」だけ**をここに切り出して固定する (規約8)。
 */
#ifndef NYANFI_GUI_EXTERNAL_H
#define NYANFI_GUI_EXTERNAL_H

#include <vector>

namespace external {

/// 起動するシェルの種類
enum class ShellKind { CommandPrompt, PowerShell, WindowsTerminal };

/// 起動する内容 (実行ファイルと引数)
struct LaunchSpec {
	UnicodeString file;        //!< 実行ファイル
	UnicodeString parameters;  //!< 引数
	UnicodeString directory;   //!< 作業ディレクトリ
};

/**
 * @brief シェルを指定のディレクトリで開くための起動内容を作る
 * @details 実測 (MainFrm.cpp:14449 / 24043): VCL は既定では**作業ディレクトリを
 *          渡すだけ**で、`RC` パラメータのときだけ `cd` するコマンドを組み立てる。
 *          こちらは既定 (作業ディレクトリを渡す) だけを実装する。
 *
 *          Windows Terminal は VCL の該当実装を読めていないので、
 *          `wt.exe -d <dir>` という**こちらの判断**。
 */
LaunchSpec ShellLaunchSpec(ShellKind kind, const UnicodeString &directory);

/**
 * @brief エクスプローラで開くための起動内容を作る
 * @param path 開く対象。ディレクトリならそこを開き、ファイルなら選択した状態で開く
 * @details 実測 (MainFrm.cpp:22591): `::{...}` / `shell:` / `/` で始まるものは
 *          そのまま引数として渡す (特殊フォルダの指定)。それ以外はパスとして扱う。
 *          ファイルを選択状態で開くには `/select,` を付ける
 */
LaunchSpec ExplorerLaunchSpec(const UnicodeString &path, bool is_dir);

}  // namespace external

#endif  // NYANFI_GUI_EXTERNAL_H
