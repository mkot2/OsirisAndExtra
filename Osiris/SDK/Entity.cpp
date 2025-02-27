#include "../Memory.h"
#include "../Interfaces.h"

#include "Entity.h"
#include "GlobalVars.h"
#include "Localize.h"
#include "ModelInfo.h"

bool Entity::isOtherEnemy(Entity* other) noexcept
{
	return memory->isOtherEnemy(this, other);
}

void Entity::getPlayerName(char(&out)[128]) noexcept
{
	if (!*memory->playerResource) {
		strcpy(out, "unknown");
		return;
	}

	wchar_t wide[128];
	memory->getDecoratedPlayerName(*memory->playerResource, index(), wide, sizeof(wide), 4);

	auto end = std::remove(wide, wide + wcslen(wide), L'\n');
	end = std::remove_if(wide, end, [](wchar_t c) { return c > 0 && c < 17; }); // remove color markup
	end = std::unique(wide, end, [](wchar_t a, wchar_t b) { return a == L' ' && a == b; });
	*end = L'\0';

	interfaces->localize->convertUnicodeToAnsi(wide, out, 128);
}

bool Entity::canSee(Entity* other, const Vector& pos) noexcept
{
	auto eyePos = getEyePosition();
	if (memory->lineGoesThroughSmoke(eyePos, pos, 1))
		return false;

	Trace trace;
	interfaces->engineTrace->traceRay({ eyePos, pos }, 0x46004009, this, trace);
	return trace.entity == other || trace.fraction > 0.97f;
}

bool Entity::visibleTo(Entity* other) noexcept
{
	assert(isAlive());

	Entity* targetEntity = other;
	if (!other->isAlive() && other->getObserverTarget())
		targetEntity = other->getObserverTarget();

	if (!targetEntity || targetEntity->getClientClass()->classId != ClassId::CSPlayer || !targetEntity->isAlive())
		return false;

	if (targetEntity->canSee(this, getAbsOrigin() + Vector{ 0.0f, 0.0f, 5.0f }))
		return true;

	if (targetEntity->canSee(this, getEyePosition() + Vector{ 0.0f, 0.0f, 5.0f }))
		return true;

	const auto model = getModel();
	if (!model)
		return false;

	const auto studioModel = interfaces->modelInfo->getStudioModel(model);
	if (!studioModel)
		return false;

	const auto set = studioModel->getHitboxSet(hitboxSet());
	if (!set)
		return false;

	matrix3x4 boneMatrices[MAXSTUDIOBONES];
	if (!unfixedSetupBones(boneMatrices, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, memory->globalVars->currenttime))
		return false;

	for (const auto boxNum : { Hitbox::Belly, Hitbox::LeftForearm, Hitbox::RightForearm }) {
		const auto hitbox = set->getHitbox(boxNum);
		if (hitbox && targetEntity->canSee(this, boneMatrices[hitbox->bone].origin()))
			return true;
	}

	return false;
}

#pragma optimize( "", off)
#pragma runtime_checks("", off) //Disable runtime checks to prevent ESP error
void Entity::getSequenceLinearMotion(CStudioHdr* studioHdr, int sequence, Vector& v) noexcept
{
	memory->getSequenceLinearMotion(studioHdr, sequence, (float*)&poseParameters(), &v);
}
#pragma runtime_checks("", restore) //Restore runtime checks
#pragma optimize( "", on )

float Entity::getSequenceMoveDist(CStudioHdr* studioHdr, int sequence) noexcept
{
	Vector v{};

	getSequenceLinearMotion(studioHdr, sequence, v);

	return v.length();
}