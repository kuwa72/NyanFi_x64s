/**
 * @file gui/rename.cpp
 * @brief gui/rename.h の実装
 */
#include "gui/rename.h"

#include <map>

#include "compat/regex.h"
#include "usr_file_ex.h"

namespace rename_core {

namespace {

/// Windows のファイル名として使用できない文字 (実測ではなく Win32 の一般的な
/// 制約から決めた最低限のチェック。DOS 予約名 (CON/NUL/...) や末尾の
/// ピリオド/空白は検証していない (未検証・要件外))
bool HasInvalidNameChar(const UnicodeString &name)
{
	for (int i = 1; i <= name.Length(); ++i) {
		wchar_t ch = name[i];
		if (ch < 0x20) return true;
		switch (ch) {
		case L'\\': case L'/': case L':': case L'*': case L'?':
		case L'"':  case L'<': case L'>': case L'|':
			return true;
		default:
			break;
		}
	}
	return false;
}

/// 新しい名前の基本的な妥当性 (空・使用不可文字・"."/".." ではないか)
bool IsValidName(const UnicodeString &name)
{
	if (name.IsEmpty()) return false;
	if (SameStr(name, _T(".")) || SameStr(name, _T(".."))) return false;
	if (HasInvalidNameChar(name)) return false;
	return true;
}

/// 対象がディレクトリかどうかを踏まえてベース名/拡張子を分ける
/// (`src/RenDlg.cpp` の `TRenameDlg::UpdateNewNameList()` L.686-691 のまま:
/// ディレクトリは拡張子を持たず、名前全体がベース名になる)
void SplitBaseExt(const RenameTarget &target, UnicodeString &base, UnicodeString &ext)
{
	if (target.is_dir) {
		base = target.name;
		ext = EmptyStr;
	}
	else {
		base = get_base_name(target.name);
		ext = get_extension(target.name);
	}
}

/// change_ext 指定時の拡張子を正規化する (先頭の "." の有無どちらでも受け付ける)
UnicodeString NormalizeExt(const UnicodeString &ext)
{
	if (ext.IsEmpty()) return EmptyStr;
	return StartsStr('.', ext) ? ext : (_T(".") + ext);
}

/// rows を targets から作る (この時点では old_name/new_name/is_dir だけを埋める)
RenamePlan MakePlan(const std::vector<RenameTarget> &targets)
{
	RenamePlan plan;
	plan.rows.reserve(targets.size());
	for (const RenameTarget &t : targets) {
		PreviewRow row;
		row.old_name = t.name;
		row.new_name = t.name;
		row.is_dir = t.is_dir;
		plan.rows.push_back(row);
	}
	return plan;
}

}  // namespace

//---------------------------------------------------------------------------
RenamePlan BuildRegexPlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                          const RegexOptions &opt)
{
	// 検索文字列の組み立て (`UpdateNewNameList` L.653-658 のまま)
	UnicodeString sea_wd = opt.use_regex ? opt.pattern : TRegEx::Escape(opt.pattern);
	TRegExOptions re_opt;
	if (!opt.case_sensitive) re_opt << roIgnoreCase;

	// 事前にパターンの妥当性を検証する (grep_dialog.cpp と同じ考え方。VCL は
	// 不正なパターンでも項目ごとに catch して以降を「変更なし」にするが、
	// ここでは先に検証してダイアログ側でやり直させられるようにする)
	RenamePlan plan;
	try {
		TRegEx probe(sea_wd, re_opt);
	}
	catch (...) {
		plan.pattern_error = true;
		plan.error = _T("正規表現が不正です");
		return plan;
	}

	plan = MakePlan(targets);
	for (std::size_t i = 0; i < targets.size(); ++i) {
		const RenameTarget &t = targets[i];
		UnicodeString base, ext;
		SplitBaseExt(t, base, ext);
		const UnicodeString old_name = base + ext;

		UnicodeString new_name = old_name;
		try {
			if (opt.only_base) {
				if (TRegEx::IsMatch(base, sea_wd, re_opt)) {
					new_name = replace_regex_2(base, sea_wd, opt.replacement, re_opt) + ext;
				}
			}
			else {
				if (TRegEx::IsMatch(old_name, sea_wd, re_opt)) {
					new_name = replace_regex_2(old_name, sea_wd, opt.replacement, re_opt);
				}
			}
		}
		catch (...) {
			// 妥当性は事前検証済みなのでここには通常来ないが、念のため
			// 元の名前のまま (変更なし) にする
			new_name = old_name;
		}

		plan.rows[i].new_name = new_name;
	}

	ResolveConflicts(plan, dir, targets);
	return plan;
}

