#include <random>

#include "../Interfaces.h"

#include "AimbotFunctions.h"
#include "AntiAim.h"
#include "EnginePrediction.h"
#include "Misc.h"
#include "Tickbase.h"

#include "../SDK/Engine.h"
#include "../SDK/EngineTrace.h"
#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

static bool isShooting{ false };
static bool didShoot{ false };
static float lastShotTime{ 0.f };

bool updateLby(bool update = false) noexcept
{
	static float timer = 0.f;
	static bool lastValue = false;

	if (!update)
		return lastValue;

	if (!(localPlayer->flags() & 1) || !localPlayer->getAnimstate()) {
		lastValue = false;
		return false;
	}

	if (localPlayer->velocity().length2D() > 0.1f || fabsf(localPlayer->velocity().z) > 100.f)
		timer = memory->globalVars->serverTime() + 0.22f;

	if (timer < memory->globalVars->serverTime()) {
		timer = memory->globalVars->serverTime() + 1.1f;
		lastValue = true;
		return true;
	}
	lastValue = false;
	return false;
}

bool invert{ false };
bool autoDirection(Vector eyeAngle) noexcept
{
	constexpr float maxRange{ 8192.0f };

	Vector eye = eyeAngle;
	eye.x = 0.f;
	Vector eyeAnglesLeft45 = eye;
	Vector eyeAnglesRight45 = eye;
	eyeAnglesLeft45.y += 45.f;
	eyeAnglesRight45.y -= 45.f;


	eyeAnglesLeft45.toAngle();

	Vector viewAnglesLeft45 = {};
	viewAnglesLeft45 = viewAnglesLeft45.fromAngle(eyeAnglesLeft45) * maxRange;

	Vector viewAnglesRight45 = {};
	viewAnglesRight45 = viewAnglesRight45.fromAngle(eyeAnglesRight45) * maxRange;

	static Trace traceLeft45;
	static Trace traceRight45;

	Vector startPosition{ localPlayer->getEyePosition() };

	interfaces->engineTrace->traceRay({ startPosition, startPosition + viewAnglesLeft45 }, 0x4600400B, { localPlayer.get() }, traceLeft45);
	interfaces->engineTrace->traceRay({ startPosition, startPosition + viewAnglesRight45 }, 0x4600400B, { localPlayer.get() }, traceRight45);

	float distanceLeft45 = sqrtf(powf(startPosition.x - traceRight45.endpos.x, 2) + powf(startPosition.y - traceRight45.endpos.y, 2) + powf(startPosition.z - traceRight45.endpos.z, 2));
	float distanceRight45 = sqrtf(powf(startPosition.x - traceLeft45.endpos.x, 2) + powf(startPosition.y - traceLeft45.endpos.y, 2) + powf(startPosition.z - traceLeft45.endpos.z, 2));

	float mindistance = std::min(distanceLeft45, distanceRight45);

	if (distanceLeft45 == mindistance)
		return false;
	return true;
}

float random_float(const float a, const float b, const float multiplier)
{
	return a + std::uniform_real_distribution<float>(RAND_MAX)(PCG::generator) / static_cast<float>(RAND_MAX) * (b - a) * multiplier;
}

