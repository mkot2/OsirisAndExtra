#pragma once

#include "VirtualMethod.h"

class BaseFileSystem {
public:
	VIRTUAL_METHOD(int, read, 0, (void* output, int size, void* handle), (this, output, size, handle))
		VIRTUAL_METHOD(void*, open, 2, (const char* fileName, const char* options, const char* pathID), (this, fileName, options, pathID))
		VIRTUAL_METHOD(void, close, 3, (void* handle), (this, handle))
		VIRTUAL_METHOD(unsigned int, size, 7, (void* handle), (this, handle))
};
