/**
 * @file compat/win_rect.h
 * @brief `TRect *` をそのまま Win32 API に渡している箇所を受けるオーバーロード
 *
 * @details C++Builder の `TRect` は `tagRECT` を継承しているため `&rc` が
 *          そのまま `LPRECT` になる。互換シムの `Graphics::TRect` は
 *          `Location` プロキシ (所有者ポインタを持つ) を含むので **RECT と
 *          同一レイアウトではなく**、ポインタのキャストは安全でない。
 *
 *          `TRect` はコードベース全域で値としてやり取りされる型なので、
 *          継承構造を変えるのは影響が大きすぎる。規約3 の「まずシム側で
 *          吸収できないか考える」に従い、**呼ばれる側の Win32 API を
 *          オーバーロードして受ける**ことにした。src は書き換えない。
 *
 *          各オーバーロードは `RECT` に詰め替えて本物を呼び、**出力のある
 *          API は結果を書き戻す** (`DT_CALCRECT` の DrawText、GetWindowRect、
 *          AdjustWindowRect、GetUpdateRect、DrawEdge)。書き戻しを忘れると
 *          「コンパイルは通るのに値が返ってこない」壊れ方をする。
 *
 *          対象は src を grep して実際に `&<TRect>` を渡している API:
 *          DrawText 6 / GetWindowRect 5 / ClipCursor 2 / AdjustWindowRect 2 /
 *          InvalidateRect 1 / GetUpdateRect 1 / DrawEdge 1。
 *          `DwmGetWindowAttribute` も 2箇所あるが、dwmapi.h を取り込んでいる
 *          ファイルがまだビルド対象に無いのでここには置いていない。
 *          新しい API が出てきたらここに足す。
 *
 *          **各オーバーロードはテンプレートにしてある。** 素の関数にすると
 *          `::InvalidateRect(hWnd, NULL, TRUE)` (Global.cpp:15076) が
 *          `const RECT*` 版と曖昧になる。テンプレートなら NULL から型を
 *          推論できず候補から外れるので、本物が選ばれる。
 */
#ifndef NYANFI_COMPAT_WIN_RECT_H
#define NYANFI_COMPAT_WIN_RECT_H

#include <type_traits>

#include "compat/graphics.h"

/// 実引数がちょうど Graphics::TRect のときだけ候補にする。
/// NULL からは型を推論できないので、`InvalidateRect(hWnd, NULL, TRUE)` は
/// 本物 (const RECT*) の方に流れる
#define NYANFI_ONLY_TRECT(R) \
	std::enable_if_t<std::is_same_v<std::remove_cv_t<R>, Graphics::TRect>, int> = 0

namespace compat {

/// TRect → RECT (値の詰め替え)
inline ::RECT to_win_rect(const Graphics::TRect &r)
{
	::RECT w;
	w.left = r.Left;
	w.top = r.Top;
	w.right = r.Right;
	w.bottom = r.Bottom;
	return w;
}

/// RECT → TRect (出力のある API の書き戻し用)
inline void from_win_rect(Graphics::TRect &r, const ::RECT &w)
{
	r.Left = w.left;
	r.Top = w.top;
	r.Right = w.right;
	r.Bottom = w.bottom;
}

}  // namespace compat

//---------------------------------------------------------------------------
// 入力のみ (書き戻し不要)
//---------------------------------------------------------------------------
template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL ClipCursor(const R *rect)
{
	const ::RECT w = compat::to_win_rect(*rect);
	return ::ClipCursor(&w);
}

template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL InvalidateRect(HWND hWnd, const R *rect, BOOL erase)
{
	const ::RECT w = compat::to_win_rect(*rect);
	return ::InvalidateRect(hWnd, &w, erase);
}

//---------------------------------------------------------------------------
// 出力あり (必ず書き戻す)
//---------------------------------------------------------------------------
template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL GetWindowRect(HWND hWnd, R *rect)
{
	::RECT w = {};
	const BOOL ok = ::GetWindowRect(hWnd, &w);
	if (rect != nullptr) compat::from_win_rect(*rect, w);
	return ok;
}

template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL GetUpdateRect(HWND hWnd, R *rect, BOOL erase)
{
	::RECT w = {};
	const BOOL ok = ::GetUpdateRect(hWnd, &w, erase);
	if (rect != nullptr) compat::from_win_rect(*rect, w);
	return ok;
}

template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL AdjustWindowRect(R *rect, DWORD style, BOOL menu)
{
	if (rect == nullptr) return FALSE;
	::RECT w = compat::to_win_rect(*rect);
	const BOOL ok = ::AdjustWindowRect(&w, style, menu);
	compat::from_win_rect(*rect, w);
	return ok;
}

template <class R, NYANFI_ONLY_TRECT(R)>
inline BOOL DrawEdge(HDC hdc, R *rect, UINT edge, UINT flags)
{
	if (rect == nullptr) return FALSE;
	::RECT w = compat::to_win_rect(*rect);
	const BOOL ok = ::DrawEdge(hdc, &w, edge, flags);
	compat::from_win_rect(*rect, w);  // BF_ADJUST 指定時に縮む
	return ok;
}

/// DrawText はマクロ (DrawTextW に展開される) なので W 付きで受ける。
/// DT_CALCRECT 指定時は矩形が書き換わるので必ず書き戻す
template <class R, NYANFI_ONLY_TRECT(R)>
inline int DrawTextW(HDC hdc, LPCWSTR text, int count, R *rect, UINT format)
{
	if (rect == nullptr) return 0;
	::RECT w = compat::to_win_rect(*rect);
	const int res = ::DrawTextW(hdc, text, count, &w, format);
	compat::from_win_rect(*rect, w);
	return res;
}

#endif  // NYANFI_COMPAT_WIN_RECT_H