//---------------------------------------------------------------------------
RenamePlan BuildSerialPlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                           const SerialOptions &opt)
{
	RenamePlan plan = MakePlan(targets);

	const UnicodeString new_ext = NormalizeExt(opt.new_ext);
	int sn = opt.start;

	for (std::size_t i = 0; i < targets.size(); ++i) {
		const RenameTarget &t = targets[i];
		UnicodeString base, ext;
		SplitBaseExt(t, base, ext);

		UnicodeString new_name = opt.prefix;
		if (opt.width > 0) new_name.cat_sprintf(_T("%0*u"), opt.width, sn);
		new_name += opt.suffix;

		if (!t.is_dir) {
			if (opt.change_ext) {
				new_name += new_ext;  // new_ext が空なら拡張子を削除
			}
			else {
				new_name += ext;
			}
		}

		plan.rows[i].new_name = new_name;
		sn += opt.step;
	}

	ResolveConflicts(plan, dir, targets);
	return plan;
}

//---------------------------------------------------------------------------
RenamePlan BuildCasePlan(const UnicodeString &dir, const std::vector<RenameTarget> &targets,
                         const CaseOptions &opt)
{
	RenamePlan plan = MakePlan(targets);

	for (std::size_t i = 0; i < targets.size(); ++i) {
		const RenameTarget &t = targets[i];
		UnicodeString base, ext;
		SplitBaseExt(t, base, ext);
		const UnicodeString old_name = base + ext;

		UnicodeString new_name;
		if (opt.only_base && !t.is_dir) {
			UnicodeString conv_base = (opt.mode == CaseMode::Upper) ? base.UpperCase() : base.LowerCase();
			new_name = conv_base + ext;
		}
		else {
			new_name = (opt.mode == CaseMode::Upper) ? old_name.UpperCase() : old_name.LowerCase();
		}

		plan.rows[i].new_name = new_name;
	}

	ResolveConflicts(plan, dir, targets);
	return plan;
}

//---------------------------------------------------------------------------
void ResolveConflicts(RenamePlan &plan, const UnicodeString &dir,
                     const std::vector<RenameTarget> &targets)
{
	const UnicodeString base_dir = IncludeTrailingPathDelimiter(dir);

	// 1. 変更なし / 不正な名前を先に確定する
	for (PreviewRow &row : plan.rows) {
		if (SameStr(row.new_name, row.old_name)) {
			row.status = RowStatus::Unchanged;
		}
		else if (!IsValidName(row.new_name)) {
			row.status = RowStatus::Invalid;
		}
		else {
			row.status = RowStatus::Ok;  // 仮。この後の判定で Conflict に変わりうる
		}
	}

	// 2. 対象同士で同じ新しい名前を狙っている行を相互衝突とする
	//    (Windows のファイル名は大小文字を区別しないので比較も区別しない)
	{
		// キーはあらかじめ大文字化して大小文字を無視した比較にする
		std::map<UnicodeString, std::vector<std::size_t>> targets_by_new_name;
		for (std::size_t i = 0; i < plan.rows.size(); ++i) {
			if (plan.rows[i].status != RowStatus::Ok) continue;
			targets_by_new_name[plan.rows[i].new_name.UpperCase()].push_back(i);
		}
		for (auto &kv : targets_by_new_name) {
			if (kv.second.size() <= 1) continue;
			for (std::size_t idx : kv.second) plan.rows[idx].status = RowStatus::Conflict;
		}
	}

	// 3. 残りの行 (単独で新しい名前を狙っている行) について、他の対象または
	//    既存のファイル/ディレクトリと衝突しないか確認する。
	//    ある行が (別の理由で) Conflict に変わると、その行が明け渡すはずだった
	//    名前を狙っていた別の行も連鎖的に Conflict になりうる (3段以上の連鎖)
	//    ため、状態が変化しなくなるまで繰り返す (不動点に達するまでの反復)。
	bool changed = true;
	while (changed) {
		changed = false;
		for (std::size_t i = 0; i < plan.rows.size(); ++i) {
			PreviewRow &row = plan.rows[i];
			if (row.status != RowStatus::Ok) continue;

			bool owned_by_batch = false;
			bool blocked = false;
			for (std::size_t j = 0; j < plan.rows.size(); ++j) {
				if (j == i) continue;
				if (!SameText(plan.rows[j].old_name, row.new_name)) continue;

				owned_by_batch = true;
				// j がこのバッチで実際に別の名前へ変わる (=元の名前を明け渡す) なら
				// 衝突ではない (a→b, b→c のような連鎖。実行は一時名経由で解決する)
				const bool j_vacates = (plan.rows[j].status == RowStatus::Ok);
				if (!j_vacates) blocked = true;
				break;  // 同じディレクトリ内で名前は一意なので一致は高々1件
			}

			// 大小文字だけの変更 (SameText は真だが SameStr は偽) は、既存の
			// ディスク上の実体が自分自身なので、既存ファイルとの衝突とは扱わない
			const bool self_case_only = SameText(row.old_name, row.new_name);

			if (!owned_by_batch && !self_case_only) {
				const UnicodeString candidate = base_dir + row.new_name;
				if (file_exists(candidate) || dir_exists(candidate)) blocked = true;
			}

			if (blocked) {
				row.status = RowStatus::Conflict;
				changed = true;
			}
		}
	}

	(void)targets;  // 現在の実装では index 対応のみ使用 (is_dir 等は rows 側に既に反映済み)
}

