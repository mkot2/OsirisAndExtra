#include <fstream>
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>

#include "nlohmann/json.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Config.h"
#include "Helpers.h"

#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Glow.h"
#include "Hacks/Sound.h"

#include "SDK/Platform.h"

int CALLBACK fontCallback(const LOGFONTW* lpelfe, const TEXTMETRICW*, DWORD, LPARAM lParam)
{
	const wchar_t* const fontName = reinterpret_cast<const ENUMLOGFONTEXW*>(lpelfe)->elfFullName;

	if (fontName[0] == L'@')
		return TRUE;

	if (HFONT font = CreateFontW(0, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, fontName)) {

		DWORD fontData = GDI_ERROR;

		if (HDC hdc = CreateCompatibleDC(nullptr)) {
			SelectObject(hdc, font);
			// Do not use TTC fonts as we only support TTF fonts
			fontData = GetFontData(hdc, 'fctt', 0, NULL, 0);
			DeleteDC(hdc);
		}
		DeleteObject(font);

		if (fontData == GDI_ERROR) {
			if (char buff[1024]; WideCharToMultiByte(CP_UTF8, 0, fontName, -1, buff, sizeof(buff), nullptr, nullptr))
				reinterpret_cast<std::vector<std::string>*>(lParam)->emplace_back(buff);
		}
	}
	return TRUE;
}

Config::Config() noexcept
{
	if (PWSTR pathToDocuments; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pathToDocuments))) {
		path = pathToDocuments;
		CoTaskMemFree(pathToDocuments);
	}

	path /= xorstr_("Osiris");
	listConfigs();
	misc.clanTag[0] = '\0';
	misc.name[0] = '\0';
	visuals.playerModel[0] = '\0';

	load(xorstr_(u8"default.json"), false);

	LOGFONTW logfont;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfPitchAndFamily = DEFAULT_PITCH;
	logfont.lfFaceName[0] = L'\0';

	EnumFontFamiliesExW(GetDC(nullptr), &logfont, fontCallback, (LPARAM)&systemFonts, 0);

	std::sort(std::next(systemFonts.begin()), systemFonts.end());
}
#pragma region  Read

static void from_json(const json& j, ColorToggle& ct)
{
	from_json(j, static_cast<Color4&>(ct));
	read(j, xorstr_("Enabled"), ct.enabled);
}

static void from_json(const json& j, Color3& c)
{
	read(j, xorstr_("Color"), c.color);
	read(j, xorstr_("Rainbow"), c.rainbow);
	read(j, xorstr_("Rainbow Speed"), c.rainbowSpeed);
}

static void from_json(const json& j, ColorToggle3& ct)
{
	from_json(j, static_cast<Color3&>(ct));
	read(j, xorstr_("Enabled"), ct.enabled);
}

static void from_json(const json& j, ColorToggleRounding& ctr)
{
	from_json(j, static_cast<ColorToggle&>(ctr));

	read(j, xorstr_("Rounding"), ctr.rounding);
}

static void from_json(const json& j, ColorToggleOutline& cto)
{
	from_json(j, static_cast<ColorToggle&>(cto));

	read(j, xorstr_("Outline"), cto.outline);
}

static void from_json(const json& j, ColorToggleThickness& ctt)
{
	from_json(j, static_cast<ColorToggle&>(ctt));

	read(j, xorstr_("Thickness"), ctt.thickness);
}

static void from_json(const json& j, ColorToggleThicknessRounding& cttr)
{
	from_json(j, static_cast<ColorToggleRounding&>(cttr));

	read(j, xorstr_("Thickness"), cttr.thickness);
}

static void from_json(const json& j, Font& f)
{
	read<value_t::string>(j, xorstr_("Name"), f.name);

	if (!f.name.empty())
		config->scheduleFontLoad(f.name);

	if (const auto it = std::find_if(config->getSystemFonts().begin(), config->getSystemFonts().end(), [&f](const auto& e) { return e == f.name; }); it != config->getSystemFonts().end())
		f.index = std::distance(config->getSystemFonts().begin(), it);
	else
		f.index = 0;
}

static void from_json(const json& j, Snapline& s)
{
	from_json(j, static_cast<ColorToggleThickness&>(s));

	read(j, xorstr_("Type"), s.type);
}

static void from_json(const json& j, Box& b)
{
	from_json(j, static_cast<ColorToggleRounding&>(b));

	read(j, xorstr_("Type"), b.type);
	read(j, xorstr_("Scale"), b.scale);
	read<value_t::object>(j, xorstr_("Fill"), b.fill);
}

static void from_json(const json& j, Shared& s)
{
	read(j, xorstr_("Enabled"), s.enabled);
	read<value_t::object>(j, xorstr_("Font"), s.font);
	read<value_t::object>(j, xorstr_("Snapline"), s.snapline);
	read<value_t::object>(j, xorstr_("Box"), s.box);
	read<value_t::object>(j, xorstr_("Name"), s.name);
	read(j, xorstr_("Text Cull Distance"), s.textCullDistance);
}

static void from_json(const json& j, Weapon& w)
{
	from_json(j, static_cast<Shared&>(w));

	read<value_t::object>(j, xorstr_("Ammo"), w.ammo);
}

static void from_json(const json& j, Trail& t)
{
	from_json(j, static_cast<ColorToggleThickness&>(t));

	read(j, xorstr_("Type"), t.type);
	read(j, xorstr_("Time"), t.time);
}

static void from_json(const json& j, Trails& t)
{
	read(j, xorstr_("Enabled"), t.enabled);
	read<value_t::object>(j, xorstr_("Local Player"), t.localPlayer);
	read<value_t::object>(j, xorstr_("Allies"), t.allies);
	read<value_t::object>(j, xorstr_("Enemies"), t.enemies);
}

static void from_json(const json& j, Projectile& p)
{
	from_json(j, static_cast<Shared&>(p));

	read<value_t::object>(j, xorstr_("Trails"), p.trails);
}

static void from_json(const json& j, HealthBar& o)
{
	from_json(j, static_cast<ColorToggle&>(o));
	read(j, xorstr_("Type"), o.type);
}

static void from_json(const json& j, Player& p)
{
	from_json(j, static_cast<Shared&>(p));

	read<value_t::object>(j, xorstr_("Weapon"), p.weapon);
	read<value_t::object>(j, xorstr_("Flash Duration"), p.flashDuration);
	read(j, xorstr_("Audible Only"), p.audibleOnly);
	read(j, xorstr_("Spotted Only"), p.spottedOnly);
	read<value_t::object>(j, xorstr_("Health Bar"), p.healthBar);
	read<value_t::object>(j, xorstr_("Skeleton"), p.skeleton);
	read<value_t::object>(j, xorstr_("Head Box"), p.headBox);
	read<value_t::object>(j, xorstr_("Line of sight"), p.lineOfSight);
}

static void from_json(const json& j, OffscreenEnemies& o)
{
	from_json(j, static_cast<ColorToggle&>(o));

	read<value_t::object>(j, xorstr_("Health Bar"), o.healthBar);
}

static void from_json(const json& j, BulletTracers& o)
{
	from_json(j, static_cast<ColorToggle&>(o));
}

static void from_json(const json& j, ImVec2& v)
{
	read(j, xorstr_("X"), v.x);
	read(j, xorstr_("Y"), v.y);
}

static void from_json(const json& j, Config::Legitbot& a)
{
	read(j, xorstr_("Enabled"), a.enabled);
	read(j, xorstr_("Aimlock"), a.aimlock);
	read(j, xorstr_("Silent"), a.silent);
	read(j, xorstr_("Friendly fire"), a.friendlyFire);
	read(j, xorstr_("Visible only"), a.visibleOnly);
	read(j, xorstr_("Scoped only"), a.scopedOnly);
	read(j, xorstr_("Ignore flash"), a.ignoreFlash);
	read(j, xorstr_("Ignore smoke"), a.ignoreSmoke);
	read(j, xorstr_("Auto scope"), a.autoScope);
	read(j, xorstr_("Fov"), a.fov);
	read(j, xorstr_("Smooth"), a.smooth);
	read(j, xorstr_("Reaction time"), a.reactionTime);
	read(j, xorstr_("Hitboxes"), a.hitboxes);
	read(j, xorstr_("Min damage"), a.minDamage);
	read(j, xorstr_("Killshot"), a.killshot);
	read(j, xorstr_("Between shots"), a.betweenShots);
}

static void from_json(const json& j, Config::RecoilControlSystem& r)
{
	read(j, xorstr_("Enabled"), r.enabled);
	read(j, xorstr_("Silent"), r.silent);
	read(j, xorstr_("Ignore Shots"), r.shotsFired);
	read(j, xorstr_("Horizontal"), r.horizontal);
	read(j, xorstr_("Vertical"), r.vertical);
}

static void from_json(const json& j, Config::Ragebot& r)
{
	read(j, xorstr_("Enabled"), r.enabled);
	read(j, xorstr_("Resolver"), r.resolver);
	read(j, xorstr_("Aimlock"), r.aimlock);
	read(j, xorstr_("Silent"), r.silent);
	read(j, xorstr_("Friendly fire"), r.friendlyFire);
	read(j, xorstr_("Visible only"), r.visibleOnly);
	read(j, xorstr_("Scoped only"), r.scopedOnly);
	read(j, xorstr_("Ignore flash"), r.ignoreFlash);
	read(j, xorstr_("Ignore smoke"), r.ignoreSmoke);
	read(j, xorstr_("Auto shot"), r.autoShot);
	read(j, xorstr_("Auto scope"), r.autoScope);
	read(j, xorstr_("Auto stop"), r.autoStop);
	read(j, xorstr_("Between shots"), r.betweenShots);
	read(j, xorstr_("Full stop"), r.fullStop);
	read(j, xorstr_("Duck stop"), r.duckStop);
	read(j, xorstr_("Priority"), r.priority);
	read(j, xorstr_("Fov"), r.fov);
	read(j, xorstr_("Hitboxes"), r.hitboxes);
	read(j, xorstr_("Hitchance"), r.hitChance);
	read(j, xorstr_("Accuracy boost"), r.accuracyBoost);
	read(j, xorstr_("Head Multipoint"), r.headMultiPoint);
	read(j, xorstr_("Body Multipoint"), r.bodyMultiPoint);
	read(j, xorstr_("Min damage"), r.minDamage);
	read(j, xorstr_("Min damage override"), r.minDamageOverride);
}

