#include <string>
#include <mutex>

#include <imgui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

#include "../Config.h"
#include "../GameData.h"
#include "../Memory.h"
#include "../Interfaces.h"

#include "GrenadePrediction.h"

#include "../SDK/Cvar.h"
#include "../SDK/ConVar.h"
#include "../SDK/Engine.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Surface.h"


std::vector<std::pair<ImVec2, ImVec2>> screenPoints;
std::vector<std::pair<ImVec2, ImVec2>> endPoints;
std::vector<std::pair<ImVec2, std::string>> dmgPoints;
std::vector<std::pair<ImVec2, ImVec2>> savedPoints;

std::mutex renderMutex;

int grenadeAct{ 1 };

void traceHull(Vector& src, Vector& end, Trace& tr) noexcept
{
	interfaces->engineTrace->traceRay({ src, end, Vector{-2.0f, -2.0f, -2.0f}, Vector{2.0f, 2.0f, 2.0f} }, 0x200400B, { localPlayer.get() }, tr);
}

void setup(Vector& vecSrc, Vector& vecThrow, Vector viewangles) noexcept
{
	Vector angThrow = viewangles;
	float pitch = angThrow.x;

	if (pitch <= 90.0f) {
		if (pitch < -90.0f) {
			pitch += 360.0f;
		}
	} else {
		pitch -= 360.0f;
	}

	const float a = pitch - (90.0f - std::abs(pitch)) * 10.0f / 90.0f;
	angThrow.x = a;

	constexpr float power[] = { 1.0f, 1.0f, 0.5f, 0.0f };
	const float b = power[grenadeAct] * .7f + .3f;
	const float flVel = 750.f * .9f * b;

	Vector vForward, vRight, vUp;
	angThrow.fromAngle(vForward, vRight, vUp);

	vecSrc = localPlayer->getEyePosition();
	const float off = (power[grenadeAct] * 12.0f) - 12.0f;
	vecSrc.z += off;

	Trace tr;
	Vector vecDest = vecSrc;
	vecDest += vForward * 22.0f;

	traceHull(vecSrc, vecDest, tr);

	Vector vecBack = vForward; vecBack *= 6.0f;
	vecSrc = tr.endpos;
	vecSrc -= vecBack;

	vecThrow = localPlayer->velocity(); vecThrow *= 1.25f;
	vecThrow += vForward * flVel;
}

int physicsClipVelocity(const Vector& in, const Vector& normal, Vector& out, float overbounce) noexcept
{
	constexpr float STOP_EPSILON = 0.1f;

	float    backoff;
	float    change;
	float    angle;
	int        i, blocked;

	blocked = 0;

	angle = normal[2];

	if (angle > 0) {
		blocked |= 1;        // floor
	}
	if (!angle) {
		blocked |= 2;        // step
	}

	backoff = in.dotProduct(normal) * overbounce;

	for (i = 0; i < 3; i++) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON) {
			out[i] = 0;
		}
	}

	return blocked;
}

void pushEntity(Vector& src, const Vector& move, Trace& tr) noexcept
{
	Vector vecAbsEnd = src;
	vecAbsEnd += move;
	traceHull(src, vecAbsEnd, tr);
}

void resolveFlyCollisionCustom(Trace& tr, Vector& vecVelocity, float interval) noexcept
{
	// Calculate elasticity
	constexpr float surfaceElasticity = 1.0f;
	constexpr float grenadeElasticity = 0.45f;
	float totalElasticity = grenadeElasticity * surfaceElasticity;
	if (totalElasticity > 0.9f) totalElasticity = 0.9f;
	if (totalElasticity < 0.0f) totalElasticity = 0.0f;

	// Calculate bounce
	Vector vecAbsVelocity;
	physicsClipVelocity(vecVelocity, tr.plane.normal, vecAbsVelocity, 2.0f);
	vecAbsVelocity *= totalElasticity;

	float speedSqr = vecAbsVelocity.squareLength();
	constexpr float minSpeedSqr = 20.0f * 20.0f;

	if (speedSqr < minSpeedSqr) {
		vecAbsVelocity.x = 0.0f;
		vecAbsVelocity.y = 0.0f;
		vecAbsVelocity.z = 0.0f;
	}

	if (tr.plane.normal.z > 0.7f) {
		vecVelocity = vecAbsVelocity;
		vecAbsVelocity *= ((1.0f - tr.fraction) * interval);
		pushEntity(tr.endpos, vecAbsVelocity, tr);
	} else {
		vecVelocity = vecAbsVelocity;
	}
}

