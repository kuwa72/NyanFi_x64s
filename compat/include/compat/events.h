/**
 * @file compat/events.h
 * @brief VCL のイベントプロパティ (`TNotifyEvent` 等の __closure) 互換シム
 *
 * C++Builder はメンバ関数名をそのままイベントプロパティへ代入すると、暗黙に
 * 「レシーバの this + メンバ関数ポインタ」の閉包 (`__closure` 拡張) を作る:
 *
 *   SwatchPaintBox->OnPaint = SwatchPaintBoxPaint;   // Delphi/BCC: 暗黙に
 *                                                     // Closure(this, &T::SwatchPaintBoxPaint)
 *
 * 標準 C++ にはこの拡張が無い。非 static メンバ関数名は「ポインタ・トゥ・
 * メンバ」を要求する文脈では暗黙に `&T::Member` へ変換されるが (このヘッダの
 * `TClosureEvent(void (T::*)(Args...))` コンストラクタが実際に使うのはこの
 * 変換)、その変換だけではレシーバ (`this`) までは伝わらない。呼び出し式を
 * 変えずに (`pp->OnPaint = Method;`) 型だけ通す都合上、この経路で作られた
 * TClosureEvent は「代入されたことは分かる (truthy になる)」が「実際に
 * 正しいレシーバで呼び出すことはできない」。
 *
 * 対象範囲 (Phase 1: usr_swatch.cpp / usr_scrpanel.cpp) では、イベントは
 * 実際の Win32 メッセージループ (WM_PAINT 等) からしか発火せず、
 * ヘッドレスな doctest では発火経路が無い。呼び出し (`operator()`) は
 * gui_stubs.h と同じ方針で「宣言のみ」にしてあり、万一実行されると
 * リンクエラー (テストバイナリでは tests/core/test_link_stubs.cpp の
 * abort() スタブ) で気付ける。
 */
#ifndef NYANFI_COMPAT_EVENTS_H
#define NYANFI_COMPAT_EVENTS_H

#include <cstddef>

/**
 * @brief TNotifyEvent / TMouseEvent / TWndMethod 等の __closure 相当
 * @tparam Args イベントハンドラの引数型 (Sender を含む)
 */
template <class... Args>
class TClosureEvent {
public:
	TClosureEvent() = default;
	TClosureEvent(std::nullptr_t) {}	//!< `OnXxx = NULL;` 用

	/// メンバ関数名をそのまま代入する形 (`OnPaint = Method;`) を通す。
	/// レシーバ (this) は保持できない (上記クラスコメント参照)
	template <class T>
	TClosureEvent(void (T::*)(Args...)) : bound_(true)
	{
	}

	explicit operator bool() const { return bound_; }
	bool operator!() const { return !bound_; }
	bool operator==(std::nullptr_t) const { return !bound_; }
	bool operator!=(std::nullptr_t) const { return bound_; }

	TClosureEvent &operator=(std::nullptr_t)
	{
		bound_ = false;
		return *this;
	}
	template <class T>
	TClosureEvent &operator=(void (T::*)(Args...))
	{
		bound_ = true;
		return *this;
	}

	/// @warning 宣言のみ。実際に呼び出す経路がリンクされると未定義参照になる
	void operator()(Args... args) const;

private:
	bool bound_ = false;
};

#endif  // NYANFI_COMPAT_EVENTS_H