void AntiAim::rage(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
	const auto moving_flag{ get_moving_flag(cmd) };
	if ((cmd->viewangles.x == currentViewAngles.x || Tickbase::isShifting()) && config->rageAntiAim[static_cast<int>(moving_flag)].enabled) {
		switch (config->rageAntiAim[static_cast<int>(moving_flag)].pitch) {
		case 0: //None
			break;
		case 1: //Down
			cmd->viewangles.x = 89.f;
			break;
		case 2: //Zero
			cmd->viewangles.x = 0.f;
			break;
		case 3: //Up
			cmd->viewangles.x = -89.f;
			break;
		default:
			break;
		}
	}
	if (cmd->viewangles.y == currentViewAngles.y || Tickbase::isShifting()) {
		if (config->rageAntiAim[static_cast<int>(moving_flag)].yawBase != Yaw::off
			&& config->rageAntiAim[static_cast<int>(moving_flag)].enabled)   //AntiAim
		{
			float yaw = 0.f;
			static bool flipJitter = false;
			if (sendPacket && !AntiAim::getDidShoot())
				flipJitter ^= 1;
			if (config->rageAntiAim[static_cast<int>(moving_flag)].atTargets) {
				Vector localPlayerEyePosition = localPlayer->getEyePosition();
				const auto aimPunch = localPlayer->getAimPunch();
				float bestFov = 255.f;
				float yawAngle = 0.f;
				for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
					const auto entity{ interfaces->entityList->getEntity(i) };
					if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
						|| !entity->isOtherEnemy(localPlayer.get()) || entity->gunGameImmunity())
						continue;

					const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, entity->getAbsOrigin(), cmd->viewangles + aimPunch) };
					const auto fov{ angle.length2D() };
					if (fov < bestFov) {
						yawAngle = angle.y;
						bestFov = fov;
					}
				}
				yaw = yawAngle;
			}

			if (config->rageAntiAim[static_cast<int>(moving_flag)].yawBase != Yaw::spin)
				static_yaw = 1.f;

			switch (config->rageAntiAim[static_cast<int>(moving_flag)].yawBase) {
			case Yaw::paranoia:
				yaw += std::uniform_real_distribution<float>(static_cast<float>(-config->rageAntiAim[static_cast<int>(moving_flag)].paranoiaMin), static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].paranoiaMax))(PCG::generator) + 180;
				break;
			case Yaw::backward:
				yaw += 180.f;
				break;
			case Yaw::right:
				yaw += -90.f;
				break;
			case Yaw::left:
				yaw += 90.f;
				break;
			case Yaw::spin:
				static_yaw += static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].spinBase);
				yaw += static_yaw;
				break;
			case Yaw::off:
			default:
				break;
			}

			if (config->autoDirection.isActive()) {
				constexpr std::array positions = { -35.0f, 0.0f, 35.0f };
				std::array active = { false, false, false };
				const auto fwd = Vector::fromAngle2D(cmd->viewangles.y);
				const auto side = fwd.crossProduct(Vector::up());

				for (std::size_t i{}; i < positions.size(); i++) {
					const auto start = localPlayer->getEyePosition() + side * positions[i];
					const auto end = start + fwd * 100.0f;

					Trace trace{};
					interfaces->engineTrace->traceRay({ start, end }, 0x1 | 0x2, nullptr, trace);

					if (trace.fraction != 1.0f)
						active[i] = true;
				}

				if (active[0] && active[1] && !active[2]) {
					cmd->viewangles.y -= 90.f;
					auto_direction_yaw = -1;
				} else if (!active[0] && active[1] && active[2]) {
					cmd->viewangles.y += 90.f;
					auto_direction_yaw = 1;
				} else
					auto_direction_yaw = 0;
			}

			const bool forward = config->manualForward.isActive();
			const bool back = config->manualBackward.isActive();
			const bool right = config->manualRight.isActive();
			const bool left = config->manualLeft.isActive();
			const bool isManualSet = forward || back || right || left;

			if (back) {
				yaw = -180.f;
				if (left)
					yaw -= 45.f;
				else if (right)
					yaw += 45.f;
			} else if (left) {
				yaw = 90.f;
				if (back)
					yaw += 45.f;
				else if (forward)
					yaw -= 45.f;
			} else if (right) {
				yaw = -90.f;
				if (back)
					yaw -= 45.f;
				else if (forward)
					yaw += 45.f;
			} else if (forward) {
				yaw = 0.f;
			}

			switch (config->rageAntiAim[static_cast<int>(moving_flag)].yawModifier) {
			case 1: //Jitter
				yaw -= static_cast<float>(flipJitter
					? config->rageAntiAim[static_cast<int>(moving_flag)].jitterRange
					: -config->rageAntiAim[static_cast<int>(moving_flag)].jitterRange);
				break;
			default:
				break;
			}

			if (!isManualSet)
				yaw += static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].yawAdd);
			cmd->viewangles.y += yaw;
		}
		if (config->fakeAngle[static_cast<int>(moving_flag)].enabled && !Tickbase::isShifting()) //Fakeangle
		{
			bool is_invert_toggled = config->invert.isActive();
			static bool invert = true;
			if (config->fakeAngle[static_cast<int>(moving_flag)].peekMode != 3)
				invert = is_invert_toggled;
			auto roll_offset_angle = static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].rollOffset);
			auto pitch_angle = static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].rollPitch);
			if (config->rageAntiAim[static_cast<int>(moving_flag)].exploitPitchSwitch)
				pitch_angle = static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].exploitPitch) + 41225040.f * 129600.f;
			if (config->rageAntiAim[static_cast<int>(moving_flag)].roll && (std::abs(config->rageAntiAim[static_cast<int>(moving_flag)].rollAdd) + std::abs(config->rageAntiAim[static_cast<int>(moving_flag)].rollOffset) < 5 || !config->rageAntiAim[static_cast<int>(moving_flag)].rollAlt || !(cmd->buttons & UserCmd::IN_JUMP || localPlayer->velocity().length2D() > 50.f))) {
				cmd->viewangles.z = invert ? static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].rollAdd) : static_cast<float>(config->rageAntiAim[static_cast<int>(moving_flag)].rollAdd) * -1.f;
				if (sendPacket) {
					cmd->viewangles.z += invert ? pitch_angle : pitch_angle * -1.f;
					if (pitch_angle > 0.01f || pitch_angle < -0.01f)
						cmd->viewangles.x = pitch_angle;
				}

				if (cmd->commandNumber % 2 == 0 || memory->globalVars->tickCount % 3 == 0)
					cmd->viewangles.z += invert ? roll_offset_angle : roll_offset_angle * -1.f;
			} else
				cmd->viewangles.z = 0.f;
			if (const auto game_rules{ *memory->gameRules }; game_rules)
				if (getGameMode() != GameMode::Competitive && game_rules->isValveDS())
					return;
			if (config->tickbase.disabledTickbase && config->tickbase.onshotFl && config->tickbase.lastFireShiftTick > memory->globalVars->tickCount)
				return;
			float left_desync_angle = static_cast<float>(config->fakeAngle[static_cast<int>(moving_flag)].leftLimit) * 2.f;
			float right_desync_angle = static_cast<float>(config->fakeAngle[static_cast<int>(moving_flag)].rightLimit) * -2.f;

			switch (config->fakeAngle[static_cast<int>(moving_flag)].peekMode) {
			case 0:
				break;
			case 1: // Peek real
				if (!is_invert_toggled)
					invert = !autoDirection(cmd->viewangles);
				else
					invert = autoDirection(cmd->viewangles);
				break;
			case 2: // Peek fake
				if (is_invert_toggled)
					invert = !autoDirection(cmd->viewangles);
				else
					invert = autoDirection(cmd->viewangles);
				break;
			case 3: // Jitter
				if (sendPacket)
					invert = !invert;
				break;
			case 4: // Switch
				if (sendPacket && localPlayer->velocity().length2D() > 5.f)
					invert = !invert;
				break;
			default:
				break;
			}

			switch (config->fakeAngle[static_cast<int>(moving_flag)].lbyMode) {
			case 0: // Normal(sidemove)
				if (fabsf(cmd->sidemove) < 5.0f) {
					if (cmd->buttons & UserCmd::IN_DUCK)
						cmd->sidemove = cmd->tickCount & 1 ? 3.25f : -3.25f;
					else
						cmd->sidemove = cmd->tickCount & 1 ? 1.1f : -1.1f;
				}
				break;
			case 1: // Opposite (Lby break)
				if (updateLby()) {
					cmd->viewangles.y += !invert ? left_desync_angle : right_desync_angle;
					sendPacket = false;
					return;
				}
				break;
			case 2: //Sway (flip every lby update)
			{
				static bool flip = false;
				if (updateLby()) {
					cmd->viewangles.y += !flip ? left_desync_angle : right_desync_angle;
					sendPacket = false;
					flip = !flip;
					return;
				}
				if (!sendPacket)
					cmd->viewangles.y += flip ? left_desync_angle : right_desync_angle;
				break;
			}
			default:
				break;
			}

			if (sendPacket)
				return;

			cmd->viewangles.y += invert ? left_desync_angle : right_desync_angle;
		}
	}
}

