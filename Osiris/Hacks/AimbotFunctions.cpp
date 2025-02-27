#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "AimbotFunctions.h"
#include "Animations.h"

#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/WeaponData.h"
#include "../SDK/ModelInfo.h"

Vector AimbotFunction::calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept
{
	return ((destination - source).toAngle() - viewAngles).normalize();
}

static bool traceToExit(const Trace& enterTrace, const Vector& start, const Vector& direction, Vector& end, Trace& exitTrace, float range = 90.f, float step = 4.0f)
{
	float distance{ 0.0f };
	int previousContents{ 0 };

	while (distance <= range) {
		distance += step;
		Vector origin{ start + direction * distance };

		if (!previousContents)
			previousContents = interfaces->engineTrace->getPointContents(origin, 0x4600400B);

		const int currentContents = interfaces->engineTrace->getPointContents(origin, 0x4600400B);
		if (!(currentContents & 0x600400B) || (currentContents & 0x40000000 && currentContents != previousContents)) {
			const Vector destination{ origin - (direction * step) };

			if (interfaces->engineTrace->traceRay({ origin, destination }, 0x4600400B, nullptr, exitTrace); exitTrace.startSolid && exitTrace.surface.flags & 0x8000) {
				if (interfaces->engineTrace->traceRay({ origin, start }, 0x600400B, { exitTrace.entity }, exitTrace); exitTrace.didHit() && !exitTrace.startSolid)
					return true;

				continue;
			}

			if (exitTrace.didHit() && !exitTrace.startSolid) {
				if (memory->isBreakableEntity(enterTrace.entity) && memory->isBreakableEntity(exitTrace.entity))
					return true;

				if (enterTrace.surface.flags & 0x0080 || (!(exitTrace.surface.flags & 0x0080) && exitTrace.plane.normal.dotProduct(direction) <= 1.0f))
					return true;

				continue;
			} else {
				if (enterTrace.entity && enterTrace.entity->index() != 0 && memory->isBreakableEntity(enterTrace.entity))
					return true;

				continue;
			}
		}
	}
	return false;
}

static float handleBulletPenetration(const SurfaceData* enterSurfaceData, const Trace& enterTrace, const Vector& direction, Vector& result, float penetration, float damage) noexcept
{
	Vector end;
	Trace exitTrace;

	if (!traceToExit(enterTrace, enterTrace.endpos, direction, end, exitTrace))
		return -1.0f;

	SurfaceData* exitSurfaceData = interfaces->physicsSurfaceProps->getSurfaceData(exitTrace.surface.surfaceProps);

	float damageModifier = 0.16f;
	float penetrationModifier = (enterSurfaceData->penetrationmodifier + exitSurfaceData->penetrationmodifier) / 2.0f;

	if (enterSurfaceData->material == 71 || enterSurfaceData->material == 89) {
		damageModifier = 0.05f;
		penetrationModifier = 3.0f;
	} else if (enterTrace.contents >> 3 & 1 || enterTrace.surface.flags >> 7 & 1) {
		penetrationModifier = 1.0f;
	}

	if (enterSurfaceData->material == exitSurfaceData->material) {
		if (exitSurfaceData->material == 85 || exitSurfaceData->material == 87)
			penetrationModifier = 3.0f;
		else if (exitSurfaceData->material == 76)
			penetrationModifier = 2.0f;
	}

	damage -= 11.25f / penetration / penetrationModifier + damage * damageModifier + (exitTrace.endpos - enterTrace.endpos).squareLength() / 24.0f / penetrationModifier;

	result = exitTrace.endpos;
	return damage;
}

void AimbotFunction::calculateArmorDamage(float armorRatio, int armorValue, bool hasHeavyArmor, float& damage) noexcept
{
	float armorScale = 1.0f;
	float armorBonusRatio = 0.5f;

	if (hasHeavyArmor) {
		armorRatio *= 0.2f;
		armorBonusRatio = 0.33f;
		armorScale = 0.25f;
	}

	float newDamage = damage * armorRatio;
	const float estiminatedDamage = (damage - newDamage) * armorScale * armorBonusRatio;

	if (estiminatedDamage > armorValue)
		newDamage = damage - armorValue / armorBonusRatio;

	damage = newDamage;
}

