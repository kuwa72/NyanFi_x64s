/**
 * @file gui/sync_dirs.cpp
 * @brief gui/sync_dirs.h の実装 (wx 非依存)
 */
#include "gui/sync_dirs.h"

#include "UIniFile.h"

namespace sync_dirs {

namespace {

// このクラス専用のセクション名 (gui/regdir.cpp の kSection と同じ考え方)
const UnicodeString kSection = _T("WxGuiSyncDirs");

}  // namespace

//---------------------------------------------------------------------------
SyncEntry ParseRecord(const UnicodeString &record)
{
	// force_size=true で足りない分は空で埋められる (VCL と同じ)
	TStringDynArray syn = get_csv_array(record, kCsvMaxItems, true);

	SyncEntry e;
	e.title = syn[0];
	e.enabled = !equal_0(syn[1]);
	e.overwrite = ContainsText(syn[2], "O");
	e.sync_delete = ContainsText(syn[2], "D");
	for (int i = 3; i < syn.Length && !syn[i].IsEmpty(); i++)
		e.dirs.push_back(syn[i]);
	return e;
}

//---------------------------------------------------------------------------
UnicodeString FormatRecord(const SyncEntry &entry, int index_for_default)
{
	UnicodeString title = entry.title;
	if (title.IsEmpty()) title.sprintf(_T("登録%u"), index_for_default + 1);
	UnicodeString rec = make_csv_str(title);
	rec.cat_sprintf(_T(",\"%s\""), entry.enabled ? _T("1") : _T("0"));
	UnicodeString opt;
	if (entry.overwrite) opt += "O";
	if (entry.sync_delete) opt += "D";
	rec.cat_sprintf(_T(",\"%s\""), opt.c_str());
	for (const UnicodeString &d : entry.dirs)
		rec.cat_sprintf(_T(",\"%s\""), d.c_str());
	return rec;
}

//---------------------------------------------------------------------------
UnicodeString DefaultTitle(int count)
{
	UnicodeString s;
	s.sprintf(_T("登録%u"), count + 1);
	return s;
}

//---------------------------------------------------------------------------
bool IsValid(const SyncEntry &entry)
{
	// OkButtonClick: syn_lst.Length<5 は不正 (title,enabled,opt + dir2件)
	return entry.dirs.size() >= 2;
}

//---------------------------------------------------------------------------
bool CanAdd(int dir_count)
{
	// AddRegActionUpdate: DirListBox->Count>=2
	return dir_count >= 2;
}

//---------------------------------------------------------------------------
UnicodeString NormalizeRecord(const UnicodeString &record)
{
	TStringDynArray syn = get_csv_array(record, kCsvMaxItems);  //***
	// 不正データをはねる
	if (syn.Length < 5) return EmptyStr;
	// 正規化 ("タイトル","有効:1/無効:0","オプション","dir1","dir2",...)
	UnicodeString lbuf;
	for (int j = 0; j < syn.Length; j++) {
		if (j > 0) lbuf += ",";
		if (j >= 3) syn[j] = IncludeTrailingPathDelimiter(syn[j]);
		lbuf += make_csv_str(syn[j]);
	}
	return lbuf;
}

//---------------------------------------------------------------------------
Resolved ResolveTargets(const UnicodeString &dnam,
                        const std::vector<SyncEntry> &entries, bool del_sw)
{
	Resolved r;
	const UnicodeString base = IncludeTrailingPathDelimiter(dnam);
	r.targets.push_back(base);

	for (const SyncEntry &e : entries) {
		if (!e.enabled) continue;
		if (!IsValid(e)) continue;
		UnicodeString opt;
		if (e.overwrite) opt += "O";
		if (e.sync_delete) opt += "D";
		if (del_sw && !e.sync_delete) continue;

		std::vector<UnicodeString> norm;
		for (const UnicodeString &d : e.dirs)
			norm.push_back(IncludeTrailingPathDelimiter(d));

		// 同期対象があるか?
		UnicodeString snam;
		bool flag = false;
		for (const UnicodeString &nd : norm) {
			if (StartsText(nd, base)) {
				snam = base;
				snam.Delete(1, nd.Length());
				flag = true;
				break;
			}
		}
		if (!flag) continue;

		r.option = opt;
		for (const UnicodeString &nd : norm) {
			const UnicodeString pnam = nd + snam;
			if (!SameText(base, pnam)) r.targets.push_back(pnam);
		}
		break;
	}
	return r;
}

//---------------------------------------------------------------------------
void SyncDirStore::SaveToIni(UsrIniFile &ini) const
{
	ini.WriteInteger(kSection, _T("Count"), static_cast<int>(items_.size()));
	for (int i = 0; i < static_cast<int>(items_.size()); i++) {
		UnicodeString key;
		ini.WriteString(kSection, key.sprintf(_T("Item%02d"), i),
		                NormalizeRecord(FormatRecord(items_[static_cast<std::size_t>(i)], i)));
	}
}

//---------------------------------------------------------------------------
void SyncDirStore::LoadFromIni(UsrIniFile &ini)
{
	const int count = ini.ReadInteger(kSection, _T("Count"), 0);
	if (count <= 0) return;  // セクションが無い/空なら何もしない

	std::vector<SyncEntry> loaded;
	loaded.reserve(static_cast<std::size_t>(count));
	for (int i = 0; i < count; i++) {
		UnicodeString key;
		// 値は CSV 形式で自前のクォートを持つため、ini 側の del_quot は切る
		// (gui/regdir.cpp と同じ理由)
		const UnicodeString rec =
			ini.ReadString(kSection, key.sprintf(_T("Item%02d"), i), EmptyStr, false);
		if (rec.IsEmpty()) continue;
		const UnicodeString norm = NormalizeRecord(rec);
		if (norm.IsEmpty()) continue;  // 不正行は落とす (OkButtonClick と同じ)
		loaded.push_back(ParseRecord(norm));
	}
	if (loaded.empty()) return;

	items_ = std::move(loaded);
}

}  // namespace sync_dirs
