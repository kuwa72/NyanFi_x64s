/**
 * @file IdURI.hpp
 * @brief Indy の `TIdURI` のうち src が使う `URLEncode()` だけの互換実装
 *
 * @details Indy は C++Builder に同梱されているライブラリで、OSS 版には無い。
 *          src 全体を grep した結果、使っているのは `TIdURI::URLEncode()` の
 *          **2箇所だけ**だった:
 *            - `src/Global.cpp:13402`  `ini_HtmConv_def()` で HTML 変換用に URL を整える
 *            - `src/TxtViewer.cpp:5281` `OpenURL` コマンドで開く URL を整える
 *          どちらも「ブラウザに渡す前に非 ASCII と空白を percent-encode する」用途で、
 *          区切り記号 (`://` `/` `?` `&` など) は残す必要がある。
 *
 *          **本物との差**: Indy の `URLEncode` は URI をスキーム・ホスト・パス・
 *          クエリに分解し、部分ごとに違う文字集合で符号化する。ここでは分解せず、
 *          「未予約文字と予約区切り文字はそのまま、それ以外を UTF-8 で
 *          percent-encode する」という単純化をしている。上記2箇所の用途では
 *          結果が変わらないことを想定しているが、**分解が要る URL では差が出る**。
 *          そのときはここを本物に寄せること。
 */
#ifndef NYANFI_COMPAT_IDURI_HPP
#define NYANFI_COMPAT_IDURI_HPP

#include "compat/ustring.h"

/// Indy の `IndyTextEncoding_UTF8()` に相当する引数。
/// 本互換実装は常に UTF-8 で符号化するため、値は使わない
/// (呼び出し側を書き換えずに済ませるためだけに置いてある)。
enum class IdTextEncoding { UTF8 };
inline IdTextEncoding IndyTextEncoding_UTF8() { return IdTextEncoding::UTF8; }

class TIdURI {
public:
	/// URL を percent-encode する。既に符号化済みの `%xx` は二重符号化しない
	static UnicodeString URLEncode(const UnicodeString &url,
	                               IdTextEncoding encoding = IdTextEncoding::UTF8);
};

#endif  // NYANFI_COMPAT_IDURI_HPP
