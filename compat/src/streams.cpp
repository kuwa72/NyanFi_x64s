/**
 * @file compat/src/streams.cpp
 * @brief compat/streams.h の実装
 */
#include "compat/streams.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "compat/sysutils.h"  // SysErrorMessage

namespace {

/// CopyFrom の 1 回あたりの転送サイズ (Delphi の MaxBufSize=$F000 に合わせる)
constexpr int kCopyBufSize = 0xF000;

}  // namespace

//===========================================================================
// TStream
//===========================================================================
int TStream::Seek(int offset, Word origin)
{
	TSeekOrigin newOrigin = soBeginning;
	if (origin == soFromCurrent)
		newOrigin = soCurrent;
	else if (origin == soFromEnd)
		newOrigin = soEnd;
	return static_cast<int>(Seek(static_cast<Int64>(offset), newOrigin));
}

int TStream::Read(TBytes &buffer, int count)
{
	if (count <= 0) return 0;
	return Read(buffer.vec().data(), count);
}

int TStream::Write(const TBytes &buffer, int count)
{
	if (count <= 0) return 0;
	return Write(buffer.vec().data(), count);
}

void TStream::ReadBuffer(void *buffer, int count)
{
	if (count <= 0) return;
	if (Read(buffer, count) != count) throw EReadError(UnicodeString(L"ストリームの読み込みに失敗しました"));
}

void TStream::ReadBuffer(TBytes &buffer, int count)
{
	if (count <= 0) return;
	if (Read(buffer, count) != count) throw EReadError(UnicodeString(L"ストリームの読み込みに失敗しました"));
}

void TStream::WriteBuffer(const void *buffer, int count)
{
	if (count <= 0) return;
	if (Write(buffer, count) != count) throw EWriteError(UnicodeString(L"ストリームの書き込みに失敗しました"));
}

void TStream::WriteBuffer(const TBytes &buffer, int count)
{
	if (count <= 0) return;
	if (Write(buffer, count) != count) throw EWriteError(UnicodeString(L"ストリームの書き込みに失敗しました"));
}

Int64 TStream::CopyFrom(TStream *source, Int64 count)
{
	// 実測 (Global.cpp / usr_file_inf.cpp / usr_id3.cpp / usr_exif.cpp など) は
	// 戻り値を要求サイズと比較して不足を検出する形で書かれており、読み取れな
	// かった分を例外にせず短く返す前提。ReadBuffer (例外送出) ではなく Read を
	// 使うのはそのため。
	if (count <= 0) {
		source->SetPosition(0);
		count = source->GetSize();
	}
	if (count <= 0) return 0;

	std::vector<Byte> buf(static_cast<std::size_t>(std::min<Int64>(count, kCopyBufSize)));
	Int64 remaining = count;
	Int64 total = 0;
	while (remaining > 0) {
		const int chunk = static_cast<int>(std::min<Int64>(remaining, kCopyBufSize));
		const int n = source->Read(buf.data(), chunk);
		if (n <= 0) break;
		const int written = Write(buf.data(), n);
		total += written;
		if (written != n) break;
		remaining -= n;
	}
	return total;
}

Int64 TStream::GetPosition() const
{
	// 汎用フォールバック。Seek(0, soCurrent) は位置を動かさない問い合わせ
	// なので、論理的には const とみなして const_cast する。
	// 注意: 第 1 引数を Int64{0} と明示しないと、Seek(int,Word) 側にも
	// TSeekOrigin -> Word の暗黙変換が効いてしまいオーバーロード解決が曖昧になる。
	return const_cast<TStream *>(this)->Seek(Int64{0}, soCurrent);
}

void TStream::SetPosition(Int64 value) { Seek(value, soBeginning); }

Int64 TStream::GetSize() const
{
	TStream *self = const_cast<TStream *>(this);
	const Int64 cur = self->Seek(Int64{0}, soCurrent);
	const Int64 sz = self->Seek(Int64{0}, soEnd);
	self->Seek(cur, soBeginning);
	return sz;
}

void TStream::SetSize(Int64 /*newSize*/)
{
	// 基底では何もしない。THandleStream/TMemoryStream がオーバーライドする。
}

//===========================================================================
// THandleStream
//===========================================================================
THandleStream::THandleStream(HANDLE handle) : handle_(handle)
{
	if (handle_ != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER cur{};
		LARGE_INTEGER zero{};
		zero.QuadPart = 0;
		if (::SetFilePointerEx(handle_, zero, &cur, FILE_CURRENT)) position_ = cur.QuadPart;
	}
}

THandleStream::~THandleStream()
{
	if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
}

int THandleStream::Read(void *buffer, int count)
{
	if (count <= 0) return 0;
	DWORD read = 0;
	if (!::ReadFile(handle_, buffer, static_cast<DWORD>(count), &read, nullptr)) return 0;
	position_ += read;
	return static_cast<int>(read);
}

int THandleStream::Write(const void *buffer, int count)
{
	if (count <= 0) return 0;
	DWORD written = 0;
	if (!::WriteFile(handle_, buffer, static_cast<DWORD>(count), &written, nullptr)) return 0;
	position_ += written;
	return static_cast<int>(written);
}

Int64 THandleStream::Seek(Int64 offset, TSeekOrigin origin)
{
	const DWORD method = (origin == soBeginning) ? FILE_BEGIN : (origin == soCurrent) ? FILE_CURRENT : FILE_END;
	LARGE_INTEGER li{};
	li.QuadPart = offset;
	LARGE_INTEGER newPos{};
	if (!::SetFilePointerEx(handle_, li, &newPos, method))
		throw EReadError(UnicodeString(L"Seek に失敗しました: ") + SysErrorMessage(::GetLastError()));
	position_ = newPos.QuadPart;
	return position_;
}

