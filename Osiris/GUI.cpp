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

#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Glow.h"
#include "Hacks/Misc.h"
#include "Hacks/Sound.h"
#include "Hacks/Visuals.h"

#include "GUI.h"
#include "Config.h"
#include "Helpers.h"
#include "Hooks.h"
#include "Interfaces.h"

#include "SDK/InputSystem.h"

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

		fonts.normal15px = io.Fonts->AddFontFromFileTTF((path / "msyh.ttc").string().c_str(), 15.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.normal15px)
			io.Fonts->AddFontDefault(&cfg);

		fonts.tahoma34 = io.Fonts->AddFontFromFileTTF((path / "msyh.ttc").string().c_str(), 34.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.tahoma34)
			io.Fonts->AddFontDefault(&cfg);

		fonts.tahoma28 = io.Fonts->AddFontFromFileTTF((path / "msyh.ttc").string().c_str(), 28.0f, &cfg, Helpers::getFontGlyphRanges());
		if (!fonts.tahoma28)
			io.Fonts->AddFontDefault(&cfg);

		cfg.MergeMode = true;
		static constexpr ImWchar symbol[]{
			0x2605, 0x2605, // ★
			0
		};
		io.Fonts->AddFontFromFileTTF((path / "seguisym.ttf").string().c_str(), 15.0f, &cfg, symbol);
		cfg.MergeMode = false;
	}

	if (!fonts.normal15px)
		io.Fonts->AddFontDefault(&cfg);
	if (!fonts.tahoma28)
		io.Fonts->AddFontDefault(&cfg);
	if (!fonts.tahoma34)
		io.Fonts->AddFontDefault(&cfg);
	addFontFromVFONT("csgo/panorama/fonts/notosanskr-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesKorean(), true);
	addFontFromVFONT("csgo/panorama/fonts/notosanssc-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesChineseFull(), true);
	constexpr auto unicodeFontSize = 16.0f;
	fonts.unicodeFont = addFontFromVFONT("csgo/panorama/fonts/notosans-bold.vfont", unicodeFontSize, Helpers::getFontGlyphRanges(), false);
	fonts.unicodeFont = addFontFromVFONT("csgo/panorama/fonts/notosans-bold.vfont", unicodeFontSize, io.Fonts->GetGlyphRangesChineseFull(), false);
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
		ImGui::Button("...", size);
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
	ImGui::PushID("Key");
	ImGui::hotkey2("Key", config->legitbotKey);
	ImGui::PopID();
	ImGui::Separator();
	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo("", &currentCategory, "All\0Pistols\0Heavy\0SMG\0Rifles\0");
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
		constexpr std::array pistols{ "All", "Glock-18", "P2000", "USP-S", "Dual Berettas", "P250", "Tec-9", "Five-Seven", "CZ-75", "Desert Eagle", "Revolver" };

		ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, pistols.size());

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		constexpr std::array heavies{ "All", "Nova", "XM1014", "Sawed-off", "MAG-7", "M249", "Negev" };

		ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, heavies.size());

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		constexpr std::array smgs{ "All", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon" };

		ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, smgs.size());

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		constexpr std::array rifles{ "All", "Galil AR", "Famas", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

		ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->legitbot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, rifles.size());

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox("Enabled", &config->legitbot[currentWeapon].enabled);
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 220.0f);
	ImGui::Checkbox("Aimlock", &config->legitbot[currentWeapon].aimlock);
	ImGui::Checkbox("Silent", &config->legitbot[currentWeapon].silent);
	ImGuiCustom::colorPicker("Draw FOV", config->legitbotFov);
	ImGui::Checkbox("Friendly fire", &config->legitbot[currentWeapon].friendlyFire);
	ImGui::Checkbox("Visible only", &config->legitbot[currentWeapon].visibleOnly);
	ImGui::Checkbox("Scoped only", &config->legitbot[currentWeapon].scopedOnly);
	ImGui::Checkbox("Ignore flash", &config->legitbot[currentWeapon].ignoreFlash);
	ImGui::Checkbox("Ignore smoke", &config->legitbot[currentWeapon].ignoreSmoke);
	ImGui::Checkbox("Auto scope", &config->legitbot[currentWeapon].autoScope);
	ImGuiCustom::multiCombo("Hitbox", config->legitbot[currentWeapon].hitboxes, "Head\0Chest\0Stomach\0Arms\0Legs\0Feet\0");
	ImGui::NextColumn();
	ImGui::PushItemWidth(240.0f);
	ImGui::SliderFloat("FOV", &config->legitbot[currentWeapon].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Smoothing", &config->legitbot[currentWeapon].smooth, 1.0f, 100.0f, "%.2f");
	ImGui::SliderInt("Reaction time", &config->legitbot[currentWeapon].reactionTime, 0, 300, "%d");
	ImGui::SliderInt("Min damage", &config->legitbot[currentWeapon].minDamage, 0, 100, "%d");
	config->legitbot[currentWeapon].minDamage = std::clamp(config->legitbot[currentWeapon].minDamage, 0, 250);
	ImGui::Checkbox("Killshot", &config->legitbot[currentWeapon].killshot);
	ImGui::Checkbox("Between shots", &config->legitbot[currentWeapon].betweenShots);

	if (!config->legitbot[currentWeapon].silent) {
		ImGui::Checkbox("Recoil control system", &config->legitbot[currentWeapon].rcs.enabled);
		ImGui::PushID("RCS");
		ImGui::SameLine();
		if (ImGui::Button("..."))
			ImGui::OpenPopup("");
		if (ImGui::BeginPopup("")) {
			ImGui::Checkbox("Silent", &config->legitbot[currentWeapon].rcs.silent);
			ImGui::Checkbox("Randomize", &config->legitbot[currentWeapon].rcs.randomize);
			ImGui::PushItemWidth(150.f);
			ImGui::SliderInt("Ignored shots", &config->legitbot[currentWeapon].rcs.ignoredShots, 0, 150, "%d");
			ImGui::SliderInt("Horizontal", &config->legitbot[currentWeapon].rcs.x, 0, 100, "%d%%");
			ImGui::SliderInt("Vertical", &config->legitbot[currentWeapon].rcs.y, 0, 100, "%d%%");
			ImGui::PopItemWidth();
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
	ImGui::Columns(1);
}

void GUI::renderRagebotWindow() noexcept
{
	ImGui::PushID("Ragebot Key");
	ImGui::hotkey2("Key", config->ragebotKey);
	ImGui::PopID();
	ImGui::Separator();
	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo("", &currentCategory, "All\0Pistols\0Heavy\0SMG\0Rifles\0");
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
		constexpr std::array pistols{ "All", "Glock-18", "P2000", "USP-S", "Dual Berettas", "P250", "Tec-9", "Five-Seven", "CZ-75", "Desert Eagle", "Revolver" };

		ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, pistols.size());

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		constexpr std::array heavies{ "All", "Nova", "XM1014", "Sawed-off", "MAG-7", "M249", "Negev" };

		ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, heavies.size());

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		constexpr std::array smgs{ "All", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon" };

		ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, smgs.size());

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		constexpr std::array rifles{ "All", "Galil AR", "Famas", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

		ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->ragebot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, rifles.size());

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox("Enabled", &config->ragebot[currentWeapon].enabled);
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 220.0f);
	ImGui::Checkbox("Aimlock", &config->ragebot[currentWeapon].aimlock);
	ImGui::Checkbox("Silent", &config->ragebot[currentWeapon].silent);
	ImGui::Checkbox("Friendly fire", &config->ragebot[currentWeapon].friendlyFire);
	ImGui::Checkbox("Visible only", &config->ragebot[currentWeapon].visibleOnly);
	ImGui::Checkbox("Scoped only", &config->ragebot[currentWeapon].scopedOnly);
	ImGui::Checkbox("Ignore flash", &config->ragebot[currentWeapon].ignoreFlash);
	ImGui::Checkbox("Ignore smoke", &config->ragebot[currentWeapon].ignoreSmoke);
	ImGui::Checkbox("Auto shot", &config->ragebot[currentWeapon].autoShot);
	ImGui::Checkbox("Auto scope", &config->ragebot[currentWeapon].autoScope);
	ImGui::Checkbox("Auto stop", &config->ragebot[currentWeapon].autoStop);
	ImGui::SameLine();
	ImGui::PushID("Auto stop");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Full stop", &config->ragebot[currentWeapon].fullStop);
		ImGui::Checkbox("Duck stop", &config->ragebot[currentWeapon].duckStop);
		ImGui::EndPopup();
	}
	ImGui::Checkbox("Resolver", &config->ragebot[currentWeapon].resolver);
	ImGui::Checkbox("Between shots", &config->ragebot[currentWeapon].betweenShots);
	ImGui::Combo("Priority", &config->ragebot[currentWeapon].priority, "Health\0Distance\0Fov\0");
	ImGuiCustom::multiCombo("Hitbox", config->ragebot[currentWeapon].hitboxes, "Head\0Chest\0Stomach\0Arms\0Legs\0Feet\0");
	ImGui::NextColumn();
	ImGui::PushItemWidth(240.0f);
	ImGui::SliderFloat("FOV", &config->ragebot[currentWeapon].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderInt("Hitchance", &config->ragebot[currentWeapon].hitChance, 0, 100, "%d");
	ImGui::SliderFloat("Accuracy boost", &config->ragebot[currentWeapon].accuracyBoost, 0, 1.0f, "%.2f");
	ImGui::SliderInt("Head multipoint", &config->ragebot[currentWeapon].headMultiPoint, 0, 100, "%d");
	ImGui::SliderInt("Body multipoint", &config->ragebot[currentWeapon].bodyMultiPoint, 0, 100, "%d");
	ImGui::SliderInt("Min damage", &config->ragebot[currentWeapon].minDamage, 0, 100, "%d");
	config->ragebot[currentWeapon].minDamage = std::clamp(config->ragebot[currentWeapon].minDamage, 0, 250);
	ImGui::PushID("Min damage override Key");
	ImGui::hotkey2("Min damage override Key", config->minDamageOverrideKey);
	ImGui::PopID();
	ImGui::SliderInt("Min damage override", &config->ragebot[currentWeapon].minDamageOverride, 0, 101, "%d");
	config->ragebot[currentWeapon].minDamageOverride = std::clamp(config->ragebot[currentWeapon].minDamageOverride, 0, 250);

	ImGui::PushID("Doubletap");
	ImGui::hotkey2("Doubletap", config->tickbase.doubletap);
	ImGui::PopID();
	ImGui::PushID("Hide shots");
	ImGui::hotkey2("Hide shots", config->tickbase.hideshots);
	ImGui::PopID();
	ImGui::Checkbox("Teleport on shift", &config->tickbase.teleport);
	ImGui::Checkbox("Onshot fakelag", &config->tickbase.onshotFl);
	ImGui::SameLine();
	ImGui::PushID("Onshot fakelag");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt("Amount", &config->tickbase.onshotFlAmount, 1, 52, "%d");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Columns(1);
}

