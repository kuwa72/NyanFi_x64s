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

}  // namespace image_view_ops
