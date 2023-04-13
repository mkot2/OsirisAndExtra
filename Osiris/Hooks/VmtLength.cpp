#include "VmtLength.h"

std::size_t calculateVmtLength(std::uintptr_t* vmt) noexcept
{
	std::size_t length = 0;
	MEMORY_BASIC_INFORMATION memoryInfo;
	while (VirtualQuery(LPCVOID(vmt[length]), &memoryInfo, sizeof(memoryInfo)) && memoryInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
		length++;
	return length;
}
