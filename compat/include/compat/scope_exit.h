/**
 * @file compat/scope_exit.h
 * @brief C++Builder の `try { } __finally { }` を置き換える RAII ヘルパ
 *
 * @details `__finally` は C++Builder (と MSVC の SEH) の拡張で、GCC には無い。
 *          src には 12箇所ある (usr_shell.cpp 7 / Global.cpp / usr_highlight.cpp /
 *          usr_file_inf.cpp / task_thread.cpp / CalcDlg.cpp 各1)。
 *
 *          先行して移植した `src/usr_file_inf.cpp` と `src/usr_highlight.cpp` は
 *          同じものをファイル内の無名名前空間に書き写している。写経が増える前に
 *          ここへ出した (既存の2ファイルは規約3 に従いそのままにしてある)。
 *
 *          **`__finally` との差**: `__finally` は「対応する try に入る前に
 *          例外が起きた場合」も動くが、こちらは `make_scope_exit()` を作った
 *          後でしか動かない。取得処理そのものが投げうる場合は、取得を
 *          `make_scope_exit()` より前に置き、その差を呼び出し側にコメントで残すこと。
 */
#ifndef NYANFI_COMPAT_SCOPE_EXIT_H
#define NYANFI_COMPAT_SCOPE_EXIT_H

#include <utility>

namespace compat {

/// スコープを抜けるときに渡された処理を必ず実行する
template <class F>
class scope_exit {
public:
	explicit scope_exit(F fn) : fn_(std::move(fn)) {}
	~scope_exit() { fn_(); }

	scope_exit(const scope_exit &) = delete;
	scope_exit &operator=(const scope_exit &) = delete;

private:
	F fn_;
};

template <class F>
scope_exit<F> make_scope_exit(F fn)
{
	return scope_exit<F>(std::move(fn));
}

}  // namespace compat

#endif  // NYANFI_COMPAT_SCOPE_EXIT_H
