#pragma once

#include <string>

#include "VirtualMethod.h"

class Localize {
public:
	VIRTUAL_METHOD(const wchar_t*, find, 11, (const char* tokenName), (this, tokenName))
		VIRTUAL_METHOD(const wchar_t*, findSafe, 12, (const char* tokenName), (this, tokenName))
		VIRTUAL_METHOD(int, convertAnsiToUnicode, 15, (const char* ansi, wchar_t* unicode, int unicodeBufferSizeInBytes), (this, ansi, unicode, unicodeBufferSizeInBytes))
		VIRTUAL_METHOD(int, convertUnicodeToAnsi, 16, (const wchar_t* unicode, char* ansi, int ansiBufferSize), (this, unicode, ansi, ansiBufferSize))
		VIRTUAL_METHOD(const char*, findAsUTF8, 47, (const char* tokenName), (this, tokenName))

		std::string convertUnicodeToAnsi(const wchar_t* unicode) noexcept
	{
		char buffer[4096];
		convertUnicodeToAnsi(unicode, buffer, sizeof(buffer));
		return buffer;
	}
};
