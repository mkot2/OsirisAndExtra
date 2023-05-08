#pragma once

struct UserCmd;

namespace Ragebot
{
	inline std::string latest_player{};
	void run(UserCmd*) noexcept;
	void updateInput() noexcept;
}