#include <array>
#include <cwctype>
#include <fstream>
#include <functional>
#include <string>
#include <ShlObj.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_stdlib.h"

#include "imguiCustom.h"

#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Glow.h"
#include "Hacks/Misc.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Sound.h"
#include "Hacks/Visuals.h"

#include "GUI.h"
#include "Config.h"
#include "Helpers.h"
#include "Hooks.h"
#include "Interfaces.h"

#include "SDK/InputSystem.h"

#pragma optimize("", off) // Too many xorstrings to compile this in a reasonable amount of time with optimizations

constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

static ImFont* addFontFromVFONT(const std::string& path, float size, const ImWchar* glyphRanges, bool merge) noexcept
{
	auto file = Helpers::loadBinaryFile(path);
	if (!Helpers::decodeVFONT(file))
		return nullptr;

	ImFontConfig cfg;
	cfg.FontData = file.data();
	cfg.FontDataSize = file.size();
	cfg.FontDataOwnedByAtlas = false;
	cfg.MergeMode = merge;
	cfg.GlyphRanges = glyphRanges;
	cfg.SizePixels = size;

	return ImGui::GetIO().Fonts->AddFont(&cfg);
}

GUI::GUI() noexcept
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	style.AntiAliasedFill = true;
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	ImFontConfig cfg;
	cfg.SizePixels = 15.0f;

	if (PWSTR pathToFonts; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &pathToFonts))) {
		const std::filesystem::path path{ pathToFonts };
		CoTaskMemFree(pathToFonts);

		fonts.normal15px = io.Fonts->AddFontFromFileTTF((path / xorstr_("msyh.ttc")).string().c_str(), 15.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.normal15px)
			io.Fonts->AddFontDefault(&cfg);

		fonts.tahoma34 = io.Fonts->AddFontFromFileTTF((path / xorstr_("msyh.ttc")).string().c_str(), 34.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.tahoma34)
			io.Fonts->AddFontDefault(&cfg);

		fonts.tahoma28 = io.Fonts->AddFontFromFileTTF((path / xorstr_("msyh.ttc")).string().c_str(), 28.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.tahoma28)
			io.Fonts->AddFontDefault(&cfg);

		cfg.MergeMode = true;
		static constexpr ImWchar symbol[]{
			0x2605, 0x2605, // ★
			0
		};
		io.Fonts->AddFontFromFileTTF((path / xorstr_("seguisym.ttf")).string().c_str(), 15.0f, &cfg, symbol);
		cfg.MergeMode = false;
	}

	if (!fonts.normal15px)
		io.Fonts->AddFontDefault(&cfg);
	if (!fonts.tahoma28)
		io.Fonts->AddFontDefault(&cfg);
	if (!fonts.tahoma34)
		io.Fonts->AddFontDefault(&cfg);
	addFontFromVFONT(xorstr_("csgo/panorama/fonts/notosanskr-regular.vfont"), 15.0f, io.Fonts->GetGlyphRangesKorean(), true);
	addFontFromVFONT(xorstr_("csgo/panorama/fonts/notosanssc-regular.vfont"), 15.0f, io.Fonts->GetGlyphRangesChineseFull(), true);
	constexpr auto unicodeFontSize = 16.0f;
	fonts.unicodeFont = addFontFromVFONT(xorstr_("csgo/panorama/fonts/notosans-bold.vfont"), unicodeFontSize, Helpers::getFontGlyphRanges(), false);
	fonts.unicodeFont = addFontFromVFONT(xorstr_("csgo/panorama/fonts/notosans-bold.vfont"), unicodeFontSize, io.Fonts->GetGlyphRangesChineseFull(), false);
}

void GUI::render() noexcept
{
	renderGuiStyle();
}

#include "InputUtil.h"

static void hotkey3(const char* label, KeyBind& key, float samelineOffset = 0.0f, const ImVec2& size = { 100.0f, 0.0f }) noexcept
{
	const auto id = ImGui::GetID(label);
	ImGui::PushID(label);

	ImGui::TextUnformatted(label);
	ImGui::SameLine(samelineOffset);

	if (ImGui::GetActiveID() == id) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
		ImGui::Button(xorstr_("..."), size);
		ImGui::PopStyleColor();

		ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
		if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || key.setToPressedKey())
			ImGui::ClearActiveID();
	} else if (ImGui::Button(key.toString(), size)) {
		ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
	}

	ImGui::PopID();
}

ImFont* GUI::getTahoma28Font() const noexcept
{
	return fonts.tahoma28;
}

ImFont* GUI::getUnicodeFont() const noexcept
{
	return fonts.unicodeFont;
}

void GUI::handleToggle() noexcept
{
	if (config->misc.menuKey.isPressed()) {
		open = !open;
		if (!open)
			interfaces->inputSystem->resetInputState();
	}
}

static void menuBarItem(const char* name, bool& enabled) noexcept
{
	if (ImGui::MenuItem(name)) {
		enabled = true;
		ImGui::SetWindowFocus(name);
		ImGui::SetWindowPos(name, { 100.0f, 100.0f });
	}
}

void GUI::renderLegitbotWindow() noexcept
{
	static const char* hitboxes[]{ xorstr_("Head"),xorstr_("Chest"),xorstr_("Stomach"),xorstr_("Arms"),xorstr_("Legs"),xorstr_("Feet")};
	static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false, false };
	static std::string previewvalue = xorstr_("");
	bool once = false;

	ImGui::PushID(xorstr_("Key"));
	ImGui::hotkey2(xorstr_("Key"), config->legitbotKey);
	ImGui::PopID();
	ImGui::Separator();
	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo(xorstr_(""), &currentCategory, xorstr_("All\0Pistols\0Heavy\0SMG\0Rifles\0"));
	ImGui::PopID();
	ImGui::SameLine();
	static int currentWeapon{ 0 };
	ImGui::PushID(1);

	switch (currentCategory) {
	case 0:
		currentWeapon = 0;
		ImGui::NewLine();
		break;
	case 1: {
		static int currentPistol{ 0 };
		static const char* pistols[]{ xorstr_("All"), xorstr_("Glock-18"), xorstr_("P2000"), xorstr_("USP-S"), xorstr_("Dual Berettas"), xorstr_("P250"), xorstr_("Tec-9"), xorstr_("Five-Seven"), xorstr_("CZ-75"), xorstr_("Desert Eagle"), xorstr_("Revolver") };

		ImGui::Combo(xorstr_(""), &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(pistols));

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		static const char* heavies[]{ xorstr_("All"), xorstr_("Nova"), xorstr_("XM1014"), xorstr_("Sawed-off"), xorstr_("MAG-7"), xorstr_("M249"), xorstr_("Negev") };

		ImGui::Combo(xorstr_(""), &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(heavies));

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		static const char* smgs[]{ xorstr_("All"), xorstr_("Mac-10"), xorstr_("MP9"), xorstr_("MP7"), xorstr_("MP5-SD"), xorstr_("UMP-45"), xorstr_("P90"), xorstr_("PP-Bizon") };

		ImGui::Combo(xorstr_(""), &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(smgs));

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		static const char* rifles[]{ xorstr_("All"), xorstr_("Galil AR"), xorstr_("Famas"), xorstr_("AK-47"), xorstr_("M4A4"), xorstr_("M4A1-S"), xorstr_("SSG-08"), xorstr_("SG-553"), xorstr_("AUG"), xorstr_("AWP"), xorstr_("G3SG1"), xorstr_("SCAR-20") };

		ImGui::Combo(xorstr_(""), &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(rifles));

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Enabled"), &config->legitbot[currentWeapon].enabled);
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 220.0f);
	ImGui::Checkbox(xorstr_("Aimlock"), &config->legitbot[currentWeapon].aimlock);
	ImGui::Checkbox(xorstr_("Silent"), &config->legitbot[currentWeapon].silent);
	ImGuiCustom::colorPicker(xorstr_("Draw FOV"), config->legitbotFov);
	ImGui::Checkbox(xorstr_("Friendly fire"), &config->legitbot[currentWeapon].friendlyFire);
	ImGui::Checkbox(xorstr_("Visible only"), &config->legitbot[currentWeapon].visibleOnly);
	ImGui::Checkbox(xorstr_("Scoped only"), &config->legitbot[currentWeapon].scopedOnly);
	ImGui::Checkbox(xorstr_("Ignore flash"), &config->legitbot[currentWeapon].ignoreFlash);
	ImGui::Checkbox(xorstr_("Ignore smoke"), &config->legitbot[currentWeapon].ignoreSmoke);
	ImGui::Checkbox(xorstr_("Auto scope"), &config->legitbot[currentWeapon].autoScope);

	for (size_t i = 0; i < ARRAYSIZE(hitbox); i++) {
		hitbox[i] = (config->legitbot[currentWeapon].hitboxes & 1 << i) == 1 << i;
	}
	if (ImGui::BeginCombo(xorstr_("Hitbox"), previewvalue.c_str())) {
		previewvalue = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
			ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
		}
		ImGui::EndCombo();
	}
	for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
		if (!once) {
			previewvalue = xorstr_("");
			once = true;
		}
		if (hitbox[i]) {
			previewvalue += previewvalue.size() ? std::string(xorstr_(", ")) + hitboxes[i] : hitboxes[i];
			config->legitbot[currentWeapon].hitboxes |= 1 << i;
		} else {
			config->legitbot[currentWeapon].hitboxes &= ~(1 << i);
		}
	}

	ImGui::NextColumn();
	ImGui::PushItemWidth(240.0f);
	ImGui::SliderFloat(xorstr_("FOV"), &config->legitbot[currentWeapon].fov, 0.0f, 255.0f, xorstr_("%.2f"), ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat(xorstr_("Smoothing"), &config->legitbot[currentWeapon].smooth, 1.0f, 100.0f, xorstr_("%.2f"));
	ImGui::SliderInt(xorstr_("Reaction time"), &config->legitbot[currentWeapon].reactionTime, 0, 300, xorstr_("%d"));
	ImGui::SliderInt(xorstr_("Min damage"), &config->legitbot[currentWeapon].minDamage, 0, 100, xorstr_("%d"));
	config->legitbot[currentWeapon].minDamage = std::clamp(config->legitbot[currentWeapon].minDamage, 0, 250);
	ImGui::Checkbox(xorstr_("Killshot"), &config->legitbot[currentWeapon].killshot);
	ImGui::Checkbox(xorstr_("Between shots"), &config->legitbot[currentWeapon].betweenShots);
	ImGui::Checkbox(xorstr_("Recoil control system"), &config->recoilControlSystem.enabled);
	ImGui::PushID(xorstr_("RCS"));
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Silent"), &config->recoilControlSystem.silent);
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Ignored shots"), &config->recoilControlSystem.shotsFired, 0, 150, xorstr_("%d"));
		ImGui::SliderInt(xorstr_("Horizontal"), &config->recoilControlSystem.horizontal, 0, 100, xorstr_("%d%%"));
		ImGui::SliderInt(xorstr_("Vertical"), &config->recoilControlSystem.vertical, 0, 100, xorstr_("%d%%"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Columns(1);
}

void GUI::renderRagebotWindow() noexcept
{
	static const char* hitboxes[]{ xorstr_("Head"),xorstr_("Chest"),xorstr_("Stomach"),xorstr_("Arms"),xorstr_("Legs"),xorstr_("Feet") };
	static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false, false };
	static std::string previewvalue = xorstr_("");
	bool once = false;

	ImGui::PushID(xorstr_("Ragebot Key"));
	ImGui::hotkey2(xorstr_("Key"), config->ragebotKey);
	ImGui::PopID();
	ImGui::Separator();
	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo(xorstr_(""), &currentCategory, xorstr_("All\0Pistols\0Heavy\0SMG\0Rifles\0"));
	ImGui::PopID();
	ImGui::SameLine();
	static int currentWeapon{ 0 };
	ImGui::PushID(1);

	switch (currentCategory) {
	case 0:
		currentWeapon = 0;
		ImGui::NewLine();
		break;
	case 1: {
		static int currentPistol{ 0 };
		static const char* pistols[]{ xorstr_("All"), xorstr_("Glock-18"), xorstr_("P2000"), xorstr_("USP-S"), xorstr_("Dual Berettas"), xorstr_("P250"), xorstr_("Tec-9"), xorstr_("Five-Seven"), xorstr_("CZ-75"), xorstr_("Desert Eagle"), xorstr_("Revolver") };

		ImGui::Combo(xorstr_(""), &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(pistols));

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		static const char* heavies[]{ xorstr_("All"), xorstr_("Nova"), xorstr_("XM1014"), xorstr_("Sawed-off"), xorstr_("MAG-7"), xorstr_("M249"), xorstr_("Negev") };

		ImGui::Combo(xorstr_(""), &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(heavies));

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		static const char* smgs[]{ xorstr_("All"), xorstr_("Mac-10"), xorstr_("MP9"), xorstr_("MP7"), xorstr_("MP5-SD"), xorstr_("UMP-45"), xorstr_("P90"), xorstr_("PP-Bizon") };

		ImGui::Combo(xorstr_(""), &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(smgs));

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		static const char* rifles[]{ xorstr_("All"), xorstr_("Galil AR"), xorstr_("Famas"), xorstr_("AK-47"), xorstr_("M4A4"), xorstr_("M4A1-S"), xorstr_("SSG-08"), xorstr_("SG-553"), xorstr_("AUG"), xorstr_("AWP"), xorstr_("G3SG1"), xorstr_("SCAR-20") };

		ImGui::Combo(xorstr_(""), &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(rifles));

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Enabled"), &config->ragebot[currentWeapon].enabled);
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 220.0f);
	ImGui::Checkbox(xorstr_("Aimlock"), &config->ragebot[currentWeapon].aimlock);
	ImGui::Checkbox(xorstr_("Silent"), &config->ragebot[currentWeapon].silent);
	ImGui::Checkbox(xorstr_("Friendly fire"), &config->ragebot[currentWeapon].friendlyFire);
	ImGui::Checkbox(xorstr_("Visible only"), &config->ragebot[currentWeapon].visibleOnly);
	ImGui::Checkbox(xorstr_("Scoped only"), &config->ragebot[currentWeapon].scopedOnly);
	ImGui::Checkbox(xorstr_("Ignore flash"), &config->ragebot[currentWeapon].ignoreFlash);
	ImGui::Checkbox(xorstr_("Ignore smoke"), &config->ragebot[currentWeapon].ignoreSmoke);
	ImGui::Checkbox(xorstr_("Auto shot"), &config->ragebot[currentWeapon].autoShot);
	ImGui::Checkbox(xorstr_("Auto scope"), &config->ragebot[currentWeapon].autoScope);
	ImGui::Checkbox(xorstr_("Auto stop"), &config->ragebot[currentWeapon].autoStop);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Auto stop"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Full stop"), &config->ragebot[currentWeapon].fullStop);
		ImGui::Checkbox(xorstr_("Duck stop"), &config->ragebot[currentWeapon].duckStop);
		ImGui::EndPopup();
	}
	ImGui::Checkbox(xorstr_("Resolver"), &config->ragebot[currentWeapon].resolver);
	ImGui::Checkbox(xorstr_("Low performance mode"), &config->optimizations.lowPerformanceMode);
	ImGui::Checkbox(xorstr_("Between shots"), &config->ragebot[currentWeapon].betweenShots);
	ImGui::Combo(xorstr_("Priority"), &config->ragebot[currentWeapon].priority, xorstr_("Health\0Distance\0Fov\0"));

	for (size_t i = 0; i < ARRAYSIZE(hitbox); i++) {
		hitbox[i] = (config->ragebot[currentWeapon].hitboxes & 1 << i) == 1 << i;
	}
	if (ImGui::BeginCombo(xorstr_("Hitbox"), previewvalue.c_str())) {
		previewvalue = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
			ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_DontClosePopups);
		}
		ImGui::EndCombo();
	}
	for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
		if (!once) {
			previewvalue = xorstr_("");
			once = true;
		}
		if (hitbox[i]) {
			previewvalue += previewvalue.size() ? std::string(xorstr_(", ")) + hitboxes[i] : hitboxes[i];
			config->ragebot[currentWeapon].hitboxes |= 1 << i;
		} else {
			config->ragebot[currentWeapon].hitboxes &= ~(1 << i);
		}
	}

	ImGui::NextColumn();
	ImGui::PushItemWidth(240.0f);
	ImGui::SliderFloat(xorstr_("FOV"), &config->ragebot[currentWeapon].fov, 0.0f, 255.0f, xorstr_("%.2f"), ImGuiSliderFlags_Logarithmic);
	ImGui::SliderInt(xorstr_("Hitchance"), &config->ragebot[currentWeapon].hitChance, 0, 100, xorstr_("%d"));
	ImGui::SliderFloat(xorstr_("Accuracy boost"), &config->ragebot[currentWeapon].accuracyBoost, 0, 1.0f, xorstr_("%.2f"));
	ImGui::SliderInt(xorstr_("Head multipoint"), &config->ragebot[currentWeapon].headMultiPoint, 0, 100, xorstr_("%d"));
	ImGui::SliderInt(xorstr_("Body multipoint"), &config->ragebot[currentWeapon].bodyMultiPoint, 0, 100, xorstr_("%d"));
	ImGui::SliderInt(xorstr_("Min damage"), &config->ragebot[currentWeapon].minDamage, 0, 100, xorstr_("%d"));
	config->ragebot[currentWeapon].minDamage = std::clamp(config->ragebot[currentWeapon].minDamage, 0, 250);
	ImGui::PushID(xorstr_("Min damage override Key"));
	ImGui::hotkey2(xorstr_("Min damage override Key"), config->minDamageOverrideKey);
	ImGui::PopID();
	ImGui::SliderInt(xorstr_("Min damage override"), &config->ragebot[currentWeapon].minDamageOverride, 0, 101, xorstr_("%d"));
	config->ragebot[currentWeapon].minDamageOverride = std::clamp(config->ragebot[currentWeapon].minDamageOverride, 0, 250);

	ImGui::PushID(xorstr_("Doubletap"));
	ImGui::hotkey2(xorstr_("Doubletap"), config->tickbase.doubletap);
	ImGui::PopID();
	ImGui::PushID(xorstr_("Hide shots"));
	ImGui::hotkey2(xorstr_("Hide shots"), config->tickbase.hideshots);
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Teleport on shift"), &config->tickbase.teleport);
	ImGui::Checkbox(xorstr_("Onshot fakelag"), &config->tickbase.onshotFl);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Onshot fakelag"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Amount"), &config->tickbase.onshotFlAmount, 1, 52, xorstr_("%d"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Columns(1);
}

