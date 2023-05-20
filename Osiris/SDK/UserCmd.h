#pragma once

#include <cstdint>

#define CRCPP_BRANCHLESS
#define CRCPP_USE_CPP11
#include <CRC.h>

#include "Vector.h"

struct UserCmd {
	std::uint32_t getChecksum() const noexcept
	{
		static const auto table = CRC::CRC_32().MakeTable();

		std::uint32_t crc;

		crc = CRC::Calculate(&commandNumber, sizeof(commandNumber), table);
		crc = CRC::Calculate(&tickCount, sizeof(tickCount), table, crc);
		crc = CRC::Calculate(&viewangles, sizeof(viewangles), table, crc);
		crc = CRC::Calculate(&aimdirection, sizeof(aimdirection), table, crc);
		crc = CRC::Calculate(&forwardmove, sizeof(forwardmove), table, crc);
		crc = CRC::Calculate(&sidemove, sizeof(sidemove), table, crc);
		crc = CRC::Calculate(&upmove, sizeof(upmove), table, crc);
		crc = CRC::Calculate(&buttons, sizeof(buttons), table, crc);
		crc = CRC::Calculate(&impulse, sizeof(impulse), table, crc);
		crc = CRC::Calculate(&weaponselect, sizeof(weaponselect), table, crc);
		crc = CRC::Calculate(&weaponsubtype, sizeof(weaponsubtype), table, crc);
		crc = CRC::Calculate(&randomSeed, sizeof(randomSeed), table, crc);
		crc = CRC::Calculate(&mousedx, sizeof(mousedx), table, crc);
		crc = CRC::Calculate(&mousedy, sizeof(mousedy), table, crc);
		return crc;
	}

	enum {
		IN_ATTACK = 1 << 0,
		IN_JUMP = 1 << 1,
		IN_DUCK = 1 << 2,
		IN_FORWARD = 1 << 3,
		IN_BACK = 1 << 4,
		IN_USE = 1 << 5,
		IN_MOVELEFT = 1 << 9,
		IN_MOVERIGHT = 1 << 10,
		IN_ATTACK2 = 1 << 11,
		IN_SCORE = 1 << 16,
		IN_SPEED = 1 << 17,
		IN_ZOOM = 1 << 19,
		IN_BULLRUSH = 1 << 22
	};
	void* vmt;
	int commandNumber;
	int tickCount;
	Vector viewangles;
	Vector aimdirection;
	float forwardmove;
	float sidemove;
	float upmove;
	int buttons;
	char impulse;
	int weaponselect;
	int weaponsubtype;
	int randomSeed;
	short mousedx;
	short mousedy;
	bool hasbeenpredicted;
	PAD(24)
};

struct VerifiedUserCmd {
public:
	UserCmd cmd;
	std::uint32_t crc;
};

int getMaxUserCmdProcessTicks() noexcept;

#define maxUserCmdProcessTicks getMaxUserCmdProcessTicks()