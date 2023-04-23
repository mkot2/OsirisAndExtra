#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "AimbotFunctions.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Ragebot.h"
#include "EnginePrediction.h"
#include "resolver.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Utils.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/ModelInfo.h"

static bool keyPressed = false;

void Ragebot::updateInput() noexcept
{
	config->ragebotKey.handleToggle();
	config->minDamageOverrideKey.handleToggle();
}

void runRagebot(UserCmd* cmd, Entity* entity, matrix3x4* matrix, const Ragebot::Enemies target, const std::array<bool,
	Max> hitbox, Entity* activeWeapon, const int weaponIndex, const Vector localPlayerEyePosition, const Vector aimPunch, const int headMultiPoint, const int bodyMultiPoint, const int minDamage, float& damageDiff, Vector& bestAngle, Vector& bestTarget, int& bestIndex, float& bestSimulationTime) noexcept
{
	Ragebot::latest_player = entity->getPlayerName();
	const auto& cfg = config->ragebot;
	int hitboxId = 0;

	damageDiff = FLT_MAX;

	const Model* model = entity->getModel();
	if (!model)
		return;

	StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
	if (!hdr)
		return;

	StudioHitboxSet* set = hdr->getHitboxSet(0);
	if (!set)
		return;

	for (size_t i = 0; i < hitbox.size(); i++) {
		if (!hitbox[i])
			continue;

		StudioBbox* hitbox = set->getHitbox(i);
		if (!hitbox)
			continue;

		for (const auto& bonePosition : AimbotFunction::multiPoint(entity, matrix, hitbox, localPlayerEyePosition, i, headMultiPoint, bodyMultiPoint)) {
			const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch) };
			const auto fov{ angle.length2D() };
			const auto extrapolatedPoint{ bonePosition + entity->velocity() * memory->globalVars->intervalPerTick };
			

			if (fov > cfg[weaponIndex].fov)
				continue;

			if (!cfg[weaponIndex].ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, extrapolatedPoint, 1))
				continue;

			float damage = AimbotFunction::getScanDamage(entity, extrapolatedPoint, activeWeapon->getWeaponData(), minDamage, cfg[weaponIndex].friendlyFire);
			damage = std::clamp(damage, 0.0f, static_cast<float>(entity->maxHealth()));
			if (damage <= 0.05f)
				continue;

			if (!entity->isVisible(extrapolatedPoint) && (cfg[weaponIndex].visibleOnly || !static_cast<bool>(damage)))
				continue;

			if (cfg[weaponIndex].scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped())
				return;

			if (cfg[weaponIndex].autoStop && !(cmd->buttons & UserCmd::IN_JUMP)) {
				const auto weaponData = activeWeapon->getWeaponData();
				const auto velocity = EnginePrediction::getVelocity();
				const auto speed = velocity.length2D();
				auto speedcoeffi = 0.33f;
				if (cfg[weaponIndex].accuracyBoost != 0.f)
					speedcoeffi = 1.f - cfg[weaponIndex].accuracyBoost;
				Vector direction = velocity.toAngle();
				direction.y = cmd->viewangles.y - direction.y;
				const auto negatedDirection = Vector::fromAngle(direction) * speed * -1;
				if (speed >= (localPlayer->isScoped() ? weaponData->maxSpeedAlt - 1 : weaponData->maxSpeed - 1) * speedcoeffi) {
					cmd->forwardmove = negatedDirection.x;
					cmd->sidemove = negatedDirection.y;
					if (cfg[weaponIndex].duckStop)
						cmd->buttons |= UserCmd::IN_DUCK;
				} else {
					cmd->forwardmove = cmd->forwardmove * speedcoeffi;
					cmd->sidemove = cmd->sidemove * speedcoeffi;//slow?
					if (cfg[weaponIndex].duckStop)
						cmd->buttons &= ~UserCmd::IN_ATTACK;
					if (cfg[weaponIndex].fullStop) {
						cmd->forwardmove = 0;
						cmd->sidemove = 0;
					}
					cmd->upmove = 0;//jump/land/duck/unduck?
				}
			}

			if (cfg[weaponIndex].autoScope && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && !activeWeapon->zoomLevel() && localPlayer->flags() & 1 && !(cmd->buttons & UserCmd::IN_JUMP))
				cmd->buttons |= UserCmd::IN_ZOOM;

			if (std::fabsf(static_cast<float>(target.health) - damage) <= damageDiff) {
				bestAngle = angle;
				damageDiff = std::fabsf(static_cast<float>(target.health) - damage);
				bestTarget = bonePosition;
				hitboxId = i;
			}
		}
	}

	if (bestTarget.notNull()) {
		if (!AimbotFunction::hitChance(localPlayer.get(), entity, set, matrix, activeWeapon, bestAngle, cmd, cfg[weaponIndex].hitChance)) {
			bestTarget = Vector{ };
			bestAngle = Vector{ };
			damageDiff = FLT_MAX;
		}
	}
}

