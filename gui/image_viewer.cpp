/**
 * @file gui/image_viewer.cpp
 * @brief 画像ビューアの実装
 */
#include "gui/image_viewer.h"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/image.h>
#include <wx/settings.h>

namespace {

/// wxString への変換 (gui/file_pane.cpp / gui/text_viewer.cpp と同じ)
inline wxString to_wx(const UnicodeString &s)
{
	return wxString(s.c_str(), static_cast<size_t>(s.Length()));
}

/**
 * @brief ズーム段階
 * @details src/Global.cpp の ZoomRatioList 既定値 ("10\n25\n50\n75\n100\n150\n
 * 200\n300\n400\n") と同じ。src/MainFrm.cpp の ZoomInIActionExecute/
 * ZoomOutIActionExecute はこの一覧を順に探して次/前の段階に飛ぶ (実測)
 */
constexpr int kZoomSteps[] = {10, 25, 50, 75, 100, 150, 200, 300, 400};
constexpr int kZoomStepCount = static_cast<int>(sizeof(kZoomSteps) / sizeof(kZoomSteps[0]));

}  // namespace

//---------------------------------------------------------------------------
ImageViewer::ImageViewer(wxWindow *parent, wxWindowID id)
	: wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_PAINT, &ImageViewer::OnPaint, this);
	Bind(wxEVT_SIZE, &ImageViewer::OnSize, this);
	Bind(wxEVT_MOUSEWHEEL, &ImageViewer::OnMouseWheel, this);
	Bind(wxEVT_MIDDLE_DOWN, &ImageViewer::OnMiddleDown, this);
}

//---------------------------------------------------------------------------
/**
 * @details ファイルを切り替えるたびにフィット表示へ戻す。VCL 版の既定
 * (src/Global.cpp: KeepZoomRatio=false) と同じ挙動 (ズーム倍率を維持する
 * KeepZoomRatio=true 相当の設定は Phase 2 骨格では対象外)
 */
void ImageViewer::LoadFile(const UnicodeString &path)
{
	path_ = path;
	fitted_ = true;
	zoom_percent_ = 100;
	scaled_for_w_ = scaled_for_h_ = -1;
	scaled_bitmap_ = wxBitmap();

	const image_load::LoadResult r = image_load::LoadForView(path);
	has_image_ = r.ok;
	if (r.ok) {
		img_w_ = r.width;
		img_h_ = r.height;
		rgb_ = r.rgb;
		error_ = EmptyStr;
	}
	else {
		img_w_ = img_h_ = 0;
		rgb_.clear();
		error_ = r.error;
	}

	Refresh();
}

//---------------------------------------------------------------------------
/**
 * @details src/UserFunc.cpp の get_ZoomRatio は画像・パネルの縦横比が入れ替わる
 * 場合 (回転表示等) まで考慮する複雑な式だが、その定義元 (UserFunc.cpp) は
 * GUI グローバル依存で未移植のため、ここでは標準的な「アスペクト比を保って
 * 収める」式に簡略化した (推測・要検証)。等倍を超えて自動拡大しない点は
 * src/Global.cpp の既定値 ImgFitMaxZoom=100 と同じ
 */
double ImageViewer::ComputeFitRatio() const
{
	if (img_w_ == 0 || img_h_ == 0) return 1.0;

	const wxSize client = GetClientSize();
	const int avail_w = std::max(1, client.x);
	const int avail_h = std::max(1, client.y - HeaderHeight());

	double r = std::min(static_cast<double>(avail_w) / static_cast<double>(img_w_),
	                     static_cast<double>(avail_h) / static_cast<double>(img_h_));
	if (r > 1.0 || r <= 0.0) r = 1.0;
	return r;
}

double ImageViewer::EffectiveRatio() const
{
	return fitted_ ? ComputeFitRatio() : (zoom_percent_ / 100.0);
}