//---------------------------------------------------------------------------
namespace {

/// 2段階リネームで使う一時名 (`src/RenDlg.cpp` の `$~NFnnnn.~TMP` に相当)
UnicodeString MakeTempName(int index)
{
	UnicodeString name;
	name.sprintf(_T("~nfren_%04d.tmp"), index);
	return name;
}

}  // namespace

RenameExecResult ExecutePlan(const UnicodeString &dir, const RenamePlan &plan)
{
	RenameExecResult result;
	const UnicodeString base_dir = IncludeTrailingPathDelimiter(dir);

	std::vector<std::size_t> ok_indices;
	for (std::size_t i = 0; i < plan.rows.size(); ++i) {
		if (plan.rows[i].status == RowStatus::Ok) ok_indices.push_back(i);
		else result.skipped_count++;
	}
	if (ok_indices.empty()) return result;

	if (ok_indices.size() == 1) {
		const PreviewRow &row = plan.rows[ok_indices[0]];
		if (rename_File(base_dir + row.old_name, base_dir + row.new_name)) {
			result.success_count++;
			result.applied.push_back(AppliedRename{row.old_name, row.new_name});
		}
		else {
			result.failures.push_back(row.old_name + _T(" -> ") + row.new_name + _T(": 名前の変更に失敗しました"));
		}
		return result;
	}

	// 2件以上: 常に「一時名へ→最終名へ」の2段階で行い、リネーム同士の衝突
	// (連鎖・入れ替え) を避ける
	std::vector<UnicodeString> temp_names(ok_indices.size());
	for (std::size_t k = 0; k < ok_indices.size(); ++k) {
		temp_names[k] = MakeTempName(static_cast<int>(k));
		const UnicodeString probe = base_dir + temp_names[k];
		if (file_exists(probe) || dir_exists(probe)) {
			// 一時名が既に使われている (通常は起こらない)。何も変更せずに中断する
			for (std::size_t m = 0; m < ok_indices.size(); ++m) {
				const PreviewRow &row = plan.rows[ok_indices[m]];
				result.failures.push_back(row.old_name + _T(": 一時ファイル名 ") + temp_names[k] +
				                           _T(" が既に存在するため中断しました"));
			}
			return result;
		}
	}

	// phase 1: 元の名前 -> 一時名。途中で失敗したら以降は着手せず中断する
	// (すでに一時名になった分は phase 2 で最終名へ戻す)
	std::size_t phase1_done = 0;
	bool phase1_failed = false;
	for (std::size_t k = 0; k < ok_indices.size(); ++k) {
		const PreviewRow &row = plan.rows[ok_indices[k]];
		if (!rename_File(base_dir + row.old_name, base_dir + temp_names[k])) {
			result.failures.push_back(row.old_name + _T(" -> ") + row.new_name +
			                           _T(": 一時名への変更に失敗しました"));
			phase1_failed = true;
			break;
		}
		phase1_done = k + 1;
	}
	// 失敗した1件より後、未着手のまま残った項目はスキップ扱い
	// (失敗した1件自体は上で failures に計上済みなので、この件数には含めない)
	if (phase1_failed) {
		result.skipped_count += static_cast<int>(ok_indices.size() - phase1_done - 1);
	}

	// phase 2: 一時名 -> 最終名 (phase1_done 件のみ)
	for (std::size_t k = 0; k < phase1_done; ++k) {
		const PreviewRow &row = plan.rows[ok_indices[k]];
		if (rename_File(base_dir + temp_names[k], base_dir + row.new_name)) {
			result.success_count++;
			result.applied.push_back(AppliedRename{row.old_name, row.new_name});
		}
		else {
			// 最終名にできなかった。一時名のまま放置するとユーザーには
			// 「ファイルが消えた」ように見えるので、元の名前へ戻す
			if (rename_File(base_dir + temp_names[k], base_dir + row.old_name)) {
				result.failures.push_back(row.old_name + _T(" -> ") + row.new_name +
				                           _T(": 名前の変更に失敗しました (元の名前に戻しました)"));
			}
			else {
				//戻すことにも失敗した。一時名が残るので、その名前を必ず知らせる
				result.failures.push_back(row.old_name + _T(" -> ") + row.new_name +
				                           _T(": 名前の変更に失敗し、元の名前にも戻せませんでした (現在の名前: ") +
				                           temp_names[k] + _T(")"));
			}
		}
	}

	return result;
}

}  // namespace rename_core
