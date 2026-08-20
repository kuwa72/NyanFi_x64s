/**
 * @file gui/file_info.cpp
 * @brief ファイル情報の組み立ての実装
 *
 * @details 設計・使用関数の一覧は gui/file_info.h の冒頭コメントを参照。
 */
#include "gui/file_info.h"

#include "usr_file_ex.h"
#include "usr_file_inf.h"
#include "usr_id3.h"
#include "usr_str.h"

namespace {

/// 拡張子 (ドット付き) を取り出す
UnicodeString ExtOf(const UnicodeString &full_path)
{
	return get_extension(full_path);
}

/// 種別ごとの詳細情報を1つ追加する (該当なしなら何もしない)
void AppendTypeSpecificLines(const UnicodeString &full_path, const UnicodeString &fext, TStringList *lst)
{
	if (test_IcoExt(fext) || test_CurExt(fext)) {
		get_IconInf(full_path, lst);
	}
	else if (test_AniExt(fext)) {
		get_AniInf(full_path, lst);
	}
	else if (SameText(fext, _T(".webp"))) {
		get_WebpInf(full_path, lst);
	}
	else if (test_PspExt(fext)) {
		get_PspInf(full_path, lst);
	}
	else if (test_MetaExt(fext)) {
		get_MetafileInf(full_path, lst);
	}
	// Exif/PNG/GIF は本来 WIC 経由でも表示できるかの判定 (is_ViewableFext /
	// usr_SH->get_PropInf) が絡むが、usr_SH (UserShell) が未移植のため、
	// ここでは拡張子から直接 Exif→PNG→GIF の優先順で1つだけ試す
	// (簡略化。gui/file_info.h の「対象外にした種別」を参照)
	else if (test_ExifExt(fext)) {
		get_ExifInf(full_path, lst);
		if (test_JpgExt(fext)) get_JpgExInf(full_path, lst);
	}
	else if (test_PngExt(fext)) {
		get_PngInf(full_path, lst);
	}
	else if (test_GifExt(fext)) {
		get_GifInf(full_path, lst);
	}
	else if (SameText(fext, _T(".wav"))) {
		get_WavInf(full_path, lst);
	}
	else if (test_Mp3Ext(fext)) {
		ID3_GetInf(full_path, lst);
	}
	else if (test_FlacExt(fext)) {
		get_FlacInf(full_path, lst);
	}
	else if (SameText(fext, _T(".opus"))) {
		get_OpusInf(full_path, lst);
	}
	else if (SameText(fext, _T(".cda"))) {
		get_CdaInf(full_path, lst);
	}
	else if (test_FileExt(fext, _T(".pdf"))) {
		get_PdfVer(full_path, lst);
	}
	else if (test_HtmlExt(fext)) {
		get_HtmlInf(full_path, lst);
	}
	else if (test_FileExt(fext, _T(".cbproj.dproj.cpp.pas.dfm.fmx.h"))) {
		get_BorlandInf(full_path, lst);
	}
	else if (SameText(ExtractFileName(full_path), _T("tags"))) {
		get_TagsInf(full_path, lst);
	}
	else if (test_ExeExt(fext)) {
		// get_AppInf は usr_SH (UserShell、未移植) に依存するため使えない。
		// 実行可能ファイルであることだけを知らせる (gui/file_info.h 参照)
		lst->Add(_T("実行可能ファイルです (詳細情報は未対応)"));
	}
}

}  // namespace

//---------------------------------------------------------------------------
void BuildFileInfoLines(const UnicodeString &full_path, const FileItem &item, TStringList *lst)
{
	lst->Add(_T("名前: ") + item.name);
	lst->Add(_T("パス: ") + full_path);
	lst->Add(_T("種類: ") + UnicodeString(item.is_dir ? _T("ディレクトリ") : _T("ファイル")));
	if (!item.is_dir) {
		UnicodeString size_line;
		size_line.sprintf(_T("サイズ: %s (%s バイト)"),
		                   get_size_str_G(item.size, 0, 2).Trim().c_str(),
		                   get_size_str_B(item.size, 0).Trim().c_str());
		lst->Add(size_line);
	}
	lst->Add(_T("更新日時: ") + FormatDateTime(_T("yyyy/mm/dd hh:nn:ss"), item.stamp));
	lst->Add(_T("属性: ") + get_file_attr_str(item.attr));

	if (item.is_dir) return;  // 種別ごとの詳細情報はファイルのみ対象

	const int ads_cnt = get_ADS_count(full_path);
	if (ads_cnt > 0) {
		lst->Add(EmptyStr);
		UnicodeString ads_line;
		ads_line.sprintf(_T("代替データストリーム: %d 件"), ads_cnt);
		lst->Add(ads_line);
		get_ADS_Inf(full_path, lst);
	}

	const UnicodeString fext = ExtOf(full_path);
	const int before = lst->Count;
	AppendTypeSpecificLines(full_path, fext, lst);
	if (lst->Count > before) lst->Insert(before, EmptyStr);  // 区切りの空行
}

//---------------------------------------------------------------------------
void AppendHashLines(const UnicodeString &full_path, TStringList *lst)
{
	lst->Add(EmptyStr);
	lst->Add(_T("SHA256: ") + get_HashStr(full_path, _T("SHA256")));
	lst->Add(_T("CRC32: ") + get_HashStr(full_path, _T("CRC32")));
}
