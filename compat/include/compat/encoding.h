/**
 * @file compat/encoding.h
 * @brief System.SysUtils::TEncoding 互換シム (文字コード変換)
 *
 * 対象コードでの実測:
 *   - `TEncoding::UTF8` / `TEncoding::GetEncoding(codepage)` を
 *     `std::unique_ptr<TEncoding>` で受ける形が多数 (Global.cpp / usr_file_inf.cpp
 *     / usr_str.cpp / TxtViewer.cpp / ChInfFrm.cpp / InspectFrm.cpp など)
 *   - `enc->GetString(Bytes, 0, Bytes.Length)` (0 始まりのバイト添字)
 *   - `enc->GetBytes(s)` (DynamicArray<Byte> を返す)
 *   - `TEncoding::UTF8->GetString(...)` のように静的インスタンスへ直接メンバ呼び出し
 *
 * 重要 (呼び出し側の delete 前提の確認結果):
 *   `GetEncoding()` は必ず `std::unique_ptr<TEncoding>` または生の `new` 相当の
 *   受け方をされており (15 箇所以上、全て同じ形)、呼び出し側が delete する前提で
 *   書かれている。そのため本シムでは Delphi 最新版のようなキャッシュ共有を行わず、
 *   呼び出す度に新規インスタンスを返す。一方 `TEncoding::UTF8` などの静的メンバは
 *   一度も unique_ptr 等でラップされておらず、プログラム終了まで生存する前提。
 *   混同すると (共有インスタンスを delete する/新規インスタンスを delete し忘れる)
 *   二重解放や UAF になるため、この非対称性を厳密に守ること。
 */
#ifndef NYANFI_COMPAT_ENCODING_H
#define NYANFI_COMPAT_ENCODING_H

#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

/**
 * @brief Delphi の TEncoding 互換 (コードページ単位の文字コード変換)
 */
class TEncoding {
public:
	explicit TEncoding(unsigned int codePage);

	//-- プロパティ ----------------------------------------------------------
	unsigned int GetCodePage() const { return code_page_; }
	compat::ROProperty<TEncoding, unsigned int, &TEncoding::GetCodePage> CodePage{this};

	//-- 変換 (実測どおり index/count は 0 始まりのバイト単位) -----------------
	UnicodeString GetString(const TBytes &bytes, int index, int count) const;
	TBytes GetBytes(const UnicodeString &s) const;

	/// このエンコードの BOM (無ければ長さ 0 の配列)
	TBytes GetPreamble() const;

	//-- 静的インスタンス: プログラム終了まで生存。呼び出し側は delete しないこと --
	static TEncoding *UTF8;             //!< コードページ 65001
	static TEncoding *Unicode;          //!< UTF-16 LE (コードページ 1200)
	static TEncoding *BigEndianUnicode; //!< UTF-16 BE (コードページ 1201)
	static TEncoding *ANSI;             //!< システム既定 ANSI コードページ (GetACP())
	static TEncoding *Default;          //!< Windows Unicode ビルドでは ANSI と同じ
	static TEncoding *ASCII;            //!< US-ASCII (コードページ 20127)

	/// 新規インスタンスを返す。呼び出し側が delete する (上記静的インスタンスとは非対称)
	static TEncoding *GetEncoding(int codePage);

	/**
	 * @brief バッファ先頭の BOM から実際のエンコードを判定する
	 * @details BOM (UTF-8 / UTF-16LE / UTF-16BE) が見つかれば `encoding` を
	 *          対応する静的インスタンスに差し替え、読み飛ばすべきバイト数を返す。
	 *          見つからなければ `encoding` が非 NULL ならそのまま維持し、NULL なら
	 *          `defaultEncoding` (未指定時は TEncoding::Default) を設定して 0 を返す。
	 *          TStrings::LoadFromFile / LoadFromStream の既定文字コード判定に使う内部処理。
	 */
	static int GetBufferEncoding(const TBytes &buffer, TEncoding *&encoding, TEncoding *defaultEncoding = nullptr);

private:
	unsigned int code_page_;
};

#endif  // NYANFI_COMPAT_ENCODING_H