float AimbotFunction::getScanDamage(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, int minDamage, bool allowFriendlyFire) noexcept
{
	if (!localPlayer)
		return 0.f;

	float damage{ static_cast<float>(weaponData->damage) };

	Vector start{ localPlayer->getEyePosition() };
	Vector direction{ destination - start };
	float maxDistance{ direction.length() };
	float curDistance{ 0.0f };
	direction /= maxDistance;

	int hitsLeft = 4;

	while (damage >= 1.0f && hitsLeft) {
		Trace trace;
		interfaces->engineTrace->traceRay({ start, destination }, 0x4600400B, localPlayer.get(), trace);

		if (!allowFriendlyFire && trace.entity && trace.entity->isPlayer() && !localPlayer->isOtherEnemy(trace.entity))
			return 0.f;

		if (trace.fraction == 1.0f)
			break;

		curDistance += trace.fraction * (maxDistance - curDistance);
		damage *= std::pow(weaponData->rangeModifier, curDistance / 500.0f);

		if (trace.entity == entity && trace.hitgroup > HitGroup::Generic && trace.hitgroup <= HitGroup::RightLeg) {
			damage *= HitGroup::getDamageMultiplier(trace.hitgroup, weaponData, trace.entity->hasHeavyArmor(), static_cast<int>(trace.entity->getTeamNumber()));

			if (float armorRatio{ weaponData->armorRatio / 2.0f }; HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet(), trace.entity->armor(), trace.entity->hasHeavyArmor()))
				calculateArmorDamage(armorRatio, trace.entity->armor(), trace.entity->hasHeavyArmor(), damage);

			if (damage >= minDamage)
				return damage;
			return 0.f;
		}
		const auto surfaceData = interfaces->physicsSurfaceProps->getSurfaceData(trace.surface.surfaceProps);

		if (surfaceData->penetrationmodifier < 0.1f)
			break;

		damage = handleBulletPenetration(surfaceData, trace, direction, start, weaponData->penetration, damage);
		hitsLeft--;
	}
	return 0.f;
}

bool AimbotFunction::canScan(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, int minDamage, bool allowFriendlyFire) noexcept
{
	return getScanDamage(entity, destination, weaponData, minDamage, allowFriendlyFire) > 0.05f;
}

float segmentToSegment(const Vector& s1, const Vector& s2, const Vector& k1, const Vector& k2) noexcept
{
	constexpr auto epsilon = 0.00000001f;

	auto u = s2 - s1;
	auto v = k2 - k1;
	auto w = s1 - k1;

	auto a = u.dotProduct(u); //-V525
	auto b = u.dotProduct(v);
	auto c = v.dotProduct(v);
	auto d = u.dotProduct(w);
	auto e = v.dotProduct(w);
	auto D = a * c - b * b;

	auto sn = 0.0f, sd = D;
	auto tn = 0.0f, td = D;

	if (D < epsilon) {
		sn = 0.0f;
		sd = 1.0f;
		tn = e;
		td = c;
	} else {
		sn = b * e - c * d;
		tn = a * e - b * d;

		if (sn < 0.0f) {
			sn = 0.0f;
			tn = e;
			td = c;
		} else if (sn > sd) {
			sn = sd;
			tn = e + b;
			td = c;
		}
	}

	if (tn < 0.0f) {
		tn = 0.0f;

		if (-d < 0.0f)
			sn = 0.0f;
		else if (-d > a)
			sn = sd;
		else {
			sn = -d;
			sd = a;
		}
	} else if (tn > td) {
		tn = td;

		if (-d + b < 0.0f)
			sn = 0.0f;
		else if (-d + b > a)
			sn = sd;
		else {
			sn = -d + b;
			sd = a;
		}
	}

	auto sc = std::abs(sn) < epsilon ? 0.0f : sn / sd;
	auto tc = std::abs(tn) < epsilon ? 0.0f : tn / td;

	auto dp = w + u * sc - v * tc;
	return dp.length();
}

bool intersectLineWithBb(Vector& start, Vector& end, Vector& min, Vector& max) noexcept
{
	float d1, d2, f;
	auto start_solid = true;
	auto t1 = -1.0f, t2 = 1.0f;

	const float s[3] = { start.x, start.y, start.z };
	const float e[3] = { end.x, end.y, end.z };
	const float mi[3] = { min.x, min.y, min.z };
	const float ma[3] = { max.x, max.y, max.z };

	for (auto i = 0; i < 6; i++) {
		if (i >= 3) {
			const auto j = i - 3;

			d1 = s[j] - ma[j];
			d2 = d1 + e[j];
		} else {
			d1 = -s[i] + mi[i];
			d2 = d1 - e[i];
		}

		if (d1 > 0.0f && d2 > 0.0f)
			return false;

		if (d1 <= 0.0f && d2 <= 0.0f)
			continue;

		if (d1 > 0)
			start_solid = false;

		if (d1 > d2) {
			f = d1;
			if (f < 0.0f)
				f = 0.0f;

			f /= d1 - d2;
			if (f > t1)
				t1 = f;
		} else {
			f = d1 / (d1 - d2);
			if (f < t2)
				t2 = f;
		}
	}

	return start_solid || (t1 < t2 && t1 >= 0.0f);
}

