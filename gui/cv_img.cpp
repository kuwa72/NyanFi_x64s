/**
 * @file gui/cv_img.cpp
 * @brief gui/cv_img.h の実装
 */
#include "gui/cv_img.h"

#include <algorithm>

namespace cv_img {

namespace {

bool valid_format(Format format)
{
	return format >= Format::Bmp && format <= Format::Hdp;
}

bool valid_scale_mode(ScaleMode mode)
{
	return mode >= ScaleMode::None && mode <= ScaleMode::Crop;
}

}  // namespace

//---------------------------------------------------------------------------
int FormatIndex(Format format)
{
	return valid_format(format) ? static_cast<int>(format) : 0;
}

//---------------------------------------------------------------------------
Format FormatFromIndex(int index)
{
	if (index < static_cast<int>(Format::Bmp) || index > static_cast<int>(Format::Hdp)) {
		return Format::Bmp;
	}
	return static_cast<Format>(index);
}

//---------------------------------------------------------------------------
UnicodeString Extension(Format format)
{
	switch (format) {
	case Format::Bmp: return _T(".bmp");
	case Format::Jpg: return _T(".jpg");
	case Format::Png: return _T(".png");
	case Format::Gif: return _T(".gif");
	case Format::Tif: return _T(".tif");
	case Format::Hdp: return _T(".hdp");
	default: return _T(".bmp");
	}
}

//---------------------------------------------------------------------------
Options Normalize(const Options &in)
{
	Options out = in;
	if (!valid_format(out.format)) out.format = Format::Bmp;
	if (!valid_scale_mode(out.scale_mode)) out.scale_mode = ScaleMode::None;
	if (out.name_mode != NameMode::Prefix && out.name_mode != NameMode::Suffix) {
		out.name_mode = NameMode::Suffix;
	}
	out.quality = std::clamp(out.quality, 0, 100);
	out.ycrcb = std::clamp(out.ycrcb, 0, 3);
	out.compression = std::clamp(out.compression, 0, 7);
	out.scale_param1 = std::max(1, out.scale_param1);
	out.scale_param2 = std::max(1, out.scale_param2);
	out.interpolation = std::max(0, out.interpolation);
	return out;
}

//---------------------------------------------------------------------------
FormatVisibility ResolveVisibility(const Options &opt)
{
	FormatVisibility state;
	state.quality = opt.format == Format::Jpg || opt.format == Format::Hdp;
	state.ycrcb = opt.format == Format::Jpg;
	state.compression = opt.format == Format::Tif;
	return state;
}

//---------------------------------------------------------------------------
ScaleState ResolveScaleState(ScaleMode mode)
{
	ScaleState state;
	state.option = valid_scale_mode(mode) && mode != ScaleMode::None;
	switch (mode) {
	case ScaleMode::Percent:
		state.param1 = true;
		state.label1 = _T("倍率％");
		break;
	case ScaleMode::LongSide:
		state.param1 = true;
		state.label1 = _T("サイズ");
		break;
	case ScaleMode::Width:
		state.param1 = true;
		state.label1 = _T("横サイズ");
		break;
	case ScaleMode::Height:
		state.param1 = true;
		state.label1 = _T("縦サイズ");
		break;
	case ScaleMode::Fit:
	case ScaleMode::Stretch:
	case ScaleMode::FitPad:
	case ScaleMode::Crop:
		state.param1 = true;
		state.param2 = true;
		state.label1 = _T("横サイズ");
		state.label2 = _T("縦サイズ");
		break;
	case ScaleMode::None:
	default:
		break;
	}
	return state;
}

//---------------------------------------------------------------------------
bool HasUnsupportedRuntimeOptions(const Options &input)
{
	const Options opt = Normalize(input);
	return opt.format == Format::Hdp
	       || opt.grayscale
	       || opt.scale_mode != ScaleMode::None
	       || opt.compression != 0
	       || (opt.format == Format::Jpg && opt.ycrcb != 1)
	       || opt.keep_time
	       || opt.not_use_preview
	       || opt.interpolation != 0
	       || opt.margin_color != 0
	       || !opt.name_text.IsEmpty()
	       || opt.from_clipboard;
}

}  // namespace cv_img