void GUI::renderTriggerbotWindow() noexcept
{
	static const char* hitboxes[]{ xorstr_("Head"),xorstr_("Chest"),xorstr_("Stomach"),xorstr_("Arms"),xorstr_("Legs"),xorstr_("Feet")};
	static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false, false };
	static std::string previewvalue = xorstr_("");

	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 350.0f);

	ImGui::hotkey2(xorstr_("Key"), config->triggerbotKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo(xorstr_(""), &currentCategory, xorstr_("All\0Pistols\0Heavy\0SMG\0Rifles\0Zeus x27\0"));
	ImGui::PopID();
	ImGui::SameLine();
	static int currentWeapon{ 0 };
	ImGui::PushID(1);
	switch (currentCategory) {
	case 0:
		currentWeapon = 0;
		ImGui::NewLine();
		break;
	case 5:
		currentWeapon = 39;
		ImGui::NewLine();
		break;

	case 1: {
		static int currentPistol{ 0 };
		static const char* pistols[]{ xorstr_("All"), xorstr_("Glock-18"), xorstr_("P2000"), xorstr_("USP-S"), xorstr_("Dual Berettas"), xorstr_("P250"), xorstr_("Tec-9"), xorstr_("Five-Seven"), xorstr_("CZ-75"), xorstr_("Desert Eagle"), xorstr_("Revolver") };

		ImGui::Combo(xorstr_(""), &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(pistols));

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		static const char* heavies[]{ xorstr_("All"), xorstr_("Nova"), xorstr_("XM1014"), xorstr_("Sawed-off"), xorstr_("MAG-7"), xorstr_("M249"), xorstr_("Negev") };

		ImGui::Combo(xorstr_(""), &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(heavies));

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		static const char* smgs[]{ xorstr_("All"), xorstr_("Mac-10"), xorstr_("MP9"), xorstr_("MP7"), xorstr_("MP5-SD"), xorstr_("UMP-45"), xorstr_("P90"), xorstr_("PP-Bizon") };

		ImGui::Combo(xorstr_(""), &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(smgs));

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		static const char* rifles[]{ xorstr_("All"), xorstr_("Galil AR"), xorstr_("Famas"), xorstr_("AK-47"), xorstr_("M4A4"), xorstr_("M4A1-S"), xorstr_("SSG-08"), xorstr_("SG-553"), xorstr_("AUG"), xorstr_("AWP"), xorstr_("G3SG1"), xorstr_("SCAR-20") };

		ImGui::Combo(xorstr_(""), &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(xorstr_(" *")).c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, IM_ARRAYSIZE(rifles));

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Enabled"), &config->triggerbot[currentWeapon].enabled);
	ImGui::Checkbox(xorstr_("Friendly fire"), &config->triggerbot[currentWeapon].friendlyFire);
	ImGui::Checkbox(xorstr_("Scoped only"), &config->triggerbot[currentWeapon].scopedOnly);
	ImGui::Checkbox(xorstr_("Ignore flash"), &config->triggerbot[currentWeapon].ignoreFlash);
	ImGui::Checkbox(xorstr_("Ignore smoke"), &config->triggerbot[currentWeapon].ignoreSmoke);
	ImGui::SetNextItemWidth(85.0f);

	for (size_t i = 0; i < ARRAYSIZE(hitbox); i++) {
		hitbox[i] = (config->triggerbot[currentWeapon].hitboxes & 1 << i) == 1 << i;
	}

	if (ImGui::BeginCombo(xorstr_("Hitbox"), previewvalue.c_str())) {
		previewvalue = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
			ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_DontClosePopups);
		}
		ImGui::EndCombo();
	}
	for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++) {
		if (i == 0)
			previewvalue = xorstr_("");

		if (hitbox[i]) {
			previewvalue += previewvalue.size() ? std::string(xorstr_(", ")) + hitboxes[i] : hitboxes[i];
			config->triggerbot[currentWeapon].hitboxes |= 1 << i;
		} else {
			config->triggerbot[currentWeapon].hitboxes &= ~(1 << i);
		}
	}
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Hitchance"), &config->triggerbot[currentWeapon].hitChance, 0, 100, xorstr_("%d"));
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Shot delay"), &config->triggerbot[currentWeapon].shotDelay, 0, 250, xorstr_("%d ms"));
	ImGui::SliderInt(xorstr_("Min damage"), &config->triggerbot[currentWeapon].minDamage, 0, 101, xorstr_("%d"));
	config->triggerbot[currentWeapon].minDamage = std::clamp(config->triggerbot[currentWeapon].minDamage, 0, 250);
	ImGui::Checkbox(xorstr_("Killshot"), &config->triggerbot[currentWeapon].killshot);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderFakelagWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo(xorstr_(""), &current_category, xorstr_("Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0"));
	ImGui::Checkbox(xorstr_("Enabled"), &config->fakelag[current_category].enabled);
	ImGui::Combo(xorstr_("Mode"), &config->fakelag[current_category].mode, xorstr_("Static\0Adaptive\0Random\0"));
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Limit"), &config->fakelag[current_category].limit, 1, 21, xorstr_("%d"));
	ImGui::SliderInt(xorstr_("Random min limit"), &config->fakelag[current_category].randomMinLimit, 1, 21, xorstr_("%d"));
	ImGui::PopItemWidth();
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderLegitAntiAimWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	ImGui::hotkey2(xorstr_("Invert Key"), config->legitAntiAim.invert, 80.0f);
	ImGui::Checkbox(xorstr_("Enabled"), &config->legitAntiAim.enabled);
	ImGui::Checkbox(xorstr_("Disable in freeze time"), &config->disableInFreezetime);
	ImGui::Checkbox(xorstr_("Extend"), &config->legitAntiAim.extend);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderRageAntiAimWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo(xorstr_(""), &current_category, xorstr_("Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0"));
	ImGui::Checkbox(xorstr_("Enabled"), &config->rageAntiAim[current_category].enabled);
	ImGui::Checkbox(xorstr_("Disable in freezetime"), &config->disableInFreezetime);
	ImGui::Combo(xorstr_("Pitch"), &config->rageAntiAim[current_category].pitch, xorstr_("Off\0Down\0Zero\0Up\0"));
	ImGui::Combo(xorstr_("Yaw base"), reinterpret_cast<int*>(&config->rageAntiAim[current_category].yawBase), xorstr_("Off\0Paranoia\0Backward\0Right\0Left\0Spin\0"));
	ImGui::Combo(xorstr_("Yaw modifier"), reinterpret_cast<int*>(&config->rageAntiAim[current_category].yawModifier), xorstr_("Off\0Jitter\0"));

	if (config->rageAntiAim[current_category].yawBase == Yaw::paranoia) {
		ImGui::SliderInt(xorstr_("Paranoia min"), &config->rageAntiAim[current_category].paranoiaMin, 0, 180, xorstr_("%d"));
		ImGui::SliderInt(xorstr_("Paranoia max"), &config->rageAntiAim[current_category].paranoiaMax, 0, 180, xorstr_("%d"));
	}

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Yaw add"), &config->rageAntiAim[current_category].yawAdd, -180, 180, xorstr_("%d"));
	ImGui::PopItemWidth();

	if (config->rageAntiAim[current_category].yawModifier == 1) //Jitter
		ImGui::SliderInt(xorstr_("Jitter yaw range"), &config->rageAntiAim[current_category].jitterRange, 0, 90, xorstr_("%d"));

	if (config->rageAntiAim[current_category].yawBase == Yaw::spin) {
		ImGui::PushItemWidth(220.0f);
		ImGui::SliderInt(xorstr_("Spin base"), &config->rageAntiAim[current_category].spinBase, -180, 180, xorstr_("%d"));
		ImGui::PopItemWidth();
	}

	ImGui::Checkbox(xorstr_("At targets"), &config->rageAntiAim[current_category].atTargets);
	ImGui::hotkey2(xorstr_("Auto direction"), config->autoDirection, 60.f);
	ImGui::hotkey2(xorstr_("Forward"), config->manualForward, 60.f);
	ImGui::hotkey2(xorstr_("Backward"), config->manualBackward, 60.f);
	ImGui::hotkey2(xorstr_("Right"), config->manualRight, 60.f);
	ImGui::hotkey2(xorstr_("Left"), config->manualLeft, 60.f);

	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderFakeAngleWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo(xorstr_(""), &current_category, xorstr_("Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0"));
	ImGui::hotkey2(xorstr_("Invert Key"), config->invert, 80.0f);
	ImGui::Checkbox(xorstr_("Enabled"), &config->fakeAngle[current_category].enabled);

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Left limit"), &config->fakeAngle[current_category].leftLimit, 0, 60, xorstr_("%d"));
	ImGui::PopItemWidth();

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt(xorstr_("Right limit"), &config->fakeAngle[current_category].rightLimit, 0, 60, xorstr_("%d"));
	ImGui::PopItemWidth();

	ImGui::Combo(xorstr_("Mode"), &config->fakeAngle[current_category].peekMode, xorstr_("Off\0Peek real\0Peek fake\0Jitter\0Switch\0"));
	ImGui::Combo(xorstr_("Lby mode"), &config->fakeAngle[current_category].lbyMode, xorstr_("Normal\0Opposite\0Sway\0"));
	ImGui::Checkbox(xorstr_("Roll"), &config->rageAntiAim[current_category].roll);
	if (config->rageAntiAim[current_category].roll) {
		ImGui::SliderInt(xorstr_("Roll add"), &config->rageAntiAim[current_category].rollAdd, -90, 90, xorstr_("%d"));
		ImGui::SliderInt(xorstr_("Roll jitter"), &config->rageAntiAim[current_category].rollOffset, -90, 90, xorstr_("%d"));

		ImGui::Checkbox(xorstr_("Using exploit pitch"), &config->rageAntiAim[current_category].exploitPitchSwitch);
		if (config->rageAntiAim[current_category].exploitPitchSwitch)
			ImGui::SliderInt(xorstr_("Exploit pitch"), &config->rageAntiAim[current_category].exploitPitch, -180, 180, xorstr_("%d"));
		else
			ImGui::SliderInt(xorstr_("Roll pitch"), &config->rageAntiAim[current_category].rollPitch, -180, 180, xorstr_("%d"));
		ImGui::Checkbox(xorstr_("Alternative roll"), &config->rageAntiAim[current_category].rollAlt);
	}

	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderBacktrackWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	ImGui::Checkbox(xorstr_("Enabled"), &config->backtrack.enabled);
	ImGui::Checkbox(xorstr_("Ignore smoke"), &config->backtrack.ignoreSmoke);
	ImGui::Checkbox(xorstr_("Ignore flash"), &config->backtrack.ignoreFlash);
	ImGui::PushItemWidth(200.0f);
	ImGui::SliderInt(xorstr_("Time limit"), &config->backtrack.timeLimit, 1, 200, xorstr_("%d ms"));
	ImGui::PopItemWidth();
	ImGui::NextColumn();
	ImGui::Checkbox(xorstr_("Enabled fake latency"), &config->backtrack.fakeLatency);
	ImGui::PushItemWidth(200.0f);
	ImGui::SliderInt(xorstr_("Ping"), &config->backtrack.fakeLatencyAmount, 1, 200, xorstr_("%d ms"));
	ImGui::PopItemWidth();
	ImGui::Columns(1);
}

