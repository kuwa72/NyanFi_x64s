/**
 * @file gui/image_view_ops.cpp
 * @brief Iモード画像操作の判断ロジック層の実装 (wx 非依存)
 */
#include "gui/image_view_ops.h"

#include <algorithm>
#include <cmath>

#include "usr_file_ex.h"
#include "usr_file_inf.h"

namespace image_view_ops {

namespace {

/// ズーム段階 (src/Global.cpp の ZoomRatioList 既定値と同じ)
constexpr int kZoomSteps[] = {10, 25, 50, 75, 100, 150, 200, 300, 400};
constexpr int kZoomStepCount = static_cast<int>(sizeof(kZoomSteps) / sizeof(kZoomSteps[0]));

}  // namespace

//---------------------------------------------------------------------------
int ExifOrientationToRotCw(int ori)
{
	// src/imgv_thread.cpp:724-728、src/thumb_thread.cpp:190-194、
	// src/SubView.cpp:132-136 と同一対応 (ミラー系は VCL も無視する)
	if (ori == 6) return 1;
	if (ori == 3) return 2;
	if (ori == 8) return 3;
	return 0;
}

//---------------------------------------------------------------------------
bool ShouldApplyExifRotation(const UnicodeString &fnam)
{
	// VCL の `res!=LOADED_BY_WIC` 条件と同値。load_ImageFile
	// (src/Global.cpp) は FEXT_WICSTD に含まれる拡張子だけ LOADED_BY_STD を
	// 返し、それ以外 (RAW/heic/webp 等) は LOADED_BY_WIC になる
	const UnicodeString fext = get_extension(fnam);
	return test_ExifExt(fext) && test_FileExt(fext, FEXT_WICSTD);
}

//---------------------------------------------------------------------------
ImageSize TransformedSize(unsigned int w, unsigned int h, const Transform &t)
{
	if (t.rot_cw % 2 != 0) return {h, w};
	return {w, h};
}

//---------------------------------------------------------------------------
PixelBuf TransformBuffer(const unsigned char *src, unsigned int w, unsigned int h, const Transform &t)
{
	const ImageSize sz = TransformedSize(w, h, t);
	PixelBuf dst{sz.w, sz.h, std::vector<unsigned char>(static_cast<std::size_t>(sz.w) * sz.h * 3)};

	const int rot = ((t.rot_cw % 4) + 4) % 4;
	for (unsigned int y = 0; y < sz.h; ++y) {
		for (unsigned int x = 0; x < sz.w; ++x) {
			// 先に回転 (dst→src の逆写像)。CW90: dst(x,y)=src(y,H-1-x)
			unsigned int sx = x, sy = y;
			switch (rot) {
			case 1: sx = y; sy = h - 1 - x; break;
			case 2: sx = w - 1 - x; sy = h - 1 - y; break;
			case 3: sx = w - 1 - y; sy = x; break;
			default: break;
			}
			// 後に反転 (VCL の WICBitmapTransform 相当の単独操作を順に適用)
			if (t.flip_h) sx = w - 1 - sx;
			if (t.flip_v) sy = h - 1 - sy;
			const unsigned char *p = src + (static_cast<std::size_t>(sy) * w + sx) * 3;
			unsigned char *q = dst.rgb.data() + (static_cast<std::size_t>(y) * sz.w + x) * 3;
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
		}
	}
	return dst;
}

//---------------------------------------------------------------------------
int RotCwStep(int rot_cw, bool right)
{
	return (((rot_cw + (right ? 1 : 3)) % 4) + 4) % 4;
}

//---------------------------------------------------------------------------
void ApplyGrayscale(std::vector<unsigned char> &rgb)
{
	// WIC の 8bppGray 変換 (BT.601 輝度) 相当の近似
	for (std::size_t i = 0; i + 2 < rgb.size(); i += 3) {
		const double y = 0.299 * rgb[i] + 0.587 * rgb[i + 1] + 0.114 * rgb[i + 2];
		const unsigned char g = static_cast<unsigned char>(std::lround(std::clamp(y, 0.0, 255.0)));
		rgb[i] = rgb[i + 1] = rgb[i + 2] = g;
	}
}

//---------------------------------------------------------------------------
int NextZoomStep(int cur, int dir)
{
	// src/MainFrm.cpp::ZoomInIActionExecute/ZoomOutIActionExecute と同じ探索。
	// 端まで来たら cur を返す ("z_over" と同じく何もしない)
	if (dir > 0) {
		for (int i = 0; i < kZoomStepCount; ++i) {
			if (cur < kZoomSteps[i]) return kZoomSteps[i];
		}
	}
	else {
		for (int i = kZoomStepCount - 1; i >= 0; --i) {
			if (cur > kZoomSteps[i]) return kZoomSteps[i];
		}
	}
	return cur;
}

//---------------------------------------------------------------------------
double FitRatio(unsigned int img_w, unsigned int img_h, int avail_w, int avail_h)
{
	if (img_w == 0 || img_h == 0 || avail_w <= 0 || avail_h <= 0) return 1.0;
	double r = std::min(static_cast<double>(avail_w) / img_w, static_cast<double>(avail_h) / img_h);
	if (r > 1.0 || r <= 0.0) r = 1.0;
	return r;
}

//---------------------------------------------------------------------------
int NextPrevIndex(int count, int cur, int dir, const std::vector<char> &failed, bool loop)
{
	if (count <= 0) return cur;
	const auto is_valid = [&](int i) {
		return i >= 0 && i < count &&
		       (static_cast<std::size_t>(i) >= failed.size() || !failed[static_cast<std::size_t>(i)]);
	};
	if (is_valid(cur) == false && (cur < 0 || cur >= count)) return cur;

	int idx = cur + dir;
	while (idx >= 0 && idx < count) {
		if (is_valid(idx)) return idx;
		idx += dir;
	}
	// 端に達した。loop なら反対側の先頭/末尾の有効項目へ (VCL の LoopViewCursor)
	if (loop) {
		if (dir > 0) {
			for (int i = 0; i < count; ++i)
				if (is_valid(i)) return i;
		}
		else {
			for (int i = count - 1; i >= 0; --i)
				if (is_valid(i)) return i;
		}
	}
	return cur;
}

//---------------------------------------------------------------------------
int FirstValidIndex(const std::vector<char> &failed)
{
	for (std::size_t i = 0; i < failed.size(); ++i)
		if (!failed[i]) return static_cast<int>(i);
	return -1;
}

//---------------------------------------------------------------------------
int LastValidIndex(const std::vector<char> &failed)
{
	for (std::size_t i = failed.size(); i > 0; --i)
		if (!failed[i - 1]) return static_cast<int>(i - 1);
	return -1;
}

//---------------------------------------------------------------------------
std::optional<int> ParseJumpIndex(const UnicodeString &param, int count, int cur)
{
	// src/MainFrm.cpp::JumpIndexActionExecute と同じ (入力ダイアログ部分を除く)。
	// 空・0・非数値は中止 (nullopt)
	if (param.IsEmpty() || count <= 0) return std::nullopt;

	UnicodeString s = param;
	int rel_sig = 0;
	if (s.Length() > 1 && (s[1] == _T('+') || s[1] == _T('-'))) {
		rel_sig = (s[1] == _T('-')) ? -1 : 1;
		s.Delete(1, 1);
	}

	// VCL は ToIntDef(0) 後に idx==0 で Abort (非数値も 0 扱いで中止)
	const int idx = s.ToIntDef(0);
	if (idx == 0) return std::nullopt;

	const int max_idx = count - 1;
	if (rel_sig != 0) {
		return std::clamp(cur + rel_sig * idx, 0, max_idx);
	}
	return std::clamp(idx - 1, 0, max_idx);
}

//---------------------------------------------------------------------------
int ScrollStepPos(int pos, int lo, int hi, int step, int dir)
{
	// src/MainFrm.cpp の ScrollUpI/ScrollDownI/ScrollLeft/ScrollRight と同じ
	// (VCL の TControlScrollBar が範囲外を clamp するのと同値)
	if (step <= 0 || lo >= hi) return (pos < lo) ? lo : (pos > hi) ? hi : pos;
	return std::clamp(pos + ((dir >= 0) ? step : -step), lo, hi);
}

//---------------------------------------------------------------------------
int PageStepIndex(int cur, int count, int page, int dir)
{
	// src/MainFrm.cpp の NextPage/PrevPage (グリッド表示件数分だけ進める) と
	// PageUpI/PageDownI (全面表示の1ページ分) の共通部分。SetThumbnailIndex
	// の clamp (0〜count-1) と同値
	if (count <= 0 || page <= 0) return cur;
	return std::clamp(cur + ((dir >= 0) ? page : -page), 0, count - 1);
}

//---------------------------------------------------------------------------
int DoubleStepIndex(int count, int cur, int dir)
{
	// src/MainFrm.cpp::NextPrevFileICore の IsDoubleStep 分岐と同じ。
	// -1 (REDRAW のみで留まる) の代わりに cur を返す
	if (count <= 0) return cur;
	if (dir >= 0) {
		if (cur >= count - 1) return cur;
		const int max_idx = count - 2;
		if (cur < max_idx) return cur + 2;
		if (cur == max_idx) return cur;
		return max_idx;
	}
	if (cur >= 2) return cur - 2;
	if (cur == 0) return cur;
	return 0;
}

//---------------------------------------------------------------------------
bool ToggleViewFlag(bool cur, const UnicodeString &param)
{
	// src/MainFrm.cpp:12651 の SetToggleAction と同じ
	if (SameText(param, _T("ON"))) return true;
	if (SameText(param, _T("OFF"))) return false;
	return !cur;
}

//---------------------------------------------------------------------------
bool NextPageBind(bool right_bind, const UnicodeString &param)
{
	// src/MainFrm.cpp::PageBindActionExecute と同じ
	if (SameText(param, _T("R"))) return true;
	if (SameText(param, _T("L"))) return false;
	return !right_bind;
}

//---------------------------------------------------------------------------
bool HasActionToken(const UnicodeString &param, const UnicodeString &token)
{
	// src/MainFrm.cpp::TestActionParam と同じ (";" 区切りの完全一致)
	if (param.IsEmpty() || token.IsEmpty()) return false;
	UnicodeString rest = param;
	while (!rest.IsEmpty()) {
		const int p = rest.Pos(_T(";"));
		const UnicodeString cur = (p == 0) ? rest : rest.SubString(1, p - 1);
		if (SameText(cur, token)) return true;
		if (p == 0) break;
		rest.Delete(1, p);
	}
	return false;
}

//---------------------------------------------------------------------------
std::optional<int> NextInterpolation(int cur, const UnicodeString &param)
{
	// src/MainFrm.cpp::SetInterpolationActionExecute と同じ。
	// idstr="NLCFHX" の中を param で絞った順に進める
	static const char kIds[] = "NLCFHX";
	if (param.IsEmpty()) return std::nullopt;
	const UnicodeString ids(kIds);
	UnicodeString cur_ch;
	if (cur >= 0 && cur < 6) {
		cur_ch = ids.SubString(cur + 1, 1);
	}
	const int p = param.Pos(cur_ch.IsEmpty() ? UnicodeString(_T("\x01")) : cur_ch);
	int next_idx;
	if (p == 0 || p >= param.Length()) {
		next_idx = 1;
	}
	else {
		next_idx = p + 1;
	}
	const UnicodeString next_ch = param.SubString(next_idx, 1);
	const int id_pos = ids.Pos(next_ch);
	if (id_pos == 0) return std::nullopt;
	return id_pos - 1;
}

//---------------------------------------------------------------------------
UnicodeString ResolveBgImagePath(const UnicodeString &param, const UnicodeString &cursor)
{
	// src/MainFrm.cpp::LoadBgImageActionExecute と同じ (指定優先)
	if (!param.IsEmpty()) return param;
	return cursor;
}

//---------------------------------------------------------------------------
bool ShouldHideSubViewer(bool visible, const UnicodeString &param)
{
	// src/MainFrm.cpp::SubViewerActionExecute の表示中分岐と同じ
	if (!visible) return false;
	return param.IsEmpty() || HasActionToken(param, _T("OFF"));
}

//---------------------------------------------------------------------------
int SubViewerRotateCode(const UnicodeString &param)
{
	// src/MainFrm.cpp::SubViewerActionExecute の回転分岐と同じ順序
	if (HasActionToken(param, _T("RL"))) return 3;
	if (HasActionToken(param, _T("RR"))) return 1;
	if (HasActionToken(param, _T("FH"))) return 4;
	if (HasActionToken(param, _T("FV"))) return 5;
	return 0;
}

//---------------------------------------------------------------------------
bool ShouldDuplicateOnNext(const UnicodeString &param)
{
	// src/MainFrm.cpp::NextNyanFiActionExecute の "DN" 分岐と同じ
	return HasActionToken(param, _T("DN"));
}

//---------------------------------------------------------------------------
std::optional<int> ParseSimilarImageSize(const UnicodeString &param)
{
	// src/MainFrm.cpp::SimilarImageActionExecute と同じ (既定 32、4..120)。
	// CB/HG/DH/AH/PH/CC トークンを除いた残りを数値として読む
	static const wchar_t *kTokens[] = {_T("CB"), _T("HG"), _T("DH"), _T("AH"), _T("PH"), _T("CC")};
	UnicodeString rest = param;
	UnicodeString num;
	while (!rest.IsEmpty()) {
		const int p = rest.Pos(_T(";"));
		const UnicodeString cur = (p == 0) ? rest : rest.SubString(1, p - 1);
		bool is_token = false;
		for (const wchar_t *t : kTokens) {
			if (SameText(cur, t)) {
				is_token = true;
				break;
			}
		}
		if (!is_token && num.IsEmpty()) num = cur;
		if (p == 0) break;
		rest.Delete(1, p);
	}
	if (num.IsEmpty()) return 32;
	const int sz = num.ToIntDef(-1);
	if (sz < 4 || sz > 120) return std::nullopt;
	return sz;
}

//---------------------------------------------------------------------------
ClipCopySrc ResolveClipCopySource(bool has_image, const UnicodeString &param)
{
	// src/MainFrm.cpp::ClipCopyActionExecute の分岐と同じ (AGif/メタファイル/
	// アイコンは Phase 3 の対象外のため、画像の有無と "VI" 指定だけで決める)
	if (!has_image) return ClipCopySrc::None;
	if (HasActionToken(param, _T("VI"))) return ClipCopySrc::Viewer;
	return ClipCopySrc::Image;
}

//---------------------------------------------------------------------------
UnicodeString ResolveFileEditPath(const UnicodeString &param, const UnicodeString &cursor)
{
	// VCL の FileEdit は指定があればそれ、無ければカーソル位置
	if (!param.IsEmpty()) return param;
	return cursor;
}

//---------------------------------------------------------------------------
bool ShouldShowCmdFileFilter(const UnicodeString &param)
{
	// src/MainFrm.cpp::CmdFileListActionExecute の "FF" 分岐と同じ
	return HasActionToken(param, _T("FF"));
}

//---------------------------------------------------------------------------
int PopupMenuIndex(const UnicodeString &param)
{
	// src/MainFrm.cpp::PopupMainMenuActionExecute と同じ
	if (param.IsEmpty()) return -1;
	static const UnicodeString kKinds = _T("FESVVVLTOH");
	const int p = kKinds.Pos(param.SubString(1, 1).UpperCase());
	return p - 1;
}

}  // namespace image_view_ops