static void from_json(const json& j, Config::Optimizations& o)
{
	read(j, xorstr_("Low Performance Mode"), o.lowPerformanceMode);
}

static void from_json(const json& j, Config::Triggerbot& t)
{
	read(j, xorstr_("Enabled"), t.enabled);
	read(j, xorstr_("Friendly fire"), t.friendlyFire);
	read(j, xorstr_("Scoped only"), t.scopedOnly);
	read(j, xorstr_("Ignore flash"), t.ignoreFlash);
	read(j, xorstr_("Ignore smoke"), t.ignoreSmoke);
	read(j, xorstr_("Hitboxes"), t.hitboxes);
	read(j, xorstr_("Hitchance"), t.hitChance);
	read(j, xorstr_("Shot delay"), t.shotDelay);
	read(j, xorstr_("Min damage"), t.minDamage);
	read(j, xorstr_("Killshot"), t.killshot);
}

static void from_json(const json& j, Config::LegitAntiAimConfig& a)
{
	read(j, xorstr_("Enabled"), a.enabled);
	read(j, xorstr_("Extend"), a.extend);
	read(j, xorstr_("Invert key"), a.invert);
}

static void from_json(const json& j, Config::RageAntiAimConfig& a)
{
	read(j, xorstr_("Enabled"), a.enabled);
	read(j, xorstr_("Pitch"), a.pitch);
	read(j, xorstr_("Yaw base"), reinterpret_cast<int&>(a.yawBase));
	read(j, xorstr_("Yaw modifier"), a.yawModifier);
	read(j, xorstr_("Paranoia min"), a.paranoiaMin);
	read(j, xorstr_("Paranoia max"), a.paranoiaMax);
	read(j, xorstr_("Yaw add"), a.yawAdd);
	read(j, xorstr_("Jitter Range"), a.jitterRange);
	read(j, xorstr_("Spin base"), a.spinBase);
	read(j, xorstr_("At targets"), a.atTargets);
	read(j, xorstr_("Roll"), a.roll);
	read(j, xorstr_("Roll add"), a.rollAdd);
	read(j, xorstr_("Roll offset"), a.rollOffset);
	read(j, xorstr_("Roll pitch"), a.rollPitch);
	read(j, xorstr_("Exploit pitch switch"), a.exploitPitchSwitch);
	read(j, xorstr_("Exploit pitch"), a.exploitPitch);
	read(j, xorstr_("Roll alt"), a.rollAlt);
}

static void from_json(const json& j, Config::FakeAngle& a)
{
	read(j, xorstr_("Enabled"), a.enabled);
	read(j, xorstr_("Left limit"), a.leftLimit);
	read(j, xorstr_("Right limit"), a.rightLimit);
	read(j, xorstr_("Peek mode"), a.peekMode);
	read(j, xorstr_("Lby mode"), a.lbyMode);
}

static void from_json(const json& j, Config::Fakelag& f)
{
	read(j, xorstr_("Enabled"), f.enabled);
	read(j, xorstr_("Mode"), f.mode);
	read(j, xorstr_("Limit"), f.limit);
	read(j, xorstr_("Random min limit"), f.randomMinLimit);
}

static void from_json(const json& j, Config::Tickbase& t)
{
	read(j, xorstr_("Doubletap"), t.doubletap);
	read(j, xorstr_("Hideshots"), t.hideshots);
	read(j, xorstr_("Teleport"), t.teleport);
	read(j, xorstr_("OnshotFl"), t.onshotFl);
	read(j, xorstr_("OnshotFl amount"), t.onshotFlAmount);
	read(j, xorstr_("Onshot desync"), t.onshotDesync);
}

static void from_json(const json& j, Config::Backtrack& b)
{
	read(j, xorstr_("Enabled"), b.enabled);
	read(j, xorstr_("Ignore smoke"), b.ignoreSmoke);
	read(j, xorstr_("Ignore flash"), b.ignoreFlash);
	read(j, xorstr_("Time limit"), b.timeLimit);
	read(j, xorstr_("Fake Latency"), b.fakeLatency);
	read(j, xorstr_("Fake Latency Amount"), b.fakeLatencyAmount);
}

static void from_json(const json& j, Config::Chams::Material& m)
{
	from_json(j, static_cast<Color4&>(m));

	read(j, xorstr_("Enabled"), m.enabled);
	read(j, xorstr_("Health based"), m.healthBased);
	read(j, xorstr_("Blinking"), m.blinking);
	read(j, xorstr_("Wireframe"), m.wireframe);
	read(j, xorstr_("Cover"), m.cover);
	read(j, xorstr_("Ignore-Z"), m.ignorez);
	read(j, xorstr_("Material"), m.material);
}

static void from_json(const json& j, Config::Chams& c)
{
	read_array_opt(j, xorstr_("Materials"), c.materials);
}

static void from_json(const json& j, Config::GlowItem& g)
{
	from_json(j, static_cast<Color4&>(g));

	read(j, xorstr_("Enabled"), g.enabled);
	read(j, xorstr_("Health based"), g.healthBased);
	read(j, xorstr_("Audible only"), g.audibleOnly);
	read(j, xorstr_("Style"), g.style);
}

static void from_json(const json& j, Config::PlayerGlow& g)
{
	read<value_t::object>(j, xorstr_("All"), g.all);
	read<value_t::object>(j, xorstr_("Visible"), g.visible);
	read<value_t::object>(j, xorstr_("Occluded"), g.occluded);
}

static void from_json(const json& j, Config::StreamProofESP& e)
{
	read(j, xorstr_("Key"), e.key);
	read(j, xorstr_("Allies"), e.allies);
	read(j, xorstr_("Enemies"), e.enemies);
	read(j, xorstr_("Weapons"), e.weapons);
	read(j, xorstr_("Projectiles"), e.projectiles);
	read(j, xorstr_("Loot Crates"), e.lootCrates);
	read(j, xorstr_("Other Entities"), e.otherEntities);
}

static void from_json(const json& j, Config::Visuals::FootstepESP& ft)
{
	read<value_t::object>(j, xorstr_("Enabled"), ft.footstepBeams);
	read(j, xorstr_("Thickness"), ft.footstepBeamThickness);
	read(j, xorstr_("Radius"), ft.footstepBeamRadius);
}

static void from_json(const json& j, Config::Visuals& v)
{
	read(j, xorstr_("Disable post-processing"), v.disablePostProcessing);
	read(j, xorstr_("Inverse ragdoll gravity"), v.inverseRagdollGravity);
	read(j, xorstr_("No fog"), v.noFog);
	read<value_t::object>(j, xorstr_("Fog controller"), v.fog);
	read<value_t::object>(j, xorstr_("Fog options"), v.fogOptions);
	read(j, xorstr_("No 3d sky"), v.no3dSky);
	read(j, xorstr_("No aim punch"), v.noAimPunch);
	read(j, xorstr_("No view punch"), v.noViewPunch);
	read(j, xorstr_("No view bob"), v.noViewBob);
	read(j, xorstr_("No hands"), v.noHands);
	read(j, xorstr_("No sleeves"), v.noSleeves);
	read(j, xorstr_("No weapons"), v.noWeapons);
	read(j, xorstr_("No smoke"), v.noSmoke);
	read(j, xorstr_("Wireframe smoke"), v.wireframeSmoke);
	read(j, xorstr_("No molotov"), v.noMolotov);
	read(j, xorstr_("Wireframe molotov"), v.wireframeMolotov);
	read(j, xorstr_("No blur"), v.noBlur);
	read(j, xorstr_("No scope overlay"), v.noScopeOverlay);
	read(j, xorstr_("No grass"), v.noGrass);
	read(j, xorstr_("No shadows"), v.noShadows);
	read<value_t::object>(j, xorstr_("Motion Blur"), v.motionBlur);
	read<value_t::object>(j, xorstr_("Shadows changer"), v.shadowsChanger);
	read(j, xorstr_("Full bright"), v.fullBright);
	read(j, xorstr_("Zoom"), v.zoom);
	read(j, xorstr_("Zoom key"), v.zoomKey);
	read(j, xorstr_("Thirdperson"), v.thirdperson);
	read(j, xorstr_("Thirdperson key"), v.thirdpersonKey);
	read(j, xorstr_("Thirdperson distance"), v.thirdpersonDistance);
	read(j, xorstr_("Freecam"), v.freeCam);
	read(j, xorstr_("Freecam key"), v.freeCamKey);
	read(j, xorstr_("Freecam speed"), v.freeCamSpeed);
	read(j, xorstr_("Keep FOV"), v.keepFov);
	read(j, xorstr_("FOV"), v.fov);
	read(j, xorstr_("Far Z"), v.farZ);
	read(j, xorstr_("Flash reduction"), v.flashReduction);
	read(j, xorstr_("Glow thickness"), v.glowOutlineWidth);
	read(j, xorstr_("Skybox"), v.skybox);
	read<value_t::object>(j, xorstr_("World"), v.world);
	read<value_t::object>(j, xorstr_("Props"), v.props);
	read<value_t::object>(j, xorstr_("Sky"), v.sky);
	read<value_t::string>(j, xorstr_("Custom skybox"), v.customSkybox);
	read(j, xorstr_("Deagle spinner"), v.deagleSpinner);
	read<value_t::object>(j, xorstr_("Footstep ESP"), v.footsteps);
	read(j, xorstr_("Screen effect"), v.screenEffect);
	read(j, xorstr_("Hit effect"), v.hitEffect);
	read(j, xorstr_("Hit effect time"), v.hitEffectTime);
	read(j, xorstr_("Hit marker"), v.hitMarker);
	read(j, xorstr_("Hit marker time"), v.hitMarkerTime);
	read(j, xorstr_("Playermodel T"), v.playerModelT);
	read(j, xorstr_("Playermodel CT"), v.playerModelCT);
	read(j, xorstr_("Custom Playermodel"), v.playerModel, sizeof(v.playerModel));
	read(j, xorstr_("Disable jiggle bones"), v.disableJiggleBones);
	read<value_t::object>(j, xorstr_("Bullet Tracers"), v.bulletTracers);
	read<value_t::object>(j, xorstr_("Bullet Impacts"), v.bulletImpacts);
	read<value_t::object>(j, xorstr_("Hitbox on Hit"), v.onHitHitbox);
	read(j, xorstr_("Bullet Impacts time"), v.bulletImpactsTime);
	read<value_t::object>(j, xorstr_("Molotov Hull"), v.molotovHull);
	read<value_t::object>(j, xorstr_("Smoke Hull"), v.smokeHull);
	read<value_t::object>(j, xorstr_("Molotov Polygon"), v.molotovPolygon);
	read<value_t::object>(j, xorstr_("Viewmodel"), v.viewModel);
	read<value_t::object>(j, xorstr_("Spread circle"), v.spreadCircle);
	read(j, xorstr_("Asus walls"), v.asusWalls);
	read(j, xorstr_("Asus props"), v.asusProps);
	read(j, xorstr_("Smoke timer"), v.smokeTimer);
	read<value_t::object>(j, xorstr_("Smoke timer BG"), v.smokeTimerBG);
	read<value_t::object>(j, xorstr_("Smoke timer TIMER"), v.smokeTimerTimer);
	read<value_t::object>(j, xorstr_("Smoke timer TEXT"), v.smokeTimerText);
	read(j, xorstr_("Molotov timer"), v.molotovTimer);
	read<value_t::object>(j, xorstr_("Molotov timer BG"), v.molotovTimerBG);
	read<value_t::object>(j, xorstr_("Molotov timer TIMER"), v.molotovTimerTimer);
	read<value_t::object>(j, xorstr_("Molotov timer TEXT"), v.molotovTimerText);
	read<value_t::object>(j, xorstr_("Footstep"), v.footsteps);
}

