#pragma once

#include <cassert>

class Entity;

class LocalPlayer {
public:
	void init(Entity** entity) noexcept
	{
		assert(!localEntity);
		localEntity = entity;
	}

	constexpr auto not_null() const noexcept
	{
		return localEntity != nullptr;
	}

	explicit constexpr operator bool() const noexcept
	{
		assert(localEntity);
		return *localEntity != nullptr;
	}

	constexpr auto operator->() const noexcept
	{
		assert(localEntity && *localEntity);
		return *localEntity;
	}

	[[nodiscard]] constexpr auto get() const noexcept
	{
		assert(localEntity && *localEntity);
		return *localEntity;
	}
private:
	Entity** localEntity = nullptr;
};

inline LocalPlayer localPlayer;
