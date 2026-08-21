/**
 * @file gui/image_load.cpp
 * @brief 画像ビューアのロジック層の実装 (WIC ラッパーの呼び出しと後始末)
 */
#include "gui/image_load.h"

#include <memory>

#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_wic.h"

namespace image_load {

namespace {

/**
 * @brief 対応拡張子の一覧
 * @details src/usr_file_inf.h の FEXT_IMAGE = FEXT_WICSTD FEXT_RAW FEXT_META
 * ".heic.webp" からメタファイル (FEXT_META = ".wmf.emf") を除いたもの。
 * IsSupportedExt() のコメントを参照
 */
const UnicodeString &SupportedExtList()
{
	static const UnicodeString list = UnicodeString(FEXT_WICSTD) + FEXT_RAW + _T(".heic.webp");
	return list;
}

}  // namespace

//---------------------------------------------------------------------------
bool IsSupportedExt(const UnicodeString &fnam)
{
	return test_FileExt(get_extension(fnam), SupportedExtList());
}

//---------------------------------------------------------------------------
LoadResult LoadForView(const UnicodeString &path)
{
	LoadResult r;

	if (!file_exists(path)) {
		r.error = _T("ファイルが見つかりません");
		return r;
	}

	unsigned int wd = 0, hi = 0;
	if (!WIC_get_img_size(path, &wd, &hi) || wd == 0 || hi == 0) {
		r.error = _T("画像として認識できません (未対応の形式か、壊れています)");
		return r;
	}

	const Int64 pixels = static_cast<Int64>(wd) * static_cast<Int64>(hi);
	if (pixels > kMaxDecodePixels) {
		r.error.sprintf(_T("画像が大きすぎるため表示できません (%u x %u)"), wd, hi);
		return r;
	}

	// WIC_load_image は例外を投げても catch(...) して false を返すので、
	// ここで追加の try/catch は不要 (usr_wic.cpp を参照)
	std::unique_ptr<Graphics::TBitmap> bmp(new Graphics::TBitmap());
	if (!WIC_load_image(path, bmp.get(), WICIMG_PREVIEW) || bmp->Empty) {
		r.error = _T("画像を読み込めませんでした");
		return r;
	}

	const int w = bmp->Width;
	const int h = bmp->Height;
	r.rgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);

	for (int y = 0; y < h; ++y) {
		const unsigned char *src = static_cast<const unsigned char *>(bmp->ScanLine[y]);
		unsigned char *dst = r.rgb.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3;
		for (int x = 0; x < w; ++x) {
			// TBitmap は 24bpp BGR (usr_wic.cpp が GUID_WICPixelFormat24bppBGR で
			// 変換している。compat/graphics.h の TBitmap 冒頭コメントも参照)。
			// 表示用に RGB へ入れ替える
			dst[x * 3 + 0] = src[x * 3 + 2];  // R
			dst[x * 3 + 1] = src[x * 3 + 1];  // G
			dst[x * 3 + 2] = src[x * 3 + 0];  // B
		}
	}

	r.ok = true;
	r.width = static_cast<unsigned int>(w);
	r.height = static_cast<unsigned int>(h);
	return r;
}

}  // namespace image_load
