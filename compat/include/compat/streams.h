/**
 * @file compat/streams.h
 * @brief TStream 系互換シム (System.Classes.hpp 相当)
 *
 * 対象コードでの実測: ->Seek() 135 / ->ReadBuffer() 80 / ->Read() 18 /
 * ->CopyFrom() 7 / ->Memory 3 / TFileStream 27 / TMemoryStream 7。
 *
 * 呼び出し形で確認できたこと:
 *   - `fs->Read(buf, n)` / `fs->Read(Bytes, n)` (TBytes& オーバーロードも来る)
 *   - `fs->ReadBuffer(&v, n)` / `fs->ReadBuffer(Bytes, n)` (同上)
 *   - `fs->Seek(n, soFromBeginning)` (旧形式 Word) と
 *     `ms->Seek((__int64)-2, soCurrent)` / `fs->Seek(p, soBeginning)` (新形式
 *     TSeekOrigin) の両方が実際に使われている (Global.cpp / usr_file_inf.cpp)
 *   - `ms->CopyFrom(fs.get(), size)` の戻り値を要求サイズと比較して
 *     `if (...<size) Abort();` する形が複数箇所にあり、読めなかった分だけ
 *     短く返す (例外を投げない) 前提で書かれている
 *   - `new TFileStream(fnam, fmOpenRead | fmShareDenyNone)` と
 *     `new TFileStream(fnam, fmCreate)` の 2 パターンのみ実測 (共有モードは
 *     常に fmShareDenyNone、書き込みは常に fmCreate)
 */
#ifndef NYANFI_COMPAT_STREAMS_H
#define NYANFI_COMPAT_STREAMS_H

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
// TStream::Seek の Origin 定数 (2 系統が混在して実測されている)
//---------------------------------------------------------------------------
/// 旧形式 32bit Seek (`Seek(int Offset, Word Origin)`) 用
constexpr Word soFromBeginning = 0;
constexpr Word soFromCurrent = 1;
constexpr Word soFromEnd = 2;

/// 新形式 64bit Seek (`Seek(Int64 Offset, TSeekOrigin Origin)`) 用。
/// 値は旧形式の soFromXxx と揃えてあり、`(TSeekOrigin)soFromBeginning` の
/// ような明示キャスト (usr_mmfile.cpp) がそのまま成立する。
enum TSeekOrigin { soBeginning = 0, soCurrent = 1, soEnd = 2 };

//---------------------------------------------------------------------------
// TFileStream の Mode 定数 (System.SysUtils 相当)
//---------------------------------------------------------------------------
constexpr Word fmOpenRead = 0x0000;
constexpr Word fmOpenWrite = 0x0001;
constexpr Word fmOpenReadWrite = 0x0002;
constexpr Word fmShareCompat = 0x0000;
constexpr Word fmShareExclusive = 0x0010;
constexpr Word fmShareDenyWrite = 0x0020;
constexpr Word fmShareDenyRead = 0x0030;
constexpr Word fmShareDenyNone = 0x0040;
/// 実測どおり単独で使われる特別値 (共有モードとの OR はしない)
constexpr Word fmCreate = 0xFFFF;

//---------------------------------------------------------------------------
/// ファイル入出力例外 (exception.h の EInOutError から派生。
/// exception.h は他モジュール所有につき編集せず、ここで派生させるに留める)
class EReadError : public EInOutError {
public:
	using EInOutError::EInOutError;
};
class EWriteError : public EInOutError {
public:
	using EInOutError::EInOutError;
};
class EFOpenError : public EInOutError {
public:
	using EInOutError::EInOutError;
};
class EFCreateError : public EInOutError {
public:
	using EInOutError::EInOutError;
};

//---------------------------------------------------------------------------
/**
 * @brief Delphi の TStream 互換基底
 * @details Read/Write/Seek (64bit 版) のみ純粋仮想。ReadBuffer/WriteBuffer/
 *          CopyFrom/Position/Size はここで共通実装する。
 */
class TStream : public TObject {
public:
	TStream() = default;
	~TStream() override = default;
	TStream(const TStream &) = delete;
	TStream &operator=(const TStream &) = delete;

	//-- 読み書き (派生クラスが実装) ------------------------------------------
	virtual int Read(void *buffer, int count) = 0;
	virtual int Write(const void *buffer, int count) = 0;
	virtual Int64 Seek(Int64 offset, TSeekOrigin origin) = 0;

	/// 旧形式 Seek。新形式へ委譲するだけなので仮想化しない
	int Seek(int offset, Word origin);

	//-- TBytes オーバーロード (実測: get_MemoryStrins / fsRead_comment_utf8) ---
	int Read(TBytes &buffer, int count);
	int Write(const TBytes &buffer, int count);

	//-- 要求バイト数に満たなければ例外を投げる版 -----------------------------
	void ReadBuffer(void *buffer, int count);
	void ReadBuffer(TBytes &buffer, int count);
	void WriteBuffer(const void *buffer, int count);
	void WriteBuffer(const TBytes &buffer, int count);

	/**
	 * @brief 別ストリームから読み書きコピーする
	 * @details count<=0 なら source を先頭 (Position=0) へ戻し、Size 全体を
	 *          コピー対象にする (実測: `ms->CopyFrom(ms.get(), 0)` は自己コピー
	 *          かつ空ストリームなので実質 no-op になる)。実測の呼び出し側は
	 *          戻り値を要求サイズと比較して不足を検出しており、読めなかった分は
	 *          例外を投げず短く返す (Delphi の一部バージョンの実装に合わせた)。
	 * @return 実際にコピーできたバイト数
	 */
	Int64 CopyFrom(TStream *source, Int64 count);