bool AimbotFunction::hitboxIntersection(const matrix3x4 matrix[MAXSTUDIOBONES], int iHitbox, StudioHitboxSet* set, const Vector& start, const Vector& end) noexcept
{
	StudioBbox* hitbox = set->getHitbox(iHitbox);
	if (!hitbox)
		return false;

	if (hitbox->capsuleRadius == -1.f)
		return false;

	Vector mins, maxs;
	const auto isCapsule = hitbox->capsuleRadius != -1.f;
	if (isCapsule) {
		mins = hitbox->bbMin.transform(matrix[hitbox->bone]);
		maxs = hitbox->bbMax.transform(matrix[hitbox->bone]);
		const auto dist = segmentToSegment(start, end, mins, maxs);

		if (dist < hitbox->capsuleRadius)
			return true;
	} else {
		mins = start.transform(matrix[hitbox->bone]);
		maxs = end.transform(matrix[hitbox->bone]);

		if (intersectLineWithBb(mins, maxs, hitbox->bbMin, hitbox->bbMax))
			return true;
	}
	return false;
}

std::vector<Vector> AimbotFunction::multiPoint(Entity* entity, const matrix3x4 matrix[MAXSTUDIOBONES], StudioBbox* hitbox, Vector localEyePos, int _hitbox, int _headMultiPoint, int _bodyMultiPoint)
{
	Vector min, max, center;
	min = hitbox->bbMin.transform(matrix[hitbox->bone]);
	max = hitbox->bbMax.transform(matrix[hitbox->bone]);
	center = (min + max) * 0.5f;

	std::vector<Vector> vecArray;

	if ((_headMultiPoint <= 0 && _hitbox == 0) || (_bodyMultiPoint <= 0 && _hitbox != 0)) {
		vecArray.emplace_back(center);
		return vecArray;
	}
	vecArray.emplace_back(center);

	Vector currentAngles = calculateRelativeAngle(center, localEyePos, Vector{});

	Vector forward;
	currentAngles.fromAngle(forward, {}, {});

	Vector right = forward.crossProduct(Vector{ 0, 0, 1 });
	Vector left = Vector{ -right.x, -right.y, right.z };

	Vector top = Vector{ 0, 0, 1 };
	Vector bottom = Vector{ 0, 0, -1 };

	float headMultiPoint = _headMultiPoint * 0.01f;
	float bodyMultiPoint = _bodyMultiPoint * 0.01f;

	switch (_hitbox) {
	case Head:
		for (auto i = 0; i < 5; ++i)
			vecArray.emplace_back(center);

		vecArray[1] += top * (hitbox->capsuleRadius * headMultiPoint);
		vecArray[2] += right * (hitbox->capsuleRadius * headMultiPoint);
		vecArray[3] += left * (hitbox->capsuleRadius * headMultiPoint);
		vecArray[4] += bottom * (hitbox->capsuleRadius * headMultiPoint);
		break;
	default:
		for (auto i = 0; i < 3; ++i)
			vecArray.emplace_back(center);
		vecArray[1] += right * (hitbox->capsuleRadius * bodyMultiPoint);
		vecArray[2] += left * (hitbox->capsuleRadius * bodyMultiPoint);
		break;
	}
	return vecArray;
}

bool AimbotFunction::hitChance(Entity* localPlayer, Entity* entity, StudioHitboxSet* set, const matrix3x4 matrix[MAXSTUDIOBONES], Entity* activeWeapon, const Vector& destination, const UserCmd* cmd, const int hitChance, const int hitbox) noexcept
{
	static auto isSpreadEnabled = interfaces->cvar->findVar("weapon_accuracy_nospread");
	if (!hitChance || isSpreadEnabled->getInt() >= 1)
		return true;

	constexpr int maxSeed = 256;

	Vector forward, right, up;
	(destination + cmd->viewangles).fromAngle(forward, right, up);

	int hits = 0;
	const int hitsNeed = static_cast<int>(static_cast<float>(maxSeed) * (static_cast<float>(hitChance) / 100.f));

	const float weapSpread = activeWeapon->getSpread();
	const float weapInaccuracy = activeWeapon->getInaccuracy();
	const Vector localEyePosition = localPlayer->getEyePosition();
	const float range = activeWeapon->getWeaponData()->range;

	for (int i = 0; i < maxSeed; i++) {
		const float spreadX = std::uniform_real_distribution<float>(0.f, 2.f * std::numbers::pi_v<float>)(PCG::generator);
		const float spreadY = std::uniform_real_distribution<float>(0.f, 2.f * std::numbers::pi_v<float>)(PCG::generator);
		const float inaccuracy = weapInaccuracy * std::uniform_real_distribution<float>(0.f, 1.f)(PCG::generator);
		const float spread = weapSpread * std::uniform_real_distribution<float>(0.f, 1.f)(PCG::generator);

		Vector spreadView{ std::cos(spreadX) * inaccuracy + std::cos(spreadY) * spread,
						   std::sin(spreadX) * inaccuracy + std::sin(spreadY) * spread };
		Vector direction{ (forward + (right * spreadView.x) + (up * spreadView.y)) * range };

		if (hitboxIntersection(matrix, hitbox, set, localEyePosition, localEyePosition + direction))
			hits++;

		if (hits >= hitsNeed)
			return true;
		if (maxSeed - i + hits < hitsNeed)
			return false;
	}
	return false;
}