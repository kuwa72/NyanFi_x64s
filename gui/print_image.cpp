/**
 * @file gui/print_image.cpp
 * @brief gui/print_image.h の実装
 */
#include "gui/print_image.h"

#include <algorithm>

namespace print_image {

namespace {

int Clamp(int value, int minimum, int maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

const wchar_t *FitLabel(ImageFit fit)
{
	switch (fit) {
	case ImageFit::FitPage:  return _T("用紙に合わせる");
	case ImageFit::FillPage: return _T("切り抜き");
	case ImageFit::Center:   return _T("中央");
	case ImageFit::TopLeft:  return _T("左上");
	}
	return _T("用紙に合わせる");
}

}  // namespace

//---------------------------------------------------------------------------
ResolvedSettings ResolvePrintSettings(const PrintOptions &options, int page_count,
                                      int current_page,
                                      const std::vector<int> &selected_pages)
{
	ResolvedSettings result;
	if (page_count <= 0) {
		result.error = _T("印刷するページがありません");
		return result;
	}

	result.orientation = options.orientation;
	result.copies = Clamp(options.copies, 1, 32767);
	result.scale_percent = Clamp(options.scale_percent, 1, 100);
	result.fit = options.fit;
	result.range = options.print_range;
	result.offset_x_percent = Clamp(options.offset_x_percent, 0, 99);
	result.offset_y_percent = Clamp(options.offset_y_percent, 0, 99);
	result.grayscale = options.grayscale;
	result.print_text = options.print_text;
	result.text_format = options.text_format;
	result.text_position = options.text_position;
	result.text_alignment = options.text_alignment;
	result.text_margin_percent = Clamp(options.text_margin_percent, 0, 99);
	result.effective_scale_percent =
		(options.fit == ImageFit::FitPage || options.fit == ImageFit::FillPage)
			? 100
			: result.scale_percent;

	switch (options.print_range) {
	case PrintRange::Current:
		result.first_page = Clamp(current_page, 1, page_count);
		result.last_page = result.first_page;
		break;
	case PrintRange::All:
		result.first_page = 1;
		result.last_page = page_count;
		break;
	case PrintRange::Selection: {
		std::vector<int> pages;
		for (int page : selected_pages) {
			if (page >= 1 && page <= page_count) pages.push_back(page);
		}
		std::sort(pages.begin(), pages.end());
		pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
		if (pages.empty()) {
			result.error = _T("選択範囲に有効なページがありません");
			return result;
		}
		result.first_page = pages.front();
		result.last_page = pages.back();
		break;
	}
	}

	result.valid = true;
	return result;
}

//---------------------------------------------------------------------------
UnicodeString FormatPrintSettings(const ResolvedSettings &settings)
{
	if (!settings.valid) return settings.error;
	UnicodeString text;
	text.sprintf(_T("%s / %d部 / %d-%d / %s / %d%%"),
	             settings.orientation == Orientation::Landscape ? _T("横") : _T("縦"),
	             settings.copies, settings.first_page, settings.last_page,
	             FitLabel(settings.fit), settings.effective_scale_percent);
	if (settings.grayscale) text += _T(" / グレー");
	if (settings.print_text) text += _T(" / 文字");
	return text;
}

}  // namespace print_image
