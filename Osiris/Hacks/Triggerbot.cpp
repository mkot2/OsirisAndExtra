#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "Animations.h"
#include "AimbotFunctions.h"
#include "Backtrack.h"
#include "Triggerbot.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ModelInfo.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponId.h"

void Triggerbot::run(UserCmd* cmd) noexcept
{
	if (!config->triggerbotKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive() || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
		return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->clip() || activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
		return;

	if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
		return;

	auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
	if (!weaponIndex)
		return;

	if (!config->triggerbot[weaponIndex].enabled)
		weaponIndex = getWeaponClass(activeWeapon->itemDefinitionIndex2());

	if (!config->triggerbot[weaponIndex].enabled)
		weaponIndex = 0;

	const auto& cfg = config->triggerbot[weaponIndex];

	if (!cfg.enabled)
		return;

	static auto lastTime = 0.0f;
	static auto lastContact = 0.0f;

	const auto now = memory->globalVars->realtime;

	if (now - lastTime < cfg.shotDelay / 1000.0f)
		return;

	if (!cfg.ignoreFlash && localPlayer->isFlashed())
		return;

	if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped())
		return;

	const auto weaponData = activeWeapon->getWeaponData();
	if (!weaponData)
		return;

	const auto aimPunch = localPlayer->getAimPunch();

	const auto startPos = localPlayer->getEyePosition();
	const auto endPos = startPos + Vector::fromAngle(cmd->viewangles + localPlayer->getAimPunch()) * weaponData->range;

	if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(startPos, endPos, 1))
		return;

	lastTime = now;

	std::array<bool, Max> hitbox{ false };
	{
		// Head
		hitbox[Head] = (cfg.hitboxes & 1 << 0) == 1 << 0;
		// Chest
		hitbox[UpperChest] = (cfg.hitboxes & 1 << 1) == 1 << 1;
		hitbox[Thorax] = (cfg.hitboxes & 1 << 1) == 1 << 1;
		hitbox[LowerChest] = (cfg.hitboxes & 1 << 1) == 1 << 1;
		// Stomach
		hitbox[Belly] = (cfg.hitboxes & 1 << 2) == 1 << 2;
		hitbox[Pelvis] = (cfg.hitboxes & 1 << 2) == 1 << 2;
		// Arms
		hitbox[RightUpperArm] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		hitbox[RightForearm] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		hitbox[RightHand] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftUpperArm] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftForearm] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		hitbox[LeftHand] = (cfg.hitboxes & 1 << 3) == 1 << 3;
		// Legs
		hitbox[RightCalf] = (cfg.hitboxes & 1 << 4) == 1 << 4;
		hitbox[RightThigh] = (cfg.hitboxes & 1 << 4) == 1 << 4;
		hitbox[LeftCalf] = (cfg.hitboxes & 1 << 4) == 1 << 4;
		hitbox[LeftThigh] = (cfg.hitboxes & 1 << 4) == 1 << 4;
		// Feet
		hitbox[RightFoot] = (cfg.hitboxes & 1 << 4) == 1 << 5;
		hitbox[LeftFoot] = (cfg.hitboxes & 1 << 4) == 1 << 5;
	}

	for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
		auto entity = interfaces->entityList->getEntity(i);
		if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
			|| !entity->isOtherEnemy(localPlayer.get()) && !cfg.friendlyFire || entity->gunGameImmunity())
			continue;

		auto player = Animations::getPlayer(i);
		if (!player.gotMatrix)
			continue;

		matrix3x4* backupBoneCache = entity->getBoneCache().memory;
		Vector backupMins = entity->getCollideable()->obbMins();
		Vector backupMaxs = entity->getCollideable()->obbMaxs();
		Vector backupOrigin = entity->getAbsOrigin();
		Vector backupAbsAngle = entity->getAbsAngle();

		std::memcpy(entity->getBoneCache().memory, player.matrix.data(), std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
		memory->setAbsOrigin(entity, player.origin);
		memory->setAbsAngle(entity, Vector{ 0.f, player.absAngle.y, 0.f });
		entity->getCollideable()->setCollisionBounds(player.mins, player.maxs);

		const Model* model = entity->getModel();
		if (!model) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
		if (!hdr) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		StudioHitboxSet* set = hdr->getHitboxSet(0);
		if (!set) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		for (int j = 0; j < Max; j++) {
			if (!hitbox[j])
				continue;

			if (AimbotFunction::hitboxIntersection(player.matrix.data(), j, set, startPos, endPos)) {
				Trace trace;
				interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);
				if (trace.entity != entity) {
					Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
					break;
				}

				float damage = (activeWeapon->itemDefinitionIndex2() != WeaponId::Taser ? HitGroup::getDamageMultiplier(trace.hitgroup, weaponData, trace.entity->hasHeavyArmor(), static_cast<int>(trace.entity->getTeamNumber())) : 1.0f) * weaponData->damage * std::pow(weaponData->rangeModifier, trace.fraction * weaponData->range / 500.0f);

				if (float armorRatio{ weaponData->armorRatio / 2.0f }; activeWeapon->itemDefinitionIndex2() != WeaponId::Taser && HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet(), trace.entity->armor(), trace.entity->hasHeavyArmor()))
					AimbotFunction::calculateArmorDamage(armorRatio, trace.entity->armor(), trace.entity->hasHeavyArmor(), damage);

				if (damage >= (cfg.killshot ? trace.entity->health() : cfg.minDamage) &&
					AimbotFunction::hitChance(localPlayer.get(), entity, set, player.matrix.data(), activeWeapon, AimbotFunction::calculateRelativeAngle(startPos, trace.endpos, cmd->viewangles + aimPunch), cmd, cfg.hitChance, j)) {
					cmd->buttons |= UserCmd::IN_ATTACK;
					cmd->tickCount = Helpers::timeToTicks(player.simulationTime + Backtrack::getLerp());
					lastTime = 0.0f;
					lastContact = now;
				}
				Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
				break;
			}
		}

		Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);

		if (!config->backtrack.enabled)
			continue;

		auto records = Animations::getBacktrackRecords(entity->index());
		if (!records || records->empty())
			continue;

		int bestTick = -1;
		auto bestFov{ 255.f };

		for (int i = static_cast<int>(records->size() - 1); i >= 0; i--) {
			if (!Backtrack::valid(records->at(i).simulationTime))
				continue;

			const auto angle = AimbotFunction::calculateRelativeAngle(startPos, records->at(i).origin, cmd->viewangles + aimPunch);
			const auto fov = std::hypot(angle.x, angle.y);
			if (fov < bestFov) {
				bestFov = fov;
				bestTick = i;
			}
		}

		if (bestTick <= -1)
			continue;

		auto record = records->at(bestTick);

		backupBoneCache = entity->getBoneCache().memory;
		backupMins = entity->getCollideable()->obbMins();
		backupMaxs = entity->getCollideable()->obbMaxs();
		backupOrigin = entity->getAbsOrigin();
		backupAbsAngle = entity->getAbsAngle();

		std::memcpy(entity->getBoneCache().memory, record.matrix, std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
		memory->setAbsOrigin(entity, record.origin);
		memory->setAbsAngle(entity, Vector{ 0.f, record.absAngle.y, 0.f });
		entity->getCollideable()->setCollisionBounds(record.mins, record.maxs);

		model = entity->getModel();
		if (!model) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		hdr = interfaces->modelInfo->getStudioModel(model);
		if (!hdr) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		set = hdr->getHitboxSet(0);
		if (!set) {
			Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
			continue;
		}

		for (int j = 0; j < Max; j++) {
			if (!hitbox[j])
				continue;

			if (AimbotFunction::hitboxIntersection(record.matrix, j, set, startPos, endPos)) {
				Trace trace;
				interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);
				if (trace.entity != entity) {
					Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
					break;
				}

				float damage = (activeWeapon->itemDefinitionIndex2() != WeaponId::Taser ? HitGroup::getDamageMultiplier(trace.hitgroup, weaponData, trace.entity->hasHeavyArmor(), static_cast<int>(trace.entity->getTeamNumber())) : 1.0f) * weaponData->damage * std::pow(weaponData->rangeModifier, trace.fraction * weaponData->range / 500.0f);

				if (float armorRatio{ weaponData->armorRatio / 2.0f }; activeWeapon->itemDefinitionIndex2() != WeaponId::Taser && HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet(), trace.entity->armor(), trace.entity->hasHeavyArmor()))
					AimbotFunction::calculateArmorDamage(armorRatio, trace.entity->armor(), trace.entity->hasHeavyArmor(), damage);

				if (damage >= (cfg.killshot ? trace.entity->health() : cfg.minDamage) &&
					AimbotFunction::hitChance(localPlayer.get(), entity, set, record.matrix, activeWeapon, AimbotFunction::calculateRelativeAngle(startPos, trace.endpos, cmd->viewangles + aimPunch), cmd, cfg.hitChance, j)) {
					cmd->buttons |= UserCmd::IN_ATTACK;
					cmd->tickCount = Helpers::timeToTicks(record.simulationTime + Backtrack::getLerp());
					lastTime = 0.0f;
					lastContact = now;
				}

				Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
				break;
			}
		}
		Animations::resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
	}
}

void Triggerbot::updateInput() noexcept
{
	config->triggerbotKey.handleToggle();
}
