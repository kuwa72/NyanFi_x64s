/**
 * @file gui/usr_shell_fmt_shim.cpp
 * @brief usr_shell.h が宣言するプロパティ行の書式化関数の実体 (GUI 非依存部分のみ)
 *
 * @details `src/usr_file_inf.cpp` の各種フォーマット別情報取得関数
 *          (`get_ExifInf` / `get_PngInf` / `get_WavInf` など、GenInfDlg.cpp の
 *          「ファイル情報」ダイアログ向けに用意されている一群) は、いずれも
 *          `add_PropLine()` / `get_PropTitle()` などで1行を組み立てている。
 *          これらの実体は本来 `src/usr_shell.cpp` にあるが、その1ファイルは
 *          `IShellFolder` / `IContextMenu` 経由のシェル統合が大半を占め、
 *          GUI/COM に強く依存していて Phase 0/1 のシムではビルドが通らない
 *          (`scripts/probe.sh usr_shell` で確認済み。エラーは Graphics::TBitmap
 *          のプロパティ、`__finally`、`TGUID` など全て usr_shell.cpp 内の
 *          他の関数に起因し、ここに複製した関数群とは無関係)。
 *
 *          このファイルは `src/usr_shell.cpp` の該当関数 (行番号 54-124 付近、
 *          `get_PropTitle` / `make_PropLine` / `add_PropLine` / `add_PropLine_if` /
 *          `add_WarnLine`) を **一字一句そのまま複製**したものであり、独自の
 *          解析ロジックではない。TStrings への文字列整形だけを行う純粋関数で
 *          GUI コントロールに一切触れないため、複製しても安全に動作する。
 *          usr_shell.cpp 側を将来移植できたら、このファイルは削除して
 *          そちらに一本化すること。
 */
#include "usr_shell.h"

#include "usr_str.h"

//---------------------------------------------------------------------------
//プロパティ名の右揃え表示文字列の取得
//---------------------------------------------------------------------------
UnicodeString get_PropTitle(UnicodeString s)
{
	return align_r_str(s, FPRP_NAM_WD, _T(": "));
}
//---------------------------------------------------------------------------
UnicodeString get_PropTitle(const _TCHAR *s)
{
	return align_r_str(s, FPRP_NAM_WD, _T(": "));
}

//---------------------------------------------------------------------------
//プロパティ項目の作成("プロパティ名: 値文字列");
//---------------------------------------------------------------------------
UnicodeString make_PropLine(UnicodeString tit, UnicodeString str)
{
	return align_r_str(tit, FPRP_NAM_WD, _T(": ")) + str;
}
//---------------------------------------------------------------------------
UnicodeString make_PropLine(const _TCHAR *tit, UnicodeString str)
{
	return align_r_str(tit, FPRP_NAM_WD, _T(": ")) + str;
}
//---------------------------------------------------------------------------
UnicodeString make_PropLine(UnicodeString tit, int n)
{
	return align_r_str(tit, FPRP_NAM_WD, _T(": ")).cat_sprintf(_T("%u"), n);
}
//---------------------------------------------------------------------------
UnicodeString make_PropLine(const _TCHAR *tit, int n)
{
	return align_r_str(tit, FPRP_NAM_WD, _T(": ")).cat_sprintf(_T("%u"), n);
}

//---------------------------------------------------------------------------
void add_PropLine(UnicodeString tit, UnicodeString str, TStrings *lst, int flag)
{
	UnicodeString lbuf = make_PropLine(tit, str);
	if (flag!=0) lst->AddObject(lbuf, (TObject*)(NativeInt)flag); else lst->Add(lbuf);
}
//---------------------------------------------------------------------------
void add_PropLine(const _TCHAR *tit, UnicodeString str, TStrings *lst, int flag)
{
	UnicodeString lbuf = make_PropLine(tit, str);
	if (flag!=0) lst->AddObject(lbuf, (TObject*)(NativeInt)flag); else lst->Add(lbuf);
}
//---------------------------------------------------------------------------
void add_PropLine(UnicodeString tit, int n, TStrings *lst, int flag)
{
	UnicodeString lbuf = make_PropLine(tit, n);
	if (flag!=0) lst->AddObject(lbuf, (TObject*)(NativeInt)flag); else lst->Add(lbuf);
}
//---------------------------------------------------------------------------
void add_PropLine(const _TCHAR *tit, int n, TStrings *lst, int flag)
{
	UnicodeString lbuf = make_PropLine(tit, n);
	if (flag!=0) lst->AddObject(lbuf, (TObject*)(NativeInt)flag); else lst->Add(lbuf);
}

//---------------------------------------------------------------------------
void add_PropLine_if(const _TCHAR *tit, UnicodeString str, TStrings *lst, int flag)
{
	if (!str.IsEmpty()) add_PropLine(tit, str, lst, flag);
}

//---------------------------------------------------------------------------
//警告を追加
//---------------------------------------------------------------------------
void add_WarnLine(UnicodeString str, TStrings *lst)
{
	lst->AddObject(make_PropLine(_T("警告"), str), (TObject*)LBFLG_ERR_FIF);
}