void AntiAim::legit(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
	if (cmd->viewangles.y == currentViewAngles.y && !Tickbase::isShifting()) {
		invert = config->legitAntiAim.invert.isActive();
		const float desyncAngle = localPlayer->getMaxDesyncAngle() * 2.f;
		if (updateLby() && config->legitAntiAim.extend) {
			cmd->viewangles.y += !invert ? desyncAngle : -desyncAngle;
			sendPacket = false;
			return;
		}

		if (fabsf(cmd->sidemove) < 5.0f && !config->legitAntiAim.extend) {
			if (cmd->buttons & UserCmd::IN_DUCK)
				cmd->sidemove = cmd->tickCount & 1 ? 3.25f : -3.25f;
			else
				cmd->sidemove = cmd->tickCount & 1 ? 1.1f : -1.1f;
		}

		if (sendPacket)
			return;

		cmd->viewangles.y += invert ? desyncAngle : -desyncAngle;
	}
}

void AntiAim::updateInput() noexcept
{
	config->legitAntiAim.invert.handleToggle();
	config->invert.handleToggle();

	config->autoDirection.handleToggle();
	config->manualForward.handleToggle();
	config->manualBackward.handleToggle();
	config->manualRight.handleToggle();
	config->manualLeft.handleToggle();
}