static void from_json(const json& j, sticker_setting& s)
{
	read(j, xorstr_("Kit"), s.kit);
	read(j, xorstr_("Wear"), s.wear);
	read(j, xorstr_("Scale"), s.scale);
	read(j, xorstr_("Rotation"), s.rotation);

	s.onLoad();
}

static void from_json(const json& j, item_setting& i)
{
	read(j, xorstr_("Enabled"), i.enabled);
	read(j, xorstr_("Definition index"), i.itemId);
	read(j, xorstr_("Quality"), i.quality);
	read(j, xorstr_("Paint Kit"), i.paintKit);
	read(j, xorstr_("Definition override"), i.definition_override_index);
	read(j, xorstr_("Seed"), i.seed);
	read(j, xorstr_("StatTrak"), i.stat_trak);
	read(j, xorstr_("Wear"), i.wear);
	read(j, xorstr_("Custom name"), i.custom_name, sizeof(i.custom_name));
	read(j, xorstr_("Stickers"), i.stickers);

	i.onLoad();
}

static void from_json(const json& j, PurchaseList& pl)
{
	read(j, xorstr_("Enabled"), pl.enabled);
	read(j, xorstr_("Only During Freeze Time"), pl.onlyDuringFreezeTime);
	read(j, xorstr_("Show Prices"), pl.showPrices);
	read(j, xorstr_("No Title Bar"), pl.noTitleBar);
	read(j, xorstr_("Mode"), pl.mode);
	read<value_t::object>(j, xorstr_("Pos"), pl.pos);
}

static void from_json(const json& j, Config::Misc::SpectatorList& sl)
{
	read(j, xorstr_("Enabled"), sl.enabled);
	read(j, xorstr_("No Title Bar"), sl.noTitleBar);
	read<value_t::object>(j, xorstr_("Pos"), sl.pos);
}

static void from_json(const json& j, Config::Misc::KeyBindList& sl)
{
	read(j, xorstr_("Enabled"), sl.enabled);
	read(j, xorstr_("No Title Bar"), sl.noTitleBar);
	read<value_t::object>(j, xorstr_("Pos"), sl.pos);
}

static void from_json(const json& j, Config::Misc::PlayerList& o)
{
	read(j, xorstr_("Enabled"), o.enabled);
	read(j, xorstr_("Steam ID"), o.steamID);
	read(j, xorstr_("Rank"), o.rank);
	read(j, xorstr_("Wins"), o.wins);
	read(j, xorstr_("Money"), o.money);
	read(j, xorstr_("Health"), o.health);
	read(j, xorstr_("Armor"), o.armor);
	read<value_t::object>(j, xorstr_("Pos"), o.pos);
}

static void from_json(const json& j, Config::Misc::JumpStats& js)
{
	read(j, xorstr_("Enabled"), js.enabled);
	read(j, xorstr_("Show fails"), js.showFails);
	read(j, xorstr_("Show color on fail"), js.showColorOnFail);
	read(j, xorstr_("Simplify naming"), js.simplifyNaming);
}

static void from_json(const json& j, Config::Misc::Velocity& v)
{
	read(j, xorstr_("Enabled"), v.enabled);
	read(j, xorstr_("Position"), v.position);
	read(j, xorstr_("Alpha"), v.alpha);
	read<value_t::object>(j, xorstr_("Color"), v.color);
}

static void from_json(const json& j, Config::Misc::KeyBoardDisplay& kbd)
{
	read(j, xorstr_("Enabled"), kbd.enabled);
	read(j, xorstr_("Position"), kbd.position);
	read(j, xorstr_("Show key Tiles"), kbd.showKeyTiles);
	read<value_t::object>(j, xorstr_("Color"), kbd.color);
}

static void from_json(const json& j, Config::Misc::Watermark& o)
{
	read(j, xorstr_("Enabled"), o.enabled);
	read<value_t::object>(j, xorstr_("Pos"), o.pos);
}

static void from_json(const json& j, PreserveKillfeed& o)
{
	read(j, xorstr_("Enabled"), o.enabled);
	read(j, xorstr_("Only Headshots"), o.onlyHeadshots);
}

static void from_json(const json& j, KillfeedChanger& o)
{
	read(j, xorstr_("Enabled"), o.enabled);
	read(j, xorstr_("Headshot"), o.headshot);
	read(j, xorstr_("Dominated"), o.dominated);
	read(j, xorstr_("Revenge"), o.revenge);
	read(j, xorstr_("Penetrated"), o.penetrated);
	read(j, xorstr_("Noscope"), o.noscope);
	read(j, xorstr_("Thrusmoke"), o.thrusmoke);
	read(j, xorstr_("Attackerblind"), o.attackerblind);
}

static void from_json(const json& j, AutoBuy& o)
{
	read(j, xorstr_("Enabled"), o.enabled);
	read(j, xorstr_("Primary weapon"), o.primaryWeapon);
	read(j, xorstr_("Secondary weapon"), o.secondaryWeapon);
	read(j, xorstr_("Armor"), o.armor);
	read(j, xorstr_("Utility"), o.utility);
	read(j, xorstr_("Grenades"), o.grenades);
}

static void from_json(const json& j, Config::Misc::Logger& o)
{
	read(j, xorstr_("Modes"), o.modes);
	read(j, xorstr_("Events"), o.events);
}

static void from_json(const json& j, Config::Visuals::MotionBlur& mb)
{
	read(j, xorstr_("Enabled"), mb.enabled);
	read(j, xorstr_("Forward"), mb.forwardEnabled);
	read(j, xorstr_("Falling min"), mb.fallingMin);
	read(j, xorstr_("Falling max"), mb.fallingMax);
	read(j, xorstr_("Falling intensity"), mb.fallingIntensity);
	read(j, xorstr_("Rotation intensity"), mb.rotationIntensity);
	read(j, xorstr_("Strength"), mb.strength);
}

static void from_json(const json& j, Config::Visuals::Fog& f)
{
	read(j, xorstr_("Start"), f.start);
	read(j, xorstr_("End"), f.end);
	read(j, xorstr_("Density"), f.density);
}

static void from_json(const json& j, Config::Visuals::ShadowsChanger& sw)
{
	read(j, xorstr_("Enabled"), sw.enabled);
	read(j, xorstr_("X"), sw.x);
	read(j, xorstr_("Y"), sw.y);
}

static void from_json(const json& j, Config::Visuals::Viewmodel& vxyz)
{
	read(j, xorstr_("Enabled"), vxyz.enabled);
	read(j, xorstr_("Fov"), vxyz.fov);
	read(j, xorstr_("X"), vxyz.x);
	read(j, xorstr_("Y"), vxyz.y);
	read(j, xorstr_("Z"), vxyz.z);
	read(j, xorstr_("Roll"), vxyz.roll);
}

static void from_json(const json& j, Config::Visuals::MolotovPolygon& mp)
{
	read(j, xorstr_("Enabled"), mp.enabled);
	read<value_t::object>(j, xorstr_("Self"), mp.self);
	read<value_t::object>(j, xorstr_("Team"), mp.team);
	read<value_t::object>(j, xorstr_("Enemy"), mp.enemy);
}

static void from_json(const json& j, Config::Visuals::OnHitHitbox& h)
{
	read<value_t::object>(j, xorstr_("Color"), h.color);
	read(j, xorstr_("Duration"), h.duration);
}

