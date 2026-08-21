/**
 * @file compat/regex.h
 * @brief System.RegularExpressions (TRegEx 等) 互換シム
 *
 * 対象コードでの実測: TRegEx::Match 18 / TRegEx::Replace 12 / TRegEx::IsMatch 8 /
 * TRegEx::Escape 2、TMatch 10。実際に使われるオプションは roIgnoreCase /
 * roMultiLine / roCompiled の 3 つのみ (grep 実測。roDivide 等は無関係な識別子の
 * 部分一致であることを確認済み)。
 *
 * バックエンド: Phase 0 では std::wregex (ECMAScript 構文) を使う。
 * Phase 1 で PCRE2 に差し替える計画のため、実装は本ファイル内の名前空間
 * detail に閉じ込め、TRegEx 自身は「パターン文字列 + オプション」から
 * 都度バックエンドを組み立てる薄いラッパに留める。差し替え時は detail 以下
 * だけを書き換えればよい構造にしてある。
 *
 * 重要な既知の差異 (docs/port/phase0-report.md にも記載):
 *  - std::wregex の既定ロケールでは \w 等の文字クラスは ASCII のみを対象とする
 *    (PCRE も UCP 無効時は同じなので実害は小さい見込み)。
 *  - roExplicitCapture / roSingleLine / roIgnorePatternSpace / roNotEmpty は
 *    列挙値として受理はするが、std::wregex に対応する機能が無いため実装して
 *    いない (現状の src/ では未使用なので実害なし)。
 *  - roCompiled は std::regex_constants::optimize にマップしている
 *    (「事前コンパイルして高速化する」という意図が近いため)。
 */
#ifndef NYANFI_COMPAT_REGEX_H
#define NYANFI_COMPAT_REGEX_H

#include <regex>
#include <vector>

#include "compat/config.h"
#include "compat/exception.h"
#include "compat/set.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// TRegExOptions の各フラグ (System.RegularExpressionsAPI 相当)
enum TRegExOption {
	roIgnoreCase,			//!< 大小文字を無視			(実装済み)
	roMultiLine,			//!< ^ $ が行頭行末にもマッチ	(実装済み)
	roExplicitCapture,		//!< 名前無しグループを無効化	(未実装: 未使用のため)
	roSingleLine,			//!< . が改行にもマッチ		(未実装: 未使用のため)
	roIgnorePatternSpace,	//!< パターン中の空白/コメントを無視 (未実装: 未使用のため)
	roNotEmpty,				//!< 空文字列マッチを許可しない	(未実装: 未使用のため)
	roCompiled,				//!< 事前コンパイル。regex_constants::optimize にマップ
};
using TRegExOptions = Set<TRegExOption, roIgnoreCase, roCompiled>;

/// 不正な正規表現パターン (Delphi の ERegularExpressionError 相当)
class ERegularExpressionError : public Exception {
public:
	using Exception::Exception;
};

//---------------------------------------------------------------------------
/// キャプチャグループ 1 個分 (TGroup 相当)
class TGroup {
public:
	bool Success = false;	//!< このグループがマッチしたか
	int Index = 0;			//!< マッチ位置 (1 始まり)。未マッチなら 0
	int Length = 0;			//!< マッチ長
	UnicodeString Value;	//!< マッチ文字列
};

/// TGroupCollection 相当。Count は plain int、Item は plain vector にして
/// コピー時の所有者ポインタ破損を避ける (TStrings 系のプロキシとは異なる設計)。
class TGroupCollection {
public:
	int Count = 0;
	std::vector<TGroup> Item;
};

/// TMatch 相当
class TMatch {
public:
	bool Success = false;
	int Index = 0;		//!< マッチ位置 (1 始まり)
	int Length = 0;
	UnicodeString Value;
	TGroupCollection Groups;
};

/// TMatchCollection 相当
class TMatchCollection {
public:
	int Count = 0;
	std::vector<TMatch> Item;
};

//---------------------------------------------------------------------------
/**
 * @brief System.RegularExpressions::TRegEx 相当
 * @details 静的メソッド (IsMatch/Match/Matches/Replace/Escape/Split) が主用途。
 *          インスタンス化は usr_str.cpp の chk_RegExPtn がパターン検証のためだけ
 *          に使う (`TRegEx x(ptn, opt);` → 例外が飛ばなければ有効なパターン)。
 */
class TRegEx {
public:
	TRegEx() = default;
	TRegEx(const UnicodeString &pattern, TRegExOptions options = TRegExOptions());

	bool IsMatch(const UnicodeString &input) const;
	TMatch Match(const UnicodeString &input) const;
	TMatchCollection Matches(const UnicodeString &input) const;
	UnicodeString Replace(const UnicodeString &input, const UnicodeString &replacement) const;

	static bool IsMatch(const UnicodeString &input, const UnicodeString &pattern,
	                     TRegExOptions options = TRegExOptions());
	static TMatch Match(const UnicodeString &input, const UnicodeString &pattern,
	                     TRegExOptions options = TRegExOptions());
	static TMatchCollection Matches(const UnicodeString &input, const UnicodeString &pattern,
	                                 TRegExOptions options = TRegExOptions());
	static UnicodeString Replace(const UnicodeString &input, const UnicodeString &pattern,
	                              const UnicodeString &replacement, TRegExOptions options = TRegExOptions());
	/// パターンの特殊文字をエスケープしリテラル一致用に変換する
	static UnicodeString Escape(const UnicodeString &input);
	/// パターンに一致する箇所で分割する (src/ では未使用。API 完全性のために実装)
	static TStringDynArray Split(const UnicodeString &input, const UnicodeString &pattern,
	                              TRegExOptions options = TRegExOptions());

private:
	std::wregex re_;
	bool compiled_ = false;
};

namespace System {
namespace RegularExpressions {
using ::ERegularExpressionError;
using ::TGroup;
using ::TGroupCollection;
using ::TMatch;
using ::TMatchCollection;
using ::TRegEx;
using ::TRegExOption;
using ::TRegExOptions;
}  // namespace RegularExpressions
using namespace RegularExpressions;
}  // namespace System

namespace RegularExpressions = System::RegularExpressions;

#endif  // NYANFI_COMPAT_REGEX_H