void GUI::renderTriggerbotWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 350.0f);

	ImGui::hotkey2("Key", config->triggerbotKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	ImGui::Combo("", &currentCategory, "All\0Pistols\0Heavy\0SMG\0Rifles\0Zeus x27\0");
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
		constexpr std::array pistols{ "All", "Glock-18", "P2000", "USP-S", "Dual Berettas", "P250", "Tec-9", "Five-Seven", "CZ-75", "Desert Eagle", "Revolver" };

		ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx : 35].enabled) {
				static std::string name;
				name = pistols[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = pistols[idx];
			}
			return true;
			}, nullptr, pistols.size());

		currentWeapon = currentPistol ? currentPistol : 35;
		break;
	}
	case 2: {
		static int currentHeavy{ 0 };
		constexpr std::array heavies{ "All", "Nova", "XM1014", "Sawed-off", "MAG-7", "M249", "Negev" };

		ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 10 : 36].enabled) {
				static std::string name;
				name = heavies[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = heavies[idx];
			}
			return true;
			}, nullptr, heavies.size());

		currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
		break;
	}
	case 3: {
		static int currentSmg{ 0 };
		constexpr std::array smgs{ "All", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon" };

		ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 16 : 37].enabled) {
				static std::string name;
				name = smgs[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = smgs[idx];
			}
			return true;
			}, nullptr, smgs.size());

		currentWeapon = currentSmg ? currentSmg + 16 : 37;
		break;
	}
	case 4: {
		static int currentRifle{ 0 };
		constexpr std::array rifles{ "All", "Galil AR", "Famas", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

		ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
			if (config->triggerbot[idx ? idx + 23 : 38].enabled) {
				static std::string name;
				name = rifles[idx];
				*out_text = name.append(" *").c_str();
			} else {
				*out_text = rifles[idx];
			}
			return true;
			}, nullptr, rifles.size());

		currentWeapon = currentRifle ? currentRifle + 23 : 38;
		break;
	}
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Checkbox("Enabled", &config->triggerbot[currentWeapon].enabled);
	ImGui::Checkbox("Friendly fire", &config->triggerbot[currentWeapon].friendlyFire);
	ImGui::Checkbox("Scoped only", &config->triggerbot[currentWeapon].scopedOnly);
	ImGui::Checkbox("Ignore flash", &config->triggerbot[currentWeapon].ignoreFlash);
	ImGui::Checkbox("Ignore smoke", &config->triggerbot[currentWeapon].ignoreSmoke);
	ImGui::SetNextItemWidth(85.0f);
	ImGuiCustom::multiCombo("Hitbox", config->triggerbot[currentWeapon].hitboxes, "Head\0Chest\0Stomach\0Arms\0Legs\0Feet\0");
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Hitchance", &config->triggerbot[currentWeapon].hitChance, 0, 100, "%d");
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Shot delay", &config->triggerbot[currentWeapon].shotDelay, 0, 300, "%d ms");
	ImGui::SliderInt("Min damage", &config->triggerbot[currentWeapon].minDamage, 0, 100, "%d");
	config->triggerbot[currentWeapon].minDamage = std::clamp(config->triggerbot[currentWeapon].minDamage, 0, 250);
	ImGui::Checkbox("Killshot", &config->triggerbot[currentWeapon].killshot);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderFakelagWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo("", &current_category, "Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0");
	ImGui::Checkbox("Enabled", &config->fakelag[current_category].enabled);
	ImGui::Combo("Mode", &config->fakelag[current_category].mode, "Static\0Adaptive\0Random\0");
	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Limit", &config->fakelag[current_category].limit, 1, 21, "%d");
	ImGui::SliderInt("Random min limit", &config->fakelag[current_category].randomMinLimit, 1, 21, "%d");
	ImGui::PopItemWidth();
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderLegitAntiAimWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	ImGui::hotkey2("Invert key", config->legitAntiAim.invert, 80.0f);
	ImGui::Checkbox("Enabled", &config->legitAntiAim.enabled);
	ImGui::Checkbox("Disable in freeze time", &config->disableInFreezetime);
	ImGui::Checkbox("Extend", &config->legitAntiAim.extend);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderRageAntiAimWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo("", &current_category, "Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0");
	ImGui::Checkbox("Enabled", &config->rageAntiAim[current_category].enabled);
	ImGui::Checkbox("Disable in freezetime", &config->disableInFreezetime);
	ImGui::Combo("Pitch", &config->rageAntiAim[current_category].pitch, "Off\0Down\0Zero\0Up\0");
	ImGui::Combo("Yaw base", reinterpret_cast<int*>(&config->rageAntiAim[current_category].yawBase), "Off\0Paranoia\0Backward\0Right\0Left\0Spin\0");
	ImGui::Combo("Yaw modifier", reinterpret_cast<int*>(&config->rageAntiAim[current_category].yawModifier), "Off\0Jitter\0");

	if (config->rageAntiAim[current_category].yawBase == Yaw::paranoia) {
		ImGui::SliderInt("Paranoia min", &config->rageAntiAim[current_category].paranoiaMin, 0, 180, "%d");
		ImGui::SliderInt("Paranoia max", &config->rageAntiAim[current_category].paranoiaMax, 0, 180, "%d");
	}

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Yaw add", &config->rageAntiAim[current_category].yawAdd, -180, 180, "%d");
	ImGui::PopItemWidth();

	if (config->rageAntiAim[current_category].yawModifier == 1) //Jitter
		ImGui::SliderInt("Jitter yaw range", &config->rageAntiAim[current_category].jitterRange, 0, 90, "%d");

	if (config->rageAntiAim[current_category].yawBase == Yaw::spin) {
		ImGui::PushItemWidth(220.0f);
		ImGui::SliderInt("Spin base", &config->rageAntiAim[current_category].spinBase, -180, 180, "%d");
		ImGui::PopItemWidth();
	}

	ImGui::Checkbox("At targets", &config->rageAntiAim[current_category].atTargets);
	ImGui::hotkey2("Auto direction", config->autoDirection, 60.f);
	ImGui::hotkey2("Forward", config->manualForward, 60.f);
	ImGui::hotkey2("Backward", config->manualBackward, 60.f);
	ImGui::hotkey2("Right", config->manualRight, 60.f);
	ImGui::hotkey2("Left", config->manualLeft, 60.f);

	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderFakeAngleWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	static int current_category{};
	ImGui::Combo("", &current_category, "Freestanding\0Moving\0Jumping\0Ducking\0Duck-jumping\0Slow-walking\0Fake-ducking\0");
	ImGui::hotkey2("Invert key", config->invert, 80.0f);
	ImGui::Checkbox("Enabled", &config->fakeAngle[current_category].enabled);

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Left limit", &config->fakeAngle[current_category].leftLimit, 0, 60, "%d");
	ImGui::PopItemWidth();

	ImGui::PushItemWidth(220.0f);
	ImGui::SliderInt("Right limit", &config->fakeAngle[current_category].rightLimit, 0, 60, "%d");
	ImGui::PopItemWidth();

	ImGui::Combo("Mode", &config->fakeAngle[current_category].peekMode, "Off\0Peek real\0Peek fake\0Jitter\0Switch\0");
	ImGui::Combo("LBY mode", &config->fakeAngle[current_category].lbyMode, "Normal\0Opposite\0Sway\0");
	ImGui::Checkbox("Roll", &config->rageAntiAim[current_category].roll);
	if (config->rageAntiAim[current_category].roll) {
		ImGui::SliderInt("Roll add", &config->rageAntiAim[current_category].rollAdd, -90, 90, "%d");
		ImGui::SliderInt("Roll jitter", &config->rageAntiAim[current_category].rollOffset, -90, 90, "%d");

		ImGui::Checkbox("Using exploit pitch", &config->rageAntiAim[current_category].exploitPitchSwitch);
		if (config->rageAntiAim[current_category].exploitPitchSwitch)
			ImGui::SliderInt("Exploit pitch", &config->rageAntiAim[current_category].exploitPitch, -180, 180, "%d");
		else
			ImGui::SliderInt("Roll pitch", &config->rageAntiAim[current_category].rollPitch, -180, 180, "%d");
		ImGui::Checkbox("Alternative roll", &config->rageAntiAim[current_category].rollAlt);
	}

	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderBacktrackWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.f);
	ImGui::Checkbox("Enabled", &config->backtrack.enabled);
	ImGui::Checkbox("Ignore smoke", &config->backtrack.ignoreSmoke);
	ImGui::Checkbox("Ignore flash", &config->backtrack.ignoreFlash);
	ImGui::PushItemWidth(200.0f);
	ImGui::SliderInt("Time limit", &config->backtrack.timeLimit, 1, 200, "%d ms");
	ImGui::PopItemWidth();
	ImGui::NextColumn();
	ImGui::Checkbox("Enabled fake latency", &config->backtrack.fakeLatency);
	ImGui::PushItemWidth(200.0f);
	ImGui::SliderInt("Ping", &config->backtrack.fakeLatencyAmount, 1, 200, "%d ms");
	ImGui::PopItemWidth();
	ImGui::Columns(1);
}