static void from_json(const json& j, Config::Misc& m)
{
	read(j, xorstr_("Menu key"), m.menuKey);
	read(j, xorstr_("Anti AFK kick"), m.antiAfkKick);
	read(j, xorstr_("Adblock"), m.adBlock);
	read(j, xorstr_("Force relay"), m.forceRelayCluster);
	read(j, xorstr_("Auto strafe"), m.autoStrafe);
	read(j, xorstr_("Bunny hop"), m.bunnyHop);
	read(j, xorstr_("Custom clan tag"), m.customClanTag);
	read(j, xorstr_("Clan tag"), m.clanTag, sizeof(m.clanTag));
	read(j, xorstr_("Clan tag type"), m.tagType);
	read(j, xorstr_("Clan tag update interval"), m.tagUpdateInterval);
	read(j, xorstr_("Clan tag animation steps"), m.tagAnimationSteps);
	read(j, xorstr_("Fast duck"), m.fastDuck);
	read(j, xorstr_("Moonwalk"), m.moonwalk);
	read(j, xorstr_("Leg break"), m.leg_break);
	read(j, xorstr_("Knifebot"), m.knifeBot);
	read(j, xorstr_("Knifebot mode"), m.knifeBotMode);
	read(j, xorstr_("Block bot"), m.blockBot);
	read(j, xorstr_("Block bot Key"), m.blockBotKey);
	read(j, xorstr_("Edge Jump"), m.edgeJump);
	read(j, xorstr_("Edge Jump Key"), m.edgeJumpKey);
	read(j, xorstr_("Mini Jump"), m.miniJump);
	read(j, xorstr_("Mini Jump Crouch lock"), m.miniJumpCrouchLock);
	read(j, xorstr_("Mini Jump Key"), m.miniJumpKey);
	read(j, xorstr_("Jump Bug"), m.jumpBug);
	read(j, xorstr_("Jump Bug Key"), m.jumpBugKey);
	read(j, xorstr_("Edge Bug"), m.edgeBug);
	read(j, xorstr_("Edge Bug Key"), m.edgeBugKey);
	read(j, xorstr_("Pred Amnt"), m.edgeBugPredAmnt);
	read(j, xorstr_("Auto pixel surf"), m.autoPixelSurf);
	read(j, xorstr_("Auto pixel surf Pred Amnt"), m.autoPixelSurfPredAmnt);
	read(j, xorstr_("Auto pixel surf Key"), m.autoPixelSurfKey);
	read<value_t::object>(j, xorstr_("Velocity"), m.velocity);
	read<value_t::object>(j, xorstr_("Keyboard display"), m.keyBoardDisplay);
	read(j, xorstr_("Slowwalk"), m.slowwalk);
	read(j, xorstr_("Slowwalk key"), m.slowwalkKey);
	read(j, xorstr_("Slowwalk Amnt"), m.slowwalkAmnt);
	read(j, xorstr_("Fake duck"), m.fakeduck);
	read(j, xorstr_("Fake duck key"), m.fakeduckKey);
	read<value_t::object>(j, xorstr_("Auto peek"), m.autoPeek);
	read(j, xorstr_("Auto peek key"), m.autoPeekKey);
	read(j, xorstr_("Noscope crosshair"), m.noscopeCrosshair);
	read(j, xorstr_("Recoil crosshair"), m.recoilCrosshair);
	read(j, xorstr_("Auto pistol"), m.autoPistol);
	read(j, xorstr_("Auto reload"), m.autoReload);
	read(j, xorstr_("Auto accept"), m.autoAccept);
	read(j, xorstr_("Radar hack"), m.radarHack);
	read(j, xorstr_("Reveal ranks"), m.revealRanks);
	read(j, xorstr_("Reveal money"), m.revealMoney);
	read(j, xorstr_("Reveal suspect"), m.revealSuspect);
	read(j, xorstr_("Reveal votes"), m.revealVotes);
	read<value_t::object>(j, xorstr_("Spectator list"), m.spectatorList);
	read<value_t::object>(j, xorstr_("Keybind list"), m.keybindList);
	read<value_t::object>(j, xorstr_("Player list"), m.playerList);
	read<value_t::object>(j, xorstr_("Jump stats"), m.jumpStats);
	read<value_t::object>(j, xorstr_("Watermark"), m.watermark);
	read<value_t::object>(j, xorstr_("Offscreen Enemies"), m.offscreenEnemies);
	read(j, xorstr_("Disable model occlusion"), m.disableModelOcclusion);
	read(j, xorstr_("Aspect Ratio"), m.aspectratio);
	read(j, xorstr_("Kill message"), m.killMessage);
	read(j, xorstr_("Kill message string"), m.killMessages);
	read(j, xorstr_("Name stealer"), m.nameStealer);
	read(j, xorstr_("Disable HUD blur"), m.disablePanoramablur);
	read(j, xorstr_("Fast plant"), m.fastPlant);
	read(j, xorstr_("Fast Stop"), m.fastStop);
	read<value_t::object>(j, xorstr_("Bomb timer"), m.bombTimer);
	read<value_t::object>(j, xorstr_("Hurt indicator"), m.hurtIndicator);
	read<value_t::object>(j, xorstr_("Yaw indicator"), m.yawIndicator);
	read(j, xorstr_("Prepare revolver"), m.prepareRevolver);
	read(j, xorstr_("Prepare revolver key"), m.prepareRevolverKey);
	read(j, xorstr_("Hit sound"), m.hitSound);
	read(j, xorstr_("Quick healthshot key"), m.quickHealthshotKey);
	read(j, xorstr_("Grenade predict"), m.nadePredict);
	read<value_t::object>(j, xorstr_("Grenade predict Damage"), m.nadeDamagePredict);
	read<value_t::object>(j, xorstr_("Grenade predict Trail"), m.nadeTrailPredict);
	read<value_t::object>(j, xorstr_("Grenade predict Circle"), m.nadeCirclePredict);
	read(j, xorstr_("Max angle delta"), m.maxAngleDelta);
	read(j, xorstr_("Fix tablet signal"), m.fixTabletSignal);
	read<value_t::string>(j, xorstr_("Custom Hit Sound"), m.customHitSound);
	read(j, xorstr_("Kill sound"), m.killSound);
	read<value_t::string>(j, xorstr_("Custom Kill Sound"), m.customKillSound);
	read<value_t::object>(j, xorstr_("Purchase List"), m.purchaseList);
	read<value_t::object>(j, xorstr_("Reportbot"), m.reportbot);
	read(j, xorstr_("Opposite Hand Knife"), m.oppositeHandKnife);
	read<value_t::object>(j, xorstr_("Preserve Killfeed"), m.preserveKillfeed);
	read<value_t::object>(j, xorstr_("Killfeed changer"), m.killfeedChanger);
	read(j, xorstr_("Sv pure bypass"), m.svPureBypass);
	read(j, xorstr_("Inventory Unlocker"), m.inventoryUnlocker);
	read(j, xorstr_("Unlock hidden cvars"), m.unhideConvars);
	read<value_t::object>(j, xorstr_("Autobuy"), m.autoBuy);
	read<value_t::object>(j, xorstr_("Logger"), m.logger);
	read<value_t::object>(j, xorstr_("Logger options"), m.loggerOptions);
	read(j, xorstr_("Name"), m.name, sizeof(m.name));
	read(j, xorstr_("Custom name"), m.customName);
}

static void from_json(const json& j, Config::Misc::Reportbot& r)
{
	read(j, xorstr_("Enabled"), r.enabled);
	read(j, xorstr_("Target"), r.target);
	read(j, xorstr_("Delay"), r.delay);
	read(j, xorstr_("Rounds"), r.rounds);
	read(j, xorstr_("Abusive Communications"), r.textAbuse);
	read(j, xorstr_("Griefing"), r.griefing);
	read(j, xorstr_("Wall Hacking"), r.wallhack);
	read(j, xorstr_("Aim Hacking"), r.aimbot);
	read(j, xorstr_("Other Hacking"), r.other);
}

void Config::load(size_t id, bool incremental) noexcept
{
	load((const char8_t*)configs[id].c_str(), incremental);
}

void Config::load(const char8_t* name, bool incremental) noexcept
{
	json j;

	if (std::ifstream in{ path / name }; in.good()) {
		j = json::parse(in, nullptr, false);
		if (j.is_discarded())
			return;
	} else {
		return;
	}

	if (!incremental)
		reset();

	read(j, xorstr_("Legitbot"), legitbot);
	read(j, xorstr_("Legitbot Key"), legitbotKey);
	read<value_t::object>(j, xorstr_("Draw legitbot fov"), legitbotFov);

	read<value_t::object>(j, xorstr_("RCS"), recoilControlSystem);
	read<value_t::object>(j, xorstr_("Optimizations"), optimizations);

	read(j, xorstr_("Ragebot"), ragebot);
	read(j, xorstr_("Ragebot Key"), ragebotKey);
	read(j, xorstr_("Min damage override Key"), minDamageOverrideKey);

	read(j, xorstr_("Triggerbot"), triggerbot);
	read(j, xorstr_("Triggerbot Key"), triggerbotKey);

	read<value_t::object>(j, xorstr_("Legit Anti aim"), legitAntiAim);
	read<value_t::array>(j, xorstr_("Rage Anti aim"), rageAntiAim);
	read(j, xorstr_("Manual forward Key"), manualForward);
	read(j, xorstr_("Manual backward Key"), manualBackward);
	read(j, xorstr_("Manual right Key"), manualRight);
	read(j, xorstr_("Manual left Key"), manualLeft);
	read(j, xorstr_("Auto direction Key"), autoDirection);
	read(j, xorstr_("Disable in freezetime"), disableInFreezetime);
	read<value_t::array>(j, xorstr_("Fake angle"), fakeAngle);
	read(j, xorstr_("Invert"), invert);
	read<value_t::array>(j, xorstr_("Fakelag"), fakelag);
	read<value_t::object>(j, xorstr_("Tickbase"), tickbase);
	read<value_t::object>(j, xorstr_("Backtrack"), backtrack);

	read(j[xorstr_("Glow")], xorstr_("Items"), glow);
	read(j[xorstr_("Glow")], xorstr_("Players"), playerGlow);
	read(j[xorstr_("Glow")], xorstr_("Key"), glowKey);

	read(j, xorstr_("Chams"), chams);
	read(j[xorstr_("Chams")], xorstr_("Key"), chamsKey);
	read<value_t::object>(j, xorstr_("ESP"), streamProofESP);
	read<value_t::object>(j, xorstr_("Visuals"), visuals);
	read(j, xorstr_("Skin changer"), skinChanger);
	Sound::fromJson(j[xorstr_("Sound")]);
	read<value_t::object>(j, xorstr_("Misc"), misc);
}