//---------------------------------------------------------------------------
void ImageViewer::RebuildScaledBitmap()
{
	if (!has_image_ || img_w_ == 0 || img_h_ == 0) {
		scaled_bitmap_ = wxBitmap();
		return;
	}

	const wxSize client = GetClientSize();
	const double ratio = EffectiveRatio();

	if (scaled_bitmap_.IsOk() && scaled_for_w_ == client.x && scaled_for_h_ == client.y && scaled_ratio_ == ratio) {
		return;  // キャッシュ有効
	}

	const int tw = std::max(1, static_cast<int>(std::lround(img_w_ * ratio)));
	const int th = std::max(1, static_cast<int>(std::lround(img_h_ * ratio)));

	// static_data=true: rgb_ の生存期間はこの ImageViewer が保証する。wxImage
	// 自身はこのバッファを解放・書き換えない (Scale()/wxBitmap変換はいずれも
	// 新しいバッファへコピーする読み取り専用の使い方)
	wxImage img(static_cast<int>(img_w_), static_cast<int>(img_h_), const_cast<unsigned char *>(rgb_.data()), true);

	if (tw != static_cast<int>(img_w_) || th != static_cast<int>(img_h_)) {
		img = img.Scale(tw, th, wxIMAGE_QUALITY_BILINEAR);
	}

	scaled_bitmap_ = wxBitmap(img);
	scaled_for_w_ = client.x;
	scaled_for_h_ = client.y;
	scaled_ratio_ = ratio;
}

//---------------------------------------------------------------------------
/// F (フィット表示のON/OFF切替、推測のキー)
void ImageViewer::ToggleFitted()
{
	if (!has_image_) return;

	if (fitted_) {
		// フィット→手動: 見た目が変わらないよう、直前のフィット倍率を手動ズームの基準にする
		zoom_percent_ = static_cast<int>(std::lround(ComputeFitRatio() * 100.0));
	}
	fitted_ = !fitted_;
	Refresh();
}

//---------------------------------------------------------------------------
/// 0 (等倍表示。usr_cmdlist.cpp の "I:EqualSize" 相当。推測のキー)
void ImageViewer::SetEqualSize()
{
	if (!has_image_) return;

	// src/MainFrm.cpp::EqualSizeActionExecute と同じ (ZOOM=100, FITTED=0)
	zoom_percent_ = 100;
	fitted_ = false;
	Refresh();
}

//---------------------------------------------------------------------------
/**
 * @details src/MainFrm.cpp::ZoomInIActionExecute/ZoomOutIActionExecute と同じ
 * 探索 (ZoomRatioList の段階を順に探し、既に最大/最小の段階なら何もしない
 * "z_over" と同じ判定)。ズームイン/アウトは (フィット中でも) 必ず手動ズーム
 * (fitted_=false) に切り替える点も VCL 版と同じ (FITTED=0 を常に要求している)
 */
void ImageViewer::ZoomStep(int direction)
{
	if (!has_image_) return;

	const int current = static_cast<int>(std::lround(EffectiveRatio() * 100.0));
	int next = current;
	bool found = false;

	if (direction > 0) {
		for (int i = 0; i < kZoomStepCount; ++i) {
			if (current < kZoomSteps[i]) {
				next = kZoomSteps[i];
				found = true;
				break;
			}
		}
	}
	else {
		for (int i = kZoomStepCount - 1; i >= 0; --i) {
			if (current > kZoomSteps[i]) {
				next = kZoomSteps[i];
				found = true;
				break;
			}
		}
	}

	if (!found) return;  // 既に最大/最小の段階 (z_over と同じ。何もしない)

	zoom_percent_ = next;
	fitted_ = false;
	Refresh();
}

//---------------------------------------------------------------------------
/**
 * @details イメージビューア中のキーは VCL 版 (src/MainFrm.cpp::FormKeyDown の
 * SCMD_IVIEW 分岐) でもほぼ全て消費されるが、ここは gui/text_viewer.cpp の
 * 慣習に合わせ、認識したキーだけ true を返す
 */