void addGravityMove(Vector& move, Vector& vel, float frametime, bool onground) noexcept
{
	if (!config->misc.nadePredict)
		return;

	Vector basevel{ 0.0f, 0.0f, 0.0f };

	move.x = (vel.x + basevel.x) * frametime;
	move.y = (vel.y + basevel.y) * frametime;

	if (onground) {
		move.z = (vel.z + basevel.z) * frametime;
	} else {
		constexpr float gravity = 800.0f * 0.4f;
		float newZ = vel.z - (gravity * frametime);
		move.z = ((vel.z + newZ) / 2.0f + basevel.z) * frametime;
		vel.z = newZ;
	}
}

enum ACT {
	ACT_NONE,
	ACT_THROW,
	ACT_LOB,
	ACT_DROP,
};

void tick(int buttons) noexcept
{
	bool in_attack = buttons & UserCmd::IN_ATTACK;
	bool in_attack2 = buttons & UserCmd::IN_ATTACK2;

	grenadeAct = (in_attack && in_attack2) ? ACT_LOB :
		(in_attack2) ? ACT_DROP :
		(in_attack) ? ACT_THROW :
		ACT_NONE;
}

bool checkDetonate(const Vector& vecThrow, const Trace& tr, int tick, float interval, Entity* activeWeapon) noexcept
{
	switch (activeWeapon->itemDefinitionIndex2()) {
	case WeaponId::SmokeGrenade:
	case WeaponId::Decoy:
		if (vecThrow.length2D() < 0.1f) {
			int det_tick_mod = (int)(0.2f / interval);
			return !(tick % det_tick_mod);
		}
		return false;
	case WeaponId::Molotov:
	case WeaponId::IncGrenade:
		if (tr.fraction != 1.0f && tr.plane.normal.z > 0.7f)
			return true;
	case WeaponId::Flashbang:
	case WeaponId::HeGrenade:
		return (float)tick * interval > 1.5f && !(tick % (int)(0.2f / interval));
	default:
		return false;
	}
}

void drawCircle(Vector position, float points, float radius) noexcept
{
	float step = 3.141592654f * 2.0f / points;
	ImVec2 end2d{}, start2d{};
	Vector lastPos{};
	for (float a = -step; a < 3.141592654f * 2.0f; a += step) {
		Vector start{ radius * std::cos(a) + position.x, radius * std::sin(a) + position.y, position.z };

		Trace tr;
		traceHull(position, start, tr);
		if (!tr.endpos.notNull())
			continue;

		if (Helpers::worldToScreen(tr.endpos, start2d) && Helpers::worldToScreen(lastPos, end2d) && lastPos != Vector{ }) {
			if (start2d != ImVec2{ }&& end2d != ImVec2{ })
				endPoints.emplace_back(std::pair<ImVec2, ImVec2>{ end2d, start2d });
		}
		lastPos = tr.endpos;
	}
}

float calculateArmor(float damage, int armorValue) noexcept
{
	if (armorValue > 0) {
		float newDamage = damage * 0.5f;
		float armor = (damage - newDamage) * 0.5f;

		if (armor > static_cast<float>(armorValue)) {
			armor = static_cast<float>(armorValue) * (1.f / 0.5f);
			newDamage = damage - armor;
		}

		damage = newDamage;
	}
	return damage;
}

void drawDamage(Vector position) noexcept
{
	static auto mp_friendlyfire = interfaces->cvar->findVar("mp_friendlyfire");
	static auto ff_damage_reduction_grenade = interfaces->cvar->findVar("ff_damage_reduction_grenade");

	ImVec2 pos{};

	GameData::Lock lock;
	for (auto& player : GameData::players()) {
		if (static_cast<bool>(player.handle) == static_cast<bool>(localPlayer) || player.dormant || !player.alive)
			continue;

		Vector center = player.origin + (player.obbMins + player.obbMaxs) * 0.5f;
		const float dist = (center - position).length();
		if (dist > 350.f)
			continue;

		Trace tr;
		traceHull(position, center, tr);
		if (!tr.endpos.notNull() || !tr.entity || tr.entity->handle() != player.handle)
			continue;

		constexpr float a = 105.0f;
		constexpr float b = 25.0f;
		constexpr float c = 140.0f;

		const float d = ((dist - b) / c);
		const float damage = a * std::exp(-d * d);
		float dmg = std::max(std::ceil(calculateArmor(damage, player.armor)), 0.0f);
		dmg = std::min(dmg, (player.armor > 0) ? 57.0f : 98.0f);

		if (mp_friendlyfire->getInt() > 0 && !player.enemy)
			dmg *= ff_damage_reduction_grenade->getFloat();

		if (dmg < 1)
			continue;

		std::string dmg2text = player.health - dmg > 0 ? std::to_string(static_cast<int>(dmg)) : "Kill";
		if (Helpers::worldToScreen(player.origin, pos))
			dmgPoints.emplace_back(std::pair<ImVec2, std::string>{ pos, dmg2text });
	}
}

