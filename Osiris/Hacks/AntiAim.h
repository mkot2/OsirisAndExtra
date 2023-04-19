#pragma once

#include "../ConfigStructs.h"
struct Vector;
struct UserCmd;

namespace AntiAim
{
	inline float static_yaw{};

	enum moving_flag {
		freestanding,
		moving,
		jumping,
		ducking,
		duck_jumping,
		slow_walking,
		fake_ducking
	};

	inline const char* moving_flag_text[]
	{
		xorstr_("Freestanding"),
		xorstr_("Moving"),
		xorstr_("Jumping"),
		xorstr_("Ducking"),
		xorstr_("Duck-jumping"),
		xorstr_("Slow-walking"),
		xorstr_("Fake-ducking")
	};

	inline const char* peek_mode_text[]
	{
		xorstr_("Off"),
		xorstr_("Peek Real"),
		xorstr_("Peek Fake"),
		xorstr_("Jitter"),
		xorstr_("Switch")
	};

	inline const char* lby_mode_text[]
	{
		xorstr_("Normal"),
		xorstr_("Opposite"),
		xorstr_("Sway")
	};

	void rage(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept;
	void legit(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept;

	void run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept;
	void updateInput() noexcept;
	bool canRun(UserCmd* cmd) noexcept;

	inline int auto_direction_yaw{};

	inline moving_flag latest_moving_flag{};

	moving_flag get_moving_flag(const UserCmd* cmd) noexcept;

	float getLastShotTime();
	bool getIsShooting();
	bool getDidShoot();
	void setLastShotTime(float shotTime);
	void setIsShooting(bool shooting);
	void setDidShoot(bool shot);
}