bool ImageViewer::HandleKey(wxKeyEvent &event)
{
	const int code = event.GetKeyCode();

	// I:Q=Close (既定)。ESC/ENTER は src/MainFrm.cpp::FormKeyDown の
	// equal_ESC()/equal_ENTER() 分岐 (KeyFuncList を介さないハードコード) で
	// 実際にイメージビューアを閉じる動作と同じにしてある
	if (code == 'Q' || code == WXK_ESCAPE || code == WXK_RETURN || code == WXK_NUMPAD_ENTER) {
		if (on_close_) on_close_();
		return true;
	}

	switch (code) {
	case WXK_LEFT:
		if (on_navigate_) on_navigate_(-1);
		return true;
	case WXK_RIGHT:
		if (on_navigate_) on_navigate_(1);
		return true;
	case 'F':
		ToggleFitted();
		return true;
	case '0':
	case WXK_NUMPAD0:
		SetEqualSize();
		return true;
	case '+':
	case WXK_ADD:
	case WXK_NUMPAD_ADD:
		ZoomStep(1);
		return true;
	case '-':
	case WXK_SUBTRACT:
	case WXK_NUMPAD_SUBTRACT:
		ZoomStep(-1);
		return true;
	default:
		break;
	}
	return false;
}

//---------------------------------------------------------------------------
/// マウスホイール: ズームイン/アウト (src/Global.cpp の既定 WheelCmdI1="ZoomIn/ZoomOut" と同じ)
void ImageViewer::OnMouseWheel(wxMouseEvent &event)
{
	ZoomStep(event.GetWheelRotation() > 0 ? 1 : -1);
}

/// ホイールボタンクリック: フィット表示切替 (src/Global.cpp の既定 WheelBtnCmdI="FittedSize" と同じ。
/// VCL 版は常にON固定の1方向アクションだが、ここではトグルにしてある。推測・要検証)
void ImageViewer::OnMiddleDown(wxMouseEvent &)
{
	ToggleFitted();
}

//---------------------------------------------------------------------------
void ImageViewer::OnSize(wxSizeEvent &event)
{
	Refresh();
	event.Skip();
}

//---------------------------------------------------------------------------
UnicodeString ImageViewer::HeaderText() const
{
	if (path_.IsEmpty()) return EmptyStr;

	UnicodeString s = path_;
	if (has_image_) {
		s.cat_sprintf(_T("   %u x %u   %d%%%s"), img_w_, img_h_,
		              static_cast<int>(std::lround(EffectiveRatio() * 100.0)),
		              fitted_ ? _T("  [フィット]") : EmptyStr);
	}
	else {
		s += _T("   [読み込みエラー]");
	}
	return s;
}

//---------------------------------------------------------------------------
void ImageViewer::OnPaint(wxPaintEvent &)
{
	wxAutoBufferedPaintDC dc(this);

	// 色は wxSystemSettings から取る (要件8。ライト/ダークに自動追従。VCL 版の
	// col_bgImage 既定値 clBlack をそのまま使わなかった点は意図的。報告に明記)
	const wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	const wxColour hdr_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
	const wxColour hdr_fg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);

	const wxSize client = GetClientSize();
	dc.SetBrush(wxBrush(bg));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(0, 0, client.x, client.y);

	const int header_h = HeaderHeight();
	dc.SetBrush(wxBrush(hdr_bg));
	dc.DrawRectangle(0, 0, client.x, header_h);
	dc.SetTextForeground(hdr_fg);
	dc.DrawText(to_wx(HeaderText()), 4, 2);

	if (path_.IsEmpty()) return;

	if (!has_image_) {
		dc.SetTextForeground(fg);
		dc.DrawText(to_wx(error_.IsEmpty() ? _T("画像を表示できません") : error_), 8, header_h + 8);
		return;
	}

	RebuildScaledBitmap();
	if (!scaled_bitmap_.IsOk()) return;

	const int x = (client.x - scaled_bitmap_.GetWidth()) / 2;
	const int y = header_h + std::max(0, (client.y - header_h - scaled_bitmap_.GetHeight()) / 2);
	dc.DrawBitmap(scaled_bitmap_, std::max(0, x), y);
}
