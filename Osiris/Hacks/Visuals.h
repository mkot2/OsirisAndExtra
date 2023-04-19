#pragma once

#include <array>

enum class FrameStage;
class GameEvent;
struct ImDrawList;
struct ViewSetup;
struct UserCmd;

namespace Visuals
{
	void shadowChanger() noexcept;
	void drawSmokeTimerEvent(GameEvent* event) noexcept;
	void drawSmokeTimer(ImDrawList* drawList) noexcept;
	void drawMolotovTimerEvent(GameEvent* event) noexcept;
	void molotovExtinguishEvent(GameEvent* event) noexcept;
	void drawMolotovTimer(ImDrawList* drawList) noexcept;
	void visualizeSpread(ImDrawList* drawList) noexcept;
	void drawAimbotFov(ImDrawList* drawList) noexcept;
	void fullBright() noexcept;
	void playerModel(FrameStage stage) noexcept;
	void modifySmoke(FrameStage stage) noexcept;
	void modifyMolotov(FrameStage stage) noexcept;
	void thirdperson() noexcept;
	void removeVisualRecoil(FrameStage stage) noexcept;
	void removeBlur(FrameStage stage) noexcept;
	void removeGrass(FrameStage stage) noexcept;
	void remove3dSky() noexcept;
	void removeShadows() noexcept;
	void applyZoom(FrameStage stage) noexcept;
	void applyScreenEffects() noexcept;
	void hitEffect(GameEvent* event = nullptr) noexcept;
	void transparentWorld(int resetType = -1) noexcept;
	void hitMarker(GameEvent* event, ImDrawList* drawList = nullptr) noexcept;
	void motionBlur(ViewSetup* setup) noexcept;
	void disablePostProcessing(FrameStage stage) noexcept;
	bool removeHands(const char* modelName) noexcept;
	bool removeSleeves(const char* modelName) noexcept;
	bool removeWeapons(const char* modelName) noexcept;
	void skybox(FrameStage stage) noexcept;
	void updateShots(UserCmd* cmd) noexcept;
	void bulletTracer(GameEvent& event) noexcept;
	void drawBulletImpacts() noexcept;
	void bulletImpact(GameEvent& event) noexcept;
	void drawHitboxMatrix(GameEvent* event = nullptr) noexcept;
	void drawMolotovHull(ImDrawList* drawList) noexcept;
	void drawSmokeHull(ImDrawList* drawList) noexcept;
	void drawMolotovPolygon(ImDrawList* draw_list) noexcept;
	void changeGlowThickness() noexcept;
	void footstepESP(GameEvent* event) noexcept;

	inline const std::array skyboxList{ xorstr_("Default"), xorstr_("cs_baggage_skybox_"), xorstr_("cs_tibet"), xorstr_("embassy"), xorstr_("italy"), xorstr_("jungle"), 
		xorstr_("nukeblank"), xorstr_("office"), xorstr_("sky_cs15_daylight01_hdr"), xorstr_("sky_cs15_daylight02_hdr"), xorstr_("sky_cs15_daylight03_hdr"), 
		xorstr_("sky_cs15_daylight04_hdr"), xorstr_("sky_csgo_cloudy01"), xorstr_("sky_csgo_night_flat"), xorstr_("sky_csgo_night02"), xorstr_("sky_day02_05_hdr"),
		xorstr_("sky_day02_05"), xorstr_("sky_dust"), xorstr_("sky_l4d_rural02_ldr"), xorstr_("sky_venice"), xorstr_("vertigo_hdr"), xorstr_("vertigo"), xorstr_("vertigoblue_hdr"), 
		xorstr_("vietnam"), xorstr_("sky_lunacy"), xorstr_("sky_hr_aztec"), xorstr_("Custom") };

	void updateEventListeners(bool forceRemove = false) noexcept;
	void updateInput() noexcept;
	void reset(int resetType) noexcept;
}
