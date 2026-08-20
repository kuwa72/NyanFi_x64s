/**
 * @file compat/exception.h
 * @brief Sysutils::Exception 階層の互換シム
 *
 * ustring.cpp (ToInt の失敗) と sysutils.cpp の両方から使うため、循環依存を
 * 避けてヘッダオンリーで完結させている。実測: catch 80 / Exception 17。
 */
#ifndef NYANFI_COMPAT_EXCEPTION_H
#define NYANFI_COMPAT_EXCEPTION_H

#include <exception>
#include <string>

#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

/**
 * @brief Delphi の Exception 互換
 * @details `catch (Exception &e) { ... e.Message ... }` の形をそのまま通す。
 */
class Exception : public std::exception {
public:
	explicit Exception(const UnicodeString &msg) : message_(msg)
	{
		// what() が返すのは UTF-8。表示用ではなく診断用と割り切る
		const int len = ::WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), msg.Length(), nullptr, 0, nullptr, nullptr);
		if (len > 0) {
			narrow_.resize(static_cast<std::size_t>(len));
			::WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), msg.Length(), narrow_.data(), len, nullptr, nullptr);
		}
	}

	const char *what() const noexcept override { return narrow_.c_str(); }

	UnicodeString GetMessage() const { return message_; }

	compat::ROProperty<Exception, UnicodeString, &Exception::GetMessage> Message{this};

private:
	UnicodeString message_;
	std::string narrow_;
};

/// SysUtils::Abort が投げる例外。メッセージを持たない
class EAbort : public Exception {
public:
	EAbort() : Exception(UnicodeString()) {}
	using Exception::Exception;  //!< 実測: usr_exif.cpp:1007 が EAbort("...") を投げる
};

/// 数値変換の失敗 (UnicodeString::ToInt など)
class EConvertError : public Exception {
public:
	using Exception::Exception;
};

/// ファイル入出力の失敗
class EInOutError : public Exception {
public:
	using Exception::Exception;
};

/// Win32 API の失敗 (GetLastError 由来)
class EOSError : public Exception {
public:
	using Exception::Exception;
	DWORD ErrorCode = 0;
};

namespace System {
namespace Sysutils {
using ::EAbort;
using ::EConvertError;
using ::EInOutError;
using ::EOSError;
using ::Exception;
}  // namespace Sysutils
}  // namespace System

#endif  // NYANFI_COMPAT_EXCEPTION_H
