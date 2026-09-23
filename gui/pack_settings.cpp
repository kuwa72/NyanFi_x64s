/**
 * @file gui/pack_settings.cpp
 * @brief gui/pack_settings.h の実装
 */
#include "gui/pack_settings.h"

#include <algorithm>

#include "usr_str.h"

namespace pack_settings {

namespace {

const int kZipLevels[] = {0, 1, 3, 5, 7, 9};
const int kCabLevels[] = {0, 15, 16, 17, 18, 19, 20, 21, 22};

bool ends_with_text(const UnicodeString &value, const UnicodeString &suffix)
{
	return value.Length() >= suffix.Length() &&
	       SameText(value.SubString(value.Length() - suffix.Length() + 1), suffix);
}

UnicodeString strip_format_extension(const UnicodeString &name, Format format)
{
	const UnicodeString ext = Extension(format);
	if (ends_with_text(name, ext)) {
		return name.SubString(1, name.Length() - ext.Length());
	}
	// TAR は VCL の FEXT_TAR に .tar.gz/.tgz 等を含む。表示拡張子だけ返す。
	if (format == Format::Tar) {
		const wchar_t *const suffixes[] = {_T(".tar.gz"), _T(".tgz"), _T(".taz"),
		                                    _T(".bz2"), _T(".xz"), _T(".lzma")};
		for (const wchar_t *suffix : suffixes) {
			const UnicodeString s(suffix);
			if (ends_with_text(name, s)) return name.SubString(1, name.Length() - s.Length());
		}
	}
	return name;
}

int default_compression(Format format)
{
	switch (format) {
	case Format::Tar: return 6;
	case Format::Cab:
	case Format::Lha: return 0;
	case Format::Zip:
	case Format::SevenZip:
	default: return 5;
	}
}

int clamp_compression(Format format, int value)
{
	switch (format) {
	case Format::Zip:
	case Format::SevenZip:
		for (int level : kZipLevels) if (value == level) return value;
		return default_compression(format);
	case Format::Cab:
		for (int level : kCabLevels) if (value == level) return value;
		return 0;
	case Format::Tar:
		return std::clamp(value, 0, 9);
	case Format::Lha:
	default:
		return 0;
	}
}

UnicodeString join_reason(const std::vector<UnicodeString> &parts)
{
	UnicodeString result;
	for (std::size_t i = 0; i < parts.size(); ++i) {
		if (i != 0) result += _T("、");
		result += parts[i];
	}
	return result;
}

}  // namespace

int ToIndex(Format format)
{
	return static_cast<int>(format);
}

Format FromIndex(int index)
{
	if (index < 0 || index > static_cast<int>(Format::Tar)) return Format::Zip;
	return static_cast<Format>(index);
}

UnicodeString Extension(Format format)
{
	switch (format) {
	case Format::SevenZip: return _T(".7z");
	case Format::Lha: return _T(".lzh");
	case Format::Cab: return _T(".cab");
	case Format::Tar: return _T(".tar");
	case Format::Zip:
	default: return _T(".zip");
	}
}

bool TryFormatFromName(const UnicodeString &name, Format &format_out)
{
	const UnicodeString lower = name.LowerCase();
	if (ends_with_text(lower, _T(".zip")) || ends_with_text(lower, _T(".apk")) ||
	    ends_with_text(lower, _T(".jar"))) {
		format_out = Format::Zip;
		return true;
	}
	if (ends_with_text(lower, _T(".7z")) || ends_with_text(lower, _T(".cb7"))) {
		format_out = Format::SevenZip;
		return true;
	}
	if (ends_with_text(lower, _T(".lzh"))) {
		format_out = Format::Lha;
		return true;
	}
	if (ends_with_text(lower, _T(".cab"))) {
		format_out = Format::Cab;
		return true;
	}
	if (ends_with_text(lower, _T(".tar")) || ends_with_text(lower, _T(".tar.gz")) ||
	    ends_with_text(lower, _T(".tgz")) || ends_with_text(lower, _T(".taz")) ||
	    ends_with_text(lower, _T(".bz2")) || ends_with_text(lower, _T(".xz")) ||
	    ends_with_text(lower, _T(".lzma"))) {
		format_out = Format::Tar;
		return true;
	}
	return false;
}

bool IsAvailable(const Availability &availability, Format format)
{
	switch (format) {
	case Format::Zip: return availability.zip;
	case Format::SevenZip: return availability.seven_zip;
	case Format::Lha: return availability.lha;
	case Format::Cab: return availability.cab;
	case Format::Tar: return availability.tar;
	}
	return false;
}

int CompressionFromUiIndex(Format format, int index)
{
	index = std::max(0, index);
	switch (format) {
	case Format::Zip:
	case Format::SevenZip:
		return kZipLevels[std::min(index, 5)];
	case Format::Cab:
		return kCabLevels[std::min(index, 8)];
	case Format::Tar:
		return std::min(index, 9);
	case Format::Lha:
	default:
		return 0;
	}
}

int UiIndexFromCompression(Format format, int compression)
{
	switch (format) {
	case Format::Zip:
	case Format::SevenZip:
		for (int i = 0; i < 6; ++i) if (kZipLevels[i] == compression) return i;
		return 3;
	case Format::Cab:
		for (int i = 0; i < 9; ++i) if (kCabLevels[i] == compression) return i;
		return 0;
	case Format::Tar:
		return std::clamp(compression, 0, 9);
	case Format::Lha:
	default:
		return 0;
	}
}

Options Normalize(const Options &options, const Availability &availability)
{
	Options out = options;
	out.name = out.name.Trim();
	out.extra_switches = out.extra_switches.Trim();
	out.password = out.password.Trim();

	if (!IsAvailable(availability, out.format)) {
		const Format candidates[] = {Format::Zip, Format::SevenZip, Format::Lha,
		                            Format::Cab, Format::Tar};
		for (Format candidate : candidates) {
			if (IsAvailable(availability, candidate)) {
				out.format = candidate;
				break;
			}
		}
	}
	out.compression = clamp_compression(out.format, out.compression);
	if (out.format != Format::SevenZip) out.self_extract = false;
	if (out.existing_mode != ExistingMode::Append &&
	    out.existing_mode != ExistingMode::Recreate) {
		out.existing_mode = ExistingMode::Append;
	}
	return out;
}

Resolved Resolve(const Options &options, const Availability &availability,
                 const UnicodeString &dst_dir)
{
	Resolved result;
	result.options = Normalize(options, availability);
	result.extension = Extension(result.options.format);
	result.available = IsAvailable(availability, result.options.format);

	const UnicodeString base = strip_format_extension(result.options.name, result.options.format);
	result.file_name = base.IsEmpty() ? EmptyStr : base + result.extension;
	if (!dst_dir.IsEmpty() && !result.file_name.IsEmpty()) {
		result.archive_path = IncludeTrailingPathDelimiter(dst_dir) + result.file_name;
	}

	std::vector<UnicodeString> reasons;
	if (!result.available) reasons.push_back(_T("書庫DLLが利用できない形式です"));
	if (result.file_name.IsEmpty()) reasons.push_back(_T("書庫名が空です"));
	if (result.options.per_directory) reasons.push_back(_T("ディレクトリごとの作成は未移植"));
	if (result.options.include_top_directory) reasons.push_back(_T("トップディレクトリを含める処理は未移植"));
	if (!result.options.password.IsEmpty()) reasons.push_back(_T("パスワードは未移植"));
	if (!result.options.extra_switches.IsEmpty()) reasons.push_back(_T("追加スイッチは未移植"));
	if (result.options.self_extract) reasons.push_back(_T("自己解凍形式は未移植"));
	if (result.options.compression != default_compression(result.options.format)) {
		reasons.push_back(_T("圧縮レベルの実処理は未移植"));
	}
	result.unsupported_reason = join_reason(reasons);
	result.core_compatible = result.available && !result.file_name.IsEmpty() &&
	                         result.unsupported_reason.IsEmpty();
	return result;
}

bool IsCoreCompatible(const Options &options, const Availability &availability)
{
	return Resolve(options, availability).core_compatible;
}

}  // namespace pack_settings
