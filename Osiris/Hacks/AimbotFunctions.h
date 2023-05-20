#pragma once
#include <PCG/pcg.h>

#include "../SDK/Entity.h"
#include "../SDK/Vector.h"
#include "../SDK/EngineTrace.h"

struct UserCmd;
struct Vector;
struct SurfaceData;
struct StudioBbox;
struct StudioHitboxSet;

namespace AimbotFunction
{
	struct Enemies {
		int id;
		int health;
		float distance;
		float fov;

		bool operator<(const Enemies& enemy) const noexcept
		{
			if (health != enemy.health)
				return health < enemy.health;
			if (fov != enemy.fov)
				return fov < enemy.fov;
			return distance < enemy.distance;
		}

		Enemies(int id, int health, float distance, float fov) noexcept : id(id), health(health), distance(distance), fov(fov) {}
	};

	struct {
		bool operator()(const Enemies& a, const Enemies& b) const
		{
			return a.health < b.health;
		}
	} healthSort;
	struct {
		bool operator()(const Enemies& a, const Enemies& b) const
		{
			return a.distance < b.distance;
		}
	} distanceSort;
	struct {
		bool operator()(const Enemies& a, const Enemies& b) const
		{
			return a.fov < b.fov;
		}
	} fovSort;

	Vector calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept;

	void calculateArmorDamage(float armorRatio, int armorValue, bool hasHeavyArmor, float& damage) noexcept;

	bool canScan(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, int minDamage, bool allowFriendlyFire) noexcept;
	float getScanDamage(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, int minDamage, bool allowFriendlyFire) noexcept;

	bool hitboxIntersection(const matrix3x4 matrix[MAXSTUDIOBONES], int iHitbox, StudioHitboxSet* set, const Vector& start, const Vector& end) noexcept;

	std::vector<Vector> multiPoint(Entity* entity, const matrix3x4 matrix[MAXSTUDIOBONES], StudioBbox* hitbox, Vector localEyePos, int _hitbox, int _headMultiPoint, int _bodyMultiPoint);

	bool hitChance(Entity* localPlayer, Entity* entity, StudioHitboxSet*, const matrix3x4 matrix[MAXSTUDIOBONES], Entity* activeWeapon, const Vector& destination, const UserCmd* cmd, const int hitChance, const int hitbox) noexcept;
}
