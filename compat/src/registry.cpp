/**
 * @file registry.cpp
 * @brief compat/registry.h の実装。RegOpenKeyExW 系の薄いラッパ。
 */
#include "compat/registry.h"

#include <cstring>
#include <vector>

#include "compat/sysutils.h"

namespace {

/// レジストリの値をバイナリのまま読み出す。取得できなければ false
bool read_raw_value(HKEY key, const UnicodeString &name, DWORD &type, std::vector<BYTE> &out)
{
	DWORD size = 0;
	LONG rc = ::RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size);
	if (rc != ERROR_SUCCESS) return false;

	out.assign(size + sizeof(wchar_t), 0);	//文字列読み出し時の終端 NUL 分の余裕を持たせる
	DWORD real_size = size;
	rc = ::RegQueryValueExW(key, name.c_str(), nullptr, &type, out.data(), &real_size);
	if (rc != ERROR_SUCCESS) return false;
	out.resize(real_size);
	return true;
}

}  // namespace

//---------------------------------------------------------------------------
TRegistry::TRegistry() = default;

//---------------------------------------------------------------------------
TRegistry::~TRegistry()
{
	CloseKey();
}

//---------------------------------------------------------------------------
bool TRegistry::OpenKeyReadOnly(const UnicodeString &key)
{
	CloseKey();

	HKEY h = nullptr;
	const LONG rc = ::RegOpenKeyExW(root_key_, key.c_str(), 0, KEY_READ, &h);
	if (rc != ERROR_SUCCESS || !h) return false;

	cur_key_ = h;
	CurrentPath = key;
	return true;
}

//---------------------------------------------------------------------------
void TRegistry::CloseKey()
{
	if (cur_key_) {
		::RegCloseKey(cur_key_);
		cur_key_ = nullptr;
	}
	CurrentPath = UnicodeString();
}

//---------------------------------------------------------------------------
UnicodeString TRegistry::ReadString(const UnicodeString &name) const
{
	if (!cur_key_) return UnicodeString();

	DWORD type = 0;
	std::vector<BYTE> buf;
	if (!read_raw_value(cur_key_, name, type, buf)) return UnicodeString();

	if (type == REG_SZ || type == REG_EXPAND_SZ) {
		const wchar_t *ws = reinterpret_cast<const wchar_t *>(buf.data());
		//NUL 終端されている前提だが、されていない場合に備え長さを明示的に切る
		std::size_t n = buf.size() / sizeof(wchar_t);
		std::size_t len = 0;
		while (len < n && ws[len] != L'\0') ++len;
		return UnicodeString(ws, static_cast<int>(len));
	}
	if (type == REG_DWORD && buf.size() >= sizeof(DWORD)) {
		DWORD v = 0;
		std::memcpy(&v, buf.data(), sizeof(DWORD));
		return IntToStr(static_cast<Int64>(v));
	}
	return UnicodeString();
}

//---------------------------------------------------------------------------
int TRegistry::ReadInteger(const UnicodeString &name) const
{
	if (!cur_key_) return 0;

	DWORD type = 0;
	std::vector<BYTE> buf;
	if (!read_raw_value(cur_key_, name, type, buf)) return 0;

	if (type == REG_DWORD && buf.size() >= sizeof(DWORD)) {
		DWORD v = 0;
		std::memcpy(&v, buf.data(), sizeof(DWORD));
		return static_cast<int>(v);
	}
	if (type == REG_SZ || type == REG_EXPAND_SZ) {
		return ReadString(name).ToIntDef(0);
	}
	return 0;
}

//---------------------------------------------------------------------------
bool TRegistry::ValueExists(const UnicodeString &name) const
{
	if (!cur_key_) return false;
	DWORD type = 0;
	const LONG rc = ::RegQueryValueExW(cur_key_, name.c_str(), nullptr, &type, nullptr, nullptr);
	return rc == ERROR_SUCCESS;
}

//---------------------------------------------------------------------------
bool TRegistry::KeyExists(const UnicodeString &subKey) const
{
	if (!cur_key_) return false;
	HKEY h = nullptr;
	const LONG rc = ::RegOpenKeyExW(cur_key_, subKey.c_str(), 0, KEY_READ, &h);
	if (rc == ERROR_SUCCESS && h) {
		::RegCloseKey(h);
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
void TRegistry::GetKeyNames(TStringList *list) const
{
	if (!list) return;
	list->Clear();
	if (!cur_key_) return;

	for (DWORD index = 0;; ++index) {
		wchar_t name[256];
		DWORD name_len = 256;
		const LONG rc = ::RegEnumKeyExW(cur_key_, index, name, &name_len, nullptr, nullptr, nullptr, nullptr);
		if (rc == ERROR_NO_MORE_ITEMS) break;
		if (rc != ERROR_SUCCESS) break;
		list->Add(UnicodeString(name, static_cast<int>(name_len)));
	}
}
