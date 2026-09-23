/**
 * @file gui/grep_opt.cpp
 * @brief gui/grep_opt.h の実装
 */
#include "gui/grep_opt.h"

namespace grep_opt {

namespace {

bool valid_output_mode(OutputMode mode)
{
	return mode == OutputMode::None || mode == OutputMode::File || mode == OutputMode::Clipboard;
}

bool valid_edit_mode(EditMode mode)
{
	return mode == EditMode::Search || mode == EditMode::Replace;
}

UnicodeString trim_left_each_line(const UnicodeString &text)
{
	UnicodeString out;
	int start = 1;
	for (int i = 1; i <= text.Length(); ++i) {
		if (text[i] == L'\n' || i == text.Length()) {
			const int count = (i == text.Length()) ? i - start : i - start;
			out += text.SubString(start, count).TrimLeft();
			if (i < text.Length()) out += _T("\n");
			start = i + 1;
		}
	}
	return out;
}

}  // namespace

//---------------------------------------------------------------------------
Options Normalize(const Options &in)
{
	Options out = in;
	if (!valid_output_mode(out.output_mode)) out.output_mode = OutputMode::None;
	if (!valid_edit_mode(out.edit_mode)) out.edit_mode = EditMode::Search;
	if (out.file_format.IsEmpty()) out.file_format = _T("$F $L:");
	return out;
}

//---------------------------------------------------------------------------
EnabledState ResolveEnabled(const Options &opt)
{
	EnabledState state;
	state.output_file = opt.output_mode == OutputMode::File;
	state.app = opt.app_enabled;
	state.app_name = opt.app_enabled;
	state.app_dir = opt.app_enabled;
	state.backup = opt.backup_replace;
	state.log = opt.save_log;
	state.insert_words = opt.edit_mode == EditMode::Search;
	return state;
}

//---------------------------------------------------------------------------
int OutputModeIndex(OutputMode mode)
{
	switch (mode) {
	case OutputMode::File: return 1;
	case OutputMode::Clipboard: return 2;
	case OutputMode::None:
	default: return 0;
	}
}

//---------------------------------------------------------------------------
OutputMode OutputModeFromIndex(int index)
{
	switch (index) {
	case 1: return OutputMode::File;
	case 2: return OutputMode::Clipboard;
	default: return OutputMode::None;
	}
}

//---------------------------------------------------------------------------
int EditModeIndex(EditMode mode)
{
	return mode == EditMode::Replace ? 1 : 0;
}

//---------------------------------------------------------------------------
EditMode EditModeFromIndex(int index)
{
	return index == 1 ? EditMode::Replace : EditMode::Search;
}

//---------------------------------------------------------------------------
UnicodeString BuildSample(const Options &input)
{
	const Options opt = Normalize(input);
	UnicodeString file_format = opt.file_format;
	if (file_format.IsEmpty()) file_format = _T("$F $L:");
	file_format = ReplaceStr(file_format, _T("$F"), _T("D:\\hoge.txt"));
	file_format = ReplaceStr(file_format, _T("$L"), _T("123"));
	file_format = conv_esc_char(file_format);

	UnicodeString line;
	if (opt.edit_mode == EditMode::Search) {
		line = _T("これは検索の");
		line += conv_esc_char(opt.insert_before);
		line += _T("マッチ語");
		line += conv_esc_char(opt.insert_after);
		line += _T("です。");
	}
	else {
		line = _T("これは置換結果のサンプルです。");
	}
	line += _T("\nこれは2行目です。行頭にタブはありません。\n\tこれは3行目です。行頭にタブがあります。");

	if (opt.replace_tab) line = ReplaceStr(line, _T("\t"), _T(" "));
	if (opt.trim_left) line = trim_left_each_line(line);
	if (opt.replace_cr) line = ReplaceStr(line, _T("\n"), conv_esc_char(opt.replacement));
	else line = ReplaceStr(line, _T("\n"), _T("\r\n"));

	return file_format + line;
}

}  // namespace grep_opt