#pragma endregion

#pragma region  Write

static void to_json(json& j, const ColorToggle& o, const ColorToggle& dummy = {})
{
	to_json(j, static_cast<const Color4&>(o), dummy);
	WRITE(xorstr_("Enabled"), enabled);
}

static void to_json(json& j, const Color3& o, const Color3& dummy = {})
{
	WRITE(xorstr_("Color"), color);
	WRITE(xorstr_("Rainbow"), rainbow);
	WRITE(xorstr_("Rainbow Speed"), rainbowSpeed);
}

static void to_json(json& j, const ColorToggle3& o, const ColorToggle3& dummy = {})
{
	to_json(j, static_cast<const Color3&>(o), dummy);
	WRITE(xorstr_("Enabled"), enabled);
}

static void to_json(json& j, const ColorToggleRounding& o, const ColorToggleRounding& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);
	WRITE(xorstr_("Rounding"), rounding);
}

static void to_json(json& j, const ColorToggleThickness& o, const ColorToggleThickness& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);
	WRITE(xorstr_("Thickness"), thickness);
}

static void to_json(json& j, const ColorToggleOutline& o, const ColorToggleOutline& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);
	WRITE(xorstr_("Outline"), outline);
}

static void to_json(json& j, const ColorToggleThicknessRounding& o, const ColorToggleThicknessRounding& dummy = {})
{
	to_json(j, static_cast<const ColorToggleRounding&>(o), dummy);
	WRITE(xorstr_("Thickness"), thickness);
}

static void to_json(json& j, const Font& o, const Font& dummy = {})
{
	WRITE(xorstr_("Name"), name);
}

static void to_json(json& j, const Snapline& o, const Snapline& dummy = {})
{
	to_json(j, static_cast<const ColorToggleThickness&>(o), dummy);
	WRITE(xorstr_("Type"), type);
}

static void to_json(json& j, const Box& o, const Box& dummy = {})
{
	to_json(j, static_cast<const ColorToggleRounding&>(o), dummy);
	WRITE(xorstr_("Type"), type);
	WRITE(xorstr_("Scale"), scale);
	WRITE(xorstr_("Fill"), fill);
}

static void to_json(json& j, const Shared& o, const Shared& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Font"), font);
	WRITE(xorstr_("Snapline"), snapline);
	WRITE(xorstr_("Box"), box);
	WRITE(xorstr_("Name"), name);
	WRITE(xorstr_("Text Cull Distance"), textCullDistance);
}

static void to_json(json& j, const HealthBar& o, const HealthBar& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);
	WRITE(xorstr_("Type"), type);
}

static void to_json(json& j, const Player& o, const Player& dummy = {})
{
	to_json(j, static_cast<const Shared&>(o), dummy);
	WRITE(xorstr_("Weapon"), weapon);
	WRITE(xorstr_("Flash Duration"), flashDuration);
	WRITE(xorstr_("Audible Only"), audibleOnly);
	WRITE(xorstr_("Spotted Only"), spottedOnly);
	WRITE(xorstr_("Health Bar"), healthBar);
	WRITE(xorstr_("Skeleton"), skeleton);
	WRITE(xorstr_("Head Box"), headBox);
	WRITE(xorstr_("Line of sight"), lineOfSight);
}

static void to_json(json& j, const Weapon& o, const Weapon& dummy = {})
{
	to_json(j, static_cast<const Shared&>(o), dummy);
	WRITE(xorstr_("Ammo"), ammo);
}

static void to_json(json& j, const Trail& o, const Trail& dummy = {})
{
	to_json(j, static_cast<const ColorToggleThickness&>(o), dummy);
	WRITE(xorstr_("Type"), type);
	WRITE(xorstr_("Time"), time);
}

static void to_json(json& j, const Trails& o, const Trails& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Local Player"), localPlayer);
	WRITE(xorstr_("Allies"), allies);
	WRITE(xorstr_("Enemies"), enemies);
}

static void to_json(json& j, const OffscreenEnemies& o, const OffscreenEnemies& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);

	WRITE(xorstr_("Health Bar"), healthBar);
}

static void to_json(json& j, const BulletTracers& o, const BulletTracers& dummy = {})
{
	to_json(j, static_cast<const ColorToggle&>(o), dummy);
}

static void to_json(json& j, const Projectile& o, const Projectile& dummy = {})
{
	j = static_cast<const Shared&>(o);

	WRITE(xorstr_("Trails"), trails);
}

static void to_json(json& j, const ImVec2& o, const ImVec2& dummy = {})
{
	WRITE(xorstr_("X"), x);
	WRITE(xorstr_("Y"), y);
}

static void to_json(json& j, const Config::Legitbot& o, const Config::Legitbot& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Aimlock"), aimlock);
	WRITE(xorstr_("Silent"), silent);
	WRITE(xorstr_("Friendly fire"), friendlyFire);
	WRITE(xorstr_("Visible only"), visibleOnly);
	WRITE(xorstr_("Scoped only"), scopedOnly);
	WRITE(xorstr_("Ignore flash"), ignoreFlash);
	WRITE(xorstr_("Ignore smoke"), ignoreSmoke);
	WRITE(xorstr_("Auto scope"), autoScope);
	WRITE(xorstr_("Hitboxes"), hitboxes);
	WRITE(xorstr_("Fov"), fov);
	WRITE(xorstr_("Smooth"), smooth);
	WRITE(xorstr_("Reaction time"), reactionTime);
	WRITE(xorstr_("Min damage"), minDamage);
	WRITE(xorstr_("Killshot"), killshot);
	WRITE(xorstr_("Between shots"), betweenShots);
}

static void to_json(json& j, const Config::RecoilControlSystem& o, const Config::RecoilControlSystem& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Silent"), silent);
	WRITE(xorstr_("Ignore Shots"), shotsFired);
	WRITE(xorstr_("Horizontal"), horizontal);
	WRITE(xorstr_("Vertical"), vertical);
}

static void to_json(json& j, const Config::Ragebot& o, const Config::Ragebot& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Aimlock"), aimlock);
	WRITE(xorstr_("Silent"), silent);
	WRITE(xorstr_("Friendly fire"), friendlyFire);
	WRITE(xorstr_("Visible only"), visibleOnly);
	WRITE(xorstr_("Scoped only"), scopedOnly);
	WRITE(xorstr_("Ignore flash"), ignoreFlash);
	WRITE(xorstr_("Ignore smoke"), ignoreSmoke);
	WRITE(xorstr_("Auto shot"), autoShot);
	WRITE(xorstr_("Auto scope"), autoScope);
	WRITE(xorstr_("Auto stop"), autoStop);
	WRITE(xorstr_("Resolver"), resolver);
	WRITE(xorstr_("Between shots"), betweenShots);
	WRITE(xorstr_("Full stop"), fullStop);
	WRITE(xorstr_("Duck stop"), duckStop);
	WRITE(xorstr_("Priority"), priority);
	WRITE(xorstr_("Fov"), fov);
	WRITE(xorstr_("Hitboxes"), hitboxes);
	WRITE(xorstr_("Hitchance"), hitChance);
	WRITE(xorstr_("Accuracy boost"), accuracyBoost);
	WRITE(xorstr_("Head Multipoint"), headMultiPoint);
	WRITE(xorstr_("Body Multipoint"), bodyMultiPoint);
	WRITE(xorstr_("Min damage"), minDamage);
	WRITE(xorstr_("Min damage override"), minDamageOverride);
}

static void to_json(json& j, const Config::Optimizations& o, const Config::Optimizations& dummy = {})
{
	WRITE(xorstr_("Low Performance Mode"), lowPerformanceMode);
}

static void to_json(json& j, const Config::Triggerbot& o, const Config::Triggerbot& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Friendly fire"), friendlyFire);
	WRITE(xorstr_("Scoped only"), scopedOnly);
	WRITE(xorstr_("Ignore flash"), ignoreFlash);
	WRITE(xorstr_("Ignore smoke"), ignoreSmoke);
	WRITE(xorstr_("Hitboxes"), hitboxes);
	WRITE(xorstr_("Hitchance"), hitChance);
	WRITE(xorstr_("Shot delay"), shotDelay);
	WRITE(xorstr_("Min damage"), minDamage);
	WRITE(xorstr_("Killshot"), killshot);
}

static void to_json(json& j, const Config::Chams::Material& o)
{
	const Config::Chams::Material dummy;

	to_json(j, static_cast<const Color4&>(o), dummy);
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Health based"), healthBased);
	WRITE(xorstr_("Blinking"), blinking);
	WRITE(xorstr_("Wireframe"), wireframe);
	WRITE(xorstr_("Cover"), cover);
	WRITE(xorstr_("Ignore-Z"), ignorez);
	WRITE(xorstr_("Material"), material);
}

static void to_json(json& j, const Config::Chams& o)
{
	j[xorstr_("Materials")] = o.materials;
}

static void to_json(json& j, const Config::GlowItem& o, const  Config::GlowItem& dummy = {})
{
	to_json(j, static_cast<const Color4&>(o), dummy);
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Health based"), healthBased);
	WRITE(xorstr_("Audible only"), audibleOnly);
	WRITE(xorstr_("Style"), style);
}

static void to_json(json& j, const  Config::PlayerGlow& o, const  Config::PlayerGlow& dummy = {})
{
	WRITE(xorstr_("All"), all);
	WRITE(xorstr_("Visible"), visible);
	WRITE(xorstr_("Occluded"), occluded);
}

static void to_json(json& j, const Config::StreamProofESP& o, const Config::StreamProofESP& dummy = {})
{
	WRITE(xorstr_("Key"), key);
	j[xorstr_("Allies")] = o.allies;
	j[xorstr_("Enemies")] = o.enemies;
	j[xorstr_("Weapons")] = o.weapons;
	j[xorstr_("Projectiles")] = o.projectiles;
	j[xorstr_("Loot Crates")] = o.lootCrates;
	j[xorstr_("Other Entities")] = o.otherEntities;
}