void GUI::renderChamsWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 300.0f);
	ImGui::hotkey2("Key", config->chamsKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);

	static int material = 1;

	if (ImGui::Combo("", &currentCategory, "Allies\0Enemies\0Planting\0Defusing\0Local player\0Weapons\0Hands\0Backtrack\0Sleeves\0Desync\0Ragdolls\0Fake lag\0"))
		material = 1;

	ImGui::PopID();

	ImGui::SameLine();

	if (material <= 1)
		ImGuiCustom::arrowButtonDisabled("##left", ImGuiDir_Left);
	else if (ImGui::ArrowButton("##left", ImGuiDir_Left))
		--material;

	ImGui::SameLine();
	ImGui::Text("%d", material);

	constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local player", "Weapons", "Hands", "Backtrack", "Sleeves", "Desync", "Ragdolls", "Fake lag" };

	ImGui::SameLine();

	if (material >= int(config->chams[categories[currentCategory]].materials.size()))
		ImGuiCustom::arrowButtonDisabled("##right", ImGuiDir_Right);
	else if (ImGui::ArrowButton("##right", ImGuiDir_Right))
		++material;

	ImGui::SameLine();

	auto& chams{ config->chams[categories[currentCategory]].materials[material - 1] };

	ImGui::Checkbox("Enabled", &chams.enabled);
	ImGui::Separator();
	ImGui::Checkbox("Health based", &chams.healthBased);
	ImGui::Checkbox("Blinking", &chams.blinking);
	ImGui::Combo("Material", &chams.material, "Normal\0Flat\0Animated\0Platinum\0Glass\0Chrome\0Crystal\0Silver\0Gold\0Plastic\0Glow\0Pearlescent\0Metallic\0");
	ImGui::Checkbox("Wireframe", &chams.wireframe);
	ImGui::Checkbox("Cover", &chams.cover);
	ImGui::Checkbox("Ignore-Z", &chams.ignorez);
	ImGuiCustom::colorPicker("Color", chams);
	ImGui::NextColumn();
	ImGui::Columns(1);
}

void GUI::renderGlowWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 400.f);
	ImGui::hotkey2("Key", config->glowKey, 80.0f);
	ImGui::Separator();

	static int currentCategory{ 0 };
	ImGui::PushItemWidth(110.0f);
	ImGui::PushID(0);
	constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local Player", "Weapons", "C4", "Planted C4", "Chickens", "Defuse Kits", "Projectiles", "Hostages" };
	ImGui::Combo("", &currentCategory, categories.data(), categories.size());
	ImGui::PopID();
	Config::GlowItem* currentItem;
	static int currentType{ 0 };
	if (currentCategory <= 3) {
		ImGui::SameLine();
		ImGui::PushID(1);
		ImGui::Combo("", &currentType, "All\0Visible\0Occluded\0");
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
	ImGui::Checkbox("Enabled", &currentItem->enabled);
	ImGui::Separator();
	ImGui::Checkbox("Health based", &currentItem->healthBased);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::Combo("Style", &currentItem->style, "Default\0Rim3d\0Edge\0Edge Pulse\0");
	if (currentType == 2 && currentCategory <= 3) ImGui::Checkbox("Audible only", &currentItem->audibleOnly);

	ImGuiCustom::colorPicker("Color", *currentItem);

	ImGui::Columns(1);
}