void GUI::renderChamsWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.0f);
	ImGui::hotkey2(xorstr_("Key"), config->chamsKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);

	static int material = 1;

	if (ImGui::Combo(xorstr_(""), &currentCategory, xorstr_("Allies\0Enemies\0Planting\0Defusing\0Local player\0Weapons\0Hands\0Backtrack\0Sleeves\0Desync\0Ragdolls\0Fake lag\0")))
		material = 1;

	ImGui::PopID();

	ImGui::SameLine();

	if (material <= 1)
		ImGuiCustom::arrowButtonDisabled(xorstr_("##left"), ImGuiDir_Left);
	else if (ImGui::ArrowButton(xorstr_("##left"), ImGuiDir_Left))
		--material;

	ImGui::SameLine();
	ImGui::Text(xorstr_("%d"), material);

	const std::array categories{ xorstr_("Allies"), xorstr_("Enemies"), xorstr_("Planting"), xorstr_("Defusing"), xorstr_("Local player"), xorstr_("Weapons"), xorstr_("Hands"), xorstr_("Backtrack"), xorstr_("Sleeves"), xorstr_("Desync"), xorstr_("Ragdolls"), xorstr_("Fake lag")};

	ImGui::SameLine();

	if (material >= int(config->chams[categories[currentCategory]].materials.size()))
		ImGuiCustom::arrowButtonDisabled(xorstr_("##right"), ImGuiDir_Right);
	else if (ImGui::ArrowButton(xorstr_("##right"), ImGuiDir_Right))
		++material;

	ImGui::SameLine();

	auto& chams{ config->chams[categories[currentCategory]].materials[material - 1] };

	ImGui::Checkbox(xorstr_("Enabled"), &chams.enabled);
	ImGui::Separator();
	ImGui::Checkbox(xorstr_("Health based"), &chams.healthBased);
	ImGui::Checkbox(xorstr_("Blinking"), &chams.blinking);
	ImGui::Combo(xorstr_("Material"), &chams.material, xorstr_("Normal\0Flat\0Animated\0Platinum\0Glass\0Chrome\0Crystal\0Silver\0Gold\0Plastic\0Glow\0Pearlescent\0Metallic\0"));
	ImGui::Checkbox(xorstr_("Wireframe"), &chams.wireframe);
	ImGui::Checkbox(xorstr_("Cover"), &chams.cover);
	ImGui::Checkbox(xorstr_("Ignore-Z"), &chams.ignorez);
	ImGuiCustom::colorPicker(xorstr_("Color"), chams);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderGlowWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 500.f);
	ImGui::hotkey2(xorstr_("Key"), config->glowKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	const std::array categories{ xorstr_("Allies"), xorstr_("Enemies"), xorstr_("Planting"), xorstr_("Defusing"), xorstr_("Local Player"), xorstr_("Weapons"), xorstr_("C4"), xorstr_("Planted C4"), xorstr_("Chickens"), xorstr_("Defuse Kits"), xorstr_("Projectiles"), xorstr_("Hostages") };
	ImGui::Combo(xorstr_(""), &currentCategory, categories.data(), categories.size());
	ImGui::PopID();
	Config::GlowItem* currentItem;
	static int currentType{ 0 };
	if (currentCategory <= 3) {
		ImGui::SameLine();
		ImGui::PushID(1);
		ImGui::Combo(xorstr_(""), &currentType, xorstr_("All\0Visible\0Occluded\0"));
		ImGui::PopID();
		auto& cfg = config->playerGlow[categories[currentCategory]];
		switch (currentType) {
		case 0: currentItem = &cfg.all; break;
		case 1: currentItem = &cfg.visible; break;
		case 2: currentItem = &cfg.occluded; break;
		}
	} else {
		currentItem = &config->glow[categories[currentCategory]];
	}

	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Enabled"), &currentItem->enabled);
	ImGui::Separator();
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 150.0f);
	ImGui::Checkbox(xorstr_("Health based"), &currentItem->healthBased);
	if (currentType == 2 && currentCategory <= 3) ImGui::Checkbox(xorstr_("Audible only"), &currentItem->audibleOnly);

	ImGuiCustom::colorPicker(xorstr_("Color"), *currentItem);

	ImGui::NextColumn();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::Combo(xorstr_("Style"), &currentItem->style, xorstr_("Default\0Rim3d\0Edge\0Edge Pulse\0"));

	ImGui::Columns(1);
}