static void to_json(json& j, const Config::Misc::Reportbot& o, const Config::Misc::Reportbot& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Target"), target);
	WRITE(xorstr_("Delay"), delay);
	WRITE(xorstr_("Rounds"), rounds);
	WRITE(xorstr_("Abusive Communications"), textAbuse);
	WRITE(xorstr_("Griefing"), griefing);
	WRITE(xorstr_("Wall Hacking"), wallhack);
	WRITE(xorstr_("Aim Hacking"), aimbot);
	WRITE(xorstr_("Other Hacking"), other);
}

static void to_json(json& j, const Config::LegitAntiAimConfig& o, const Config::LegitAntiAimConfig& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Extend"), extend);
	WRITE(xorstr_("Invert key"), invert);
}

static void to_json(json& j, const Config::RageAntiAimConfig& o, const Config::RageAntiAimConfig& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Pitch"), pitch);
	WRITE_ENUM(xorstr_("Yaw base"), yawBase);
	WRITE(xorstr_("Yaw modifier"), yawModifier);
	WRITE(xorstr_("Paranoia min"), paranoiaMin);
	WRITE(xorstr_("Paranoia max"), paranoiaMax);
	WRITE(xorstr_("Yaw add"), yawAdd);
	WRITE(xorstr_("Jitter Range"), jitterRange);
	WRITE(xorstr_("Spin base"), spinBase);
	WRITE(xorstr_("At targets"), atTargets);
	WRITE(xorstr_("Roll"), roll);
	WRITE(xorstr_("Roll add"), rollAdd);
	WRITE(xorstr_("Roll offset"), rollOffset);
	WRITE(xorstr_("Roll pitch"), rollPitch);
	WRITE(xorstr_("Exploit pitch switch"), exploitPitchSwitch);
	WRITE(xorstr_("Exploit pitch"), exploitPitch);
	WRITE(xorstr_("Roll alt"), rollAlt);
}

static void to_json(json& j, const Config::FakeAngle& o, const Config::FakeAngle& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Left limit"), leftLimit);
	WRITE(xorstr_("Right limit"), rightLimit);
	WRITE(xorstr_("Peek mode"), peekMode);
	WRITE(xorstr_("Lby mode"), lbyMode);
}

static void to_json(json& j, const Config::Fakelag& o, const Config::Fakelag& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Mode"), mode);
	WRITE(xorstr_("Limit"), limit);
	WRITE(xorstr_("Random min limit"), randomMinLimit);
}

static void to_json(json& j, const Config::Tickbase& o, const Config::Tickbase& dummy = {})
{
	WRITE(xorstr_("Doubletap"), doubletap);
	WRITE(xorstr_("Hideshots"), hideshots);
	WRITE(xorstr_("Teleport"), teleport);
	WRITE(xorstr_("OnshotFl"), onshotFl);
	WRITE(xorstr_("OnshotFl amount"), onshotFlAmount);
	WRITE(xorstr_("Onshot desync"), onshotDesync);
}

static void to_json(json& j, const Config::Backtrack& o, const Config::Backtrack& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Ignore smoke"), ignoreSmoke);
	WRITE(xorstr_("Ignore flash"), ignoreFlash);
	WRITE(xorstr_("Time limit"), timeLimit);
	WRITE(xorstr_("Fake Latency"), fakeLatency);
	WRITE(xorstr_("Fake Latency Amount"), fakeLatencyAmount);
}

static void to_json(json& j, const PurchaseList& o, const PurchaseList& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Only During Freeze Time"), onlyDuringFreezeTime);
	WRITE(xorstr_("Show Prices"), showPrices);
	WRITE(xorstr_("No Title Bar"), noTitleBar);
	WRITE(xorstr_("Mode"), mode);

	if (const auto window = ImGui::FindWindowByName(xorstr_("Purchases"))) {
		j[xorstr_("Pos")] = window->Pos;
	}
}

static void to_json(json& j, const Config::Misc::SpectatorList& o, const Config::Misc::SpectatorList& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("No Title Bar"), noTitleBar);

	if (const auto window = ImGui::FindWindowByName(xorstr_("Spectator list"))) {
		j[xorstr_("Pos")] = window->Pos;
	}
}

static void to_json(json& j, const Config::Misc::KeyBindList& o, const Config::Misc::KeyBindList& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("No Title Bar"), noTitleBar);

	if (const auto window = ImGui::FindWindowByName(xorstr_("Keybind list"))) {
		j[xorstr_("Pos")] = window->Pos;
	}
}

static void to_json(json& j, const Config::Misc::PlayerList& o, const Config::Misc::PlayerList& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Steam ID"), steamID);
	WRITE(xorstr_("Rank"), rank);
	WRITE(xorstr_("Wins"), wins);
	WRITE(xorstr_("Money"), money);
	WRITE(xorstr_("Health"), health);
	WRITE(xorstr_("Armor"), armor);

	if (const auto window = ImGui::FindWindowByName(xorstr_("Player List"))) {
		j[xorstr_("Pos")] = window->Pos;
	}
}

static void to_json(json& j, const Config::Misc::JumpStats& o, const Config::Misc::JumpStats& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Show fails"), showFails);
	WRITE(xorstr_("Show color on fail"), showColorOnFail);
	WRITE(xorstr_("Simplify naming"), simplifyNaming);
}

static void to_json(json& j, const Config::Misc::Velocity& o, const Config::Misc::Velocity& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Position"), position);
	WRITE(xorstr_("Alpha"), alpha);
	WRITE(xorstr_("Color"), color);
}

static void to_json(json& j, const Config::Misc::KeyBoardDisplay& o, const Config::Misc::KeyBoardDisplay& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Position"), position);
	WRITE(xorstr_("Show key Tiles"), showKeyTiles);
	WRITE(xorstr_("Color"), color);
}

static void to_json(json& j, const Config::Misc::Watermark& o, const Config::Misc::Watermark& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);

	if (const auto window = ImGui::FindWindowByName(xorstr_("Watermark"))) {
		j[xorstr_("Pos")] = window->Pos;
	}
}

static void to_json(json& j, const PreserveKillfeed& o, const PreserveKillfeed& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Only Headshots"), onlyHeadshots);
}

static void to_json(json& j, const KillfeedChanger& o, const KillfeedChanger& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Headshot"), headshot);
	WRITE(xorstr_("Dominated"), dominated);
	WRITE(xorstr_("Revenge"), revenge);
	WRITE(xorstr_("Penetrated"), penetrated);
	WRITE(xorstr_("Noscope"), noscope);
	WRITE(xorstr_("Thrusmoke"), thrusmoke);
	WRITE(xorstr_("Attackerblind"), attackerblind);
}

static void to_json(json& j, const AutoBuy& o, const AutoBuy& dummy = {})
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Primary weapon"), primaryWeapon);
	WRITE(xorstr_("Secondary weapon"), secondaryWeapon);
	WRITE(xorstr_("Armor"), armor);
	WRITE(xorstr_("Utility"), utility);
	WRITE(xorstr_("Grenades"), grenades);
}

static void to_json(json& j, const Config::Misc::Logger& o, const Config::Misc::Logger& dummy = {})
{
	WRITE(xorstr_("Modes"), modes);
	WRITE(xorstr_("Events"), events);
}

static void to_json(json& j, const Config::Visuals::FootstepESP& o, const Config::Visuals::FootstepESP& dummy)
{
	WRITE(xorstr_("Enabled"), footstepBeams);
	WRITE(xorstr_("Thickness"), footstepBeamThickness);
	WRITE(xorstr_("Radius"), footstepBeamRadius);
}

static void to_json(json& j, const Config::Visuals::MotionBlur& o, const Config::Visuals::MotionBlur& dummy)
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Forward"), forwardEnabled);
	WRITE(xorstr_("Falling min"), fallingMin);
	WRITE(xorstr_("Falling max"), fallingMax);
	WRITE(xorstr_("Falling intensity"), fallingIntensity);
	WRITE(xorstr_("Rotation intensity"), rotationIntensity);
	WRITE(xorstr_("Strength"), strength);
}

static void to_json(json& j, const Config::Visuals::Fog& o, const Config::Visuals::Fog& dummy)
{
	WRITE(xorstr_("Start"), start);
	WRITE(xorstr_("End"), end);
	WRITE(xorstr_("Density"), density);
}

static void to_json(json& j, const Config::Visuals::ShadowsChanger& o, const Config::Visuals::ShadowsChanger& dummy)
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("X"), x);
	WRITE(xorstr_("Y"), y);
}

static void to_json(json& j, const Config::Visuals::Viewmodel& o, const Config::Visuals::Viewmodel& dummy)
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Fov"), fov);
	WRITE(xorstr_("X"), x);
	WRITE(xorstr_("Y"), y);
	WRITE(xorstr_("Z"), z);
	WRITE(xorstr_("Roll"), roll);
}

static void to_json(json& j, const Config::Visuals::MolotovPolygon& o, const Config::Visuals::MolotovPolygon& dummy)
{
	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Self"), self);
	WRITE(xorstr_("Team"), team);
	WRITE(xorstr_("Enemy"), enemy);
}

static void to_json(json& j, const Config::Visuals::OnHitHitbox& o, const Config::Visuals::OnHitHitbox& dummy)
{
	WRITE(xorstr_("Color"), color);
	WRITE(xorstr_("Duration"), duration);
}

