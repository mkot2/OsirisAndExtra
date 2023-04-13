#pragma once
#include <cstddef>
#include <memory>
#include <Windows.h>

std::size_t calculateVmtLength(std::uintptr_t* vmt) noexcept;