void GrenadePrediction::run(UserCmd* cmd) noexcept
{
	screenPoints.clear();
	endPoints.clear();
	dmgPoints.clear();

	if (!config->misc.nadePredict)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (interfaces->engine->isHLTV())
		return;

	if (localPlayer->moveType() == MoveType::NOCLIP)
		return;

	auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->isGrenade())
		return;

	const auto itemDefinition = activeWeapon->itemDefinitionIndex2();
	if (itemDefinition != WeaponId::SmokeGrenade
		&& itemDefinition != WeaponId::Decoy
		&& itemDefinition != WeaponId::Molotov
		&& itemDefinition != WeaponId::IncGrenade
		&& itemDefinition != WeaponId::Flashbang
		&& itemDefinition != WeaponId::HeGrenade)
		return;

	if (!renderMutex.try_lock())
		return;

	tick(cmd->buttons);

	Vector vecSrc, vecThrow;
	setup(vecSrc, vecThrow, cmd->viewangles);

	float interval = memory->globalVars->intervalPerTick;
	int logstep = static_cast<int>(0.05f / interval);
	int logtimer = 0;

	std::vector<Vector> path;

	for (unsigned int i = 0; i < path.max_size() - 1; ++i) {
		if (!logtimer)
			path.emplace_back(vecSrc);

		Vector move;
		addGravityMove(move, vecThrow, interval, false);

		// Push entity
		Trace tr;
		pushEntity(vecSrc, move, tr);

		int result = 0;
		if (checkDetonate(vecThrow, tr, i, interval, activeWeapon))
			result |= 1;

		if (tr.fraction != 1.0f) {
			result |= 2; // Collision!
			resolveFlyCollisionCustom(tr, vecThrow, interval);
		}

		vecSrc = tr.endpos;

		if (result & 1)
			break;

		if ((result & 2) || logtimer >= logstep)
			logtimer = 0;
		else
			++logtimer;
	}

	path.emplace_back(vecSrc);

	Vector prev = path[0];
	ImVec2 nadeStart, nadeEnd;
	Vector lastPos{ };
	for (auto& nade : path) {
		if (Helpers::worldToScreen(prev, nadeStart) && Helpers::worldToScreen(nade, nadeEnd)) {
			screenPoints.emplace_back(std::pair<ImVec2, ImVec2>{ nadeStart, nadeEnd });
			prev = nade;
			lastPos = nade;
		}
	}

	if (lastPos.notNull()) {
		drawCircle(lastPos, 120, 150);
		if (config->misc.nadeDamagePredict.enabled && itemDefinition == WeaponId::HeGrenade)
			drawDamage(lastPos);
	}


	renderMutex.unlock();
}

void GrenadePrediction::draw() noexcept
{
	if (!config->misc.nadePredict)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (interfaces->engine->isHLTV())
		return;

	if (renderMutex.try_lock()) {
		savedPoints = screenPoints;

		renderMutex.unlock();
	}

	if (savedPoints.empty())
		return;

	const auto trailColor = Helpers::calculateColor(config->misc.nadeTrailPredict);
	const auto circleColor = Helpers::calculateColor(config->misc.nadeCirclePredict);

	auto drawList = ImGui::GetBackgroundDrawList();
	// draw end nade path
	for (auto& point : endPoints)
		drawList->AddLine(ImVec2(point.first.x, point.first.y), ImVec2(point.second.x, point.second.y), circleColor, 2.f);

	//	draw nade path
	for (auto& point : savedPoints)
		drawList->AddLine(ImVec2(point.first.x, point.first.y), ImVec2(point.second.x, point.second.y), trailColor, 1.5f);

	// draw nade damage
	if (config->misc.nadeDamagePredict.enabled) {
		for (auto& point : dmgPoints) {
			const auto textSize = ImGui::CalcTextSize(point.second.c_str());
			const auto horizontalOffset = textSize.x / 2;
			const auto verticalOffset = textSize.y;

			drawList->AddText({ point.first.x - horizontalOffset + 1.0f, point.first.y - verticalOffset + 1.0f }, Helpers::calculateColor(config->misc.nadeDamagePredict) & IM_COL32_A_MASK, point.second.c_str());
			drawList->AddText({ point.first.x - horizontalOffset, point.first.y - verticalOffset }, Helpers::calculateColor(config->misc.nadeDamagePredict), point.second.c_str());
		}
	}
}