static void to_json(json& j, const Config::Misc& o)
{
	const Config::Misc dummy;

	WRITE(xorstr_("Menu key"), menuKey);
	WRITE(xorstr_("Anti AFK kick"), antiAfkKick);
	WRITE(xorstr_("Adblock"), adBlock);
	WRITE(xorstr_("Force relay"), forceRelayCluster);
	WRITE(xorstr_("Auto strafe"), autoStrafe);
	WRITE(xorstr_("Bunny hop"), bunnyHop);
	WRITE(xorstr_("Custom clan tag"), customClanTag);

	if (o.clanTag[0])
		j[xorstr_("Clan tag")] = o.clanTag;


	WRITE(xorstr_("Clan tag type"), tagType);
	WRITE(xorstr_("Clan tag update interval"), tagUpdateInterval);
	WRITE(xorstr_("Clan tag animation steps"), tagAnimationSteps);
	WRITE(xorstr_("Fast duck"), fastDuck);
	WRITE(xorstr_("Moonwalk"), moonwalk);
	WRITE(xorstr_("Leg break"), leg_break);
	WRITE(xorstr_("Knifebot"), knifeBot);
	WRITE(xorstr_("Knifebot mode"), knifeBotMode);
	WRITE(xorstr_("Block bot"), blockBot);
	WRITE(xorstr_("Block bot Key"), blockBotKey);
	WRITE(xorstr_("Edge Jump"), edgeJump);
	WRITE(xorstr_("Edge Jump Key"), edgeJumpKey);
	WRITE(xorstr_("Mini Jump"), miniJump);
	WRITE(xorstr_("Mini Jump Crouch lock"), miniJumpCrouchLock);
	WRITE(xorstr_("Mini Jump Key"), miniJumpKey);
	WRITE(xorstr_("Jump Bug"), jumpBug);
	WRITE(xorstr_("Jump Bug Key"), jumpBugKey);
	WRITE(xorstr_("Edge Bug"), edgeBug);
	WRITE(xorstr_("Edge Bug Key"), edgeBugKey);
	WRITE(xorstr_("Pred Amnt"), edgeBugPredAmnt);
	WRITE(xorstr_("Auto pixel surf"), autoPixelSurf);
	WRITE(xorstr_("Auto pixel surf Pred Amnt"), autoPixelSurfPredAmnt);
	WRITE(xorstr_("Auto pixel surf Key"), autoPixelSurfKey);
	WRITE(xorstr_("Velocity"), velocity);
	WRITE(xorstr_("Keyboard display"), keyBoardDisplay);
	WRITE(xorstr_("Slowwalk"), slowwalk);
	WRITE(xorstr_("Slowwalk key"), slowwalkKey);
	WRITE(xorstr_("Slowwalk Amnt"), slowwalkAmnt);
	WRITE(xorstr_("Fake duck"), fakeduck);
	WRITE(xorstr_("Fake duck key"), fakeduckKey);
	WRITE(xorstr_("Auto peek"), autoPeek);
	WRITE(xorstr_("Auto peek key"), autoPeekKey);
	WRITE(xorstr_("Noscope crosshair"), noscopeCrosshair);
	WRITE(xorstr_("Recoil crosshair"), recoilCrosshair);
	WRITE(xorstr_("Auto pistol"), autoPistol);
	WRITE(xorstr_("Auto reload"), autoReload);
	WRITE(xorstr_("Auto accept"), autoAccept);
	WRITE(xorstr_("Radar hack"), radarHack);
	WRITE(xorstr_("Reveal ranks"), revealRanks);
	WRITE(xorstr_("Reveal money"), revealMoney);
	WRITE(xorstr_("Reveal suspect"), revealSuspect);
	WRITE(xorstr_("Reveal votes"), revealVotes);
	WRITE(xorstr_("Spectator list"), spectatorList);
	WRITE(xorstr_("Keybind list"), keybindList);
	WRITE(xorstr_("Player list"), playerList);
	WRITE(xorstr_("Jump stats"), jumpStats);
	WRITE(xorstr_("Watermark"), watermark);
	WRITE(xorstr_("Offscreen Enemies"), offscreenEnemies);
	WRITE(xorstr_("Disable model occlusion"), disableModelOcclusion);
	WRITE(xorstr_("Aspect Ratio"), aspectratio);
	WRITE(xorstr_("Kill message"), killMessage);
	WRITE(xorstr_("Kill message string"), killMessages);
	WRITE(xorstr_("Name stealer"), nameStealer);
	WRITE(xorstr_("Disable HUD blur"), disablePanoramablur);
	WRITE(xorstr_("Fast plant"), fastPlant);
	WRITE(xorstr_("Fast Stop"), fastStop);
	WRITE(xorstr_("Bomb timer"), bombTimer);
	WRITE(xorstr_("Hurt indicator"), hurtIndicator);
	WRITE(xorstr_("Yaw indicator"), yawIndicator);
	WRITE(xorstr_("Prepare revolver"), prepareRevolver);
	WRITE(xorstr_("Prepare revolver key"), prepareRevolverKey);
	WRITE(xorstr_("Hit sound"), hitSound);
	WRITE(xorstr_("Quick healthshot key"), quickHealthshotKey);
	WRITE(xorstr_("Grenade predict"), nadePredict);
	WRITE(xorstr_("Grenade predict Damage"), nadeDamagePredict);
	WRITE(xorstr_("Grenade predict Trail"), nadeTrailPredict);
	WRITE(xorstr_("Grenade predict Circle"), nadeCirclePredict);
	WRITE(xorstr_("Max angle delta"), maxAngleDelta);
	WRITE(xorstr_("Fix tablet signal"), fixTabletSignal);
	WRITE(xorstr_("Custom Hit Sound"), customHitSound);
	WRITE(xorstr_("Kill sound"), killSound);
	WRITE(xorstr_("Custom Kill Sound"), customKillSound);
	WRITE(xorstr_("Purchase List"), purchaseList);
	WRITE(xorstr_("Reportbot"), reportbot);
	WRITE(xorstr_("Opposite Hand Knife"), oppositeHandKnife);
	WRITE(xorstr_("Preserve Killfeed"), preserveKillfeed);
	WRITE(xorstr_("Killfeed changer"), killfeedChanger);
	WRITE(xorstr_("Sv pure bypass"), svPureBypass);
	WRITE(xorstr_("Inventory Unlocker"), inventoryUnlocker);
	WRITE(xorstr_("Unlock hidden cvars"), unhideConvars);
	WRITE(xorstr_("Autobuy"), autoBuy);
	WRITE(xorstr_("Logger"), logger);
	WRITE(xorstr_("Logger options"), loggerOptions);
	WRITE(xorstr_("Custom clantag"), customClanTag);
	WRITE(xorstr_("Custom name"), customName);

	if (o.clanTag[0])
		j[xorstr_("Name")] = o.name;
}

static void to_json(json& j, const Config::Visuals& o)
{
	const Config::Visuals dummy;

	WRITE(xorstr_("Disable post-processing"), disablePostProcessing);
	WRITE(xorstr_("Inverse ragdoll gravity"), inverseRagdollGravity);
	WRITE(xorstr_("No fog"), noFog);
	WRITE(xorstr_("Fog controller"), fog);
	WRITE(xorstr_("Fog options"), fogOptions);
	WRITE(xorstr_("No 3d sky"), no3dSky);
	WRITE(xorstr_("No aim punch"), noAimPunch);
	WRITE(xorstr_("No view punch"), noViewPunch);
	WRITE(xorstr_("No view bob"), noViewBob);
	WRITE(xorstr_("No hands"), noHands);
	WRITE(xorstr_("No sleeves"), noSleeves);
	WRITE(xorstr_("No weapons"), noWeapons);
	WRITE(xorstr_("No smoke"), noSmoke);
	WRITE(xorstr_("Wireframe smoke"), wireframeSmoke);
	WRITE(xorstr_("No molotov"), noMolotov);
	WRITE(xorstr_("Wireframe molotov"), wireframeMolotov);
	WRITE(xorstr_("No blur"), noBlur);
	WRITE(xorstr_("No scope overlay"), noScopeOverlay);
	WRITE(xorstr_("No grass"), noGrass);
	WRITE(xorstr_("No shadows"), noShadows);
	WRITE(xorstr_("Shadows changer"), shadowsChanger);
	WRITE(xorstr_("Motion Blur"), motionBlur);
	WRITE(xorstr_("Full bright"), fullBright);
	WRITE(xorstr_("Zoom"), zoom);
	WRITE(xorstr_("Zoom key"), zoomKey);
	WRITE(xorstr_("Thirdperson"), thirdperson);
	WRITE(xorstr_("Thirdperson key"), thirdpersonKey);
	WRITE(xorstr_("Thirdperson distance"), thirdpersonDistance);
	WRITE(xorstr_("Freecam"), freeCam);
	WRITE(xorstr_("Freecam key"), freeCamKey);
	WRITE(xorstr_("Freecam speed"), freeCamSpeed);
	WRITE(xorstr_("Keep FOV"), keepFov);
	WRITE(xorstr_("FOV"), fov);
	WRITE(xorstr_("Far Z"), farZ);
	WRITE(xorstr_("Flash reduction"), flashReduction);
	WRITE(xorstr_("Glow thickness"), glowOutlineWidth);
	WRITE(xorstr_("Skybox"), skybox);
	WRITE(xorstr_("World"), world);
	WRITE(xorstr_("Props"), props);
	WRITE(xorstr_("Sky"), sky);
	WRITE(xorstr_("Custom skybox"), customSkybox);
	WRITE(xorstr_("Deagle spinner"), deagleSpinner);
	WRITE(xorstr_("Footstep ESP"), footsteps);
	WRITE(xorstr_("Screen effect"), screenEffect);
	WRITE(xorstr_("Hit effect"), hitEffect);
	WRITE(xorstr_("Hit effect time"), hitEffectTime);
	WRITE(xorstr_("Hit marker"), hitMarker);
	WRITE(xorstr_("Hit marker time"), hitMarkerTime);
	WRITE(xorstr_("Playermodel T"), playerModelT);
	WRITE(xorstr_("Playermodel CT"), playerModelCT);
	if (o.playerModel[0])
		j[xorstr_("Custom Playermodel")] = o.playerModel;
	WRITE(xorstr_("Disable jiggle bones"), disableJiggleBones);
	WRITE(xorstr_("Bullet Tracers"), bulletTracers);
	WRITE(xorstr_("Bullet Impacts"), bulletImpacts);
	WRITE(xorstr_("Hitbox on Hit"), onHitHitbox);
	WRITE(xorstr_("Bullet Impacts time"), bulletImpactsTime);
	WRITE(xorstr_("Molotov Hull"), molotovHull);
	WRITE(xorstr_("Smoke Hull"), smokeHull);
	WRITE(xorstr_("Molotov Polygon"), molotovPolygon);
	WRITE(xorstr_("Viewmodel"), viewModel);
	WRITE(xorstr_("Spread circle"), spreadCircle);
	WRITE(xorstr_("Asus walls"), asusWalls);
	WRITE(xorstr_("Asus props"), asusProps);
	WRITE(xorstr_("Smoke timer"), smokeTimer);
	WRITE(xorstr_("Smoke timer BG"), smokeTimerBG);
	WRITE(xorstr_("Smoke timer TIMER"), smokeTimerTimer);
	WRITE(xorstr_("Smoke timer TEXT"), smokeTimerText);
	WRITE(xorstr_("Molotov timer"), molotovTimer);
	WRITE(xorstr_("Molotov timer BG"), molotovTimerBG);
	WRITE(xorstr_("Molotov timer TIMER"), molotovTimerTimer);
	WRITE(xorstr_("Molotov timer TEXT"), molotovTimerText);
	WRITE(xorstr_("Footstep"), footsteps);
}