void AntiAim::run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
	const auto moving_flag{ get_moving_flag(cmd) };
	if (cmd->buttons & (UserCmd::IN_USE))
		return;

	if (localPlayer->moveType() == MoveType::LADDER || localPlayer->moveType() == MoveType::NOCLIP)
		return;

	if (config->legitAntiAim.enabled)
		legit(cmd, previousViewAngles, currentViewAngles, sendPacket);
	else if (config->rageAntiAim[static_cast<int>(moving_flag)].enabled || config->fakeAngle[static_cast<int>(moving_flag)].enabled)
		rage(cmd, previousViewAngles, currentViewAngles, sendPacket);
}

bool AntiAim::canRun(UserCmd* cmd) noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return false;

	updateLby(true); //Update lby timer

	if (config->disableInFreezetime && (!*memory->gameRules || (*memory->gameRules)->freezePeriod()))
		return false;

	if (localPlayer->flags() & (1 << 6))
		return false;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->clip())
		return true;

	if (activeWeapon->isThrowing())
		return false;

	if (activeWeapon->isGrenade() && cmd->buttons & (UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2))
		return false;

	if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto() || localPlayer->waitForNoAttack())
		return true;

	if (localPlayer->nextAttack() > memory->globalVars->serverTime())
		return true;

	if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
		return true;

	if (activeWeapon->nextSecondaryAttack() > memory->globalVars->serverTime())
		return true;

	if (localPlayer->nextAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
		return false;

	if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
		return false;

	if (activeWeapon->isKnife()) {
		if (activeWeapon->nextSecondaryAttack() <= memory->globalVars->serverTime() && cmd->buttons & (UserCmd::IN_ATTACK2))
			return false;
	}

	if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver && activeWeapon->readyTime() <= memory->globalVars->serverTime() && cmd->buttons & (UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2))
		return false;

	const auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
	if (!weaponIndex)
		return true;

	return true;
}

AntiAim::moving_flag AntiAim::get_moving_flag(const UserCmd* cmd) noexcept
{
	if ((EnginePrediction::getFlags() & 3) == 2)
		return latest_moving_flag = duck_jumping;
	if (EnginePrediction::getFlags() & 2)
		return latest_moving_flag = ducking;
	if (!localPlayer->getAnimstate()->onGround)
		return latest_moving_flag = jumping;
	if (localPlayer->velocity().length2D() > 0.f) {
		if (config->misc.slowwalkKey.isActive())
			return latest_moving_flag = slow_walking;
		return latest_moving_flag = moving;
	}
	if (config->misc.fakeduckKey.isActive())
		return latest_moving_flag = fake_ducking;
	return latest_moving_flag = freestanding;
}

float AntiAim::getLastShotTime()
{
	return lastShotTime;
}

bool AntiAim::getIsShooting()
{
	return isShooting;
}

bool AntiAim::getDidShoot()
{
	return didShoot;
}

void AntiAim::setLastShotTime(float shotTime)
{
	lastShotTime = shotTime;
}

void AntiAim::setIsShooting(bool shooting)
{
	isShooting = shooting;
}

void AntiAim::setDidShoot(bool shot)
{
	didShoot = shot;
}