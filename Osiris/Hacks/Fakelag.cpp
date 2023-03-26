#include <random>

#include "AntiAim.h"
#include "EnginePrediction.h"
#include "Fakelag.h"
#include "EnginePrediction.h"
#include "Tickbase.h"
#include "AntiAim.h"

#include "../SDK/Entity.h"
#include "../SDK/Localplayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"

pcg_extras::seed_seq_from<std::random_device> seedSource3;
pcg32_c1024_fast rng3(seedSource3);

void Fakelag::run(const UserCmd* cmd, bool& sendPacket) noexcept
{
	const auto moving_flag{ AntiAim::get_moving_flag(cmd) };
	if (!localPlayer || !localPlayer->isAlive())
		return;

	const auto netChannel = interfaces->engine->getNetworkChannel();
	if (!netChannel)
		return;

	if (AntiAim::getDidShoot()) {
		sendPacket = true;
		return;
	}
	if (EnginePrediction::getVelocity().length2D() < 1)
		return;

	auto choked_packets = config->legitAntiAim.enabled || config->fakeAngle[static_cast<int>(moving_flag)].enabled ? std::uniform_int_distribution<int>(0, 2)(rng3) : 0;

	if (config->tickbase.disabledTickbase && config->tickbase.onshotFl && config->tickbase.readyFire) {
		choked_packets = -1;

		latest_choked_packets = choked_packets;

		if (interfaces->engine->isVoiceRecording())
			sendPacket = netChannel->chokedPackets >= 0;
		else
			sendPacket = netChannel->chokedPackets >= choked_packets;

		config->tickbase.readyFire = false;
		return;
	}

	if (config->tickbase.disabledTickbase && config->tickbase.onshotFl && config->tickbase.lastFireShiftTick > memory->globalVars->tickCount + choked_packets) {
		choked_packets = 0;

		latest_choked_packets = choked_packets;

		if (interfaces->engine->isVoiceRecording())
			sendPacket = netChannel->chokedPackets >= 0;
		else
			sendPacket = netChannel->chokedPackets >= choked_packets;

		config->tickbase.readyFire = false;
		return;
	}

	if (config->fakelag[static_cast<int>(moving_flag)].enabled) {
		const float speed = EnginePrediction::getVelocity().length2D() >= 15.0f ? EnginePrediction::getVelocity().length2D() : 0.0f;
		switch (config->fakelag[static_cast<int>(moving_flag)].mode) {
		case 0: //Static
			choked_packets = config->fakelag[static_cast<int>(moving_flag)].limit;
			break;
		case 1: //Adaptive
			choked_packets = std::clamp(static_cast<int>(std::ceilf(64 / (speed * memory->globalVars->intervalPerTick))), 1, config->fakelag[static_cast<int>(moving_flag)].limit);
			break;
		case 2: // Random
			choked_packets = std::uniform_int_distribution<int>(config->fakelag[static_cast<int>(moving_flag)].randomMinLimit, config->fakelag[static_cast<int>(moving_flag)].limit)(rng3);
			break;
		case 3: // rand() Random
			srand(static_cast<unsigned>(time(nullptr)));
			choked_packets = rand() % config->fakelag[static_cast<int>(moving_flag)].limit + config->fakelag[static_cast<int>(moving_flag)].randomMinLimit; // NOLINT(concurrency-mt-unsafe)
			break;
		default:
			break;
		}
	}

	choked_packets = std::clamp(choked_packets, 0, maxUserCmdProcessTicks - Tickbase::getTargetTickShift());

	latest_choked_packets = choked_packets;

	if (interfaces->engine->isVoiceRecording())
		sendPacket = netChannel->chokedPackets >= 0;
	else
		sendPacket = netChannel->chokedPackets >= choked_packets;
}