	//-- プロパティアクセサ ----------------------------------------------------
	// 汎用実装 (Seek 経由)。THandleStream/TMemoryStream はより効率的な実装で
	// 上書きする。GetPosition/GetSize を const にするため、実体を書き換えない
	// Seek(0, soCurrent) 相当の問い合わせに限り const_cast で逃がす。
	virtual Int64 GetPosition() const;
	virtual void SetPosition(Int64 value);
	virtual Int64 GetSize() const;
	virtual void SetSize(Int64 newSize);

	compat::RWValueProperty<TStream, Int64, &TStream::GetPosition, &TStream::SetPosition> Position{this};
	compat::RWValueProperty<TStream, Int64, &TStream::GetSize, &TStream::SetSize> Size{this};
};

//---------------------------------------------------------------------------
/// Win32 HANDLE をラップする TStream (実測での直接利用は無いが TFileStream の基底として必要)
class THandleStream : public TStream {
public:
	explicit THandleStream(HANDLE handle);
	~THandleStream() override;

	// Read/Write/Seek を再宣言すると、同名の TStream 側オーバーロード
	// (TBytes& 版の Read/Write、Word 版の Seek) が名前隠蔽で見えなくなるため
	// using 宣言で引き戻す (実測: fs->Read(buf.get(),n) と
	// fs->Seek(p, soFromBeginning) のような旧形式呼び出しが両方存在する)。
	using TStream::Read;
	using TStream::Seek;
	using TStream::Write;

	int Read(void *buffer, int count) override;
	int Write(const void *buffer, int count) override;
	Int64 Seek(Int64 offset, TSeekOrigin origin) override;

	Int64 GetPosition() const override { return position_; }
	Int64 GetSize() const override;
	void SetSize(Int64 newSize) override;

	HANDLE GetHandle() const { return handle_; }
	compat::ROProperty<THandleStream, HANDLE, &THandleStream::GetHandle> Handle{this};

protected:
	HANDLE handle_ = INVALID_HANDLE_VALUE;
	Int64 position_ = 0;
};

//---------------------------------------------------------------------------
/**
 * @brief ファイルストリーム
 * @details 実測は `fmOpenRead | fmShareDenyNone` (読み取り専用) と `fmCreate`
 *          (新規作成/上書き) の 2 パターンのみ。コンストラクタ失敗時は
 *          EFOpenError/EFCreateError (EInOutError 派生) を投げる。
 */
class TFileStream : public THandleStream {
public:
	TFileStream(const UnicodeString &fileName, Word mode);

private:
	static HANDLE OpenHandle(const UnicodeString &fileName, Word mode);
};

//---------------------------------------------------------------------------
/**
 * @brief メモリストリーム
 * @details Write で自動的に確保領域を拡張する (2 倍成長)。Memory は現在の
 *          先頭ポインタ (再確保のたびに変わりうるので、都度読み直すこと)。
 */
class TMemoryStream : public TStream {
public:
	TMemoryStream();
	~TMemoryStream() override;
	TMemoryStream(const TMemoryStream &) = delete;
	TMemoryStream &operator=(const TMemoryStream &) = delete;

	// THandleStream と同じ理由 (名前隠蔽対策) で using 宣言が必要
	using TStream::Read;
	using TStream::Seek;
	using TStream::Write;

	int Read(void *buffer, int count) override;
	int Write(const void *buffer, int count) override;
	Int64 Seek(Int64 offset, TSeekOrigin origin) override;

	Int64 GetPosition() const override { return position_; }
	void SetPosition(Int64 value) override;
	Int64 GetSize() const override { return size_; }
	void SetSize(Int64 newSize) override;

	/**
	 * @brief バッファ先頭ポインタ
	 * @details 実測では `(BYTE*)ms->Memory + ofs` のような C 形式キャストだけで
	 *          なく `reinterpret_cast<BYTE*>(ms->Memory)` (usr_wic.cpp) も
	 *          来る。reinterpret_cast はクラス型に対してユーザー定義変換を一切
	 *          呼ばない (スカラ型同士でしか使えない) ため、compat::ROProperty の
	 *          ようなプロキシクラスでは表現できない。実測の呼び出し形をすべて
	 *          通すため、あえてプロパティらしいラッパを使わず素の `void*` を
	 *          直接公開する (再確保のたびに値が変わりうるので、都度読み直すこと)。
	 */
	void *Memory = nullptr;

	void LoadFromFile(const UnicodeString &fileName);
	void SaveToFile(const UnicodeString &fileName) const;
	void Clear();

private:
	void EnsureCapacity(Int64 required);

	Int64 size_ = 0;
	Int64 capacity_ = 0;
	Int64 position_ = 0;
};

namespace System {
namespace Classes {
using ::EFCreateError;
using ::EFOpenError;
using ::EReadError;
using ::EWriteError;
using ::TFileStream;
using ::THandleStream;
using ::TMemoryStream;
using ::TSeekOrigin;
using ::TStream;
}  // namespace Classes
}  // namespace System

#endif  // NYANFI_COMPAT_STREAMS_H
