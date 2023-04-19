#include <cstring>
#include <functional>

#include "../Config.h"
#include "../Helpers.h"
#include "../Hooks.h"
#include "../Interfaces.h"

#include "Animations.h"
#include "Backtrack.h"
#include "Chams.h"

#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/Material.h"
#include "../SDK/MaterialSystem.h"
#include "../SDK/StudioRender.h"
#include "../SDK/KeyValues.h"

static Material* normal;
static Material* flat;
static Material* animated;
static Material* platinum;
static Material* glass;
static Material* crystal;
static Material* chrome;
static Material* silver;
static Material* gold;
static Material* plastic;
static Material* glow;
static Material* pearlescent;
static Material* metallic;

static constexpr auto dispatchMaterial(int id) noexcept
{
	switch (id) {
	default:
	case 0: return normal;
	case 1: return flat;
	case 2: return animated;
	case 3: return platinum;
	case 4: return glass;
	case 5: return chrome;
	case 6: return crystal;
	case 7: return silver;
	case 8: return gold;
	case 9: return plastic;
	case 10: return glow;
	case 11: return pearlescent;
	case 12: return metallic;
	}
}

static void initializeMaterials() noexcept
{
	normal = interfaces->materialSystem->createMaterial(xorstr_("normal"), KeyValues::fromString(xorstr_("VertexLitGeneric"), nullptr));
	flat = interfaces->materialSystem->createMaterial(xorstr_("flat"), KeyValues::fromString(xorstr_("UnlitGeneric"), nullptr));
	chrome = interfaces->materialSystem->createMaterial(xorstr_("chrome"), KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$envmap env_cubemap")));
	glow = interfaces->materialSystem->createMaterial(xorstr_("glow"), KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$additive 1 $envmap models/effects/cube_white $envmapfresnel 1 $alpha .8")));
	pearlescent = interfaces->materialSystem->createMaterial(xorstr_("pearlescent"), KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$basetexture vgui/white_additive $ambientonly 1 $phong 1 $pearlescent 3 $basemapalphaphongmask 1")));
	metallic = interfaces->materialSystem->createMaterial(xorstr_("metallic"), KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$basetexture white $ignorez 0 $envmap env_cubemap $normalmapalphaenvmapmask 1 $envmapcontrast 1 $nofog 1 $model 1 $nocull 0 $selfillum 1 $halfambert 1 $znearer 0 $flat 1")));

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$envmap editor/cube_vertigo $envmapcontrast 1 $basetexture dev/zone_warning proxies { texturescroll { texturescrollvar $basetexturetransform texturescrollrate 0.6 texturescrollangle 90 } }"));
		kv->setString(xorstr_("$envmaptint"), xorstr_("[.7 .7 .7]"));
		animated = interfaces->materialSystem->createMaterial(xorstr_("animated"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture models/player/ct_fbi/ct_fbi_glass $envmap env_cubemap"));
		kv->setString(xorstr_("$envmaptint"), xorstr_("[.4 .6 .7]"));
		platinum = interfaces->materialSystem->createMaterial(xorstr_("platinum"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture detail/dt_metal1 $additive 1 $envmap editor/cube_vertigo"));
		kv->setString(xorstr_("$color"), xorstr_("[.05 .05 .05]"));
		glass = interfaces->materialSystem->createMaterial(xorstr_("glass"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture black $bumpmap effects/flat_normal $translucent 1 $envmap models/effects/crystal_cube_vertigo_hdr $envmapfresnel 0 $phong 1 $phongexponent 16 $phongboost 2"));
		kv->setString(xorstr_("$phongtint"), xorstr_("[.2 .35 .6]"));
		crystal = interfaces->materialSystem->createMaterial(xorstr_("crystal"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture white $bumpmap effects/flat_normal $envmap editor/cube_vertigo $envmapfresnel .6 $phong 1 $phongboost 2 $phongexponent 8"));
		kv->setString(xorstr_("$color2"), xorstr_("[.05 .05 .05]"));
		kv->setString(xorstr_("$envmaptint"), xorstr_("[.2 .2 .2]"));
		kv->setString(xorstr_("$phongfresnelranges"), xorstr_("[.7 .8 1]"));
		kv->setString(xorstr_("$phongtint"), xorstr_("[.8 .9 1]"));
		silver = interfaces->materialSystem->createMaterial(xorstr_("silver"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture white $bumpmap effects/flat_normal $envmap editor/cube_vertigo $envmapfresnel .6 $phong 1 $phongboost 6 $phongexponent 128 $phongdisablehalflambert 1"));
		kv->setString(xorstr_("$color2"), xorstr_("[.18 .15 .06]"));
		kv->setString(xorstr_("$envmaptint"), xorstr_("[.6 .5 .2]"));
		kv->setString(xorstr_("$phongfresnelranges"), xorstr_("[.7 .8 1]"));
		kv->setString(xorstr_("$phongtint"), xorstr_("[.6 .5 .2]"));
		gold = interfaces->materialSystem->createMaterial(xorstr_("gold"), kv);
	}

	{
		const auto kv = KeyValues::fromString(xorstr_("VertexLitGeneric"), xorstr_("$baseTexture black $bumpmap models/inventory_items/trophy_majors/matte_metal_normal $additive 1 $envmap editor/cube_vertigo $envmapfresnel 1 $normalmapalphaenvmapmask 1 $phong 1 $phongboost 20 $phongexponent 3000 $phongdisablehalflambert 1"));
		kv->setString(xorstr_("$phongfresnelranges"), xorstr_("[.1 .4 1]"));
		kv->setString(xorstr_("$phongtint"), xorstr_("[.8 .9 1]"));
		plastic = interfaces->materialSystem->createMaterial(xorstr_("plastic"), kv);
	}
}

void Chams::updateInput() noexcept
{
	config->chamsKey.handleToggle();
}

bool Chams::render(void* ctx, void* state, const ModelRenderInfo& info, matrix3x4* customBoneToWorld) noexcept
{
	if (!config->chamsKey.isActive())
		return false;

	static bool materialsInitialized = false;
	if (!materialsInitialized) {
		initializeMaterials();
		materialsInitialized = true;
	}

	appliedChams = false;
	this->ctx = ctx;
	this->state = state;
	this->info = &info;
	this->customBoneToWorld = customBoneToWorld;

	if (std::string_view{ info.model->name }.starts_with(xorstr_("models/weapons/w_"))) {
		if (std::strstr(info.model->name + 17, xorstr_("sleeve")))
			renderSleeves();
		else if (std::strstr(info.model->name + 17, xorstr_("arms")))
			renderHands();
		else if (!std::strstr(info.model->name + 17, xorstr_("tablet"))
			&& !std::strstr(info.model->name + 17, xorstr_("parachute"))
			&& !std::strstr(info.model->name + 17, xorstr_("fists")))
			renderWeapons();
	} else if (std::string_view{ info.model->name }.starts_with(xorstr_("models/weapons/v_"))) {
		if (std::strstr(info.model->name + 17, xorstr_("sleeve")))
			renderSleeves();
		else if (std::strstr(info.model->name + 17, xorstr_("arms")))
			renderHands();
		else if (!std::strstr(info.model->name + 17, xorstr_("tablet"))
			&& !std::strstr(info.model->name + 17, xorstr_("parachute"))
			&& !std::strstr(info.model->name + 17, xorstr_("fists")))
			renderWeapons();
	} else {
		const auto entity = interfaces->entityList->getEntity(info.entityIndex);
		if (entity && !entity->isDormant()) {
			if (entity->isPlayer())
				renderPlayer(entity);

			if (entity->getClientClass()->classId == ClassId::CSRagdoll)
				applyChams(config->chams[xorstr_("Ragdolls")].materials);
		}
	}

	return appliedChams;
}

void Chams::renderPlayer(Entity* player) noexcept
{
	if (!localPlayer)
		return;

	const auto health = player->health();

	if (const auto activeWeapon = player->getActiveWeapon(); activeWeapon && activeWeapon->getClientClass()->classId == ClassId::C4 && activeWeapon->c4StartedArming() && std::any_of(config->chams[xorstr_("Planting")].materials.cbegin(), config->chams[xorstr_("Planting")].materials.cend(), [](const Config::Chams::Material& mat) { return mat.enabled; })) {
		applyChams(config->chams[xorstr_("Planting")].materials, health);
	} else if (player->isDefusing() && std::any_of(config->chams[xorstr_("Defusing")].materials.cbegin(), config->chams[xorstr_("Defusing")].materials.cend(), [](const Config::Chams::Material& mat) { return mat.enabled; })) {
		applyChams(config->chams[xorstr_("Defusing")].materials, health);
	} else if (player == localPlayer.get()) {
		applyChams(config->chams[xorstr_("Local player")].materials, health);
		renderDesync(health);
		renderFakelag(health);
	} else if (localPlayer->isOtherEnemy(player)) {
		applyChams(config->chams[xorstr_("Enemies")].materials, health);

		if (config->backtrack.enabled) {
			const auto records = Animations::getBacktrackRecords(player->index());
			if (records && !records->empty()) {
				int lastTick = -1;

				for (int i = static_cast<int>(records->size() - 1U); i >= 0; i--) {
					if (Backtrack::valid(records->at(i).simulationTime) && records->at(i).origin != player->origin()) {
						lastTick = i;
						break;
					}
				}

				if (lastTick != -1) {
					if (!appliedChams)
						hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
					applyChams(config->chams[xorstr_("Backtrack")].materials, health, records->at(lastTick).matrix);
					interfaces->studioRender->forcedMaterialOverride(nullptr);
				}
			}
		}
	} else {
		applyChams(config->chams[xorstr_("Allies")].materials, health);
	}
}

void Chams::renderFakelag(int health) noexcept
{
	if (!config->fakeAngle[0].enabled && !config->fakeAngle[1].enabled && !config->fakeAngle[2].enabled && !config->fakeAngle[3].enabled && !config->fakeAngle[4].enabled && !config->fakeAngle[5].enabled && !config->fakeAngle[6].enabled && !config->fakelag[0].enabled && !config->fakelag[1].enabled && !config->fakelag[2].enabled && !config->fakelag[3].enabled && !config->fakelag[4].enabled && !config->fakelag[5].enabled && !config->fakelag[6].enabled)
		return;

	if (!localPlayer->isAlive())
		return;

	if (localPlayer->velocity().length2D() < 1.0f)
		return;

	if (Animations::gotFakelagMatrix()) {
		auto fakelagMatrix = Animations::getFakelagMatrix();
		if (!appliedChams)
			hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
		applyChams(config->chams[xorstr_("Fake lag")].materials, health, fakelagMatrix.data());
		interfaces->studioRender->forcedMaterialOverride(nullptr);
	}
}

void Chams::renderDesync(int health) noexcept
{
	if (Animations::gotFakeMatrix()) {
		auto fakeMatrix = Animations::getFakeMatrix();
		for (auto& i : fakeMatrix) {
			i[0][3] += info->origin.x;
			i[1][3] += info->origin.y;
			i[2][3] += info->origin.z;
		}
		if (!appliedChams)
			hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
		applyChams(config->chams[xorstr_("Desync")].materials, health, fakeMatrix.data());
		interfaces->studioRender->forcedMaterialOverride(nullptr);
		for (auto& i : fakeMatrix) {
			i[0][3] -= info->origin.x;
			i[1][3] -= info->origin.y;
			i[2][3] -= info->origin.z;
		}
	}
}

void Chams::renderWeapons() noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	applyChams(config->chams[xorstr_("Weapons")].materials, localPlayer->health());
}

void Chams::renderHands() noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	applyChams(config->chams[xorstr_("Hands")].materials, localPlayer->health());
}

void Chams::renderSleeves() noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	applyChams(config->chams[xorstr_("Sleeves")].materials, localPlayer->health());
}

void Chams::applyChams(const std::array<Config::Chams::Material, 7>& chams, int health, const matrix3x4* customMatrix) noexcept
{
	for (const auto& cham : chams) {
		if (!cham.enabled || !cham.ignorez)
			continue;

		const auto material = dispatchMaterial(cham.material);
		if (!material)
			continue;

		float r, g, b;
		if (cham.healthBased && health) {
			Helpers::healthColor(std::clamp(health / 100.0f, 0.0f, 1.0f), r, g, b);
		} else if (cham.rainbow) {
			std::tie(r, g, b) = rainbowColor(cham.rainbowSpeed);
		} else {
			r = cham.color[0];
			g = cham.color[1];
			b = cham.color[2];
		}

		if (material == glow || material == chrome || material == plastic || material == glass || material == crystal)
			material->findVar(xorstr_("$envmaptint"))->setVectorValue(r, g, b);
		else
			material->colorModulate(r, g, b);

		const auto pulse = cham.color[3] * (cham.blinking ? std::sin(memory->globalVars->currenttime * 5) * 0.5f + 0.5f : 1.0f);

		if (material == glow)
			material->findVar(xorstr_("$envmapfresnelminmaxexp"))->setVecComponentValue(9.0f * (1.2f - pulse), 2);
		else
			material->alphaModulate(pulse);

		material->setMaterialVarFlag(MaterialVarFlag::IGNOREZ, true);
		material->setMaterialVarFlag(MaterialVarFlag::WIREFRAME, cham.wireframe);
		interfaces->studioRender->forcedMaterialOverride(material);
		hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);
		interfaces->studioRender->forcedMaterialOverride(nullptr);
	}

	for (const auto& cham : chams) {
		if (!cham.enabled || cham.ignorez)
			continue;

		const auto material = dispatchMaterial(cham.material);
		if (!material)
			continue;

		float r, g, b;
		if (cham.healthBased && health) {
			Helpers::healthColor(std::clamp(health / 100.0f, 0.0f, 1.0f), r, g, b);
		} else if (cham.rainbow) {
			std::tie(r, g, b) = rainbowColor(cham.rainbowSpeed);
		} else {
			r = cham.color[0];
			g = cham.color[1];
			b = cham.color[2];
		}

		if (material == glow || material == chrome || material == plastic || material == glass || material == crystal)
			material->findVar(xorstr_("$envmaptint"))->setVectorValue(r, g, b);
		else
			material->colorModulate(r, g, b);

		const auto pulse = cham.color[3] * (cham.blinking ? std::sin(memory->globalVars->currenttime * 5) * 0.5f + 0.5f : 1.0f);

		if (material == glow)
			material->findVar(xorstr_("$envmapfresnelminmaxexp"))->setVecComponentValue(9.0f * (1.2f - pulse), 2);
		else
			material->alphaModulate(pulse);

		if (cham.cover && !appliedChams)
			hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);

		material->setMaterialVarFlag(MaterialVarFlag::IGNOREZ, false);
		material->setMaterialVarFlag(MaterialVarFlag::WIREFRAME, cham.wireframe);
		interfaces->studioRender->forcedMaterialOverride(material);
		hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);
		appliedChams = true;
	}
}
