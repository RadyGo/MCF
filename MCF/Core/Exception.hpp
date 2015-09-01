// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2015, LH_Mouse. All wrongs reserved.

#ifndef MCF_CORE_EXCEPTION_HPP_
#define MCF_CORE_EXCEPTION_HPP_

#include "LastError.hpp"
#include <exception>
#include <cstring>

namespace MCF {

class Exception : public std::exception {
private:
	const char *const x_pszFile;
	const unsigned long x_ulLine;

	char x_achMessage[1024 + 8];
	unsigned long x_ulCode;

public:
	Exception(const char *pszFile, unsigned long ulLine,
		const char *pszMessage, unsigned long ulCode) noexcept
		: x_pszFile(pszFile), x_ulLine(ulLine)
	{
		auto uLen = std::strlen(pszMessage);
		if(uLen > sizeof(x_achMessage) - 1){
			uLen = sizeof(x_achMessage) - 1;
		}
		std::memcpy(x_achMessage, pszMessage, uLen);
		x_achMessage[uLen] = 0;

		x_ulCode = ulCode;
	}
	~Exception() override;

public:
	const char *what() const noexcept override {
		return x_achMessage;
	}

	const char *GetFile() const noexcept {
		return x_pszFile;
	}
	unsigned long GetLine() const noexcept {
		return x_ulLine;
	}

	const char *GetMessage() const noexcept {
		return x_achMessage;
	}
	unsigned long GetCode() const noexcept {
		return x_ulCode;
	}
};

class SystemError : public Exception {
public:
	SystemError(const char *pszFile, unsigned long ulLine,
		const char *pszMessage, unsigned long ulCode = GetWin32LastError()) noexcept
		: Exception(pszFile, ulLine, pszMessage, ulCode)
	{
	}
	~SystemError() override;
};

}

#define DEBUG_THROW(etype_, ...)	\
	([&]() [[noreturn]] {	\
		etype_ e_ (__FILE__, __LINE__, __VA_ARGS__);	\
		throw e_;	\
	}())

#endif
