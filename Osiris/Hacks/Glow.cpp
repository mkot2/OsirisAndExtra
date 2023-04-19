#include <array>

#include "../nlohmann/json.hpp"

#include "../Config.h"
#include "../Helpers.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../imguiCustom.h"

#include "Glow.h"

#include "../SDK/Entity.h"
#include "../SDK/ClientClass.h"
#include "../SDK/GlowObjectManager.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Utils.h"
#include "../SDK/Sound.h"

static std::vector<std::pair<int, int>> customGlowEntities;

void Glow::render() noexcept
{
	if (!localPlayer)
		return;

	auto& glow = config->glow;

	clearCustomObjects();

	if (!config->glowKey.isActive())
		return;

	const auto highestEntityIndex = interfaces->entityList->getHighestEntityIndex();
	for (int i = interfaces->engine->getMaxClients() + 1; i <= highestEntityIndex; ++i) {
		const auto entity = interfaces->entityList->getEntity(i);
		if (!entity || entity->isDormant())
			continue;

		switch (entity->getClientClass()->classId) {
		case ClassId::EconEntity:
		case ClassId::BaseCSGrenadeProjectile:
		case ClassId::BreachChargeProjectile:
		case ClassId::BumpMineProjectile:
		case ClassId::DecoyProjectile:
		case ClassId::MolotovProjectile:
		case ClassId::SensorGrenadeProjectile:
		case ClassId::SmokeGrenadeProjectile:
		case ClassId::SnowballProjectile:
		case ClassId::Hostage:
			if (!memory->glowObjectManager->hasGlowEffect(entity)) {
				if (auto index{ memory->glowObjectManager->registerGlowObject(entity) }; index != -1)
					customGlowEntities.emplace_back(i, index);
			}
			break;
		default:
			break;
		}
	}

	for (int i = 0; i < memory->glowObjectManager->glowObjectDefinitions.size; i++) {
		GlowObjectDefinition& glowobject = memory->glowObjectManager->glowObjectDefinitions[i];

		auto entity = glowobject.entity;

		if (glowobject.isUnused() || !entity || entity->isDormant())
			continue;

		auto applyGlow = [&glowobject](const Config::GlowItem& glow, int health = 0) noexcept {
			if (glow.enabled) {
				glowobject.renderWhenOccluded = true;
				glowobject.glowAlpha = glow.color[3];
				glowobject.glowStyle = glow.style;
				glowobject.glowAlphaMax = 0.6f;
				if (glow.healthBased && health) {
					Helpers::healthColor(std::clamp(health / 100.0f, 0.0f, 1.0f), glowobject.glowColor.x, glowobject.glowColor.y, glowobject.glowColor.z);
				} else if (glow.rainbow) {
					const auto [r, g, b] { rainbowColor(glow.rainbowSpeed) };
					glowobject.glowColor = { r, g, b };
				} else {
					glowobject.glowColor = { glow.color[0], glow.color[1], glow.color[2] };
				}
			}
		};

		constexpr auto isEntityAudible = [](int entityIndex) noexcept {
			for (int i = 0; i < memory->activeChannels->count; ++i)
				if (memory->channels[memory->activeChannels->list[i]].soundSource == entityIndex)
					return true;
			return false;
		};

		auto applyPlayerGlow = [applyGlow](const std::string& name, Entity* entity) noexcept {
			const auto& cfg = config->playerGlow[name];
			bool audible = isEntityAudible(entity->index());
			if (cfg.all.enabled)
				applyGlow(cfg.all, entity->health());
			else if (cfg.visible.enabled && entity->visibleTo(localPlayer.get()))
				applyGlow(cfg.visible, entity->health());
			else if (cfg.occluded.enabled && !entity->visibleTo(localPlayer.get()) && (!cfg.occluded.audibleOnly || audible))
				applyGlow(cfg.occluded, entity->health());
		};

		switch (entity->getClientClass()->classId) {
		case ClassId::CSPlayer:
			if (!entity->isAlive())
				break;
			if (auto activeWeapon{ entity->getActiveWeapon() }; activeWeapon && activeWeapon->getClientClass()->classId == ClassId::C4 && activeWeapon->c4StartedArming())
				applyPlayerGlow(xorstr_("Planting"), entity);
			else if (entity->isDefusing())
				applyPlayerGlow(xorstr_("Defusing"), entity);
			else if (entity == localPlayer.get())
				applyGlow(glow[xorstr_("Local Player")], entity->health());
			else if (entity->isOtherEnemy(localPlayer.get()))
				applyPlayerGlow(xorstr_("Enemies"), entity);
			else
				applyPlayerGlow(xorstr_("Allies"), entity);
			break;
		case ClassId::C4: applyGlow(glow[xorstr_("C4")]); break;
		case ClassId::PlantedC4: applyGlow(glow[xorstr_("Planted C4")]); break;
		case ClassId::Chicken: applyGlow(glow[xorstr_("Chickens")]); break;
		case ClassId::EconEntity: applyGlow(glow[xorstr_("Defuse Kits")]); break;

		case ClassId::BaseCSGrenadeProjectile:
		case ClassId::BreachChargeProjectile:
		case ClassId::BumpMineProjectile:
		case ClassId::DecoyProjectile:
		case ClassId::MolotovProjectile:
		case ClassId::SensorGrenadeProjectile:
		case ClassId::SmokeGrenadeProjectile:
		case ClassId::SnowballProjectile:
			applyGlow(glow[xorstr_("Projectiles")]); break;

		case ClassId::Hostage: applyGlow(glow[xorstr_("Hostages")]); break;
		default:
			if (entity->isWeapon()) {
				applyGlow(glow[xorstr_("Weapons")]);
				if (!glow[xorstr_("Weapons")].enabled) glowobject.renderWhenOccluded = false;
			}
		}
	}
}

void Glow::clearCustomObjects() noexcept
{
	for (const auto& [entityIndex, glowObjectIndex] : customGlowEntities)
		memory->glowObjectManager->unregisterGlowObject(glowObjectIndex);

	customGlowEntities.clear();
}

void Glow::updateInput() noexcept
{
	config->glowKey.handleToggle();
}

void Glow::resetConfig() noexcept
{
	config->glow = {};
	config->playerGlow = {};
	config->glowKey.reset();
}