Int64 THandleStream::GetSize() const
{
	LARGE_INTEGER sz{};
	if (!::GetFileSizeEx(handle_, &sz)) return 0;
	return sz.QuadPart;
}

void THandleStream::SetSize(Int64 newSize)
{
	LARGE_INTEGER li{};
	li.QuadPart = newSize;
	::SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN);
	::SetEndOfFile(handle_);
	// 位置が新しいサイズを超えていたら詰める (Delphi の TStream.SetSize と同じ)
	if (position_ > newSize) position_ = newSize;
	LARGE_INTEGER cur{};
	cur.QuadPart = position_;
	::SetFilePointerEx(handle_, cur, nullptr, FILE_BEGIN);
}

//===========================================================================
// TFileStream
//===========================================================================
HANDLE TFileStream::OpenHandle(const UnicodeString &fileName, Word mode)
{
	DWORD access = 0;
	DWORD share = 0;
	DWORD creation = OPEN_EXISTING;

	if ((mode & 0xFFFF) == fmCreate) {
		access = GENERIC_READ | GENERIC_WRITE;
		share = 0;
		creation = CREATE_ALWAYS;
	}
	else {
		switch (mode & 0x0003) {
		case fmOpenWrite: access = GENERIC_WRITE; break;
		case fmOpenReadWrite: access = GENERIC_READ | GENERIC_WRITE; break;
		case fmOpenRead:
		default: access = GENERIC_READ; break;
		}
		switch (mode & 0x00F0) {
		case fmShareDenyWrite: share = FILE_SHARE_READ; break;
		case fmShareDenyRead: share = FILE_SHARE_WRITE; break;
		case fmShareDenyNone: share = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
		case fmShareExclusive:
		default: share = 0; break;
		}
		creation = OPEN_EXISTING;
	}

	HANDLE h = ::CreateFileW(fileName.c_str(), access, share, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		const UnicodeString msg = (creation == CREATE_ALWAYS)
			? UnicodeString(L"ファイルを作成できません: ") + fileName
			: UnicodeString(L"ファイルを開けません: ") + fileName;
		if (creation == CREATE_ALWAYS)
			throw EFCreateError(msg);
		else
			throw EFOpenError(msg);
	}
	return h;
}

TFileStream::TFileStream(const UnicodeString &fileName, Word mode) : THandleStream(OpenHandle(fileName, mode)) {}

//===========================================================================
// TMemoryStream
//===========================================================================
TMemoryStream::TMemoryStream() = default;

TMemoryStream::~TMemoryStream()
{
	if (Memory) std::free(Memory);
}

void TMemoryStream::EnsureCapacity(Int64 required)
{
	if (required <= capacity_) return;
	Int64 newCap = (capacity_ > 0) ? capacity_ * 2 : 4096;
	if (newCap < required) newCap = required;
	void *p = std::realloc(Memory, static_cast<std::size_t>(newCap));
	// realloc 失敗時は元のメモリが維持されるが、続行不能なので EReadError 系を流用する
	if (!p) throw EWriteError(UnicodeString(L"メモリストリームの領域確保に失敗しました"));
	Memory = p;
	capacity_ = newCap;
}

int TMemoryStream::Read(void *buffer, int count)
{
	if (count <= 0 || position_ >= size_) return 0;
	const Int64 avail = size_ - position_;
	const int actual = static_cast<int>(std::min<Int64>(count, avail));
	std::memcpy(buffer, static_cast<Byte *>(Memory) + position_, static_cast<std::size_t>(actual));
	position_ += actual;
	return actual;
}

int TMemoryStream::Write(const void *buffer, int count)
{
	if (count <= 0) return 0;
	EnsureCapacity(position_ + count);
	std::memcpy(static_cast<Byte *>(Memory) + position_, buffer, static_cast<std::size_t>(count));
	position_ += count;
	if (position_ > size_) size_ = position_;
	return count;
}

Int64 TMemoryStream::Seek(Int64 offset, TSeekOrigin origin)
{
	Int64 newPos = 0;
	switch (origin) {
	case soBeginning: newPos = offset; break;
	case soCurrent: newPos = position_ + offset; break;
	case soEnd: newPos = size_ + offset; break;
	}
	if (newPos < 0) newPos = 0;
	position_ = newPos;
	return position_;
}

void TMemoryStream::SetPosition(Int64 value) { Seek(value, soBeginning); }

void TMemoryStream::SetSize(Int64 newSize)
{
	if (newSize < 0) newSize = 0;
	if (newSize > 0) EnsureCapacity(newSize);
	size_ = newSize;
	if (position_ > size_) position_ = size_;
}

void TMemoryStream::Clear()
{
	if (Memory) std::free(Memory);
	Memory = nullptr;
	size_ = 0;
	capacity_ = 0;
	position_ = 0;
}

void TMemoryStream::LoadFromFile(const UnicodeString &fileName)
{
	TFileStream fs(fileName, fmOpenRead | fmShareDenyNone);
	const Int64 sz = fs.GetSize();
	Clear();
	if (sz > 0) {
		SetSize(sz);
		fs.ReadBuffer(Memory, static_cast<int>(sz));
	}
	position_ = 0;
}

void TMemoryStream::SaveToFile(const UnicodeString &fileName) const
{
	TFileStream fs(fileName, fmCreate);
	if (size_ > 0) fs.WriteBuffer(Memory, static_cast<int>(size_));
}
