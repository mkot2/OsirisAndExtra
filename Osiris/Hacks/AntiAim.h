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