static void to_json(json& j, const ImVec4& o)
{
	j[0] = o.x;
	j[1] = o.y;
	j[2] = o.z;
	j[3] = o.w;
}

static void to_json(json& j, const sticker_setting& o)
{
	const sticker_setting dummy;

	WRITE(xorstr_("Kit"), kit);
	WRITE(xorstr_("Wear"), wear);
	WRITE(xorstr_("Scale"), scale);
	WRITE(xorstr_("Rotation"), rotation);
}

static void to_json(json& j, const item_setting& o)
{
	const item_setting dummy;

	WRITE(xorstr_("Enabled"), enabled);
	WRITE(xorstr_("Definition index"), itemId);
	WRITE(xorstr_("Quality"), quality);
	WRITE(xorstr_("Paint Kit"), paintKit);
	WRITE(xorstr_("Definition override"), definition_override_index);
	WRITE(xorstr_("Seed"), seed);
	WRITE(xorstr_("StatTrak"), stat_trak);
	WRITE(xorstr_("Wear"), wear);
	if (o.custom_name[0])
		j[xorstr_("Custom name")] = o.custom_name;
	WRITE(xorstr_("Stickers"), stickers);
}

#pragma endregion

void removeEmptyObjects(json& j) noexcept
{
	for (auto it = j.begin(); it != j.end();) {
		auto& val = it.value();
		if (val.is_object() || val.is_array())
			removeEmptyObjects(val);
		if (val.empty() && !j.is_array())
			it = j.erase(it);
		else
			++it;
	}
}

void Config::save(size_t id) const noexcept
{
	createConfigDir();

	if (std::ofstream out{ path / (const char8_t*)configs[id].c_str() }; out.good()) {
		json j;

		j[xorstr_("Legitbot")] = legitbot;
		to_json(j[xorstr_("Legitbot Key")], legitbotKey, KeyBind::NONE);
		j[xorstr_("Draw legitbot fov")] = legitbotFov;

		j[xorstr_("RCS")] = recoilControlSystem;
		j[xorstr_("Optimizations")] = optimizations;

		j[xorstr_("Ragebot")] = ragebot;
		to_json(j[xorstr_("Ragebot Key")], ragebotKey, KeyBind::NONE);
		to_json(j[xorstr_("Min damage override Key")], minDamageOverrideKey, KeyBind::NONE);

		j[xorstr_("Triggerbot")] = triggerbot;
		to_json(j[xorstr_("Triggerbot Key")], triggerbotKey, KeyBind::NONE);

		j[xorstr_("Legit Anti aim")] = legitAntiAim;
		j[xorstr_("Rage Anti aim")] = rageAntiAim;
		to_json(j[xorstr_("Manual forward Key")], manualForward, KeyBind::NONE);
		to_json(j[xorstr_("Manual backward Key")], manualBackward, KeyBind::NONE);
		to_json(j[xorstr_("Manual right Key")], manualRight, KeyBind::NONE);
		to_json(j[xorstr_("Manual left Key")], manualLeft, KeyBind::NONE);
		to_json(j[xorstr_("Auto direction Key")], autoDirection, KeyBind::NONE);
		j[xorstr_("Disable in freezetime")] = disableInFreezetime;
		j[xorstr_("Fake angle")] = fakeAngle;
		to_json(j[xorstr_("Invert")], invert, KeyBind::NONE);
		j[xorstr_("Fakelag")] = fakelag;
		j[xorstr_("Tickbase")] = tickbase;
		j[xorstr_("Backtrack")] = backtrack;

		j[xorstr_("Glow")][xorstr_("Items")] = glow;
		j[xorstr_("Glow")][xorstr_("Players")] = playerGlow;
		to_json(j[xorstr_("Glow")][xorstr_("Key")], glowKey, KeyBind::NONE);

		j[xorstr_("Chams")] = chams;
		to_json(j[xorstr_("Chams")][xorstr_("Key")], chamsKey, KeyBind::NONE);
		j[xorstr_("ESP")] = streamProofESP;
		j[xorstr_("Sound")] = Sound::toJson();
		j[xorstr_("Visuals")] = visuals;
		j[xorstr_("Misc")] = misc;
		j[xorstr_("Skin changer")] = skinChanger;

		removeEmptyObjects(j);
		out << std::setw(2) << j;
	}
}

void Config::add(const char* name) noexcept
{
	if (*name && std::find(configs.cbegin(), configs.cend(), name) == configs.cend()) {
		configs.emplace_back(name);
		save(configs.size() - 1);
	}
}

void Config::remove(size_t id) noexcept
{
	std::error_code ec;
	std::filesystem::remove(path / (const char8_t*)configs[id].c_str(), ec);
	configs.erase(configs.cbegin() + id);
}

void Config::rename(size_t item, const char* newName) noexcept
{
	std::error_code ec;
	std::filesystem::rename(path / (const char8_t*)configs[item].c_str(), path / (const char8_t*)newName, ec);
	configs[item] = newName;
}

void Config::reset() noexcept
{
	legitbot = { };
	recoilControlSystem = { };
	legitAntiAim = { };
	ragebot = { };
	rageAntiAim = { };
	disableInFreezetime = true;
	fakeAngle = { };
	fakelag = { };
	tickbase = { };
	backtrack = { };
	triggerbot = { };
	Glow::resetConfig();
	chams = { };
	streamProofESP = { };
	visuals = { };
	skinChanger = { };
	Sound::resetConfig();
	misc = { };
}

void Config::listConfigs() noexcept
{
	configs.clear();

	std::error_code ec;
	std::transform(std::filesystem::directory_iterator{ path, ec },
		std::filesystem::directory_iterator{ },
		std::back_inserter(configs),
		[](const auto& entry) { return std::string{ (const char*)entry.path().filename().u8string().c_str() }; });
}

void Config::createConfigDir() const noexcept
{
	std::error_code ec;
	create_directory(path, ec);
}

void Config::openConfigDir() const noexcept
{
	createConfigDir();
	ShellExecuteW(nullptr, xorstr_(L"open"), path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Config::scheduleFontLoad(const std::string& name) noexcept
{
	scheduledFonts.push_back(name);
}

static auto getFontData(const std::string& fontName) noexcept
{
	HFONT font = CreateFontA(0, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, fontName.c_str());

	std::unique_ptr<std::byte[]> data;
	DWORD dataSize = GDI_ERROR;

	if (font) {
		HDC hdc = CreateCompatibleDC(nullptr);

		if (hdc) {
			SelectObject(hdc, font);
			dataSize = GetFontData(hdc, 0, 0, nullptr, 0);

			if (dataSize != GDI_ERROR) {
				data = std::make_unique<std::byte[]>(dataSize);
				dataSize = GetFontData(hdc, 0, 0, data.get(), dataSize);

				if (dataSize == GDI_ERROR)
					data.reset();
			}
			DeleteDC(hdc);
		}
		DeleteObject(font);
	}
	return std::make_pair(std::move(data), dataSize);
}

bool Config::loadScheduledFonts() noexcept
{
	bool result = false;

	for (const auto& fontName : scheduledFonts) {
		if (fontName == xorstr_("Default")) {
			if (fonts.find(xorstr_("Default")) == fonts.cend()) {
				ImFontConfig cfg;
				cfg.OversampleH = cfg.OversampleV = 1;
				cfg.PixelSnapH = true;
				cfg.RasterizerMultiply = 1.7f;

				Font newFont;

				cfg.SizePixels = 13.0f;
				newFont.big = ImGui::GetIO().Fonts->AddFontDefault(&cfg);

				cfg.SizePixels = 10.0f;
				newFont.medium = ImGui::GetIO().Fonts->AddFontDefault(&cfg);

				cfg.SizePixels = 8.0f;
				newFont.tiny = ImGui::GetIO().Fonts->AddFontDefault(&cfg);

				fonts.emplace(fontName, newFont);
				result = true;
			}
			continue;
		}

		const auto [fontData, fontDataSize] = getFontData(fontName);
		if (fontDataSize == GDI_ERROR)
			continue;

		if (fonts.find(fontName) == fonts.cend()) {
			const auto ranges = Helpers::getFontGlyphRanges();
			ImFontConfig cfg;
			cfg.FontDataOwnedByAtlas = false;
			cfg.RasterizerMultiply = 1.7f;

			Font newFont;
			newFont.tiny = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontData.get(), fontDataSize, 8.0f, &cfg, ranges);
			newFont.medium = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontData.get(), fontDataSize, 10.0f, &cfg, ranges);
			newFont.big = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontData.get(), fontDataSize, 13.0f, &cfg, ranges);
			fonts.emplace(fontName, newFont);
			result = true;
		}
	}
	scheduledFonts.clear();
	return result;
}
