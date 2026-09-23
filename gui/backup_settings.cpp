/**
 * @file gui/backup_settings.cpp
 * @brief gui/backup_settings.h の実装
 */
#include "gui/backup_settings.h"

#include <algorithm>
#include <cctype>

#include "gui/f_misc_ops.h"
#include "usr_str.h"

namespace backup_settings {

namespace {

bool is_digit(wchar_t ch)
{
	return ch >= _T('0') && ch <= _T('9');
}

bool parse_unsigned(const UnicodeString &text, int &value_out)
{
	if (text.IsEmpty()) return false;
	int value = 0;
	for (int i = 1; i <= text.Length(); ++i) {
		const wchar_t ch = text[i];
		if (!is_digit(ch)) return false;
		value = value * 10 + (ch - _T('0'));
		if (value > 1000000) return false;
	}
	value_out = value;
	return true;
}

bool parse_absolute_date(const UnicodeString &text, UnicodeString &error_out)
{
	// VCL の正規表現 ^\d{4}/\d{2}/\d{2}$ と同じ形.accept
	if (text.Length() != 10 || text[5] != _T('/') || text[8] != _T('/')) {
		error_out = _T("日付は yyyy/mm/dd 形式で指定してください");
		return false;
	}
	int year = 0, month = 0, day = 0;
	if (!parse_unsigned(text.SubString(1, 4), year) ||
	    !parse_unsigned(text.SubString(6, 2), month) ||
	    !parse_unsigned(text.SubString(9, 2), day) || year < 1 || month < 1 || month > 12 ||
	    day < 1 || day > 31) {
		error_out = _T("日付が不正です");
		return false;
	}
	return true;
}

UnicodeString csv_quote(const UnicodeString &value)
{
	if (!ContainsStr(value, _T(",")) && !ContainsStr(value, _T("\"")) &&
	    !ContainsStr(value, _T("\r")) && !ContainsStr(value, _T("\n"))) {
		return value;
	}
	UnicodeString out = _T("\"");
	for (int i = 1; i <= value.Length(); ++i) {
		if (value[i] == _T('"')) out += _T("\"");
		out += value[i];
	}
	out += _T("\"");
	return out;
}

UnicodeString one_char(wchar_t ch)
{
	UnicodeString out;
	out += ch;
	return out;
}

int find_setup_separator(const UnicodeString &record)
{
	bool quoted = false;
	for (int i = 1; i <= record.Length(); ++i) {
		const wchar_t ch = record[i];
		if (ch == _T('"')) {
			if (quoted && i < record.Length() && record[i + 1] == _T('"')) {
				++i;
				continue;
			}
			quoted = !quoted;
		}
		else if (ch == _T('=') && !quoted) return i;
	}
	return 0;
}

std::vector<UnicodeString> csv_split(const UnicodeString &text)
{
	std::vector<UnicodeString> fields;
	UnicodeString field;
	bool quoted = false;
	for (int i = 1; i <= text.Length(); ++i) {
		const wchar_t ch = text[i];
		if (quoted) {
			if (ch == _T('"')) {
				if (i < text.Length() && text[i + 1] == _T('"')) {
					field += _T('"');
					++i;
				}
				else {
					quoted = false;
				}
			}
			else {
				field += ch;
			}
		}
		else if (ch == _T('"')) {
			quoted = true;
		}
		else if (ch == _T(',')) {
			fields.push_back(field);
			field = EmptyStr;
		}
		else {
			field += ch;
		}
	}
	fields.push_back(field);
	return fields;
}

}  // namespace

bool ParseDateCondition(const UnicodeString &text, DateKind &kind_out,
                        UnicodeString &normalized_out, UnicodeString &error_out)
{
	kind_out = DateKind::None;
	normalized_out = EmptyStr;
	error_out = EmptyStr;
	const UnicodeString value = text.Trim();
	if (value.IsEmpty()) return true;

	if (SameText(value, _T("TD")) || SameText(value, _T("CP"))) {
		kind_out = DateKind::OnOrEqual;
		normalized_out = _T("=") + value.UpperCase();
		return true;
	}

	wchar_t op = value[1];
	wchar_t second = 0;
	int pos = 1;
	if (value.Length() >= 2 && (value[2] == _T('=') || value[2] == _T('>'))) {
		second = value[2];
		pos = 2;
	}
	if (op != _T('<') && op != _T('=') && op != _T('>')) {
		error_out = _T("先頭に <、=、> の比較演算子が必要です");
		return false;
	}

	const UnicodeString operand = value.SubString(pos + 1);
	if (operand.IsEmpty()) {
		error_out = _T("日付条件の右辺がありません");
		return false;
	}

	if (SameText(operand, _T("TD")) || SameText(operand, _T("CP"))) {
		kind_out = DateKind::OnOrEqual;
		normalized_out = one_char(op) + operand.UpperCase();
		return true;
	}

	const bool relative = !operand.IsEmpty() &&
	                      (operand[operand.Length()] == _T('D') ||
	                       operand[operand.Length()] == _T('M') ||
	                       operand[operand.Length()] == _T('Y'));
	if (relative) {
		const int number_len = operand.Length() - 1;
		int number = 0;
		UnicodeString number_text = operand.SubString(1, number_len);
		if (!number_text.IsEmpty() && number_text[1] == _T('-')) {
			number_text = number_text.SubString(2, number_len - 1);
			if (!parse_unsigned(number_text, number)) {
				error_out = _T("相対日数の数値が不正です");
				return false;
			}
			number = -number;
		}
		else if (!parse_unsigned(number_text, number)) {
			error_out = _T("相対日数の数値が不正です");
			return false;
		}
		if (op == _T('<')) kind_out = second == _T('=') ? DateKind::BeforeOrEqual : DateKind::Before;
		else if (op == _T('>')) kind_out = second == _T('=') ? DateKind::AfterOrEqual : DateKind::After;
		else kind_out = DateKind::OnOrEqual;
		normalized_out = one_char(op) + operand.UpperCase();
		return true;
	}

	if (operand.Length() == 10 && is_digit(operand[1])) {
		if (!parse_absolute_date(operand, error_out)) return false;
		if (op == _T('<')) kind_out = second == _T('=') ? DateKind::BeforeOrEqual : DateKind::Before;
		else if (op == _T('>')) kind_out = second == _T('=') ? DateKind::AfterOrEqual : DateKind::After;
		else kind_out = DateKind::OnOrEqual;
		normalized_out = one_char(op) + operand;
		return true;
	}

	error_out = _T("日付条件の書式が不正です");
	return false;
}

UnicodeString FormatSetupRecord(const Setup &setup)
{
	UnicodeString out = csv_quote(setup.name) + _T("=");
	const UnicodeString fields[] = {
		setup.options.include_mask,
		setup.options.exclude_mask,
		setup.options.skip_dirs,
		setup.options.sub_dirs ? _T("1") : _T("0"),
		setup.options.mirror ? _T("1") : _T("0"),
		setup.options.sync ? _T("1") : _T("0"),
		setup.options.date_condition,
	};
	for (std::size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
		if (i != 0) out += _T(",");
		out += csv_quote(fields[i]);
	}
	return out;
}

bool ParseSetupRecord(const UnicodeString &record, Setup &setup_out)
{
	const int eq = find_setup_separator(record);
	if (eq <= 0) return false;
	const std::vector<UnicodeString> name_fields = csv_split(record.SubString(1, eq - 1));
	const std::vector<UnicodeString> fields = csv_split(record.SubString(eq + 1));
	if (name_fields.empty() || name_fields[0].Trim().IsEmpty() || fields.size() < 7) return false;

	Setup out;
	out.name = name_fields[0];
	out.options.include_mask = fields[0];
	out.options.exclude_mask = fields[1];
	out.options.skip_dirs = fields[2];
	out.options.sub_dirs = SameText(fields[3], _T("1")) || SameText(fields[3], _T("true"));
	out.options.mirror = SameText(fields[4], _T("1")) || SameText(fields[4], _T("true"));
	out.options.sync = SameText(fields[5], _T("1")) || SameText(fields[5], _T("true"));
	out.options.date_condition = fields[6];
	// VCL の設定行には SureCheckBox を含めない。確認設定は現在の ini 値として
	// 別に保存する (BakDlg.cpp:123-131 の7項目)。
	out.options.confirm = true;
	setup_out = out;
	return true;
}

int FindSetupIndex(const std::vector<Setup> &setups, const UnicodeString &name)
{
	for (std::size_t i = 0; i < setups.size(); ++i) {
		if (SameText(setups[i].name, name)) return static_cast<int>(i);
	}
	return -1;
}

void UpsertSetup(std::vector<Setup> &setups, const UnicodeString &name, const Options &options)
{
	const int index = FindSetupIndex(setups, name);
	Setup setup;
	setup.name = name;
	setup.options = options;
	if (index >= 0) setups[static_cast<std::size_t>(index)] = setup;
	else setups.insert(setups.begin(), setup);
}

bool DeleteSetup(std::vector<Setup> &setups, const UnicodeString &name)
{
	const int index = FindSetupIndex(setups, name);
	if (index < 0) return false;
	setups.erase(setups.begin() + index);
	return true;
}

bool ValidateOptions(const Options &options, const UnicodeString &source_dir,
                     const UnicodeString &dest_dir, UnicodeString &error_out)
{
	if (!f_misc_ops::ValidateBackupPaths(source_dir, dest_dir, error_out)) return false;
	DateKind kind = DateKind::None;
	UnicodeString normalized;
	if (!ParseDateCondition(options.date_condition, kind, normalized, error_out)) return false;
	return true;
}

std::vector<UnicodeString> ResolveDestinations(
    const UnicodeString &dest_dir, bool sync,
    const std::vector<sync_dirs::SyncEntry> &sync_settings)
{
	if (!sync) return {dest_dir};
	return sync_dirs::ResolveTargets(dest_dir, sync_settings, false).targets;
}

UnicodeString MakeCommandText(const UnicodeString &source_dir, const UnicodeString &dest_dir,
                              const Setup &setup)
{
	UnicodeString text;
	text += _T(";バックアップ\r\n");
	text += _T("PushDir\r\n");
	text += _T("PushDir_OP\r\n");
	text += _T("ChangeDir_\"") + source_dir + _T("\"\r\n");
	text += _T("ChangeOppDir_\"") + dest_dir + _T("\"\r\n");
	text += _T("BackUp_\"") + setup.name + _T("\"\r\n");
	text += _T("PopDir_OP\r\n");
	text += _T("PopDir\r\n");
	return text;
}

}  // namespace backup_settings