void Ragebot::run(UserCmd* cmd) noexcept
{
	const auto& cfg = config->ragebot;

	if (!config->ragebotKey.isActive())
		return;

	if (!localPlayer || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
		return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->clip())
		return;

	if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
		return;

	auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
	if (!weaponIndex)
		return;

	auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
	if (!cfg[weaponIndex].enabled)
		weaponIndex = weaponClass;

	if (!cfg[weaponIndex].enabled)
		weaponIndex = 0;

	if (!cfg[weaponIndex].betweenShots && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
		return;

	if (!cfg[weaponIndex].ignoreFlash && localPlayer->isFlashed())
		return;

	if (!(cfg[weaponIndex].enabled && (cmd->buttons & UserCmd::IN_ATTACK || cfg[weaponIndex].autoShot || cfg[weaponIndex].aimlock)))
		return;

	float damageDiff = FLT_MAX;
	Vector bestTarget{ };
	Vector bestAngle{ };
	int bestIndex{ -1 };
	float bestSimulationTime = 0;
	const auto localPlayerEyePosition = localPlayer->getEyePosition();
	const auto aimPunch = localPlayer->getAimPunch();
	static auto frameRate = 1.0f;
	frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;

	std::array<bool, Max> hitbox{ false };

	// Head
	hitbox[Head] = (cfg[weaponIndex].hitboxes & 1 << 0) == 1 << 0;
	// Chest
	hitbox[UpperChest] = (cfg[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
	hitbox[Thorax] = (cfg[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
	hitbox[LowerChest] = (cfg[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
	// Stomach
	hitbox[Belly] = (cfg[weaponIndex].hitboxes & 1 << 2) == 1 << 2;
	hitbox[Pelvis] = (cfg[weaponIndex].hitboxes & 1 << 2) == 1 << 2;
	// Limbs will only be scanned when we can afford it, unless they are the only selected hitboxes
	if ((static_cast<int>(1 / frameRate) > 1 / memory->globalVars->intervalPerTick && !((cfg[weaponIndex].hitboxes & 1 << 2) == (cfg[weaponIndex].hitboxes & 1 << 0) == (cfg[weaponIndex].hitboxes & 1 << 1) == 0)) || !config->optimizations.lowPerformanceMode) {
		// Arms
		hitbox[RightUpperArm] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		hitbox[RightForearm] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		hitbox[RightHand] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftUpperArm] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftForearm] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftHand] = (cfg[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
		// Legs
		hitbox[RightCalf] = (cfg[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
		hitbox[RightThigh] = (cfg[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
		hitbox[LeftCalf] = (cfg[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
		hitbox[LeftThigh] = (cfg[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
		// Feet
		hitbox[RightFoot] = (cfg[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
		hitbox[LeftFoot] = (cfg[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
	}


	std::vector<Enemies> enemies;
	const auto& localPlayerOrigin{ localPlayer->getAbsOrigin() };
	for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
		const auto player = Animations::getPlayer(i);
		if (!player.gotMatrix)
			continue;

		const auto entity{ interfaces->entityList->getEntity(i) };
		if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
			|| !entity->isOtherEnemy(localPlayer.get()) && !cfg[weaponIndex].friendlyFire || entity->gunGameImmunity())
			continue;

		const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, player.matrix[8].origin(), cmd->viewangles + aimPunch) };
		const auto& origin{ entity->getAbsOrigin() };
		const auto fov{ angle.length2D() }; //fov
		const auto health{ entity->health() }; //health
		const auto distance{ localPlayerOrigin.distTo(origin) }; //distance
		enemies.emplace_back(i, health, distance, fov);
	}

	if (enemies.empty())
		return;

	switch (cfg[weaponIndex].priority) {
	case 0:
		std::ranges::sort(enemies, healthSort);
		break;
	case 1:
		std::ranges::sort(enemies, distanceSort);
		break;
	case 2:
		std::ranges::sort(enemies, fovSort);
		break;
	default:
		break;
	}

	for (const auto& target : enemies) {
		auto entity{ interfaces->entityList->getEntity(target.id) };
		auto player = Animations::getPlayer(target.id);
		int minDamage = std::clamp(std::clamp(config->minDamageOverrideKey.isActive() ? cfg[weaponIndex].minDamageOverride : cfg[weaponIndex].minDamage, 0, target.health), 0, activeWeapon->getWeaponData()->damage);

		matrix3x4* backupBoneCache = entity->getBoneCache().memory;
		Vector backupMins = entity->getCollideable()->obbMins();
		Vector backupMaxs = entity->getCollideable()->obbMaxs();
		Vector backupOrigin = entity->getAbsOrigin();
		Vector backupAbsAngle = entity->getAbsAngle();

		for (int cycle = 0; cycle < 2; cycle++) {
			float currentSimulationTime = -1.0f;

			if (config->backtrack.enabled) {
				const auto records = Animations::getBacktrackRecords(entity->index());
				if (!records || records->empty())
					continue;

				int bestTick = -1;
				if (cycle == 0) {
					for (size_t i = 0; i < records->size(); i++) {
						if (Backtrack::valid(records->at(i).simulationTime)) {
							bestTick = static_cast<int>(i);
							break;
						}
					}
				} else {
					for (int i = static_cast<int>(records->size() - 1U); i >= 0; i--) {
						if (Backtrack::valid(records->at(i).simulationTime)) {
							bestTick = i;
							break;
						}
					}
				}

				if (bestTick <= -1)
					continue;

				memcpy(entity->getBoneCache().memory, records->at(bestTick).matrix, std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
				memory->setAbsOrigin(entity, records->at(bestTick).origin);
				memory->setAbsAngle(entity, Vector{ 0.f, records->at(bestTick).absAngle.y, 0.f });
				memory->setCollisionBounds(entity->getCollideable(), records->at(bestTick).mins, records->at(bestTick).maxs);

				currentSimulationTime = records->at(bestTick).simulationTime;
			} else {
				//We skip backtrack
				if (cycle == 1)
					continue;

				memcpy(entity->getBoneCache().memory, player.matrix.data(), std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
				memory->setAbsOrigin(entity, player.origin);
				memory->setAbsAngle(entity, Vector{ 0.f, player.absAngle.y, 0.f });
				memory->setCollisionBounds(entity->getCollideable(), player.mins, player.maxs);

				currentSimulationTime = player.simulationTime;
			}

			runRagebot(cmd, entity, entity->getBoneCache().memory, target, hitbox, activeWeapon, weaponIndex, localPlayerEyePosition, aimPunch, cfg[weaponIndex].headMultiPoint, cfg[weaponIndex].bodyMultiPoint, minDamage, damageDiff, bestAngle, bestTarget, bestIndex, bestSimulationTime);
			resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			if (bestTarget.notNull()) {
				bestSimulationTime = currentSimulationTime;
				bestIndex = target.id;
				break;
			}
		}
		if (bestTarget.notNull())
			break;
	}

	if (bestTarget.notNull()) {
		static Vector lastAngles{ cmd->viewangles };
		static int lastCommand{ };

		if (lastCommand == cmd->commandNumber - 1 && lastAngles.notNull() && cfg[weaponIndex].silent)
			cmd->viewangles = lastAngles;

		auto angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, bestTarget, cmd->viewangles + aimPunch);
		bool clamped{ false };

		if (std::abs(angle.x) > config->misc.maxAngleDelta || std::abs(angle.y) > config->misc.maxAngleDelta) {
			angle.x = std::clamp(angle.x, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);
			angle.y = std::clamp(angle.y, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);
			clamped = true;
		}

		if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime()) {
			config->tickbase.readyFire = true;
			if (getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 9 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 10 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 11 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 12 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 13 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 14 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 29 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 32 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 33 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 34 ||
				getWeaponIndex(activeWeapon->itemDefinitionIndex2()) != 39)
				config->tickbase.lastFireShiftTick = memory->globalVars->tickCount + config->tickbase.onshotFlAmount;
			else
				config->tickbase.lastFireShiftTick = memory->globalVars->tickCount + config->tickbase.onshotFlAmount + 1;
			cmd->viewangles += angle;
			if (!cfg[weaponIndex].silent)
				interfaces->engine->setViewAngles(cmd->viewangles);

			if (cfg[weaponIndex].autoShot && !clamped)
				cmd->buttons |= UserCmd::IN_ATTACK;
		}

		if (clamped)
			cmd->buttons &= ~UserCmd::IN_ATTACK;

		if (cmd->buttons & UserCmd::IN_ATTACK) {
			cmd->tickCount = timeToTicks(bestSimulationTime + Backtrack::getLerp());
			resolver::save_record(bestIndex, bestSimulationTime);
		}

		if (clamped) lastAngles = cmd->viewangles;
		else lastAngles = Vector{ };

		lastCommand = cmd->commandNumber;
	}
}
