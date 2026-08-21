/**
 * @file compat/netencoding.h
 * @brief System.NetEncoding::TURLEncoding のうち src が使う分の互換実装
 *
 * @details src 全体を grep した結果、使っているのは **2箇所だけ**:
 *
 *   | 箇所 | 呼び出し | 用途 |
 *   |---|---|---|
 *   | `src/usr_shell.cpp:420` | `TURLEncoding::URL->Decode(fnam)` | URL から取り出した**ファイル名**のパーセントデコード |
 *   | `src/UserFunc.cpp:1635` | `TURLEncoding::URL->Encode(kwd)` | Web 検索の**キーワード**を URL テンプレートの `\S` に埋める |
 *
 * @warning **本物の Delphi の既定と一致するか未確認**。Delphi の `TURLEncoding`
 *          は「安全でない文字」の集合と `SpacesAsPlus` などのオプションで挙動が
 *          変わる。手元に RTL のソースが無いため、**2つの呼び出し箇所の用途から
 *          逆算して決めた**。判断は docs/port/decisions-needed.md に出してある。
 *
 *          - `Encode` は**未予約文字 (`A-Za-z0-9-._~`) 以外をすべて**符号化する。
 *            検索キーワードは URL のクエリに埋め込まれるので、`&` や `=` を
 *            残すと**キーワードの中身でクエリが壊れる**
 *          - `Decode` は `+` を空白に**しない**。取り出す対象がファイル名なので、
 *            `a+b.txt` の `+` は `+` のままであってほしい
 */
#ifndef NYANFI_COMPAT_NETENCODING_H
#define NYANFI_COMPAT_NETENCODING_H

#include "compat/ustring.h"

/// System.NetEncoding::TURLEncoding 相当
class TURLEncoding {
public:
	/// 未予約文字 (`A-Za-z0-9-._~`) 以外を UTF-8 で percent-encode する
	UnicodeString Encode(const UnicodeString &s) const;
	/// `%XX` を復号する (UTF-8 として解釈)。`+` は空白にしない
	UnicodeString Decode(const UnicodeString &s) const;

	/// `TURLEncoding::URL->Encode(...)` の形で呼べるようにするための共有インスタンス
	static TURLEncoding *const URL;
};

//---------------------------------------------------------------------------
namespace System {
namespace Netencoding {
using ::TURLEncoding;
}  // namespace Netencoding
using namespace Netencoding;
}  // namespace System

namespace Netencoding = System::Netencoding;
namespace NetEncoding = System::Netencoding;

#endif  // NYANFI_COMPAT_NETENCODING_H