void GUI::renderStreamProofESPWindow() noexcept
{
	ImGui::hotkey2("Key", config->streamProofESP.key, 80.0f);
	ImGui::Separator();

	static std::size_t currentCategory;
	static auto currentItem = "All";

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

	if (ImGui::BeginListBox("##list", { 170.0f, 300.0f })) {
		constexpr std::array categories{ "Enemies", "Allies", "Weapons", "Projectiles", "Loot Crates", "Other Entities" };

		for (std::size_t i = 0; i < categories.size(); ++i) {
			if (ImGui::Selectable(categories[i], currentCategory == i && std::string_view{ currentItem } == "All")) {
				currentCategory = i;
				currentItem = "All";
			}

			if (ImGui::BeginDragDropSource()) {
				switch (i) {
				case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, "All"), sizeof(Player), ImGuiCond_Once); break;
				case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons["All"], sizeof(Weapon), ImGuiCond_Once); break;
				case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles["All"], sizeof(Projectile), ImGuiCond_Once); break;
				default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, "All"), sizeof(Shared), ImGuiCond_Once); break;
				}
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
					const auto& data = *(Player*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, "All") = data; break;
					case 2: config->streamProofESP.weapons["All"] = data; break;
					case 3: config->streamProofESP.projectiles["All"] = data; break;
					default: getConfigShared(i, "All") = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
					const auto& data = *(Weapon*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, "All") = data; break;
					case 2: config->streamProofESP.weapons["All"] = data; break;
					case 3: config->streamProofESP.projectiles["All"] = data; break;
					default: getConfigShared(i, "All") = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
					const auto& data = *(Projectile*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, "All") = data; break;
					case 2: config->streamProofESP.weapons["All"] = data; break;
					case 3: config->streamProofESP.projectiles["All"] = data; break;
					default: getConfigShared(i, "All") = data; break;
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
					const auto& data = *(Shared*)payload->Data;

					switch (i) {
					case 0: case 1: getConfigPlayer(i, "All") = data; break;
					case 2: config->streamProofESP.weapons["All"] = data; break;
					case 3: config->streamProofESP.projectiles["All"] = data; break;
					default: getConfigShared(i, "All") = data; break;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PushID(i);
			ImGui::Indent();

			const auto items = [](std::size_t category) noexcept -> std::vector<const char*> {
				switch (category) {
				case 0:
				case 1: return { "Visible", "Occluded" };
				case 2: return { "Pistols", "SMGs", "Rifles", "Sniper Rifles", "Shotguns", "Machineguns", "Grenades", "Melee", "Other" };
				case 3: return { "Flashbang", "HE Grenade", "Breach Charge", "Bump Mine", "Decoy Grenade", "Molotov", "TA Grenade", "Smoke Grenade", "Snowball" };
				case 4: return { "Pistol Case", "Light Case", "Heavy Case", "Explosive Case", "Tools Case", "Cash Dufflebag" };
				case 5: return { "Defuse Kit", "Chicken", "Planted C4", "Hostage", "Sentry", "Cash", "Ammo Box", "Radar Jammer", "Snowball Pile", "Collectable Coin" };
				default: return { };
				}
			}(i);

			const auto categoryEnabled = getConfigShared(i, "All").enabled;

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
						case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, items[j]), sizeof(Player), ImGuiCond_Once); break;
						case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons[items[j]], sizeof(Weapon), ImGuiCond_Once); break;
						case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles[items[j]], sizeof(Projectile), ImGuiCond_Once); break;
						default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, items[j]), sizeof(Shared), ImGuiCond_Once); break;
						}
						ImGui::EndDragDropSource();
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
							const auto& data = *(Player*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
							const auto& data = *(Weapon*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
							const auto& data = *(Projectile*)payload->Data;

							switch (i) {
							case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
							case 2: config->streamProofESP.weapons[items[j]] = data; break;
							case 3: config->streamProofESP.projectiles[items[j]] = data; break;
							default: getConfigShared(i, items[j]) = data; break;
							}
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
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

				const auto subItems = [](std::size_t item) noexcept -> std::vector<const char*> {
					switch (item) {
					case 0: return { "Glock-18", "P2000", "USP-S", "Dual Berettas", "P250", "Tec-9", "Five-SeveN", "CZ75-Auto", "Desert Eagle", "R8 Revolver" };
					case 1: return { "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon" };
					case 2: return { "Galil AR", "FAMAS", "AK-47", "M4A4", "M4A1-S", "SG 553", "AUG" };
					case 3: return { "SSG 08", "AWP", "G3SG1", "SCAR-20" };
					case 4: return { "Nova", "XM1014", "Sawed-Off", "MAG-7" };
					case 5: return { "M249", "Negev" };
					case 6: return { "Flashbang", "HE Grenade", "Smoke Grenade", "Molotov", "Decoy Grenade", "Incendiary", "TA Grenade", "Fire Bomb", "Diversion", "Frag Grenade", "Snowball" };
					case 7: return { "Axe", "Hammer", "Wrench" };
					case 8: return { "C4", "Healthshot", "Bump Mine", "Zone Repulsor", "Shield" };
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
						ImGui::SetDragDropPayload("Weapon", &subItemConfig, sizeof(Weapon), ImGuiCond_Once);
						ImGui::EndDragDropSource();
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
							const auto& data = *(Player*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
							const auto& data = *(Weapon*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
							const auto& data = *(Projectile*)payload->Data;
							subItemConfig = data;
						}

						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
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
	if (ImGui::BeginChild("##child", { 400.0f, 0.0f })) {
		auto& sharedConfig = getConfigShared(currentCategory, currentItem);

		ImGui::Checkbox("Enabled", &sharedConfig.enabled);
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 260.0f);
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("Font", config->getSystemFonts()[sharedConfig.font.index].c_str())) {
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
		ImGuiCustom::colorPicker("Snapline", sharedConfig.snapline);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::Combo("##1", &sharedConfig.snapline.type, "Bottom\0Top\0Crosshair\0");
		ImGui::SameLine(spacing);
		ImGuiCustom::colorPicker("Box", sharedConfig.box);
		ImGui::SameLine();

		ImGui::PushID("Box");

		if (ImGui::Button("..."))
			ImGui::OpenPopup("");

		if (ImGui::BeginPopup("")) {
			ImGui::SetNextItemWidth(95.0f);
			ImGui::Combo("Type", &sharedConfig.box.type, "2D\0" "2D corners\0" "3D\0" "3D corners\0");
			ImGui::SetNextItemWidth(275.0f);
			ImGui::SliderFloat3("Scale", sharedConfig.box.scale.data(), 0.0f, 0.50f, "%.2f");
			ImGuiCustom::colorPicker("Fill", sharedConfig.box.fill);
			ImGui::EndPopup();
		}

		ImGui::PopID();

		ImGuiCustom::colorPicker("Name", sharedConfig.name);
		ImGui::SameLine(spacing);

		if (currentCategory < 2) {
			auto& playerConfig = getConfigPlayer(currentCategory, currentItem);

			ImGuiCustom::colorPicker("Weapon", playerConfig.weapon);
			ImGuiCustom::colorPicker("Flash Duration", playerConfig.flashDuration);
			ImGui::SameLine(spacing);
			ImGuiCustom::colorPicker("Skeleton", playerConfig.skeleton);
			ImGui::Checkbox("Audible Only", &playerConfig.audibleOnly);
			ImGui::SameLine(spacing);
			ImGui::Checkbox("Spotted Only", &playerConfig.spottedOnly);

			ImGuiCustom::colorPicker("Head Box", playerConfig.headBox);
			ImGui::SameLine();

			ImGui::PushID("Head Box");

			if (ImGui::Button("..."))
				ImGui::OpenPopup("");

			if (ImGui::BeginPopup("")) {
				ImGui::SetNextItemWidth(95.0f);
				ImGui::Combo("Type", &playerConfig.headBox.type, "2D\0" "2D corners\0" "3D\0" "3D corners\0");
				ImGui::SetNextItemWidth(275.0f);
				ImGui::SliderFloat3("Scale", playerConfig.headBox.scale.data(), 0.0f, 0.50f, "%.2f");
				ImGuiCustom::colorPicker("Fill", playerConfig.headBox.fill);
				ImGui::EndPopup();
			}

			ImGui::PopID();

			ImGui::SameLine(spacing);
			ImGui::Checkbox("Health Bar", &playerConfig.healthBar.enabled);
			ImGui::SameLine();

			ImGui::PushID("Health Bar");

			if (ImGui::Button("..."))
				ImGui::OpenPopup("");

			if (ImGui::BeginPopup("")) {
				ImGui::SetNextItemWidth(95.0f);
				ImGui::Combo("Type", &playerConfig.healthBar.type, "Gradient\0Solid\0Health-based\0");
				if (playerConfig.healthBar.type == HealthBar::Solid) {
					ImGui::SameLine();
					ImGuiCustom::colorPicker("", static_cast<Color4&>(playerConfig.healthBar));
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();

			ImGuiCustom::colorPicker("Footsteps", config->visuals.footsteps.footstepBeams);
			ImGui::SliderInt("Thickness", &config->visuals.footsteps.footstepBeamThickness, 0, 30, "Thickness: %d%%");
			ImGui::SliderInt("Radius", &config->visuals.footsteps.footstepBeamRadius, 0, 230, "Radius: %d%%");

			ImGuiCustom::colorPicker("Line of sight", playerConfig.lineOfSight);

		} else if (currentCategory == 2) {
			auto& weaponConfig = config->streamProofESP.weapons[currentItem];
			ImGuiCustom::colorPicker("Ammo", weaponConfig.ammo);
		} else if (currentCategory == 3) {
			auto& trails = config->streamProofESP.projectiles[currentItem].trails;

			ImGui::Checkbox("Trails", &trails.enabled);
			ImGui::SameLine(spacing + 77.0f);
			ImGui::PushID("Trails");

			if (ImGui::Button("..."))
				ImGui::OpenPopup("");

			if (ImGui::BeginPopup("")) {
				constexpr auto trailPicker = [](const char* name, Trail& trail) noexcept {
					ImGui::PushID(name);
					ImGuiCustom::colorPicker(name, trail);
					ImGui::SameLine(150.0f);
					ImGui::SetNextItemWidth(95.0f);
					ImGui::Combo("", &trail.type, "Line\0Circles\0Filled Circles\0");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(95.0f);
					ImGui::InputFloat("Time", &trail.time, 0.1f, 0.5f, "%.1fs");
					trail.time = std::clamp(trail.time, 1.0f, 60.0f);
					ImGui::PopID();
				};

				trailPicker("Local Player", trails.localPlayer);
				trailPicker("Allies", trails.allies);
				trailPicker("Enemies", trails.enemies);
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::SetNextItemWidth(95.0f);
		ImGui::InputFloat("Text Cull Distance", &sharedConfig.textCullDistance, 0.4f, 0.8f, "%.1fm");
		sharedConfig.textCullDistance = std::clamp(sharedConfig.textCullDistance, 0.0f, 999.9f);
	}

	ImGui::EndChild();
}

void GUI::renderVisualsWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 280.0f);
	constexpr const char* playerModels = "Default\0Special Agent Ava | FBI\0Operator | FBI SWAT\0Markus Delrow | FBI HRT\0Michael Syfers | FBI Sniper\0B Squadron Officer | SAS\0Seal Team 6 Soldier | NSWC SEAL\0Buckshot | NSWC SEAL\0Lt. Commander Ricksaw | NSWC SEAL\0Third Commando Company | KSK\0'Two Times' McCoy | USAF TACP\0Dragomir | Sabre\0Rezan The Ready | Sabre\0'The Doctor' Romanov | Sabre\0Maximus | Sabre\0Blackwolf | Sabre\0The Elite Mr. Muhlik | Elite Crew\0Ground Rebel | Elite Crew\0Osiris | Elite Crew\0Prof. Shahmat | Elite Crew\0Enforcer | Phoenix\0Slingshot | Phoenix\0Soldier | Phoenix\0Pirate\0Pirate Variant A\0Pirate Variant B\0Pirate Variant C\0Pirate Variant D\0Anarchist\0Anarchist Variant A\0Anarchist Variant B\0Anarchist Variant C\0Anarchist Variant D\0Balkan Variant A\0Balkan Variant B\0Balkan Variant C\0Balkan Variant D\0Balkan Variant E\0Jumpsuit Variant A\0Jumpsuit Variant B\0Jumpsuit Variant C\0GIGN\0GIGN Variant A\0GIGN Variant B\0GIGN Variant C\0GIGN Variant D\0Street Soldier | Phoenix\0'Blueberries' Buckshot | NSWC SEAL\0'Two Times' McCoy | TACP Cavalry\0Rezan the Redshirt | Sabre\0Dragomir | Sabre Footsoldier\0Cmdr. Mae 'Dead Cold' Jamison | SWAT\0001st Lieutenant Farlow | SWAT\0John 'Van Healen' Kask | SWAT\0Bio-Haz Specialist | SWAT\0Sergeant Bombson | SWAT\0Chem-Haz Specialist | SWAT\0Sir Bloody Miami Darryl | The Professionals\0Sir Bloody Silent Darryl | The Professionals\0Sir Bloody Skullhead Darryl | The Professionals\0Sir Bloody Darryl Royale | The Professionals\0Sir Bloody Loudmouth Darryl | The Professionals\0Safecracker Voltzmann | The Professionals\0Little Kev | The Professionals\0Number K | The Professionals\0Getaway Sally | The Professionals\0";
	ImGui::Combo("T model", &config->visuals.playerModelT, playerModels);
	ImGui::Combo("CT model", &config->visuals.playerModelCT, playerModels);
	ImGui::InputText("Custom model", config->visuals.playerModel, sizeof(config->visuals.playerModel));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("file must be in csgo/models/ directory and must start with models/...");
	ImGui::Checkbox("Disable post-processing", &config->visuals.disablePostProcessing);
	ImGui::Checkbox("Disable jiggle bones", &config->visuals.disableJiggleBones);
	ImGui::Checkbox("Inverse ragdoll gravity", &config->visuals.inverseRagdollGravity);
	ImGui::Checkbox("No fog", &config->visuals.noFog);

	ImGuiCustom::colorPicker("Fog controller", config->visuals.fog);
	ImGui::SameLine();

	ImGui::PushID("Fog controller");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {

		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(7);
		ImGui::SliderFloat("Start", &config->visuals.fogOptions.start, 0.0f, 5000.0f, "Start: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(8);
		ImGui::SliderFloat("End", &config->visuals.fogOptions.end, 0.0f, 5000.0f, "End: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(9);
		ImGui::SliderFloat("Density", &config->visuals.fogOptions.density, 0.001f, 1.0f, "Density: %.3f");
		ImGui::PopID();

		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox("No 3D sky", &config->visuals.no3dSky);
	ImGui::Checkbox("No aim punch", &config->visuals.noAimPunch);
	ImGui::Checkbox("No view punch", &config->visuals.noViewPunch);
	ImGui::Checkbox("No view bob", &config->visuals.noViewBob);
	ImGui::Checkbox("No hands", &config->visuals.noHands);
	ImGui::Checkbox("No sleeves", &config->visuals.noSleeves);
	ImGui::Checkbox("No weapons", &config->visuals.noWeapons);
	ImGui::Checkbox("No smoke", &config->visuals.noSmoke);
	ImGui::Checkbox("Smoke Circle", &config->visuals.smokeCircle);
	ImGui::SameLine();
	ImGui::Checkbox("Wireframe smoke", &config->visuals.wireframeSmoke);
	ImGui::Checkbox("No molotov", &config->visuals.noMolotov);
	ImGui::SameLine();
	ImGui::Checkbox("Wireframe molotov", &config->visuals.wireframeMolotov);
	ImGui::Checkbox("No blur", &config->visuals.noBlur);
	ImGui::Checkbox("No scope overlay", &config->visuals.noScopeOverlay);
	ImGui::Checkbox("No scope zoom", &config->visuals.noScopeZoom);
	ImGui::Checkbox("No grass", &config->visuals.noGrass);
	ImGui::Checkbox("No shadows", &config->visuals.noShadows);

	ImGui::Checkbox("Shadow changer", &config->visuals.shadowsChanger.enabled);
	ImGui::SameLine();

	ImGui::PushID("Shadow changer");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(5);
		ImGui::SliderInt("", &config->visuals.shadowsChanger.x, 0, 360, "x rotation: %d");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(6);
		ImGui::SliderInt("", &config->visuals.shadowsChanger.y, 0, 360, "y rotation: %d");
		ImGui::PopID();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Motion blur", &config->visuals.motionBlur.enabled);
	ImGui::SameLine();

	ImGui::PushID("Motion blur");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {

		ImGui::Checkbox("Forward enabled", &config->visuals.motionBlur.forwardEnabled);

		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(10);
		ImGui::SliderFloat("Falling min", &config->visuals.motionBlur.fallingMin, 0.0f, 50.0f, "Falling min: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(11);
		ImGui::SliderFloat("Falling max", &config->visuals.motionBlur.fallingMax, 0.0f, 50.0f, "Falling max: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat("Falling intesity", &config->visuals.motionBlur.fallingIntensity, 0.0f, 8.0f, "Falling intensity: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat("Rotation intensity", &config->visuals.motionBlur.rotationIntensity, 0.0f, 8.0f, "Rotation intensity: %.2f");
		ImGui::PopID();
		ImGui::PushItemWidth(290.0f);
		ImGui::PushID(12);
		ImGui::SliderFloat("Strength", &config->visuals.motionBlur.strength, 0.0f, 8.0f, "Strength: %.2f");
		ImGui::PopID();

		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Full bright", &config->visuals.fullBright);
	ImGui::NextColumn();
	ImGui::Checkbox("Zoom", &config->visuals.zoom);
	ImGui::SameLine();
	ImGui::PushID("Zoom Key");
	ImGui::hotkey2("", config->visuals.zoomKey);
	ImGui::PopID();
	ImGui::Checkbox("Thirdperson", &config->visuals.thirdperson);
	ImGui::SameLine();
	ImGui::PushID("Thirdperson Key");
	ImGui::hotkey2("", config->visuals.thirdpersonKey);
	ImGui::PopID();
	ImGui::PushItemWidth(290.0f);
	ImGui::PushID(0);
	ImGui::SliderInt("", &config->visuals.thirdpersonDistance, 0, 1000, "Thirdperson distance: %d");
	ImGui::PopID();
	ImGui::PushID("Thirdperson Transparency");
    ImGui::SliderFloat("", &config->visuals.thirdpersonTransparency, 0.0f, 100.0f, "Scope Transparency: %2.f%");
    ImGui::PopID();
	ImGui::Checkbox("Freecam", &config->visuals.freeCam);
	ImGui::SameLine();
	ImGui::PushID("Freecam Key");
	ImGui::hotkey2("", config->visuals.freeCamKey);
	ImGui::PopID();
	ImGui::PushItemWidth(290.0f);
	ImGui::PushID(1);
	ImGui::SliderInt("", &config->visuals.freeCamSpeed, 1, 10, "Freecam speed: %d");
	ImGui::PopID();
	ImGui::PushID(2);
	ImGui::SliderInt("", &config->visuals.fov, -60, 60, "FOV: %d");
	ImGui::PopID();
	ImGui::PushID(3);
	ImGui::SliderInt("", &config->visuals.farZ, 0, 2000, "Far Z: %d");
	ImGui::PopID();
	ImGui::PushID(4);
	ImGui::SliderInt("", &config->visuals.flashReduction, 0, 100, "Flash reduction: %d%%");
	ImGui::PopID();
	ImGui::PushID(5);
	ImGui::SliderFloat("", &config->visuals.glowOutlineWidth, 0.0f, 100.0f, "Glow thickness: %.2f");
	ImGui::PopID();
	ImGui::Combo("Skybox", &config->visuals.skybox, Visuals::skyboxList.data(), Visuals::skyboxList.size());
	if (config->visuals.skybox == 26) {
		ImGui::InputText("Skybox filename", &config->visuals.customSkybox);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("skybox files must be put in csgo/materials/skybox/ ");
	}
	ImGuiCustom::colorPicker("Console color", config->visuals.console);
	ImGuiCustom::colorPicker("World color", config->visuals.world);
	ImGuiCustom::colorPicker("Props color", config->visuals.props);
	ImGuiCustom::colorPicker("Sky color", config->visuals.sky);
	ImGuiCustom::colorPicker("Molotov color", config->visuals.molotovColor);
    ImGuiCustom::colorPicker("Smoke color", config->visuals.smokeColor);
	ImGui::PushID(13);
	ImGui::SliderInt("", &config->visuals.asusWalls, 0, 100, "Asus walls: %d");
	ImGui::PopID();
	ImGui::PushID(14);
	ImGui::SliderInt("", &config->visuals.asusProps, 0, 100, "Asus props: %d");
	ImGui::PopID();
	ImGui::Checkbox("Deagle spinner", &config->visuals.deagleSpinner);
	ImGui::Combo("Screen effect", &config->visuals.screenEffect, "None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0");
	ImGui::Combo("Hit effect", &config->visuals.hitEffect, "None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0");
	ImGui::SliderFloat("Hit effect time", &config->visuals.hitEffectTime, 0.1f, 1.5f, "%.2fs");
	ImGui::Combo("Hit marker", &config->visuals.hitMarker, "None\0Default (Cross)\0");
	ImGui::SliderFloat("Hit marker time", &config->visuals.hitMarkerTime, 0.1f, 1.5f, "%.2fs");
	ImGuiCustom::colorPicker("Bullet tracers", config->visuals.bulletTracers.color.data(), &config->visuals.bulletTracers.color[3], nullptr, nullptr, &config->visuals.bulletTracers.enabled);
	ImGuiCustom::colorPicker("Bullet impacts", config->visuals.bulletImpacts.color.data(), &config->visuals.bulletImpacts.color[3], nullptr, nullptr, &config->visuals.bulletImpacts.enabled);
	ImGui::SliderFloat("Bullet impacts time", &config->visuals.bulletImpactsTime, 0.1f, 5.0f, "Bullet Impacts time: %.2fs");
	ImGuiCustom::colorPicker("On hit hitbox", config->visuals.onHitHitbox.color.color.data(), &config->visuals.onHitHitbox.color.color[3], nullptr, nullptr, &config->visuals.onHitHitbox.color.enabled);
	ImGui::SliderFloat("On hit hitbox time", &config->visuals.onHitHitbox.duration, 0.1f, 60.0f, "On Hit Hitbox time: % .2fs");
	ImGuiCustom::colorPicker("Molotov hull", config->visuals.molotovHull);
	ImGuiCustom::colorPicker("Smoke hull", config->visuals.smokeHull);
	ImGuiCustom::colorPicker("Nade blast", config->visuals.nadeBlast);
	ImGui::Checkbox("Molotov polygon", &config->visuals.molotovPolygon.enabled);
	ImGui::SameLine();
	if (ImGui::Button("...##molotov_polygon"))
		ImGui::OpenPopup("popup_molotovPolygon");

	if (ImGui::BeginPopup("popup_molotovPolygon")) {
		ImGuiCustom::colorPicker("Self", config->visuals.molotovPolygon.self.color.data(), &config->visuals.molotovPolygon.self.color[3], &config->visuals.molotovPolygon.self.rainbow, &config->visuals.molotovPolygon.self.rainbowSpeed, nullptr);
		ImGuiCustom::colorPicker("Team", config->visuals.molotovPolygon.team.color.data(), &config->visuals.molotovPolygon.team.color[3], &config->visuals.molotovPolygon.team.rainbow, &config->visuals.molotovPolygon.team.rainbowSpeed, nullptr);
		ImGuiCustom::colorPicker("Enemy", config->visuals.molotovPolygon.enemy.color.data(), &config->visuals.molotovPolygon.enemy.color[3], &config->visuals.molotovPolygon.enemy.rainbow, &config->visuals.molotovPolygon.enemy.rainbowSpeed, nullptr);
		ImGui::EndPopup();
	}

	ImGui::Checkbox("Smoke timer", &config->visuals.smokeTimer);
	ImGui::SameLine();
	if (ImGui::Button("...##smoke_timer"))
		ImGui::OpenPopup("popup_smokeTimer");

	if (ImGui::BeginPopup("popup_smokeTimer")) {
		ImGuiCustom::colorPicker("Background color", config->visuals.smokeTimerBG);
		ImGuiCustom::colorPicker("Text color", config->visuals.smokeTimerText);
		ImGuiCustom::colorPicker("Timer color", config->visuals.smokeTimerTimer);
		ImGui::EndPopup();
	}
	ImGui::Checkbox("Molotov timer", &config->visuals.molotovTimer);
	ImGui::SameLine();
	if (ImGui::Button("...##molotov_timer"))
		ImGui::OpenPopup("popup_molotovTimer");

	if (ImGui::BeginPopup("popup_molotovTimer")) {
		ImGuiCustom::colorPicker("Background color", config->visuals.molotovTimerBG);
		ImGuiCustom::colorPicker("Text color", config->visuals.molotovTimerText);
		ImGuiCustom::colorPicker("Timer color", config->visuals.molotovTimerTimer);
		ImGui::EndPopup();
	}
	ImGuiCustom::colorPicker("Spread circle", config->visuals.spreadCircle);

	ImGui::Checkbox("Viewmodel", &config->visuals.viewModel.enabled);
	ImGui::SameLine();

	if (bool ccPopup = ImGui::Button("Edit"))
		ImGui::OpenPopup("##viewmodel");

	if (ImGui::BeginPopup("##viewmodel")) {
		ImGui::PushItemWidth(290.0f);
		ImGui::SliderFloat("##x", &config->visuals.viewModel.x, -20.0f, 20.0f, "X: %.4f");
		ImGui::SliderFloat("##y", &config->visuals.viewModel.y, -20.0f, 20.0f, "Y: %.4f");
		ImGui::SliderFloat("##z", &config->visuals.viewModel.z, -20.0f, 20.0f, "Z: %.4f");
		ImGui::SliderInt("##fov", &config->visuals.viewModel.fov, -60, 60, "Viewmodel FOV: %d");
		ImGui::SliderFloat("##roll", &config->visuals.viewModel.roll, -90.0f, 90.0f, "Viewmodel roll: %.2f");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}

	ImGui::Columns(1);
}

void GUI::renderMiscWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 250.0f);
	hotkey3("Menu Key", config->misc.menuKey);
	ImGui::Checkbox("Anti AFK kick", &config->misc.antiAfkKick);
	ImGui::Checkbox("Adblock", &config->misc.adBlock);
	ImGui::Combo("Force region", &config->misc.forceRelayCluster, "Off\0Australia\0Austria\0Brazil\0Chile\0Dubai\0France\0Germany\0Hong Kong\0India (Chennai)\0India (Mumbai)\0Japan\0Luxembourg\0Netherlands\0Peru\0Philipines\0Poland\0Singapore\0South Africa\0Spain\0Sweden\0UK\0USA (Atlanta)\0USA (Seattle)\0USA (Chicago)\0USA (Los Angeles)\0USA (Moses Lake)\0USA (Oklahoma)\0USA (Seattle)\0USA (Washington DC)\0");
	ImGui::Checkbox("Auto strafe", &config->misc.autoStrafe);
	ImGui::Checkbox("Bunny hop", &config->misc.bunnyHop);
	ImGui::Checkbox("Fast duck", &config->misc.fastDuck);
	ImGui::Checkbox("Moonwalk", &config->misc.moonwalk);
	ImGui::SameLine();
	ImGui::Checkbox("Break", &config->misc.leg_break);
	ImGui::Checkbox("Knifebot", &config->misc.knifeBot);
	ImGui::PushID("Knifebot");
	ImGui::SameLine();
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::Combo("Mode", &config->misc.knifeBotMode, "Trigger\0Rage\0");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Blockbot", &config->misc.blockBot);
	ImGui::SameLine();
	ImGui::PushID("Blockbot Key");
	ImGui::hotkey2("", config->misc.blockBotKey);
	ImGui::PopID();
	ImGui::PushID("Blockbot");
	ImGui::SameLine();
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGuiCustom::colorPicker("Visualize", config->misc.blockBotVisualize);
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Edge jump", &config->misc.edgeJump);
	ImGui::SameLine();
	ImGui::PushID("Edge jump Key");
	ImGui::hotkey2("", config->misc.edgeJumpKey);
	ImGui::PopID();

	ImGui::Checkbox("Mini jump", &config->misc.miniJump);
	ImGui::SameLine();
	ImGui::PushID("Mini jump Key");
	ImGui::hotkey2("", config->misc.miniJumpKey);
	ImGui::PopID();
	ImGui::PushID("Mini jump");
	ImGui::SameLine();
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt("Crouch lock", &config->misc.miniJumpCrouchLock, 0, 12, "%d ticks");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Jump bug", &config->misc.jumpBug);
	ImGui::SameLine();
	ImGui::PushID("Jump bug Key");
	ImGui::hotkey2("", config->misc.jumpBugKey);
	ImGui::PopID();

	ImGui::Checkbox("Edge bug", &config->misc.edgeBug);
	ImGui::SameLine();
	ImGui::PushID("Edge bug Key");
	ImGui::hotkey2("", config->misc.edgeBugKey);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("Edge bug");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt("Prediction amount", &config->misc.edgeBugPredAmnt, 0, 128, "%d ticks");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Auto pixel surf", &config->misc.autoPixelSurf);
	ImGui::SameLine();
	ImGui::PushID("Auto pixel surf Key");
	ImGui::hotkey2("", config->misc.autoPixelSurfKey);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("Auto pixel surf");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt("Prediction amount", &config->misc.autoPixelSurfPredAmnt, 2, 4, "%d ticks");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Draw velocity", &config->misc.velocity.enabled);
	ImGui::SameLine();

	ImGui::PushID("Draw velocity");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::SliderFloat("Position", &config->misc.velocity.position, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha", &config->misc.velocity.alpha, 0.0f, 1.0f);
		ImGuiCustom::colorPicker("Force color", config->misc.velocity.color.color.data(), nullptr, &config->misc.velocity.color.rainbow, &config->misc.velocity.color.rainbowSpeed, &config->misc.velocity.color.enabled);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Keyboard display", &config->misc.keyBoardDisplay.enabled);
	ImGui::SameLine();

	ImGui::PushID("Keyboard display");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::SliderFloat("Position", &config->misc.keyBoardDisplay.position, 0.0f, 1.0f);
		ImGui::Checkbox("Show key tiles", &config->misc.keyBoardDisplay.showKeyTiles);
		ImGuiCustom::colorPicker("Color", config->misc.keyBoardDisplay.color);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Slowwalk", &config->misc.slowwalk);
	ImGui::SameLine();
	ImGui::PushID("Slowwalk Key");
	ImGui::hotkey2("", config->misc.slowwalkKey);
	ImGui::PopID();
	ImGui::PushID("Slowwalk");
	ImGui::SameLine();
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");
	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.f);
		ImGui::SliderInt("Velocity", &config->misc.slowwalkAmnt, 0, 50, config->misc.slowwalkAmnt ? "%d u/s" : "Default");
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Fakeduck", &config->misc.fakeduck);
	ImGui::SameLine();
	ImGui::PushID("Fakeduck Key");
	ImGui::hotkey2("", config->misc.fakeduckKey);
	ImGui::PopID();
	ImGuiCustom::colorPicker("Auto peek", config->misc.autoPeek.color.data(), &config->misc.autoPeek.color[3], &config->misc.autoPeek.rainbow, &config->misc.autoPeek.rainbowSpeed, &config->misc.autoPeek.enabled);
	ImGui::SameLine();
	ImGui::PushID("Auto peek Key");
	ImGui::hotkey2("", config->misc.autoPeekKey);
	ImGui::PopID();
	ImGui::Checkbox("Noscope crosshair", &config->misc.noscopeCrosshair);
	ImGui::Checkbox("Recoil crosshair", &config->misc.recoilCrosshair);
	ImGui::Checkbox("Auto pistol", &config->misc.autoPistol);
	ImGui::Checkbox("Auto reload", &config->misc.autoReload);
	ImGui::Checkbox("Auto accept", &config->misc.autoAccept);
	ImGui::Checkbox("Radar hack", &config->misc.radarHack);
	ImGui::Checkbox("Reveal ranks", &config->misc.revealRanks);
	ImGui::Checkbox("Reveal money", &config->misc.revealMoney);
	ImGui::Checkbox("Reveal suspect", &config->misc.revealSuspect);
	ImGui::Checkbox("Reveal votes", &config->misc.revealVotes);

	ImGui::Checkbox("Spectator list", &config->misc.spectatorList.enabled);
	ImGui::SameLine();

	ImGui::PushID("Spectator list");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("No Title Bar", &config->misc.spectatorList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Keybinds list", &config->misc.keybindList.enabled);
	ImGui::SameLine();

	ImGui::PushID("Keybinds list");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("No Title Bar", &config->misc.keybindList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::PushID("Player list");
	ImGui::Checkbox("Player list", &config->misc.playerList.enabled);
	ImGui::SameLine();

	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Steam ID", &config->misc.playerList.steamID);
		ImGui::Checkbox("Rank", &config->misc.playerList.rank);
		ImGui::Checkbox("Wins", &config->misc.playerList.wins);
		ImGui::Checkbox("Health", &config->misc.playerList.health);
		ImGui::Checkbox("Armor", &config->misc.playerList.armor);
		ImGui::Checkbox("Money", &config->misc.playerList.money);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::PushID("Jump stats");
	ImGui::Checkbox("Jump stats", &config->misc.jumpStats.enabled);
	ImGui::SameLine();

	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Show fails", &config->misc.jumpStats.showFails);
		ImGui::Checkbox("Show color on fails", &config->misc.jumpStats.showColorOnFail);
		ImGui::Checkbox("Simplify naming", &config->misc.jumpStats.simplifyNaming);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Watermark", &config->misc.watermark.enabled);
	ImGuiCustom::colorPicker("Offscreen enemies", config->misc.offscreenEnemies, &config->misc.offscreenEnemies.enabled);
	ImGui::SameLine();
	ImGui::PushID("Offscreen enemies");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Health bar", &config->misc.offscreenEnemies.healthBar.enabled);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(95.0f);
		ImGui::Combo("Type", &config->misc.offscreenEnemies.healthBar.type, "Gradient\0Solid\0Health-based\0");
		if (config->misc.offscreenEnemies.healthBar.type == HealthBar::Solid) {
			ImGui::SameLine();
			ImGuiCustom::colorPicker("", static_cast<Color4&>(config->misc.offscreenEnemies.healthBar));
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox("Disable model occlusion", &config->misc.disableModelOcclusion);
	ImGui::SliderFloat("Aspect ratio", &config->misc.aspectratio, 0.0f, 5.0f, "%.2f");
	ImGui::NextColumn();
	ImGui::Checkbox("Disable HUD blur", &config->misc.disablePanoramablur);
	if (ImGui::Checkbox("Custom clantag", &config->misc.customClanTag))
		Misc::updateClanTag(true);
	ImGui::SameLine();
	ImGui::PushID("Clantag options");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.0f);

		if (config->misc.tagType < 4) {
			ImGui::PushID(0);
			if (ImGui::InputText("Tag", config->misc.clanTag, sizeof(config->misc.clanTag)))
				Misc::updateClanTag(true);
			ImGui::PopID();
		}

		if (ImGui::Combo("Animation", &config->misc.tagType, "Static\0Animated\0Auto reverse\0Reverse auto reverse\0Custom\0Clock\0"))
			Misc::updateClanTag(true);
		if (config->misc.tagType != 0 && config->misc.tagType != 5) // Don't ask for update rate if it is static or clock
			ImGui::SliderFloat("Update rate", &config->misc.tagUpdateInterval, 0.25f, 3.f, "%.2f");
		if (config->misc.tagType == 4) {
			for (size_t i = 0; i < config->misc.tagAnimationSteps.size(); i++) {
				ImGui::PushID(i + 100);
				ImGui::InputText("", &config->misc.tagAnimationSteps[i]);
				ImGui::PopID();
			}

			if (ImGui::Button("+", ImVec2(30.f, 30.f))) {
				config->misc.tagAnimationSteps.push_back("");
				Misc::updateClanTag(true);
			}

			ImGui::SameLine();
			if (ImGui::Button("-", ImVec2(30.f, 30.f)) && config->misc.tagAnimationSteps.size() > 2) {
				config->misc.tagAnimationSteps.pop_back();
				Misc::updateClanTag(true);
			}
		}
		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();
	ImGui::Checkbox("Kill message", &config->misc.killMessage);
	ImGui::SameLine();
	ImGui::PushID("Kill messages");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(150.0f);

		for (size_t i = 0; i < config->misc.killMessages.size(); i++) {
			ImGui::PushID(i + 200);
			ImGui::InputText("", &config->misc.killMessages[i]);
			ImGui::PopID();
		}

		if (ImGui::Button("+", ImVec2(30.f, 30.f)))
			config->misc.killMessages.push_back("");

		ImGui::SameLine();
		if (ImGui::Button("-", ImVec2(30.f, 30.f)) && config->misc.killMessages.size() > 1)
			config->misc.killMessages.pop_back();

		ImGui::PopItemWidth();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Custom name", &config->misc.customName);
	ImGui::SameLine();
	ImGui::PushItemWidth(150.0f);
	ImGui::PushID("Custom name change");

	if (ImGui::InputText("", config->misc.name, sizeof(config->misc.name)) && config->misc.customName)
		Misc::changeName(false, (std::string{ config->misc.name } + '\x1').c_str(), 0.0f);
	ImGui::PopID();
	ImGui::Checkbox("Name stealer", &config->misc.nameStealer);
	ImGui::Checkbox("Fast plant", &config->misc.fastPlant);
	ImGui::Checkbox("Fast stop", &config->misc.fastStop);
	ImGuiCustom::colorPicker("Bomb timer", config->misc.bombTimer);
	ImGuiCustom::colorPicker("Hurt indicator", config->misc.hurtIndicator);
	ImGuiCustom::colorPicker("Yaw indicator", config->misc.yawIndicator);
	ImGui::Checkbox("Prepare revolver", &config->misc.prepareRevolver);
	ImGui::SameLine();
	ImGui::PushID("Prepare revolver Key");
	ImGui::hotkey2("", config->misc.prepareRevolverKey);
	ImGui::PopID();
	ImGui::Combo("Hit sound", &config->misc.hitSound, "None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0");
	if (config->misc.hitSound == 5) {
		ImGui::InputText("Hit sound filename", &config->misc.customHitSound);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("audio file must be put in csgo/sound/ directory");
	}
	ImGui::PushID(2);
	ImGui::Combo("Kill sound", &config->misc.killSound, "None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0");
	if (config->misc.killSound == 5) {
		ImGui::InputText("Kill sound filename", &config->misc.customKillSound);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("audio file must be put in csgo/sound/ directory");
	}
	ImGui::PopID();
	ImGui::Checkbox("Grenade prediction", &config->misc.nadePredict);
	ImGui::SameLine();
	ImGui::PushID("Grenade prediction");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGuiCustom::colorPicker("Damage", config->misc.nadeDamagePredict);
		ImGuiCustom::colorPicker("Trail", config->misc.nadeTrailPredict);
		ImGuiCustom::colorPicker("Circle", config->misc.nadeCirclePredict);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Fix tablet signal", &config->misc.fixTabletSignal);
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("Max angle delta", &config->misc.maxAngleDelta, 0.0f, 255.0f, "%.2f");
	ImGui::Checkbox("Opposite hand knife", &config->misc.oppositeHandKnife);
	ImGui::Checkbox("Sv_pure bypass", &config->misc.svPureBypass);
	ImGui::Checkbox("Unlock inventory", &config->misc.inventoryUnlocker);
	ImGui::Checkbox("Unlock hidden convars", &config->misc.unhideConvars);
	ImGui::Checkbox("Preserve killfeed", &config->misc.preserveKillfeed.enabled);
	ImGui::SameLine();

	ImGui::PushID("Preserve killfeed");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Only headshots", &config->misc.preserveKillfeed.onlyHeadshots);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Killfeed changer", &config->misc.killfeedChanger.enabled);
	ImGui::SameLine();

	ImGui::PushID("Killfeed changer");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Checkbox("Headshot", &config->misc.killfeedChanger.headshot);
		ImGui::Checkbox("Dominated", &config->misc.killfeedChanger.dominated);
		ImGui::SameLine();
		ImGui::Checkbox("Revenge", &config->misc.killfeedChanger.revenge);
		ImGui::Checkbox("Penetrated", &config->misc.killfeedChanger.penetrated);
		ImGui::Checkbox("Noscope", &config->misc.killfeedChanger.noscope);
		ImGui::Checkbox("Thrusmoke", &config->misc.killfeedChanger.thrusmoke);
		ImGui::Checkbox("Attackerblind", &config->misc.killfeedChanger.attackerblind);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Purchase list", &config->misc.purchaseList.enabled);
	ImGui::SameLine();

	ImGui::PushID("Purchase list");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::SetNextItemWidth(75.0f);
		ImGui::Combo("Mode", &config->misc.purchaseList.mode, "Details\0Summary\0");
		ImGui::Checkbox("Only during freeze time", &config->misc.purchaseList.onlyDuringFreezeTime);
		ImGui::Checkbox("Show prices", &config->misc.purchaseList.showPrices);
		ImGui::Checkbox("No title bar", &config->misc.purchaseList.noTitleBar);
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Reportbot", &config->misc.reportbot.enabled);
	ImGui::SameLine();
	ImGui::PushID("Reportbot");

	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::PushItemWidth(80.0f);
		ImGui::Combo("Target", &config->misc.reportbot.target, "Enemies\0Allies\0All\0");
		ImGui::InputInt("Delay (s)", &config->misc.reportbot.delay);
		config->misc.reportbot.delay = (std::max)(config->misc.reportbot.delay, 1);
		ImGui::InputInt("Rounds", &config->misc.reportbot.rounds);
		config->misc.reportbot.rounds = (std::max)(config->misc.reportbot.rounds, 1);
		ImGui::PopItemWidth();
		ImGui::Checkbox("Abusive Communications", &config->misc.reportbot.textAbuse);
		ImGui::Checkbox("Griefing", &config->misc.reportbot.griefing);
		ImGui::Checkbox("Wall Hacking", &config->misc.reportbot.wallhack);
		ImGui::Checkbox("Aim Hacking", &config->misc.reportbot.aimbot);
		ImGui::Checkbox("Other Hacking", &config->misc.reportbot.other);
		if (ImGui::Button("Reset"))
			Misc::resetReportbot();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGui::Checkbox("Autobuy", &config->misc.autoBuy.enabled);
	ImGui::SameLine();

	ImGui::PushID("Autobuy");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGui::Combo("Primary weapon", &config->misc.autoBuy.primaryWeapon, "None\0MAC-10 | MP9\0MP7 | MP5-SD\0UMP-45\0P90\0PP-Bizon\0Galil AR | FAMAS\0AK-47 | M4A4 | M4A1-S\0SSG 08\0SG553 |AUG\0AWP\0G3SG1 | SCAR-20\0Nova\0XM1014\0Sawed-Off | MAG-7\0M249\0Negev\0");
		ImGui::Combo("Secondary weapon", &config->misc.autoBuy.secondaryWeapon, "None\0Glock-18 | P2000 | USP-S\0Dual Berettas\0P250\0CZ75-Auto | Five-SeveN | Tec-9\0Desert Eagle | R8 Revolver\0");
		ImGui::Combo("Armor", &config->misc.autoBuy.armor, "None\0Kevlar\0Kevlar + Helmet\0");
		ImGuiCustom::multiCombo("Utility", config->misc.autoBuy.utility, "Defuser\0Taser\0");
		ImGuiCustom::multiCombo("Grenades", config->misc.autoBuy.grenades, "HE Grenade\0Smoke Grenade\0Molotov\0Flashbang\0Decoy\0");
		ImGui::EndPopup();
	}
	ImGui::PopID();

	ImGuiCustom::colorPicker("Logger", config->misc.logger);
	ImGui::SameLine();

	ImGui::PushID("Logger");
	if (ImGui::Button("..."))
		ImGui::OpenPopup("");

	if (ImGui::BeginPopup("")) {
		ImGuiCustom::multiCombo("Log output", config->misc.loggerOptions.modes, "Console\0Event log\0");
		ImGuiCustom::multiCombo("Log events", config->misc.loggerOptions.events, "Damage dealt\0Damage received\0Hostage taken\0Bomb plants\0");
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (ImGui::Button("Unhook"))
		hooks->uninstall();
#ifdef DEBUG
	static bool metrics_show{};
	ImGui::Checkbox("Metrics", &metrics_show);
	if (metrics_show) ImGui::ShowMetricsWindow(&metrics_show);
#endif
	ImGui::Columns(1);
}

void GUI::renderConfigWindow() noexcept
{
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnOffset(1, 170.0f);

	static bool incrementalLoad = false;
	ImGui::Checkbox("Incremental load", &incrementalLoad);

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

	if (ImGui::ListBox("", &currentConfig, [](void* data, int idx, const char** out_text) {
		auto& vector = *static_cast<std::vector<std::string>*>(data);
		*out_text = vector[idx].c_str();
		return true;
		}, &configItems, configItems.size(), 5) && currentConfig != -1)
		buffer = configItems[currentConfig];

		ImGui::PushID(0);
		if (ImGui::InputTextWithHint("", "config name", &buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
			if (currentConfig != -1)
				config->rename(currentConfig, buffer.c_str());
		}
		ImGui::PopID();
		ImGui::NextColumn();

		ImGui::PushItemWidth(100.0f);

		if (ImGui::Button("Open config directory"))
			config->openConfigDir();

		if (ImGui::Button("Create config", { 100.0f, 25.0f }))
			config->add(buffer.c_str());

		if (ImGui::Button("Reset config", { 100.0f, 25.0f }))
			ImGui::OpenPopup("Config to reset");

		if (ImGui::BeginPopup("Config to reset")) {
			constexpr std::array names{ "Whole", "Legitbot", "Legit Anti Aim", "Ragebot", "Rage Anti aim", "Fake angle", "Fakelag", "Backtrack", "Triggerbot", "Glow", "Chams", "ESP", "Visuals", "Sound", "Misc" };
			for (size_t i = 0; i < names.size(); i++) {
				if (i == 1) ImGui::Separator();

				if (ImGui::Selectable(names[i])) {
					switch (i) {
					case 0: config->reset(); Misc::updateClanTag(true); break;
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
					case 13: Sound::resetConfig(); break;
					case 14: config->misc = { };  Misc::updateClanTag(true); break;
					}
				}
			}
			ImGui::EndPopup();
		}
		if (currentConfig != -1) {
			if (ImGui::Button("Load selected", { 100.0f, 25.0f }))
				ImGui::OpenPopup("##reallyLoad");
			if (ImGui::BeginPopup("##reallyLoad")) {
				ImGui::TextUnformatted("Are you sure?");
				if (ImGui::Button("No", { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button("Yes", { 45.0f, 0.0f })) {
					config->load(currentConfig, incrementalLoad);
					Misc::updateClanTag(true);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			if (ImGui::Button("Save selected", { 100.0f, 25.0f }))
				ImGui::OpenPopup("##reallySave");
			if (ImGui::BeginPopup("##reallySave")) {
				ImGui::TextUnformatted("Are you sure?");
				if (ImGui::Button("No", { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button("Yes", { 45.0f, 0.0f })) {
					config->save(currentConfig);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			if (ImGui::Button("Delete selected", { 100.0f, 25.0f }))
				ImGui::OpenPopup("##reallyDelete");
			if (ImGui::BeginPopup("##reallyDelete")) {
				ImGui::TextUnformatted("Are you sure?");
				if (ImGui::Button("No", { 45.0f, 0.0f }))
					ImGui::CloseCurrentPopup();
				ImGui::SameLine();
				if (ImGui::Button("Yes", { 45.0f, 0.0f })) {
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

	static auto Name = "Menu";
	static auto Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	static int activeTab = 1;
	static int activeSubTabLegitbot = 1;
	static int activeSubTabRagebot = 1;
	static int activeSubTabVisuals = 1;
	static int activeSubTabMisc = 1;
	static int activeSubTabConfigs = 1;

	if (ImGui::Begin(Name, NULL, Flags)) {
		Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);

		ImGui::BeginChild("##Back", ImVec2{ 704, 434 }, false);
		{
			ImGui::SetCursorPos(ImVec2{ 2, 2 });

			Style->Colors[ImGuiCol_ChildBg] = ImColor(19, 22, 27);

			ImGui::BeginChild("##Main", ImVec2{ 700, 430 }, false);
			{
				ImGui::BeginChild("##UP", ImVec2{ 700, 45 }, false);
				{
					ImGui::SetCursorPos(ImVec2{ 10, 6 });
					ImGui::PushFont(fonts.tahoma34); ImGui::Text("Osiris"); ImGui::PopFont();

					float pos = 305;
					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 1) Active(); else Hovered();
					if (ImGui::Button("Legitbot", ImVec2{ 75, 45 }))
						activeTab = 1;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 2) Active(); else Hovered();
					if (ImGui::Button("Ragebot", ImVec2{ 75, 45 }))
						activeTab = 2;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 3) Active(); else Hovered();
					if (ImGui::Button("Visuals", ImVec2{ 75, 45 }))
						activeTab = 3;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 4) Active(); else Hovered();
					if (ImGui::Button("Misc", ImVec2{ 75, 45 }))
						activeTab = 4;

					pos += 80;

					ImGui::SetCursorPos(ImVec2{ pos, 0 });
					if (activeTab == 5) Active(); else Hovered();
					if (ImGui::Button("Configs", ImVec2{ 75, 45 }))
						activeTab = 5;
				}
				ImGui::EndChild();

				ImGui::SetCursorPos(ImVec2{ 0, 45 });
				Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34);
				Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27);
				ImGui::BeginChild("##Childs", ImVec2{ 700, 365 }, false);
				{
					ImGui::SetCursorPos(ImVec2{ 15, 5 });
					Style->ChildRounding = 0;
					ImGui::BeginChild("##Left", ImVec2{ 155, 320 }, false);
					{
						switch (activeTab) {
						case 1: //Legitbot
							ImGui::SetCursorPosY(10);
							if (ImGui::Button("Main                    ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 1;
							if (ImGui::Button("Backtrack               ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 2;
							if (ImGui::Button("Triggerbot              ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 3;
							if (ImGui::Button("Anti aim                ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 4;
							break;
						case 2: //Ragebot
							ImGui::SetCursorPosY(10);
							if (ImGui::Button("Main                    ", ImVec2{ 80, 20 })) activeSubTabRagebot = 1;
							if (ImGui::Button("Backtrack               ", ImVec2{ 80, 20 })) activeSubTabRagebot = 2;
							if (ImGui::Button("Anti aim                ", ImVec2{ 80, 20 })) activeSubTabRagebot = 3;
							if (ImGui::Button("Desync                  ", ImVec2{ 80, 20 })) activeSubTabRagebot = 4;
							if (ImGui::Button("Fakelag                 ", ImVec2{ 80, 20 })) activeSubTabRagebot = 5;
							break;
						case 3: //Visuals
							ImGui::SetCursorPosY(10);
							if (ImGui::Button("Main                    ", ImVec2{ 80, 20 })) activeSubTabVisuals = 1;
							if (ImGui::Button("ESP                     ", ImVec2{ 80, 20 })) activeSubTabVisuals = 2;
							if (ImGui::Button("Chams                   ", ImVec2{ 80, 20 })) activeSubTabVisuals = 3;
							if (ImGui::Button("Glow                    ", ImVec2{ 80, 20 })) activeSubTabVisuals = 4;
							break;
						case 4: //Misc
							ImGui::SetCursorPosY(10);
							if (ImGui::Button("Main                    ", ImVec2{ 80, 20 })) activeSubTabMisc = 1;
							if (ImGui::Button("Sound                   ", ImVec2{ 80, 20 })) activeSubTabMisc = 2;
							break;
						default:
							break;
						}

						ImGui::EndChild();

						ImGui::SetCursorPos(ImVec2{ 100, 5 });
						Style->Colors[ImGuiCol_ChildBg] = ImColor(29, 34, 38);
						Style->ChildRounding = 5;
						ImGui::BeginChild("##SubMain", ImVec2{ 590, 350 }, false);
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
					ImGui::BeginChild("##Text", ImVec2{ 700, 20 }, false);
					{
						ImGui::SetCursorPos(ImVec2{ 2, 2 });
						ImGui::Text("local mom's spaghetti found on https://github.com/mkot2/OsirisAndExtra | Build Date: " __DATE__ " " __TIME__);
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