void GUI::renderStreamProofESPWindow() noexcept
{
	ImGui::hotkey2(xorstr_("Key"), config->streamProofESP.key, 80.0f);
	ImGui::Separator();

	static std::size_t currentCategory;
	static auto currentItem = xorstr_("All");

	constexpr auto getConfigShared = [](std::size_t category, const char* item) noexcept -> Shared& {
		switch (category) {
		case 0: default: return config->streamProofESP.enemies[item];
		case 1: return config->streamProofESP.allies[item];
		case 2: return config->streamProofESP.weapons[item];
		case 3: return config->streamProofESP.projectiles[item];
		case 4: return config->streamProofESP.lootCrates[item];
		case 5: return config->streamProofESP.otherEntities[item];
		}
	};

	constexpr auto getConfigPlayer = [](std::size_t category, const char* item) noexcept -> Player& {
		switch (category) {
		case 0: default: return config->streamProofESP.enemies[item];
		case 1: return config->streamProofESP.allies[item];
		}
	};

	if (ImGui::BeginListBox(xorstr_("##list"), { 170.0f, 300.0f })) {
		const std::array categories{ xorstr_("Enemies"), xorstr_("Allies"), xorstr_("Weapons"), xorstr_("Projectiles"), xorstr_("Loot Crates"), xorstr_("Other Entities") };

		for (std::size_t i = 0; i < categories.size(); ++i) {
			if (ImGui::Selectable(categories[i], currentCategory == i && std::string_view{ currentItem } == xorstr_("All"))) {
				currentCategory = i;
				currentItem = xorstr_("All");
			}

			if (ImGui::BeginDragDropSource()) {
				switch (i) {
				case 0: case 1: ImGui::SetDragDropPayload(xorstr_("Player"), &getConfigPlayer(i, xorstr_("All")), sizeof(Player), ImGuiCond_Once); break;
				case 2: ImGui::SetDragDropPayload(xorstr_("Weapon"), &config->streamProofESP.weapons[xorstr_("All")], sizeof(Weapon), ImGuiCond_Once); break;
				case 3: ImGui::SetDragDropPayload(xorstr_("Projectile"), &config->streamProofESP.projectiles[xorstr_("All")], sizeof(Projectile), ImGuiCond_Once); break;
				default: ImGui::SetDragDropPayload(xorstr_("Entity"), &getConfigShared(i, xorstr_("All")), sizeof(Shared), ImGuiCond_Once); break;
				}
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Player"))) {
					const auto& data = *(Player*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, xorstr_("All")) = data; break;
					case 2: config->streamProofESP.weapons[xorstr_("All")] = data; break;
					case 3: config->streamProofESP.projectiles[xorstr_("All")] = data; break;
					default: getConfigShared(i, xorstr_("All")) = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Weapon"))) {
					const auto& data = *(Weapon*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, xorstr_("All")) = data; break;
					case 2: config->streamProofESP.weapons[xorstr_("All")] = data; break;
					case 3: config->streamProofESP.projectiles[xorstr_("All")] = data; break;
					default: getConfigShared(i, xorstr_("All")) = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Projectile"))) {
					const auto& data = *(Projectile*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, xorstr_("All")) = data; break;
					case 2: config->streamProofESP.weapons[xorstr_("All")] = data; break;
					case 3: config->streamProofESP.projectiles[xorstr_("All")] = data; break;
					default: getConfigShared(i, xorstr_("All")) = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Entity"))) {
					const auto& data = *(Shared*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, xorstr_("All")) = data; break;
					case 2: config->streamProofESP.weapons[xorstr_("All")] = data; break;
					case 3: config->streamProofESP.projectiles[xorstr_("All")] = data; break;
					default: getConfigShared(i, xorstr_("All")) = data; break;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PushID(i);
			ImGui::Indent();

			const auto items = [](std::size_t category) noexcept -> std::vector<char*> {
				switch (category) {
				case 0:
				case 1: return { xorstr_("Visible"), xorstr_("Occluded") };
				case 2: return { xorstr_("Pistols"), xorstr_("SMGs"), xorstr_("Rifles"), xorstr_("Sniper Rifles"), xorstr_("Shotguns"), xorstr_("Machineguns"), xorstr_("Grenades"), xorstr_("Melee"), xorstr_("Other") };
				case 3: return { xorstr_("Flashbang"), xorstr_("HE Grenade"), xorstr_("Breach Charge"), xorstr_("Bump Mine"), xorstr_("Decoy Grenade"), xorstr_("Molotov"), xorstr_("TA Grenade"), xorstr_("Smoke Grenade"), xorstr_("Snowball") };
				case 4: return { xorstr_("Pistol Case"), xorstr_("Light Case"), xorstr_("Heavy Case"), xorstr_("Explosive Case"), xorstr_("Tools Case"), xorstr_("Cash Dufflebag") };
				case 5: return { xorstr_("Defuse Kit"), xorstr_("Chicken"), xorstr_("Planted C4"), xorstr_("Hostage"), xorstr_("Sentry"), xorstr_("Cash"), xorstr_("Ammo Box"), xorstr_("Radar Jammer"), xorstr_("Snowball Pile"), xorstr_("Collectable Coin") };
				default: return { };
				}
			}(i);

			const auto categoryEnabled = getConfigShared(i, xorstr_("All")).enabled;

			for (std::size_t j = 0; j < items.size(); ++j) {
				static bool selectedSubItem;
				if (!categoryEnabled || getConfigShared(i, items[j]).enabled) {
					if (ImGui::Selectable(items[j], currentCategory == i && !selectedSubItem && std::string_view{ currentItem } == items[j])) {
						currentCategory = i;
						currentItem = items[j];
						selectedSubItem = false;
					}

					if (ImGui::BeginDragDropSource()) {
						switch (i) {
						case 0: case 1: ImGui::SetDragDropPayload(xorstr_("Player"), &getConfigPlayer(i, items[j]), sizeof(Player), ImGuiCond_Once); break;
						case 2: ImGui::SetDragDropPayload(xorstr_("Weapon"), &config->streamProofESP.weapons[items[j]], sizeof(Weapon), ImGuiCond_Once); break;
						case 3: ImGui::SetDragDropPayload(xorstr_("Projectile"), &config->streamProofESP.projectiles[items[j]], sizeof(Projectile), ImGuiCond_Once); break;
						default: ImGui::SetDragDropPayload(xorstr_("Entity"), &getConfigShared(i, items[j]), sizeof(Shared), ImGuiCond_Once); break;
						}
						ImGui::EndDragDropSource();
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Player"))) {
							const auto& data = *(Player*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Weapon"))) {
							const auto& data = *(Weapon*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Projectile"))) {
							const auto& data = *(Projectile*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Entity"))) {
							const auto& data = *(Shared*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}
						ImGui::EndDragDropTarget();
					}
				}

				if (i != 2)
					continue;

				ImGui::Indent();

				const auto subItems = [](std::size_t item) noexcept -> std::vector<char*> {
					switch (item) {
					case 0: return { xorstr_("Glock-18"), xorstr_("P2000"), xorstr_("USP-S"), xorstr_("Dual Berettas"), xorstr_("P250"), xorstr_("Tec-9"), xorstr_("Five-SeveN"), xorstr_("CZ75-Auto"), xorstr_("Desert Eagle"), xorstr_("R8 Revolver") };
					case 1: return { xorstr_("MAC-10"), xorstr_("MP9"), xorstr_("MP7"), xorstr_("MP5-SD"), xorstr_("UMP-45"), xorstr_("P90"), xorstr_("PP-Bizon") };
					case 2: return { xorstr_("Galil AR"), xorstr_("FAMAS"), xorstr_("AK-47"), xorstr_("M4A4"), xorstr_("M4A1-S"), xorstr_("SG 553"), xorstr_("AUG") };
					case 3: return { xorstr_("SSG 08"), xorstr_("AWP"), xorstr_("G3SG1"), xorstr_("SCAR-20") };
					case 4: return { xorstr_("Nova"), xorstr_("XM1014"), xorstr_("Sawed-Off"), xorstr_("MAG-7") };
					case 5: return { xorstr_("M249"), xorstr_("Negev") };
					case 6: return { xorstr_("Flashbang"), xorstr_("HE Grenade"), xorstr_("Smoke Grenade"), xorstr_("Molotov"), xorstr_("Decoy Grenade"), xorstr_("Incendiary"), xorstr_("TA Grenade"), xorstr_("Fire Bomb"), xorstr_("Diversion"), xorstr_("Frag Grenade"), xorstr_("Snowball") };
					case 7: return { xorstr_("Axe"), xorstr_("Hammer"), xorstr_("Wrench") };
					case 8: return { xorstr_("C4"), xorstr_("Healthshot"), xorstr_("Bump Mine"), xorstr_("Zone Repulsor"), xorstr_("Shield") };
					default: return { };
					}
				}(j);

				const auto itemEnabled = getConfigShared(i, items[j]).enabled;

				for (const auto subItem : subItems) {
					auto& subItemConfig = config->streamProofESP.weapons[subItem];
					if ((categoryEnabled || itemEnabled) && !subItemConfig.enabled)
						continue;

					if (ImGui::Selectable(subItem, currentCategory == i && selectedSubItem && std::string_view{ currentItem } == subItem)) {
						currentCategory = i;
						currentItem = subItem;
						selectedSubItem = true;
					}

					if (ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(xorstr_("Weapon"), &subItemConfig, sizeof(Weapon), ImGuiCond_Once);
						ImGui::EndDragDropSource();
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Player"))) {
							const auto& data = *(Player*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Weapon"))) {
							const auto& data = *(Weapon*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Projectile"))) {
							const auto& data = *(Projectile*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(xorstr_("Entity"))) {
							const auto& data = *(Shared*)payload->Data;
							subItemConfig = data;
						}
						ImGui::EndDragDropTarget();
					}
				}

				ImGui::Unindent();
			}
			ImGui::Unindent();
			ImGui::PopID();
		}
		ImGui::EndListBox();
	}

	ImGui::SameLine();
	if (ImGui::BeginChild(xorstr_("##child"), { 400.0f, 0.0f })) {
		auto& sharedConfig = getConfigShared(currentCategory, currentItem);

		ImGui::Checkbox(xorstr_("Enabled"), &sharedConfig.enabled);
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 260.0f);
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo(xorstr_("Font"), config->getSystemFonts()[sharedConfig.font.index].c_str())) {
			for (size_t i = 0; i < config->getSystemFonts().size(); i++) {
				bool isSelected = config->getSystemFonts()[i] == sharedConfig.font.name;
				if (ImGui::Selectable(config->getSystemFonts()[i].c_str(), isSelected, 0, { 250.0f, 0.0f })) {
					sharedConfig.font.index = i;
					sharedConfig.font.name = config->getSystemFonts()[i];
					config->scheduleFontLoad(sharedConfig.font.name);
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		constexpr auto spacing = 250.0f;
		ImGuiCustom::colorPicker(xorstr_("Snapline"), sharedConfig.snapline);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::Combo(xorstr_("##1"), &sharedConfig.snapline.type, xorstr_("Bottom\0Top\0Crosshair\0"));
		ImGui::SameLine(spacing);
		ImGuiCustom::colorPicker(xorstr_("Box"), sharedConfig.box);
		ImGui::SameLine();

		ImGui::PushID(xorstr_("Box"));

		if (ImGui::Button(xorstr_("...")))
			ImGui::OpenPopup(xorstr_(""));

		if (ImGui::BeginPopup(xorstr_(""))) {
			ImGui::SetNextItemWidth(95.0f);
			ImGui::Combo(xorstr_("Type"), &sharedConfig.box.type, xorstr_("2D\0""2D corners\0""3D\0""3D corners\0"));
			ImGui::SetNextItemWidth(275.0f);
			ImGui::SliderFloat3(xorstr_("Scale"), sharedConfig.box.scale.data(), 0.0f, 0.50f, xorstr_("%.2f"));
			ImGuiCustom::colorPicker(xorstr_("Fill"), sharedConfig.box.fill);
			ImGui::EndPopup();
		}

		ImGui::PopID();

		ImGuiCustom::colorPicker(xorstr_("Name"), sharedConfig.name);
		ImGui::SameLine(spacing);

		if (currentCategory < 2) {
			auto& playerConfig = getConfigPlayer(currentCategory, currentItem);

			ImGuiCustom::colorPicker(xorstr_("Weapon"), playerConfig.weapon);
			ImGuiCustom::colorPicker(xorstr_("Flash Duration"), playerConfig.flashDuration);
			ImGui::SameLine(spacing);
			ImGuiCustom::colorPicker(xorstr_("Skeleton"), playerConfig.skeleton);
			ImGui::Checkbox(xorstr_("Audible Only"), &playerConfig.audibleOnly);
			ImGui::SameLine(spacing);
			ImGui::Checkbox(xorstr_("Spotted Only"), &playerConfig.spottedOnly);

			ImGuiCustom::colorPicker(xorstr_("Head Box"), playerConfig.headBox);
			ImGui::SameLine();

			ImGui::PushID(xorstr_("Head Box"));

			if (ImGui::Button(xorstr_("...")))
				ImGui::OpenPopup(xorstr_(""));

			if (ImGui::BeginPopup(xorstr_(""))) {
				ImGui::SetNextItemWidth(95.0f);
				ImGui::Combo(xorstr_("Type"), &playerConfig.headBox.type, xorstr_("2D\0""2D corners\0""3D\0""3D corners\0"));
				ImGui::SetNextItemWidth(275.0f);
				ImGui::SliderFloat3(xorstr_("Scale"), playerConfig.headBox.scale.data(), 0.0f, 0.50f, xorstr_("%.2f"));
				ImGuiCustom::colorPicker(xorstr_("Fill"), playerConfig.headBox.fill);
				ImGui::EndPopup();
			}

			ImGui::PopID();

			ImGui::SameLine(spacing);
			ImGui::Checkbox(xorstr_("Health Bar"), &playerConfig.healthBar.enabled);
			ImGui::SameLine();

			ImGui::PushID(xorstr_("Health Bar"));

			if (ImGui::Button(xorstr_("...")))
				ImGui::OpenPopup(xorstr_(""));

			if (ImGui::BeginPopup(xorstr_(""))) {
				ImGui::SetNextItemWidth(95.0f);
				ImGui::Combo(xorstr_("Type"), &playerConfig.healthBar.type, xorstr_("Gradient\0Solid\0Health-based\0"));
				if (playerConfig.healthBar.type == HealthBar::Solid) {
					ImGui::SameLine();
					ImGuiCustom::colorPicker(xorstr_(""), static_cast<Color4&>(playerConfig.healthBar));
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();

			ImGuiCustom::colorPicker(xorstr_("Footsteps"), config->visuals.footsteps.footstepBeams);
			ImGui::SliderInt(xorstr_("Thickness"), &config->visuals.footsteps.footstepBeamThickness, 0, 30, xorstr_("Thickness: %d%%"));
			ImGui::SliderInt(xorstr_("Radius"), &config->visuals.footsteps.footstepBeamRadius, 0, 230, xorstr_("Radius: %d%%"));

			ImGuiCustom::colorPicker(xorstr_("Line of sight"), playerConfig.lineOfSight);

		} else if (currentCategory == 2) {
			auto& weaponConfig = config->streamProofESP.weapons[currentItem];
			ImGuiCustom::colorPicker(xorstr_("Ammo"), weaponConfig.ammo);
		} else if (currentCategory == 3) {
			auto& trails = config->streamProofESP.projectiles[currentItem].trails;

			ImGui::Checkbox(xorstr_("Trails"), &trails.enabled);
			ImGui::SameLine(spacing + 77.0f);
			ImGui::PushID(xorstr_("Trails"));

			if (ImGui::Button(xorstr_("...")))
				ImGui::OpenPopup(xorstr_(""));

			if (ImGui::BeginPopup(xorstr_(""))) {
				constexpr auto trailPicker = [](const char* name, Trail& trail) noexcept {
					ImGui::PushID(name);
					ImGuiCustom::colorPicker(name, trail);
					ImGui::SameLine(150.0f);
					ImGui::SetNextItemWidth(95.0f);
					ImGui::Combo(xorstr_(""), &trail.type, xorstr_("Line\0Circles\0Filled Circles\0"));
					ImGui::SameLine();
					ImGui::SetNextItemWidth(95.0f);
					ImGui::InputFloat(xorstr_("Time"), &trail.time, 0.1f, 0.5f, xorstr_("%.1fs"));
					trail.time = std::clamp(trail.time, 1.0f, 60.0f);
					ImGui::PopID();
				};

				trailPicker(xorstr_("Local Player"), trails.localPlayer);
				trailPicker(xorstr_("Allies"), trails.allies);
				trailPicker(xorstr_("Enemies"), trails.enemies);
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::SetNextItemWidth(95.0f);
		ImGui::InputFloat(xorstr_("Text Cull Distance"), &sharedConfig.textCullDistance, 0.4f, 0.8f, xorstr_("%.1fm"));
		sharedConfig.textCullDistance = std::clamp(sharedConfig.textCullDistance, 0.0f, 999.9f);
	}

	ImGui::EndChild();
}

void GUI::renderVisualsWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 280.0f);
	const auto playerModels = xorstr_("Default\0Special Agent Ava | FBI\0Operator | FBI SWAT\0Markus Delrow | FBI HRT\0Michael Syfers | FBI Sniper\0B Squadron Officer | SAS\0Seal Team 6 Soldier | NSWC SEAL\0Buckshot | NSWC SEAL\0Lt. Commander Ricksaw | NSWC SEAL\0Third Commando Company | KSK\0'Two Times' McCoy | USAF TACP\0Dragomir | Sabre\0Rezan The Ready | Sabre\0'The Doctor' Romanov | Sabre\0Maximus | Sabre\0Blackwolf | Sabre\0The Elite Mr. Muhlik | Elite Crew\0Ground Rebel | Elite Crew\0Osiris | Elite Crew\0Prof. Shahmat | Elite Crew\0Enforcer | Phoenix\0Slingshot | Phoenix\0Soldier | Phoenix\0Pirate\0Pirate Variant A\0Pirate Variant B\0Pirate Variant C\0Pirate Variant D\0Anarchist\0Anarchist Variant A\0Anarchist Variant B\0Anarchist Variant C\0Anarchist Variant D\0Balkan Variant A\0Balkan Variant B\0Balkan Variant C\0Balkan Variant D\0Balkan Variant E\0Jumpsuit Variant A\0Jumpsuit Variant B\0Jumpsuit Variant C\0GIGN\0GIGN Variant A\0GIGN Variant B\0GIGN Variant C\0GIGN Variant D\0Street Soldier | Phoenix\0'Blueberries' Buckshot | NSWC SEAL\0'Two Times' McCoy | TACP Cavalry\0Rezan the Redshirt | Sabre\0Dragomir | Sabre Footsoldier\0Cmdr. Mae 'Dead Cold' Jamison | SWAT\0001st Lieutenant Farlow | SWAT\0John 'Van Healen' Kask | SWAT\0Bio-Haz Specialist | SWAT\0Sergeant Bombson | SWAT\0Chem-Haz Specialist | SWAT\0Sir Bloody Miami Darryl | The Professionals\0Sir Bloody Silent Darryl | The Professionals\0Sir Bloody Skullhead Darryl | The Professionals\0Sir Bloody Darryl Royale | The Professionals\0Sir Bloody Loudmouth Darryl | The Professionals\0Safecracker Voltzmann | The Professionals\0Little Kev | The Professionals\0Number K | The Professionals\0Getaway Sally | The Professionals\0");
	ImGui::Combo(xorstr_("T model"), &config->visuals.playerModelT, playerModels);
	ImGui::Combo(xorstr_("CT model"), &config->visuals.playerModelCT, playerModels);
	ImGui::InputText(xorstr_("Custom model"), config->visuals.playerModel, sizeof(config->visuals.playerModel));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(xorstr_("file must be in csgo/models/ directory and must start with models/..."));
	ImGui::Checkbox(xorstr_("Disable post-processing"), &config->visuals.disablePostProcessing);
	ImGui::Checkbox(xorstr_("Disable jiggle bones"), &config->visuals.disableJiggleBones);
	ImGui::Checkbox(xorstr_("Inverse ragdoll gravity"), &config->visuals.inverseRagdollGravity);
	ImGui::Checkbox(xorstr_("No fog"), &config->visuals.noFog);

	ImGuiCustom::colorPicker(xorstr_("Fog controller"), config->visuals.fog);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Fog controller"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {

		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(7);
		ImGui::SliderFloat(xorstr_("Start"), &config->visuals.fogOptions.start, 0.0f, 5000.0f, xorstr_("Start: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(8);
		ImGui::SliderFloat(xorstr_("End"), &config->visuals.fogOptions.end, 0.0f, 5000.0f, xorstr_("End: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(9);
		ImGui::SliderFloat(xorstr_("Density"), &config->visuals.fogOptions.density, 0.001f, 1.0f, xorstr_("Density: %.3f"));
		ImGui::PopID();

		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("No 3D sky"), &config->visuals.no3dSky);
	ImGui::Checkbox(xorstr_("No aim punch"), &config->visuals.noAimPunch);
	ImGui::Checkbox(xorstr_("No view punch"), &config->visuals.noViewPunch);
	ImGui::Checkbox(xorstr_("No view bob"), &config->visuals.noViewBob);
	ImGui::Checkbox(xorstr_("No hands"), &config->visuals.noHands);
	ImGui::Checkbox(xorstr_("No sleeves"), &config->visuals.noSleeves);
	ImGui::Checkbox(xorstr_("No weapons"), &config->visuals.noWeapons);
	ImGui::Checkbox(xorstr_("No smoke"), &config->visuals.noSmoke);
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Wireframe smoke"), &config->visuals.wireframeSmoke);
	ImGui::Checkbox(xorstr_("No molotov"), &config->visuals.noMolotov);
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Wireframe molotov"), &config->visuals.wireframeMolotov);
	ImGui::Checkbox(xorstr_("No blur"), &config->visuals.noBlur);
	ImGui::Checkbox(xorstr_("No scope overlay"), &config->visuals.noScopeOverlay);
	ImGui::Checkbox(xorstr_("No scope zoom"), &config->visuals.keepFov);
	ImGui::Checkbox(xorstr_("No grass"), &config->visuals.noGrass);
	ImGui::Checkbox(xorstr_("No shadows"), &config->visuals.noShadows);

	ImGui::Checkbox(xorstr_("Shadow changer"), &config->visuals.shadowsChanger.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Shadow changer"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(5);
		ImGui::SliderInt(xorstr_(""), &config->visuals.shadowsChanger.x, 0, 360, xorstr_("x rotation: %d"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(6);
		ImGui::SliderInt(xorstr_(""), &config->visuals.shadowsChanger.y, 0, 360, xorstr_("y rotation: %d"));
		ImGui::PopID();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Motion Blur"), &config->visuals.motionBlur.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Motion Blur"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {

		ImGui::Checkbox(xorstr_("Forward enabled"), &config->visuals.motionBlur.forwardEnabled);

		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(10);
		ImGui::SliderFloat(xorstr_("Falling min"), &config->visuals.motionBlur.fallingMin, 0.0f, 50.0f, xorstr_("Falling min: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(11);
		ImGui::SliderFloat(xorstr_("Falling max"), &config->visuals.motionBlur.fallingMax, 0.0f, 50.0f, xorstr_("Falling max: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat(xorstr_("Falling intesity"), &config->visuals.motionBlur.fallingIntensity, 0.0f, 8.0f, xorstr_("Falling intensity: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat(xorstr_("Rotation intensity"), &config->visuals.motionBlur.rotationIntensity, 0.0f, 8.0f, xorstr_("Rotation intensity: %.2f"));
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat(xorstr_("Strength"), &config->visuals.motionBlur.strength, 0.0f, 8.0f, xorstr_("Strength: %.2f"));
		ImGui::PopID();

		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Full bright"), &config->visuals.fullBright);
	ImGui::NextColumn();
	ImGui::Checkbox(xorstr_("Zoom"), &config->visuals.zoom);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Zoom Key"));
	ImGui::hotkey2(xorstr_(""), config->visuals.zoomKey);
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Thirdperson"), &config->visuals.thirdperson);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Thirdperson Key"));
	ImGui::hotkey2(xorstr_(""), config->visuals.thirdpersonKey);
	ImGui::PopID();
	ImGui::PushItemWidth(290.0f);
	ImGui::PushID(0);
	ImGui::SliderInt(xorstr_(""), &config->visuals.thirdpersonDistance, 0, 1000, xorstr_("Thirdperson distance: %d"));
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Freecam"), &config->visuals.freeCam);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Freecam Key"));
	ImGui::hotkey2(xorstr_(""), config->visuals.freeCamKey);
	ImGui::PopID();
	ImGui::PushItemWidth(290.0f);
	ImGui::PushID(1);
	ImGui::SliderInt(xorstr_(""), &config->visuals.freeCamSpeed, 1, 10, xorstr_("Freecam speed: %d"));
	ImGui::PopID();
	ImGui::PushID(2);
	ImGui::SliderInt(xorstr_(""), &config->visuals.fov, -60, 60, xorstr_("FOV: %d"));
	ImGui::PopID();
	ImGui::PushID(3);
	ImGui::SliderInt(xorstr_(""), &config->visuals.farZ, 0, 2000, xorstr_("Far Z: %d"));
	ImGui::PopID();
	ImGui::PushID(4);
	ImGui::SliderInt(xorstr_(""), &config->visuals.flashReduction, 0, 100, xorstr_("Flash reduction: %d%%"));
	ImGui::PopID();
	ImGui::PushID(5);
	ImGui::SliderFloat(xorstr_(""), &config->visuals.glowOutlineWidth, 0.0f, 100.0f, xorstr_("Glow thickness: %.2f"));
	ImGui::PopID();
	ImGui::Combo(xorstr_("Skybox"), &config->visuals.skybox, Visuals::skyboxList.data(), Visuals::skyboxList.size());
	if (config->visuals.skybox == 26) {
		ImGui::InputText(xorstr_("Skybox filename"), &config->visuals.customSkybox);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(xorstr_("skybox files must be put in csgo/materials/skybox/ "));
	}
	ImGuiCustom::colorPicker(xorstr_("World color"), config->visuals.world);
	ImGuiCustom::colorPicker(xorstr_("Props color"), config->visuals.props);
	ImGuiCustom::colorPicker(xorstr_("Sky color"), config->visuals.sky);
	ImGui::PushID(13);
	ImGui::SliderInt(xorstr_(""), &config->visuals.asusWalls, 0, 100, xorstr_("Asus walls: %d"));
	ImGui::PopID();
	ImGui::PushID(14);
	ImGui::SliderInt(xorstr_(""), &config->visuals.asusProps, 0, 100, xorstr_("Asus props: %d"));
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Deagle spinner"), &config->visuals.deagleSpinner);
	ImGui::Combo(xorstr_("Screen effect"), &config->visuals.screenEffect, xorstr_("None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0"));
	ImGui::Combo(xorstr_("Hit effect"), &config->visuals.hitEffect, xorstr_("None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0"));
	ImGui::SliderFloat(xorstr_("Hit effect time"), &config->visuals.hitEffectTime, 0.1f, 1.5f, xorstr_("%.2fs"));
	ImGui::Combo(xorstr_("Hit marker"), &config->visuals.hitMarker, xorstr_("None\0Default (Cross)\0"));
	ImGui::SliderFloat(xorstr_("Hit marker time"), &config->visuals.hitMarkerTime, 0.1f, 1.5f, xorstr_("%.2fs"));
	ImGuiCustom::colorPicker(xorstr_("Bullet Tracers"), config->visuals.bulletTracers.color.data(), &config->visuals.bulletTracers.color[3], nullptr, nullptr, &config->visuals.bulletTracers.enabled);
	ImGuiCustom::colorPicker(xorstr_("Bullet Impacts"), config->visuals.bulletImpacts.color.data(), &config->visuals.bulletImpacts.color[3], nullptr, nullptr, &config->visuals.bulletImpacts.enabled);
	ImGui::SliderFloat(xorstr_("Bullet Impacts time"), &config->visuals.bulletImpactsTime, 0.1f, 5.0f, xorstr_("Bullet Impacts time: %.2fs"));
	ImGuiCustom::colorPicker(xorstr_("On Hit Hitbox"), config->visuals.onHitHitbox.color.color.data(), &config->visuals.onHitHitbox.color.color[3], nullptr, nullptr, &config->visuals.onHitHitbox.color.enabled);
	ImGui::SliderFloat(xorstr_("On Hit Hitbox Time"), &config->visuals.onHitHitbox.duration, 0.1f, 60.0f, xorstr_("On Hit Hitbox time: % .2fs"));
	ImGuiCustom::colorPicker(xorstr_("Molotov Hull"), config->visuals.molotovHull);
	ImGuiCustom::colorPicker(xorstr_("Smoke Hull"), config->visuals.smokeHull);
	ImGui::Checkbox(xorstr_("Molotov Polygon"), &config->visuals.molotovPolygon.enabled);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...##molotov_polygon")))
		ImGui::OpenPopup(xorstr_("popup_molotovPolygon"));

	if (ImGui::BeginPopup(xorstr_("popup_molotovPolygon"))) {
		ImGuiCustom::colorPicker(xorstr_("Self"), config->visuals.molotovPolygon.self.color.data(), &config->visuals.molotovPolygon.self.color[3], &config->visuals.molotovPolygon.self.rainbow, &config->visuals.molotovPolygon.self.rainbowSpeed, nullptr);
		ImGuiCustom::colorPicker(xorstr_("Team"), config->visuals.molotovPolygon.team.color.data(), &config->visuals.molotovPolygon.team.color[3], &config->visuals.molotovPolygon.team.rainbow, &config->visuals.molotovPolygon.team.rainbowSpeed, nullptr);
		ImGuiCustom::colorPicker(xorstr_("Enemy"), config->visuals.molotovPolygon.enemy.color.data(), &config->visuals.molotovPolygon.enemy.color[3], &config->visuals.molotovPolygon.enemy.rainbow, &config->visuals.molotovPolygon.enemy.rainbowSpeed, nullptr);
		ImGui::EndPopup();
	}

	ImGui::Checkbox(xorstr_("Smoke Timer"), &config->visuals.smokeTimer);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...##smoke_timer")))
		ImGui::OpenPopup(xorstr_("popup_smokeTimer"));

	if (ImGui::BeginPopup(xorstr_("popup_smokeTimer"))) {
		ImGuiCustom::colorPicker(xorstr_("BackGround color"), config->visuals.smokeTimerBG);
		ImGuiCustom::colorPicker(xorstr_("Text color"), config->visuals.smokeTimerText);
		ImGuiCustom::colorPicker(xorstr_("Timer color"), config->visuals.smokeTimerTimer);
		ImGui::EndPopup();
	}
	ImGui::Checkbox(xorstr_("Molotov Timer"), &config->visuals.molotovTimer);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...##molotov_timer")))
		ImGui::OpenPopup(xorstr_("popup_molotovTimer"));

	if (ImGui::BeginPopup(xorstr_("popup_molotovTimer"))) {
		ImGuiCustom::colorPicker(xorstr_("BackGround color"), config->visuals.molotovTimerBG);
		ImGuiCustom::colorPicker(xorstr_("Text color"), config->visuals.molotovTimerText);
		ImGuiCustom::colorPicker(xorstr_("Timer color"), config->visuals.molotovTimerTimer);
		ImGui::EndPopup();
	}
	ImGuiCustom::colorPicker(xorstr_("Spread circle"), config->visuals.spreadCircle);

	ImGui::Checkbox(xorstr_("Viewmodel"), &config->visuals.viewModel.enabled);
	ImGui::SameLine();

	if (bool ccPopup = ImGui::Button(xorstr_("Edit")))
		ImGui::OpenPopup(xorstr_("##viewmodel"));

	if (ImGui::BeginPopup(xorstr_("##viewmodel"))) {
		ImGui::PushItemWidth(290.0f);
		ImGui::SliderFloat(xorstr_("##x"), &config->visuals.viewModel.x, -20.0f, 20.0f, xorstr_("X: %.4f"));
		ImGui::SliderFloat(xorstr_("##y"), &config->visuals.viewModel.y, -20.0f, 20.0f, xorstr_("Y: %.4f"));
		ImGui::SliderFloat(xorstr_("##z"), &config->visuals.viewModel.z, -20.0f, 20.0f, xorstr_("Z: %.4f"));
		ImGui::SliderInt(xorstr_("##fov"), &config->visuals.viewModel.fov, -60, 60, xorstr_("Viewmodel FOV: %d"));
		ImGui::SliderFloat(xorstr_("##roll"), &config->visuals.viewModel.roll, -90.0f, 90.0f, xorstr_("Viewmodel roll: %.2f"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}

	ImGui::Columns(1);
}

void GUI::renderSkinChangerWindow() noexcept
{
	static auto itemIndex = 0;

	ImGui::PushItemWidth(110.0f);
	ImGui::Combo(xorstr_("##1"), &itemIndex, [](void* data, int idx, const char** out_text) {
		*out_text = SkinChanger::weapon_names[idx].name;
		return true;
		}, nullptr, SkinChanger::weapon_names.size(), 5);
	ImGui::PopItemWidth();

	auto& selected_entry = config->skinChanger[itemIndex];
	selected_entry.itemIdIndex = itemIndex;

	constexpr auto rarityColor = [](int rarity) {
		constexpr auto rarityColors = std::to_array<ImU32>({
			IM_COL32(0,     0,   0,   0),
			IM_COL32(176, 195, 217, 255),
			IM_COL32(94, 152, 217, 255),
			IM_COL32(75, 105, 255, 255),
			IM_COL32(136,  71, 255, 255),
			IM_COL32(211,  44, 230, 255),
			IM_COL32(235,  75,  75, 255),
			IM_COL32(228, 174,  57, 255)
			});
		return rarityColors[static_cast<std::size_t>(rarity) < rarityColors.size() ? rarity : 0];
	};

	constexpr auto passesFilter = [](const std::wstring& str, std::wstring filter) {
		const auto delimiter = xorstr_(L" ");
		wchar_t* _;
		wchar_t* token = std::wcstok(filter.data(), delimiter, &_);
		while (token) {
			if (!std::wcsstr(str.c_str(), token))
				return false;
			token = std::wcstok(nullptr, delimiter, &_);
		}
		return true;
	};

	{
		ImGui::SameLine();
		ImGui::Checkbox(xorstr_("Enabled"), &selected_entry.enabled);
		ImGui::Separator();
		ImGui::Columns(2, nullptr, false);
		ImGui::InputInt(xorstr_("Seed"), &selected_entry.seed);
		ImGui::InputInt(xorstr_("StatTrak\u2122"), &selected_entry.stat_trak);
		selected_entry.stat_trak = (std::max)(selected_entry.stat_trak, -1);
		ImGui::SliderFloat(xorstr_("Wear"), &selected_entry.wear, FLT_MIN, 1.f, xorstr_("%.10f"), ImGuiSliderFlags_Logarithmic);

		const auto& kits = itemIndex == 1 ? SkinChanger::getGloveKits() : SkinChanger::getSkinKits();

		if (ImGui::BeginCombo(xorstr_("Paint Kit"), kits[selected_entry.paint_kit_vector_index].name.c_str())) {
			ImGui::PushID(xorstr_("Paint Kit"));
			ImGui::PushID(xorstr_("Search"));
			ImGui::SetNextItemWidth(-1.0f);
			static std::array<std::string, SkinChanger::weapon_names.size()> filters;
			auto& filter = filters[itemIndex];
			ImGui::InputTextWithHint(xorstr_(""), xorstr_("Search"), &filter);
			if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
				ImGui::SetKeyboardFocusHere(-1);
			ImGui::PopID();

			const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
			if (ImGui::BeginChild(xorstr_("##scrollarea"), { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
				for (std::size_t i = 0; i < kits.size(); ++i) {
					if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
						ImGui::PushID(i);
						const auto selected = i == selected_entry.paint_kit_vector_index;
						if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
							selected_entry.paint_kit_vector_index = i;
							ImGui::CloseCurrentPopup();
						}

						if (ImGui::IsItemHovered()) {
							if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
								ImGui::BeginTooltip();
								ImGui::Image(icon, { 200.0f, 150.0f });
								ImGui::EndTooltip();
							}
						}
						if (selected && ImGui::IsWindowAppearing())
							ImGui::SetScrollHereY();
						ImGui::PopID();
					}
				}
			}
			ImGui::EndChild();
			ImGui::PopID();
			ImGui::EndCombo();
		}

		ImGui::Combo(xorstr_("Quality"), &selected_entry.entity_quality_vector_index, [](void* data, int idx, const char** out_text) {
			*out_text = SkinChanger::getQualities()[idx].name.c_str(); // safe within this lamba
			return true;
			}, nullptr, SkinChanger::getQualities().size(), 5);

		if (itemIndex == 0) {
			ImGui::Combo(xorstr_("Knife"), &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
				*out_text = SkinChanger::getKnifeTypes()[idx].name.c_str();
				return true;
				}, nullptr, SkinChanger::getKnifeTypes().size(), 5);
		} else if (itemIndex == 1) {
			ImGui::Combo(xorstr_("Glove"), &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
				*out_text = SkinChanger::getGloveTypes()[idx].name.c_str();
				return true;
				}, nullptr, SkinChanger::getGloveTypes().size(), 5);
		} else {
			static auto unused_value = 0;
			selected_entry.definition_override_vector_index = 0;
			ImGui::Combo(xorstr_("Unavailable"), &unused_value, xorstr_("For knives or gloves\0"));
		}

		ImGui::InputText(xorstr_("Name Tag"), selected_entry.custom_name, 32);
	}

	ImGui::NextColumn();

	{
		ImGui::PushID(xorstr_("sticker"));

		static std::size_t selectedStickerSlot = 0;

		ImGui::PushItemWidth(-1);
		ImVec2 size;
		size.x = 0.0f;
		size.y = ImGui::GetTextLineHeightWithSpacing() * 5.25f + ImGui::GetStyle().FramePadding.y * 2.0f;

		if (ImGui::BeginListBox(xorstr_(""), size)) {
			for (int i = 0; i < 5; ++i) {
				ImGui::PushID(i);

				const auto kit_vector_index = config->skinChanger[itemIndex].stickers[i].kit_vector_index;
				const std::string text = '#' + std::to_string(i + 1) + xorstr_("  ") + SkinChanger::getStickerKits()[kit_vector_index].name;

				if (ImGui::Selectable(text.c_str(), i == selectedStickerSlot))
					selectedStickerSlot = i;

				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		ImGui::PopItemWidth();

		auto& selected_sticker = selected_entry.stickers[selectedStickerSlot];

		const auto& kits = SkinChanger::getStickerKits();
		if (ImGui::BeginCombo(xorstr_("Sticker"), kits[selected_sticker.kit_vector_index].name.c_str())) {
			ImGui::PushID(xorstr_("Sticker"));
			ImGui::PushID(xorstr_("Search"));
			ImGui::SetNextItemWidth(-1.0f);
			static std::array<std::string, SkinChanger::weapon_names.size()> filters;
			auto& filter = filters[itemIndex];
			ImGui::InputTextWithHint(xorstr_(""), xorstr_("Search"), &filter);
			if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
				ImGui::SetKeyboardFocusHere(-1);
			ImGui::PopID();

			const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
			if (ImGui::BeginChild(xorstr_("##scrollarea"), { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
				for (std::size_t i = 0; i < kits.size(); ++i) {
					if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
						ImGui::PushID(i);
						const auto selected = i == selected_sticker.kit_vector_index;
						if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
							selected_sticker.kit_vector_index = i;
							ImGui::CloseCurrentPopup();
						}
						if (ImGui::IsItemHovered()) {
							if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
								ImGui::BeginTooltip();
								ImGui::Image(icon, { 200.0f, 150.0f });
								ImGui::EndTooltip();
							}
						}
						if (selected && ImGui::IsWindowAppearing())
							ImGui::SetScrollHereY();
						ImGui::PopID();
					}
				}
			}
			ImGui::EndChild();
			ImGui::PopID();
			ImGui::EndCombo();
		}

		ImGui::SliderFloat(xorstr_("Wear"), &selected_sticker.wear, FLT_MIN, 1.0f, xorstr_("%.10f"), ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat(xorstr_("Scale"), &selected_sticker.scale, 0.1f, 5.0f);
		ImGui::SliderFloat(xorstr_("Rotation"), &selected_sticker.rotation, 0.0f, 360.0f);

		ImGui::PopID();
	}
	selected_entry.update();

	ImGui::Columns(1);

	ImGui::Separator();

	if (ImGui::Button(xorstr_("Update"), { 130.0f, 30.0f }))
		SkinChanger::scheduleHudUpdate();
}

void GUI::renderMiscWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 250.0f);
	hotkey3(xorstr_("Menu Key"), config->misc.menuKey);
	ImGui::Checkbox(xorstr_("Anti AFK kick"), &config->misc.antiAfkKick);
	ImGui::Checkbox(xorstr_("Adblock"), &config->misc.adBlock);
	ImGui::Combo(xorstr_("Force region"), &config->misc.forceRelayCluster, xorstr_("Off\0Australia\0Austria\0Brazil\0Chile\0Dubai\0France\0Germany\0Hong Kong\0India (Chennai)\0India (Mumbai)\0Japan\0Luxembourg\0Netherlands\0Peru\0Philipines\0Poland\0Singapore\0South Africa\0Spain\0Sweden\0UK\0USA (Atlanta)\0USA (Seattle)\0USA (Chicago)\0USA (Los Angeles)\0USA (Moses Lake)\0USA (Oklahoma)\0USA (Seattle)\0USA (Washington DC)\0"));
	ImGui::Checkbox(xorstr_("Auto strafe"), &config->misc.autoStrafe);
	ImGui::Checkbox(xorstr_("Bunny hop"), &config->misc.bunnyHop);
	ImGui::Checkbox(xorstr_("Fast duck"), &config->misc.fastDuck);
	ImGui::Checkbox(xorstr_("Moonwalk"), &config->misc.moonwalk);
	ImGui::SameLine();
	ImGui::Checkbox(xorstr_("Break"), &config->misc.leg_break);
	ImGui::Checkbox(xorstr_("Knifebot"), &config->misc.knifeBot);
	ImGui::PushID(xorstr_("Knifebot"));
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::Combo(xorstr_("Mode"), &config->misc.knifeBotMode, xorstr_("Trigger\0Rage\0"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Blockbot"), &config->misc.blockBot);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Blockbot Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.blockBotKey);
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Edge jump"), &config->misc.edgeJump);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Edge jump Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.edgeJumpKey);
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Mini jump"), &config->misc.miniJump);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Mini jump"));
	ImGui::hotkey2(xorstr_(""), config->misc.miniJumpKey);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Crouch lock"), &config->misc.miniJumpCrouchLock, 0, 12, xorstr_("%d ticks"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Jump bug"), &config->misc.jumpBug);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Jump bug Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.jumpBugKey);
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Edge bug"), &config->misc.edgeBug);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Edge bug Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.edgeBugKey);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Prediction amount"), &config->misc.edgeBugPredAmnt, 0, 128, xorstr_("%d ticks"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Auto pixel surf"), &config->misc.autoPixelSurf);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Auto pixel surf"));
	ImGui::hotkey2(xorstr_(""), config->misc.autoPixelSurfKey);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Prediction amount"), &config->misc.autoPixelSurfPredAmnt, 2, 4, xorstr_("%d ticks"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Draw velocity"), &config->misc.velocity.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Draw velocity"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::SliderFloat(xorstr_("Position"), &config->misc.velocity.position, 0.0f, 1.0f);
		ImGui::SliderFloat(xorstr_("Alpha"), &config->misc.velocity.alpha, 0.0f, 1.0f);
		ImGuiCustom::colorPicker(xorstr_("Force color"), config->misc.velocity.color.color.data(), nullptr, &config->misc.velocity.color.rainbow, &config->misc.velocity.color.rainbowSpeed, &config->misc.velocity.color.enabled);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Keyboard display"), &config->misc.keyBoardDisplay.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Keyboard display"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::SliderFloat(xorstr_("Position"), &config->misc.keyBoardDisplay.position, 0.0f, 1.0f);
		ImGui::Checkbox(xorstr_("Show key tiles"), &config->misc.keyBoardDisplay.showKeyTiles);
		ImGuiCustom::colorPicker(xorstr_("Color"), config->misc.keyBoardDisplay.color);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Slowwalk"), &config->misc.slowwalk);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Slowwalk Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.slowwalkKey);
	ImGui::SameLine();
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));
	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt(xorstr_("Velocity"), &config->misc.slowwalkAmnt, 0, 50, config->misc.slowwalkAmnt ? xorstr_("%d u/s") : xorstr_("Default"));
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Fakeduck"), &config->misc.fakeduck);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Fakeduck Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.fakeduckKey);
	ImGui::PopID();
	ImGuiCustom::colorPicker(xorstr_("Auto peek"), config->misc.autoPeek.color.data(), &config->misc.autoPeek.color[3], &config->misc.autoPeek.rainbow, &config->misc.autoPeek.rainbowSpeed, &config->misc.autoPeek.enabled);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Auto peek Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.autoPeekKey);
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Noscope crosshair"), &config->misc.noscopeCrosshair);
	ImGui::Checkbox(xorstr_("Recoil crosshair"), &config->misc.recoilCrosshair);
	ImGui::Checkbox(xorstr_("Auto pistol"), &config->misc.autoPistol);
	ImGui::Checkbox(xorstr_("Auto reload"), &config->misc.autoReload);
	ImGui::Checkbox(xorstr_("Auto accept"), &config->misc.autoAccept);
	ImGui::Checkbox(xorstr_("Radar hack"), &config->misc.radarHack);
	ImGui::Checkbox(xorstr_("Reveal ranks"), &config->misc.revealRanks);
	ImGui::Checkbox(xorstr_("Reveal money"), &config->misc.revealMoney);
	ImGui::Checkbox(xorstr_("Reveal suspect"), &config->misc.revealSuspect);
	ImGui::Checkbox(xorstr_("Reveal votes"), &config->misc.revealVotes);

	ImGui::Checkbox(xorstr_("Spectator list"), &config->misc.spectatorList.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Spectator list"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("No Title Bar"), &config->misc.spectatorList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Keybinds list"), &config->misc.keybindList.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Keybinds list"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("No Title Bar"), &config->misc.keybindList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::PushID(xorstr_("Player list"));
	ImGui::Checkbox(xorstr_("Player list"), &config->misc.playerList.enabled);
	ImGui::SameLine();

	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Steam ID"), &config->misc.playerList.steamID);
		ImGui::Checkbox(xorstr_("Rank"), &config->misc.playerList.rank);
		ImGui::Checkbox(xorstr_("Wins"), &config->misc.playerList.wins);
		ImGui::Checkbox(xorstr_("Health"), &config->misc.playerList.health);
		ImGui::Checkbox(xorstr_("Armor"), &config->misc.playerList.armor);
		ImGui::Checkbox(xorstr_("Money"), &config->misc.playerList.money);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::PushID(xorstr_("Jump stats"));
	ImGui::Checkbox(xorstr_("Jump stats"), &config->misc.jumpStats.enabled);
	ImGui::SameLine();

	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Show fails"), &config->misc.jumpStats.showFails);
		ImGui::Checkbox(xorstr_("Show color on fails"), &config->misc.jumpStats.showColorOnFail);
		ImGui::Checkbox(xorstr_("Simplify naming"), &config->misc.jumpStats.simplifyNaming);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Watermark"), &config->misc.watermark.enabled);
	ImGuiCustom::colorPicker(xorstr_("Offscreen enemies"), config->misc.offscreenEnemies, &config->misc.offscreenEnemies.enabled);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Offscreen enemies"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Health bar"), &config->misc.offscreenEnemies.healthBar.enabled);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(95.0f);
		ImGui::Combo(xorstr_("Type"), &config->misc.offscreenEnemies.healthBar.type, xorstr_("Gradient\0Solid\0Health-based\0"));
		if (config->misc.offscreenEnemies.healthBar.type == HealthBar::Solid) {
			ImGui::SameLine();
			ImGuiCustom::colorPicker(xorstr_(""), static_cast<Color4&>(config->misc.offscreenEnemies.healthBar));
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Disable model occlusion"), &config->misc.disableModelOcclusion);
	ImGui::SliderFloat(xorstr_("Aspect ratio"), &config->misc.aspectratio, 0.0f, 5.0f, xorstr_("%.2f"));
	ImGui::NextColumn();
	ImGui::Checkbox(xorstr_("Disable HUD blur"), &config->misc.disablePanoramablur);
	if (ImGui::Checkbox(xorstr_("Custom clantag"), &config->misc.customClanTag))
		Misc::updateClanTag(true);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Clantag options"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.0f);

		if (config->misc.tagType < 4) {
			ImGui::PushID(0);
			if (ImGui::InputText(xorstr_("Tag"), config->misc.clanTag, sizeof(config->misc.clanTag)))
				Misc::updateClanTag(true);
			ImGui::PopID();
		}

		if (ImGui::Combo(xorstr_("Animation"), &config->misc.tagType, xorstr_("Static\0Animated\0Auto reverse\0Reverse auto reverse\0Custom\0Clock\0")))
			Misc::updateClanTag(true);
		if (config->misc.tagType != 0 && config->misc.tagType != 5) // Don't ask for update rate if it is static or clock
			ImGui::SliderFloat(xorstr_("Update rate"), &config->misc.tagUpdateInterval, 0.25f, 3.f, xorstr_("%.2f"));
		if (config->misc.tagType == 4) {
			for (size_t i = 0; i < config->misc.tagAnimationSteps.size(); i++) {
				ImGui::PushID(i + 100);
				ImGui::InputText(xorstr_(""), &config->misc.tagAnimationSteps[i]);
				ImGui::PopID();
			}

			if (ImGui::Button(xorstr_("+"), ImVec2(30.f, 30.f))) {
				config->misc.tagAnimationSteps.push_back(xorstr_(""));
				Misc::updateClanTag(true);
			}

			ImGui::SameLine();
			if (ImGui::Button(xorstr_("-"), ImVec2(30.f, 30.f)) && config->misc.tagAnimationSteps.size() > 2) {
				config->misc.tagAnimationSteps.pop_back();
				Misc::updateClanTag(true);
			}
		}
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Kill message"), &config->misc.killMessage);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Kill messages"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(150.0f);

		for (size_t i = 0; i < config->misc.killMessages.size(); i++) {
			ImGui::PushID(i + 200);
			ImGui::InputText(xorstr_(""), &config->misc.killMessages[i]);
			ImGui::PopID();
		}

		if (ImGui::Button(xorstr_("+"), ImVec2(30.f, 30.f)))
			config->misc.killMessages.push_back(xorstr_(""));

		ImGui::SameLine();
		if (ImGui::Button(xorstr_("-"), ImVec2(30.f, 30.f)) && config->misc.killMessages.size() > 1)
			config->misc.killMessages.pop_back();

		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Custom name"), &config->misc.customName);
	ImGui::SameLine();
	ImGui::PushItemWidth(150.0f);
	ImGui::PushID(xorstr_("Custom name change"));

	if (ImGui::InputText(xorstr_(""), config->misc.name, sizeof(config->misc.name)) && config->misc.customName)
		Misc::changeName(false, (std::string{ config->misc.name } + '\x1').c_str(), 0.0f);
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Name stealer"), &config->misc.nameStealer);
	ImGui::Checkbox(xorstr_("Fast plant"), &config->misc.fastPlant);
	ImGui::Checkbox(xorstr_("Fast stop"), &config->misc.fastStop);
	ImGuiCustom::colorPicker(xorstr_("Bomb timer"), config->misc.bombTimer);
	ImGuiCustom::colorPicker(xorstr_("Hurt indicator"), config->misc.hurtIndicator);
	ImGuiCustom::colorPicker(xorstr_("Yaw indicator"), config->misc.yawIndicator);
	ImGui::Checkbox(xorstr_("Prepare revolver"), &config->misc.prepareRevolver);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Prepare revolver Key"));
	ImGui::hotkey2(xorstr_(""), config->misc.prepareRevolverKey);
	ImGui::PopID();
	ImGui::Combo(xorstr_("Hit sound"), &config->misc.hitSound, xorstr_("None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0"));
	if (config->misc.hitSound == 5) {
		ImGui::InputText(xorstr_("Hit sound filename"), &config->misc.customHitSound);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(xorstr_("audio file must be put in csgo/sound/ directory"));
	}
	ImGui::PushID(2);
	ImGui::Combo(xorstr_("Kill sound"), &config->misc.killSound, xorstr_("None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0"));
	if (config->misc.killSound == 5) {
		ImGui::InputText(xorstr_("Kill sound filename"), &config->misc.customKillSound);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(xorstr_("audio file must be put in csgo/sound/ directory"));
	}
	ImGui::PopID();
	ImGui::Checkbox(xorstr_("Grenade prediction"), &config->misc.nadePredict);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Grenade prediction"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGuiCustom::colorPicker(xorstr_("Damage"), config->misc.nadeDamagePredict);
		ImGuiCustom::colorPicker(xorstr_("Trail"), config->misc.nadeTrailPredict);
		ImGuiCustom::colorPicker(xorstr_("Circle"), config->misc.nadeCirclePredict);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Fix tablet signal"), &config->misc.fixTabletSignal);
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat(xorstr_("Max angle delta"), &config->misc.maxAngleDelta, 0.0f, 255.0f, xorstr_("%.2f"));
	ImGui::Checkbox(xorstr_("Opposite hand knife"), &config->misc.oppositeHandKnife);
	ImGui::Checkbox(xorstr_("Sv_pure bypass"), &config->misc.svPureBypass);
	ImGui::Checkbox(xorstr_("Unlock inventory"), &config->misc.inventoryUnlocker);
	ImGui::Checkbox(xorstr_("Unlock hidden convars"), &config->misc.unhideConvars);
	ImGui::Checkbox(xorstr_("Preserve killfeed"), &config->misc.preserveKillfeed.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Preserve killfeed"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Only headshots"), &config->misc.preserveKillfeed.onlyHeadshots);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Killfeed changer"), &config->misc.killfeedChanger.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Killfeed changer"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Checkbox(xorstr_("Headshot"), &config->misc.killfeedChanger.headshot);
		ImGui::Checkbox(xorstr_("Dominated"), &config->misc.killfeedChanger.dominated);
		ImGui::SameLine();
		ImGui::Checkbox(xorstr_("Revenge"), &config->misc.killfeedChanger.revenge);
		ImGui::Checkbox(xorstr_("Penetrated"), &config->misc.killfeedChanger.penetrated);
		ImGui::Checkbox(xorstr_("Noscope"), &config->misc.killfeedChanger.noscope);
		ImGui::Checkbox(xorstr_("Thrusmoke"), &config->misc.killfeedChanger.thrusmoke);
		ImGui::Checkbox(xorstr_("Attackerblind"), &config->misc.killfeedChanger.attackerblind);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Purchase list"), &config->misc.purchaseList.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Purchase list"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::SetNextItemWidth(75.0f);
		ImGui::Combo(xorstr_("Mode"), &config->misc.purchaseList.mode, xorstr_("Details\0Summary\0"));
		ImGui::Checkbox(xorstr_("Only during freeze time"), &config->misc.purchaseList.onlyDuringFreezeTime);
		ImGui::Checkbox(xorstr_("Show prices"), &config->misc.purchaseList.showPrices);
		ImGui::Checkbox(xorstr_("No title bar"), &config->misc.purchaseList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Reportbot"), &config->misc.reportbot.enabled);
	ImGui::SameLine();
	ImGui::PushID(xorstr_("Reportbot"));

	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::PushItemWidth(80.0f);
		ImGui::Combo(xorstr_("Target"), &config->misc.reportbot.target, xorstr_("Enemies\0Allies\0All\0"));
		ImGui::InputInt(xorstr_("Delay (s)"), &config->misc.reportbot.delay);
		config->misc.reportbot.delay = (std::max)(config->misc.reportbot.delay, 1);
		ImGui::InputInt(xorstr_("Rounds"), &config->misc.reportbot.rounds);
		config->misc.reportbot.rounds = (std::max)(config->misc.reportbot.rounds, 1);
		ImGui::PopItemWidth();
		ImGui::Checkbox(xorstr_("Abusive Communications"), &config->misc.reportbot.textAbuse);
		ImGui::Checkbox(xorstr_("Griefing"), &config->misc.reportbot.griefing);
		ImGui::Checkbox(xorstr_("Wall Hacking"), &config->misc.reportbot.wallhack);
		ImGui::Checkbox(xorstr_("Aim Hacking"), &config->misc.reportbot.aimbot);
		ImGui::Checkbox(xorstr_("Other Hacking"), &config->misc.reportbot.other);
		if (ImGui::Button(xorstr_("Reset")))
			Misc::resetReportbot();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox(xorstr_("Autobuy"), &config->misc.autoBuy.enabled);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Autobuy"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {
		ImGui::Combo(xorstr_("Primary weapon"), &config->misc.autoBuy.primaryWeapon, xorstr_("None\0MAC-10 | MP9\0MP7 | MP5-SD\0UMP-45\0P90\0PP-Bizon\0Galil AR | FAMAS\0AK-47 | M4A4 | M4A1-S\0SSG 08\0SG553 |AUG\0AWP\0G3SG1 | SCAR-20\0Nova\0XM1014\0Sawed-Off | MAG-7\0M249\0Negev\0"));
		ImGui::Combo(xorstr_("Secondary weapon"), &config->misc.autoBuy.secondaryWeapon, xorstr_("None\0Glock-18 | P2000 | USP-S\0Dual Berettas\0P250\0CZ75-Auto | Five-SeveN | Tec-9\0Desert Eagle | R8 Revolver\0"));
		ImGui::Combo(xorstr_("Armor"), &config->misc.autoBuy.armor, xorstr_("None\0Kevlar\0Kevlar + Helmet\0"));

		static bool utilities[2]{ false, false };
		static const char* utility[]{ xorstr_("Defuser"),xorstr_("Taser") };
		static std::string previewvalueutility = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(utilities); i++) {
			utilities[i] = (config->misc.autoBuy.utility & 1 << i) == 1 << i;
		}
		if (ImGui::BeginCombo(xorstr_("Utility"), previewvalueutility.c_str())) {
			previewvalueutility = xorstr_("");
			for (size_t i = 0; i < ARRAYSIZE(utilities); i++) {
				ImGui::Selectable(utility[i], &utilities[i], ImGuiSelectableFlags_DontClosePopups);
			}
			ImGui::EndCombo();
		}
		for (size_t i = 0; i < ARRAYSIZE(utilities); i++) {
			if (i == 0)
				previewvalueutility = xorstr_("");

			if (utilities[i]) {
				previewvalueutility += previewvalueutility.size() ? std::string(xorstr_(", ")) + utility[i] : utility[i];
				config->misc.autoBuy.utility |= 1 << i;
			} else {
				config->misc.autoBuy.utility &= ~(1 << i);
			}
		}

		static bool nading[5]{ false, false, false, false, false };
		static const char* nades[]{ xorstr_("HE Grenade"),xorstr_("Smoke Grenade"),xorstr_("Molotov"),xorstr_("Flashbang"),xorstr_("Decoy") };
		static std::string previewvaluenades = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(nading); i++) {
			nading[i] = (config->misc.autoBuy.grenades & 1 << i) == 1 << i;
		}
		if (ImGui::BeginCombo(xorstr_("Nades"), previewvaluenades.c_str())) {
			previewvaluenades = xorstr_("");
			for (size_t i = 0; i < ARRAYSIZE(nading); i++) {
				ImGui::Selectable(nades[i], &nading[i], ImGuiSelectableFlags_DontClosePopups);
			}
			ImGui::EndCombo();
		}
		for (size_t i = 0; i < ARRAYSIZE(nading); i++) {
			if (i == 0)
				previewvaluenades = xorstr_("");

			if (nading[i]) {
				previewvaluenades += previewvaluenades.size() ? std::string(xorstr_(", ")) + nades[i] : nades[i];
				config->misc.autoBuy.grenades |= 1 << i;
			} else {
				config->misc.autoBuy.grenades &= ~(1 << i);
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGuiCustom::colorPicker(xorstr_("Logger"), config->misc.logger);
	ImGui::SameLine();

	ImGui::PushID(xorstr_("Logger"));
	if (ImGui::Button(xorstr_("...")))
		ImGui::OpenPopup(xorstr_(""));

	if (ImGui::BeginPopup(xorstr_(""))) {

		static bool modes[2]{ false, false };
		static const char* mode[]{ xorstr_("Console"), xorstr_("Event log") };
		static std::string previewvaluemode = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(modes); i++) {
			modes[i] = (config->misc.loggerOptions.modes & 1 << i) == 1 << i;
		}
		if (ImGui::BeginCombo(xorstr_("Log output"), previewvaluemode.c_str())) {
			previewvaluemode = xorstr_("");
			for (size_t i = 0; i < ARRAYSIZE(modes); i++) {
				ImGui::Selectable(mode[i], &modes[i], ImGuiSelectableFlags_DontClosePopups);
			}
			ImGui::EndCombo();
		}
		for (size_t i = 0; i < ARRAYSIZE(modes); i++) {
			if (i == 0)
				previewvaluemode = xorstr_("");

			if (modes[i]) {
				previewvaluemode += previewvaluemode.size() ? std::string(xorstr_(", ")) + mode[i] : mode[i];
				config->misc.loggerOptions.modes |= 1 << i;
			} else {
				config->misc.loggerOptions.modes &= ~(1 << i);
			}
		}

		static bool events[4]{ false, false, false, false };
		static const char* event[]{ xorstr_("Damage dealt"), xorstr_("Damage received"), xorstr_("Hostage taken"), xorstr_("Bomb plants") };
		static std::string previewvalueevent = xorstr_("");
		for (size_t i = 0; i < ARRAYSIZE(events); i++) {
			events[i] = (config->misc.loggerOptions.events & 1 << i) == 1 << i;
		}
		if (ImGui::BeginCombo(xorstr_("Log events"), previewvalueevent.c_str())) {
			previewvalueevent = xorstr_("");
			for (size_t i = 0; i < ARRAYSIZE(events); i++) {
				ImGui::Selectable(event[i], &events[i], ImGuiSelectableFlags_DontClosePopups);
			}
			ImGui::EndCombo();
		}
		for (size_t i = 0; i < ARRAYSIZE(events); i++) {
			if (i == 0)
				previewvalueevent = xorstr_("");

			if (events[i]) {
				previewvalueevent += previewvalueevent.size() ? std::string(xorstr_(", ")) + event[i] : event[i];
				config->misc.loggerOptions.events |= 1 << i;
			} else {
				config->misc.loggerOptions.events &= ~(1 << i);
			}
		}

		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (ImGui::Button(xorstr_("Unhook")))
		hooks->uninstall();
#ifdef DEBUG
	static bool metrics_show{};
	ImGui::Checkbox(xorstr_("Metrics"), &metrics_show);
	if (metrics_show) ImGui::ShowMetricsWindow(&metrics_show);
#endif
	ImGui::Columns(1);
}

void GUI::renderConfigWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 170.0f);

	static bool incrementalLoad = false;
	ImGui::Checkbox(xorstr_("Incremental load"), &incrementalLoad);

	ImGui::PushItemWidth(160.0f);

	auto& configItems = config->getConfigs();
	static int currentConfig = -1;

	static std::string buffer;

	timeToNextConfigRefresh -= ImGui::GetIO().DeltaTime;
	if (timeToNextConfigRefresh <= 0.0f) {
		config->listConfigs();
		if (const auto it = std::find(configItems.begin(), configItems.end(), buffer); it != configItems.end())
			currentConfig = std::distance(configItems.begin(), it);
		timeToNextConfigRefresh = 0.1f;
	}

	if (static_cast<std::size_t>(currentConfig) >= configItems.size())
		currentConfig = -1;

	if (ImGui::ListBox(xorstr_(""), &currentConfig, [](void* data, int idx, const char** out_text) {
		auto& vector = *static_cast<std::vector<std::string>*>(data);
		*out_text = vector[idx].c_str();
		return true;
		}, &configItems, configItems.size(), 5) && currentConfig != -1)
		buffer = configItems[currentConfig];

		ImGui::PushID(0);
		if (ImGui::InputTextWithHint(xorstr_(""), xorstr_("config name"), &buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
			if (currentConfig != -1)
				config->rename(currentConfig, buffer.c_str());
		}
		ImGui::PopID();
		ImGui::NextColumn();

		ImGui::PushItemWidth(100.0f);

		if (ImGui::Button(xorstr_("Open config directory")))
			config->openConfigDir();

		if (ImGui::Button(xorstr_("Create config"), { 100.0f, 25.0f }))
			config->add(buffer.c_str());

		if (ImGui::Button(xorstr_("Reset config"), { 100.0f, 25.0f }))
			ImGui::OpenPopup(xorstr_("Config to reset"));

		if (ImGui::BeginPopup(xorstr_("Config to reset"))) {
			static const char* names[]{ xorstr_("Whole"), xorstr_("Legitbot"), xorstr_("Legit Anti Aim"), xorstr_("Ragebot"), xorstr_("Rage Anti aim"), xorstr_("Fake angle"), xorstr_("Fakelag"), xorstr_("Backtrack"), xorstr_("Triggerbot"), xorstr_("Glow"), xorstr_("Chams"), xorstr_("ESP"), xorstr_("Visuals"), xorstr_("Skin changer"), xorstr_("Sound"), xorstr_("Misc") };
			for (int i = 0; i < IM_ARRAYSIZE(names); i++) {
				if (i == 1) ImGui::Separator();

				if (ImGui::Selectable(names[i])) {
					switch (i) {
					case 0: config->reset(); Misc::updateClanTag(true); SkinChanger::scheduleHudUpdate(); break;
					case 1: config->legitbot = { }; config->legitbotKey.reset(); break;
					case 2: config->legitAntiAim = { }; break;
					case 3: config->ragebot = { }; config->ragebotKey.reset();  break;
					case 4: config->rageAntiAim = { };  break;
					case 5: config->fakeAngle = { }; break;
					case 6: config->fakelag = { }; break;
					case 7: config->backtrack = { }; break;
					case 8: config->triggerbot = { }; config->triggerbotKey.reset(); break;
					case 9: Glow::resetConfig(); break;
					case 10: config->chams = { }; config->chamsKey.reset(); break;
					case 11: config->streamProofESP = { }; break;
					case 12: config->visuals = { }; break;
					case 13: config->skinChanger = { }; SkinChanger::scheduleHudUpdate(); break;
					case 14: Sound::resetConfig(); break;
					case 15: config->misc = { };  Misc::updateClanTag(true); break;
					}
				}
			}
			ImGui::EndPopup();
		}
		if (currentConfig != -1) {
			if (ImGui::Button(xorstr_("Load selected"), { 100.0f, 25.0f }))
				ImGui::OpenPopup(xorstr_("##reallyLoad"));
			if (ImGui::BeginPopup(xorstr_("##reallyLoad"))) {
				ImGui::TextUnformatted(xorstr_("Are you sure?"));
				if (ImGui::Button(xorstr_("No"), { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button(xorstr_("Yes"), { 45.0f, 0.0f })) {
					config->load(currentConfig, incrementalLoad);
					SkinChanger::scheduleHudUpdate();
					Misc::updateClanTag(true);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			if (ImGui::Button(xorstr_("Save selected"), { 100.0f, 25.0f }))
				ImGui::OpenPopup(xorstr_("##reallySave"));
			if (ImGui::BeginPopup(xorstr_("##reallySave"))) {
				ImGui::TextUnformatted(xorstr_("Are you sure?"));
				if (ImGui::Button(xorstr_("No"), { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button(xorstr_("Yes"), { 45.0f, 0.0f })) {
					config->save(currentConfig);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			if (ImGui::Button(xorstr_("Delete selected"), { 100.0f, 25.0f }))
				ImGui::OpenPopup(xorstr_("##reallyDelete"));
			if (ImGui::BeginPopup(xorstr_("##reallyDelete"))) {
				ImGui::TextUnformatted(xorstr_("Are you sure?"));
				if (ImGui::Button(xorstr_("No"), { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button(xorstr_("Yes"), { 45.0f, 0.0f })) {
					config->remove(currentConfig);
					if (static_cast<std::size_t>(currentConfig) < configItems.size())
						buffer = configItems[currentConfig];
					else
						buffer.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::Columns(1);
}

void Active() { ImGuiStyle* Style = &ImGui::GetStyle(); Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34); Style->Colors[ImGuiCol_ButtonActive] = ImColor(25, 30, 34); Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34); }
void Hovered() { ImGuiStyle* Style = &ImGui::GetStyle(); Style->Colors[ImGuiCol_Button] = ImColor(19, 22, 27); Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27); Style->Colors[ImGuiCol_ButtonHovered] = ImColor(19, 22, 27); }



void GUI::renderGuiStyle() noexcept
{
	ImGuiStyle* Style = &ImGui::GetStyle();
	Style->WindowRounding = 5.5f;
	Style->WindowBorderSize = 2.5f;
	Style->ChildRounding = 5.5f;
	Style->FrameBorderSize = 2.f;
	Style->FrameRounding = 5.5f;
	Style->Colors[ImGuiCol_WindowBg] = ImColor(0, 0, 0, 0);
	Style->Colors[ImGuiCol_ChildBg] = ImColor(31, 31, 31);
	Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34);
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34);
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27);

	Style->Colors[ImGuiCol_ScrollbarGrab] = ImColor(25, 30, 34);
	Style->Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(25, 30, 34);
	Style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(25, 30, 34);

	static auto Name = xorstr_("Menu");
	static auto Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	static int activeTab = 1;
	static int activeSubTabLegitbot = 1;
	static int activeSubTabRagebot = 1;
	static int activeSubTabVisuals = 1;
	static int activeSubTabMisc = 1;
	static int activeSubTabConfigs = 1;

	if (ImGui::Begin(Name, NULL, Flags)) {
		Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);

		ImGui::BeginChild(xorstr_("##Back"), ImVec2{ 704, 434 }, false);
		{
			ImGui::SetCursorPos(ImVec2{ 2, 2 });

			Style->Colors[ImGuiCol_ChildBg] = ImColor(19, 22, 27);

			ImGui::BeginChild(xorstr_("##Main"), ImVec2{ 700, 430 }, false);
			{
				ImGui::BeginChild(xorstr_("##UP"), ImVec2{ 700, 45 }, false);
				{
					ImGui::SetCursorPos(ImVec2{ 10, 6 });
					ImGui::PushFont(fonts.tahoma34); ImGui::Text(xorstr_("Osiris")); ImGui::PopFont();

					float pos = 305;
					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 1) Active(); else Hovered();
					if (ImGui::Button(xorstr_("Legitbot"), ImVec2{ 75, 45 }))
						activeTab = 1;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 2) Active(); else Hovered();
					if (ImGui::Button(xorstr_("Ragebot"), ImVec2{ 75, 45 }))
						activeTab = 2;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 3) Active(); else Hovered();
					if (ImGui::Button(xorstr_("Visuals"), ImVec2{ 75, 45 }))
						activeTab = 3;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 4) Active(); else Hovered();
					if (ImGui::Button(xorstr_("Misc"), ImVec2{ 75, 45 }))
						activeTab = 4;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 5) Active(); else Hovered();
					if (ImGui::Button(xorstr_("Configs"), ImVec2{ 75, 45 }))
						activeTab = 5;
				}
				ImGui::EndChild();

				ImGui::SetCursorPos(ImVec2{ 0, 45 });
				Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27);
				ImGui::BeginChild(xorstr_("##Childs"), ImVec2{ 700, 365 }, false);
				{
					ImGui::SetCursorPos(ImVec2{ 15, 5 });
					Style->ChildRounding = 0;
					ImGui::BeginChild(xorstr_("##Left"), ImVec2{ 155, 320 }, false);
					{
						switch (activeTab) {
						case 1: //Legitbot
							ImGui::SetCursorPosY(10);
							if (ImGui::Button(xorstr_("Main                    "), ImVec2{ 80, 20 })) activeSubTabLegitbot = 1;
							if (ImGui::Button(xorstr_("Backtrack               "), ImVec2{ 80, 20 })) activeSubTabLegitbot = 2;
							if (ImGui::Button(xorstr_("Triggerbot              "), ImVec2{ 80, 20 })) activeSubTabLegitbot = 3;
							if (ImGui::Button(xorstr_("Anti aim                "), ImVec2{ 80, 20 })) activeSubTabLegitbot = 4;
							break;
						case 2: //Ragebot
							ImGui::SetCursorPosY(10);
							if (ImGui::Button(xorstr_("Main                    "), ImVec2{ 80, 20 })) activeSubTabRagebot = 1;
							if (ImGui::Button(xorstr_("Backtrack               "), ImVec2{ 80, 20 })) activeSubTabRagebot = 2;
							if (ImGui::Button(xorstr_("Anti aim                "), ImVec2{ 80, 20 })) activeSubTabRagebot = 3;
							if (ImGui::Button(xorstr_("Desync                  "), ImVec2{ 80, 20 })) activeSubTabRagebot = 4;
							if (ImGui::Button(xorstr_("Fakelag                 "), ImVec2{ 80, 20 })) activeSubTabRagebot = 5;
							break;
						case 3: //Visuals
							ImGui::SetCursorPosY(10);
							if (ImGui::Button(xorstr_("Main                    "), ImVec2{ 80, 20 })) activeSubTabVisuals = 1;
							if (ImGui::Button(xorstr_("ESP                     "), ImVec2{ 80, 20 })) activeSubTabVisuals = 2;
							if (ImGui::Button(xorstr_("Chams                   "), ImVec2{ 80, 20 })) activeSubTabVisuals = 3;
							if (ImGui::Button(xorstr_("Glow                    "), ImVec2{ 80, 20 })) activeSubTabVisuals = 4;
							if (ImGui::Button(xorstr_("Skins                   "), ImVec2{ 80, 20 })) activeSubTabVisuals = 5;
							break;
						case 4: //Misc
							ImGui::SetCursorPosY(10);
							if (ImGui::Button(xorstr_("Main                    "), ImVec2{ 80, 20 })) activeSubTabMisc = 1;
							if (ImGui::Button(xorstr_("Sound                   "), ImVec2{ 80, 20 })) activeSubTabMisc = 2;
							break;
						default:
							break;
						}

						ImGui::EndChild();

						ImGui::SetCursorPos(ImVec2{ 100, 5 });
						Style->Colors[ImGuiCol_ChildBg] = ImColor(29, 34, 38);
						Style->ChildRounding = 5;
						ImGui::BeginChild(xorstr_("##SubMain"), ImVec2{ 590, 350 }, false);
						{
							ImGui::SetCursorPos(ImVec2{ 10, 10 });
							switch (activeTab) {
							case 1: //Legitbot
								switch (activeSubTabLegitbot) {
								case 1:
									//Main
									renderLegitbotWindow();
									break;
								case 2:
									//Backtrack
									renderBacktrackWindow();
									break;
								case 3:
									//Triggerbot
									renderTriggerbotWindow();
									break;
								case 4:
									//AntiAim
									renderLegitAntiAimWindow();
									break;
								default:
									break;
								}
								break;
							case 2: //Ragebot
								switch (activeSubTabRagebot) {
								case 1:
									//Main
									renderRagebotWindow();
									break;
								case 2:
									//Backtrack
									renderBacktrackWindow();
									break;
								case 3:
									//Anti aim
									renderRageAntiAimWindow();
									break;
								case 4:
									//Desync
									renderFakeAngleWindow();
									break;
								case 5:
									//Fakelag
									renderFakelagWindow();
									break;
								default:
									break;
								}
								break;
							case 3: // Visuals
								switch (activeSubTabVisuals) {
								case 1:
									//Main
									renderVisualsWindow();
									break;
								case 2:
									//ESP
									renderStreamProofESPWindow();
									break;
								case 3:
									//Chams
									renderChamsWindow();
									break;
								case 4:
									//Glow
									renderGlowWindow();
									break;
								case 5:
									//Skins
									renderSkinChangerWindow();
									break;
								default:
									break;
								}
								break;
							case 4:
								switch (activeSubTabMisc) {
								case 1:
									//Main
									renderMiscWindow();
									break;
								case 2:
									//Sound
									Sound::drawGUI();
									break;
								default:
									break;
								}
								break;
							case 5:
								//Configs
								renderConfigWindow();
								break;
							default:
								break;
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();

					ImGui::SetCursorPos(ImVec2{ 0, 410 });
					Style->Colors[ImGuiCol_ChildBg] = ImColor(45, 50, 54);
					Style->ChildRounding = 0;
					ImGui::BeginChild(xorstr_("##Text"), ImVec2{ 700, 20 }, false);
					{
						ImGui::SetCursorPos(ImVec2{ 2, 2 });
						ImGui::Text(xorstr_("local mom's spaghetti found on https://github.com/mkot2/OsirisAndExtra | Build Date: %s%s%s"), __DATE__, xorstr_(" "), __TIME__);
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
	Style->Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.75f);
}
#pragma optimize("", on)