#include <mutex>
#include <numeric>
#include <sstream>
#include <string>

#include <imgui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>
#include <PCG/pcg.h>

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../Netvars.h"
#include "../GUI.h"
#include "../Helpers.h"
#include "../GameData.h"

#include "AimbotFunctions.h"
#include "EnginePrediction.h"
#include "Misc.h"
#include "Fakelag.h"
#include "Ragebot.h"
#include "Resolver.h"

#include "../SDK/Client.h"
#include "../SDK/ClientMode.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/GameEvent.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Input.h"
#include "../SDK/ItemSchema.h"
#include "../SDK/Localize.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Panorama.h"
#include "../SDK/Prediction.h"
#include "../SDK/ProtoWriter.h"
#include "../SDK/Surface.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ViewSetup.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponSystem.h"

bool Misc::isInChat() noexcept
{
	if (!localPlayer)
		return false;

	const auto hudChat = memory->findHudElement(memory->hud, "CCSGO_HudChat");
	if (!hudChat)
		return false;

	const bool isInChat = *(bool*)((uintptr_t)hudChat + 0xD);

	return isInChat;
}

std::string currentForwardKey = "";
std::string currentBackKey = "";
std::string currentRightKey = "";
std::string currentLeftKey = "";
int currentButtons = 0;

void Misc::gatherDataOnTick(UserCmd* cmd) noexcept
{
	currentButtons = cmd->buttons;
}

void Misc::handleKeyEvent(int keynum, const char* currentBinding) noexcept
{
	if (!currentBinding || keynum <= 0 || keynum >= static_cast<int>(ButtonCodes.size()))
		return;

	const auto buttonName = ButtonCodes[keynum];

	switch (fnv::hash(currentBinding)) {
	case fnv::hash("+forward"):
		currentForwardKey = std::string(buttonName);
		break;
	case fnv::hash("+back"):
		currentBackKey = std::string(buttonName);
		break;
	case fnv::hash("+moveright"):
		currentRightKey = std::string(buttonName);
		break;
	case fnv::hash("+moveleft"):
		currentLeftKey = std::string(buttonName);
		break;
	default:
		break;
	}
}

void Misc::drawKeyDisplay(ImDrawList* drawList) noexcept
{
	if (!config->misc.keyBoardDisplay.enabled)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	int screenSizeX, screenSizeY;
	interfaces->engine->getScreenSize(screenSizeX, screenSizeY);
	const float Ypos = static_cast<float>(screenSizeY) * config->misc.keyBoardDisplay.position;

	std::string keys[3][2];
	if (config->misc.keyBoardDisplay.showKeyTiles) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 2; j++) {
				keys[i][j] = "_";
			}
		}
	}

	if (currentButtons & UserCmd::IN_DUCK)
		keys[0][0] = "C";
	if (currentButtons & UserCmd::IN_FORWARD)
		keys[1][0] = currentForwardKey;
	if (currentButtons & UserCmd::IN_JUMP)
		keys[2][0] = "J";
	if (currentButtons & UserCmd::IN_MOVELEFT)
		keys[0][1] = currentLeftKey;
	if (currentButtons & UserCmd::IN_BACK)
		keys[1][1] = currentBackKey;
	if (currentButtons & UserCmd::IN_MOVERIGHT)
		keys[2][1] = currentRightKey;

	const float positions[3] =
	{
	   -35.0f, 0.0f, 35.0f
	};

	ImGui::PushFont(gui->getTahoma28Font());
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 2; j++) {
			if (keys[i][j] == "")
				continue;

			const auto size = ImGui::CalcTextSize(keys[i][j].c_str());
			drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - size.x / 2 + positions[i]) + 1, (Ypos + (j * 25)) + 1 }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.keyBoardDisplay.color.color[3] }), keys[i][j].c_str());
			drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - size.x / 2 + positions[i], Ypos + (j * 25) }, Helpers::calculateColor(config->misc.keyBoardDisplay.color), keys[i][j].c_str());
		}
	}

	ImGui::PopFont();
}

void Misc::drawVelocity(ImDrawList* drawList) noexcept
{
	if (!config->misc.velocity.enabled)
		return;

	if (!localPlayer)
		return;

	const auto entity = localPlayer->isAlive() ? localPlayer.get() : localPlayer->getObserverTarget();
	if (!entity)
		return;

	int screenSizeX, screenSizeY;
	interfaces->engine->getScreenSize(screenSizeX, screenSizeY);
	const float Ypos = static_cast<float>(screenSizeY) * config->misc.velocity.position;

	static float colorTime = 0.f;
	static float takeOffTime = 0.f;

	static auto lastVelocity = 0;
	const auto velocity = static_cast<int>(round(entity->velocity().length2D()));

	static auto takeOffVelocity = 0;
	static bool lastOnGround = true;
	const bool onGround = entity->flags() & 1;
	if (lastOnGround && !onGround) {
		takeOffVelocity = velocity;
		takeOffTime = memory->globalVars->realtime + 2.f;
	}

	const bool shouldDrawTakeOff = !onGround || (takeOffTime > memory->globalVars->realtime);
	const std::string finalText = std::to_string(velocity);

	const Color4 trueColor = config->misc.velocity.color.enabled ? Color4{ config->misc.velocity.color.color[0], config->misc.velocity.color.color[1], config->misc.velocity.color.color[2], config->misc.velocity.alpha, config->misc.velocity.color.rainbowSpeed, config->misc.velocity.color.rainbow }
	: (velocity == lastVelocity ? Color4{ 1.0f, 0.78f, 0.34f, config->misc.velocity.alpha } : velocity < lastVelocity ? Color4{ 1.0f, 0.46f, 0.46f, config->misc.velocity.alpha } : Color4{ 0.11f, 1.0f, 0.42f, config->misc.velocity.alpha });

	ImGui::PushFont(gui->getTahoma28Font());

	const auto size = ImGui::CalcTextSize(finalText.c_str());
	drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - size.x / 2) + 1, Ypos + 1.0f }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.velocity.alpha }), finalText.c_str());
	drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - size.x / 2, Ypos }, Helpers::calculateColor(trueColor), finalText.c_str());

	if (shouldDrawTakeOff) {
		const std::string bottomText = "(" + std::to_string(takeOffVelocity) + ")";
		const Color4 bottomTrueColor = config->misc.velocity.color.enabled ? Color4{ config->misc.velocity.color.color[0], config->misc.velocity.color.color[1], config->misc.velocity.color.color[2], config->misc.velocity.alpha, config->misc.velocity.color.rainbowSpeed, config->misc.velocity.color.rainbow }
		: (takeOffVelocity <= 250.0f ? Color4{ 0.75f, 0.75f, 0.75f, config->misc.velocity.alpha } : Color4{ 0.11f, 1.0f, 0.42f, config->misc.velocity.alpha });
		const auto bottomSize = ImGui::CalcTextSize(bottomText.c_str());
		drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - bottomSize.x / 2) + 1, Ypos + 20.0f + 1 }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.velocity.alpha }), bottomText.c_str());
		drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - bottomSize.x / 2, Ypos + 20.0f }, Helpers::calculateColor(bottomTrueColor), bottomText.c_str());
	}

	ImGui::PopFont();

	if (colorTime <= memory->globalVars->realtime) {
		colorTime = memory->globalVars->realtime + 0.1f;
		lastVelocity = velocity;
	}
	lastOnGround = onGround;
}

class JumpStatsCalculations {
private:
	static constexpr auto white = '\x01';
	static constexpr auto violet = '\x03';
	static constexpr auto green = '\x04';
	static constexpr auto red = '\x07';
	static constexpr auto golden = '\x09';
public:
	void resetStats() noexcept
	{
		units = 0.0f;
		strafes = 0;
		pre = 0.0f;
		maxVelocity = 0.0f;
		maxHeight = 0.0f;
		jumps = 0;
		bhops = 0;
		sync = 0.0f;
		startPosition = Vector{ };
		landingPosition = Vector{ };
	}

	bool show() noexcept
	{
		if (!onGround || jumping || jumpbugged)
			return false;

		if (!shouldShow)
			return false;

		units = (startPosition - landingPosition).length2D() + (isLadderJump ? 0.0f : 32.0f);

		const float z = std::abs(startPosition.z - landingPosition.z) - (isJumpbug ? 9.0f : 0.0f);
		const bool fail = z >= (isLadderJump ? 32.0f : (jumps > 0 ? (jumps > 1 ? 46.0f : 2.0f) : 46.0f));
		const bool simplifyNames = config->misc.jumpStats.simplifyNaming;

		std::string jump = "null";

		//Values taken from
		//https://github.com/KZGlobalTeam/gokz/blob/33a3a49bc7a0e336e71c7f59c14d26de4db62957/cfg/sourcemod/gokz/gokz-jumpstats-tiers.cfg
		auto color = white;
		switch (jumps) {
		case 1:
			if (!isJumpbug) {
				jump = simplifyNames ? "LJ" : "Long jump";
				if (units < 230.0f)
					color = white;
				else if (units >= 230.0f && units < 235.0f)
					color = violet;
				else if (units >= 235.0f && units < 238.0f)
					color = green;
				else if (units >= 238.0f && units < 240.0f)
					color = red;
				else if (units >= 240.0f)
					color = golden;
			} else {
				jump = simplifyNames ? "JB" : "Jump bug";
				if (units < 250.0f)
					color = white;
				else if (units >= 250.0f && units < 260.0f)
					color = violet;
				else if (units >= 260.0f && units < 265.0f)
					color = green;
				else if (units >= 265.0f && units < 270.0f)
					color = red;
				else if (units >= 270.0f)
					color = golden;
			}
			break;
		case 2:
			jump = simplifyNames ? "BH" : "Bunnyhop";
			if (units < 230.0f)
				color = white;
			else if (units >= 230.0f && units < 233.0f)
				color = violet;
			else if (units >= 233.0f && units < 235.0f)
				color = green;
			else if (units >= 235.0f && units < 240.0f)
				color = red;
			else if (units >= 240.0f)
				color = golden;
			break;
		default:
			if (jumps >= 3) {
				jump = simplifyNames ? "MBH" : "Multi Bunnyhop";
				if (units < 230.0f)
					color = white;
				else if (units >= 230.0f && units < 233.0f)
					color = violet;
				else if (units >= 233.0f && units < 235.0f)
					color = green;
				else if (units >= 235.0f && units < 240.0f)
					color = red;
				else if (units >= 240.0f)
					color = golden;
			}
			break;
		}

		if (isLadderJump) {
			jump = simplifyNames ? "LAJ" : "Ladder jump";
			if (units < 80.0f)
				color = white;
			else if (units >= 80.0f && units < 90.0f)
				color = violet;
			else if (units >= 90.0f && units < 105.0f)
				color = green;
			else if (units >= 105.0f && units < 109.0f)
				color = red;
			else if (units >= 109.0f)
				color = golden;
		}

		if (!config->misc.jumpStats.showColorOnFail && fail)
			color = white;

		if (fail)
			jump += simplifyNames ? "-F" : " Failed";

		const bool show = (isLadderJump ? units >= 50.0f : units >= 186.0f) && (!(!config->misc.jumpStats.showFails && fail) || (config->misc.jumpStats.showFails));
		if (show && config->misc.jumpStats.enabled) {
			//Certain characters are censured on printf
			if (jumps > 2)
				memory->clientMode->getHudChat()->printf(0,
				" \x0C\u2022Osiris\u2022\x01 %c%s: %.2f units \x01[\x05%d\x01 Strafes | \x05%.0f\x01 Pre | \x05%.0f\x01 Max | \x05%.1f\x01 Height | \x05%d\x01 Bhops | \x05%.0f\x01 Sync]",
				color, jump.c_str(),
				units, strafes, pre, maxVelocity, maxHeight, jumps, sync);
			else
				memory->clientMode->getHudChat()->printf(0,
				" \x0C\u2022Osiris\u2022\x01 %c%s: %.2f units \x01[\x05%d\x01 Strafes | \x05%.0f\x01 Pre | \x05%.0f\x01 Max | \x05%.1f\x01 Height | \x05%.0f\x01 Sync]",
				color, jump.c_str(),
				units, strafes, pre, maxVelocity, maxHeight, sync);
		}

		shouldShow = false;
		return true;
	}

	void run(UserCmd* cmd) noexcept
	{
		if (localPlayer->moveType() == MoveType::NOCLIP) {
			resetStats();
			shouldShow = false;
			return;
		}

		velocity = localPlayer->velocity().length2D();
		origin = localPlayer->getAbsOrigin();
		onGround = localPlayer->flags() & 1;
		onLadder = localPlayer->moveType() == MoveType::LADDER;
		jumping = cmd->buttons & UserCmd::IN_JUMP && !(lastButtons & UserCmd::IN_JUMP) && onGround;
		jumpbugged = !jumpped && hasJumped;

		//We jumped so we should show this jump
		if (jumping || jumpbugged)
			shouldShow = true;

		if (onLadder) {
			startPosition = origin;
			pre = velocity;
			startedOnLadder = true;
		}

		if (onGround) {
			if (!onLadder) {
				if (jumping) {
					//We save pre velocity and the starting position
					startPosition = origin;
					pre = velocity;
					jumps++;
					startedOnLadder = false;
					isLadderJump = false;
				} else {
					landingPosition = origin;
					//We reset our jumps after logging them, and incase we do log our jumps and need to reset anyways we do this
					if (!shouldShow)
						jumps = 0;

					if (startedOnLadder) {
						isLadderJump = true;
						shouldShow = true;
					}
					startedOnLadder = false;
				}
			}

			//Calculate sync
			if (ticksInAir > 0 && !jumping)
				sync = (static_cast<float>(ticksSynced) / static_cast<float>(ticksInAir)) * 100.0f;

			//Reset both counters used for calculating sync
			ticksInAir = 0;
			ticksSynced = 0;
		} else if (!onGround && !onLadder) {
			if (jumpbugged) {
				if (oldOrigin.notNull())
					startPosition = oldOrigin;
				pre = oldVelocity;
				jumps = 1;
				isJumpbug = true;
				jumpbugged = false;
			}
			//Check for strafes
			if (cmd->mousedx != 0 && cmd->sidemove != 0.0f) {
				if (cmd->mousedx > 0 && lastMousedx <= 0.0f && cmd->sidemove > 0.0f) {
					strafes++;
				}
				if (cmd->mousedx < 0 && lastMousedx >= 0.0f && cmd->sidemove < 0.0f) {
					strafes++;
				}
			}

			//If we gain velocity, we gain more sync
			if (oldVelocity != 0.0f) {
				float deltaSpeed = velocity - oldVelocity;
				bool gained = deltaSpeed > 0.000001f;
				//bool lost = deltaSpeed < -0.000001f;
				if (gained) {
					ticksSynced++;
				}
			}

			//Get max height and max velocity
			maxHeight = std::max(std::abs(startPosition.z - origin.z), maxHeight);
			maxVelocity = std::max(velocity, maxVelocity);

			ticksInAir++; //We are in air
			sync = 0; //We dont calculate sync yet
		}

		lastMousedx = cmd->mousedx;
		lastOnGround = onGround;
		lastButtons = cmd->buttons;
		oldVelocity = velocity;
		oldOrigin = origin;
		jumpped = jumping;
		hasJumped = false;

		if (show())
			resetStats();

		if (onGround && !onLadder) {
			isJumpbug = false;
		}
		isLadderJump = false;
	}

	//Last values
	short lastMousedx{ 0 };
	bool lastOnGround{ false };
	int lastButtons{ 0 };
	float oldVelocity{ 0.0f };
	bool jumpped{ false };
	Vector oldOrigin{ };
	Vector startPosition{ };

	//Current values
	float velocity{ 0.0f };
	bool onLadder{ false };
	bool onGround{ false };
	bool jumping{ false };
	bool jumpbugged{ false };
	bool isJumpbug{ false };
	bool hasJumped{ false };
	bool startedOnLadder{ false };
	bool isLadderJump{ false };
	bool shouldShow{ false };
	int jumps{ 0 };
	Vector origin{ };
	Vector landingPosition{ };
	int ticksInAir{ 0 };
	int ticksSynced{ 0 };

	//Final values
	float units{ 0.0f };
	int strafes{ 0 };
	float pre{ 0.0f };
	float maxVelocity{ 0.0f };
	float maxHeight{ 0.0f };
	int bhops{ 0 };
	float sync{ 0.0f };
} jumpStatsCalculations;

void Misc::gotJump() noexcept
{
	jumpStatsCalculations.hasJumped = true;
}

void Misc::jumpStats(UserCmd* cmd) noexcept
{
	if (!localPlayer)
		return;

	if (!localPlayer->isAlive()) {
		jumpStatsCalculations = { };
		return;
	}

	static bool once = true;
	if ((!*memory->gameRules || (*memory->gameRules)->freezePeriod()) || localPlayer->flags() & (1 << 6)) {
		if (once) {
			jumpStatsCalculations = { };
			once = false;
		}
		return;
	}

	jumpStatsCalculations.run(cmd);

	once = true;
}

void Misc::miniJump(UserCmd* cmd) noexcept
{
	static int lockedTicks = 0;
	if (!config->misc.miniJump || (!config->misc.miniJumpKey.isActive() && config->misc.miniJumpCrouchLock <= 0)) {
		lockedTicks = 0;
		return;
	}

	if (!localPlayer || !localPlayer->isAlive()) {
		lockedTicks = 0;
		return;
	}

	if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER) {
		lockedTicks = 0;
		return;
	}

	if (lockedTicks >= 1) {
		cmd->buttons |= UserCmd::IN_DUCK;
		lockedTicks--;
	}

	if ((EnginePrediction::getFlags() & 1) && !(localPlayer->flags() & 1)) {
		cmd->buttons |= UserCmd::IN_JUMP;
		cmd->buttons |= UserCmd::IN_DUCK;
		lockedTicks = config->misc.miniJumpCrouchLock;
	}
}

void Misc::autoPixelSurf(UserCmd* cmd) noexcept
{
	if (!config->misc.autoPixelSurf || !config->misc.autoPixelSurfKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (EnginePrediction::getFlags() & 1)
		return;

	if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
		return;

	//Restore before prediction
	memory->restoreEntityToPredictedFrame(0, interfaces->prediction->split->commandsPredicted - 1);

	bool detectedPixelSurf = false;

	for (int i = 0; i < config->misc.autoPixelSurfPredAmnt; i++) {
		auto backupButtons = cmd->buttons;
		cmd->buttons |= UserCmd::IN_DUCK;

		EnginePrediction::run(cmd);

		cmd->buttons = backupButtons;

		detectedPixelSurf = (localPlayer->velocity().z == -6.25f || localPlayer->velocity().z == -6.f || localPlayer->velocity().z == -3.125) && !(localPlayer->flags() & 1) && localPlayer->moveType() != MoveType::LADDER;

		if (detectedPixelSurf)
			break;
	}

	if (detectedPixelSurf) {
		cmd->buttons |= UserCmd::IN_DUCK;
	}

	memory->restoreEntityToPredictedFrame(0, interfaces->prediction->split->commandsPredicted - 1);
	EnginePrediction::run(cmd);
}

bool shouldEdgebug = false;
float zVelBackup = 0.0f;
float bugSpeed = 0.0f;
int edgebugButtons = 0;

void Misc::edgeBug(UserCmd* cmd, Vector& angView) noexcept
{
	if (!config->misc.edgeBug || !config->misc.edgeBugKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (localPlayer->flags() & 1)
		return;

	shouldEdgebug = zVelBackup < -bugSpeed && std::round(localPlayer->velocity().z) == -std::round(bugSpeed) && localPlayer->moveType() != MoveType::LADDER;
	if (shouldEdgebug)
		return;

	const int commandsPredicted = interfaces->prediction->split->commandsPredicted;

	const Vector angViewOriginal = angView;
	const Vector angCmdViewOriginal = cmd->viewangles;
	const int buttonsOriginal = cmd->buttons;
	Vector vecMoveOriginal;
	vecMoveOriginal.x = cmd->sidemove;
	vecMoveOriginal.y = cmd->forwardmove;

	static Vector vecMoveLastStrafe;
	static Vector angViewLastStrafe;
	static Vector angViewOld = angView;
	static Vector angViewDeltaStrafe;
	static bool appliedStrafeLast = false;
	if (!appliedStrafeLast) {
		angViewLastStrafe = angView;
		vecMoveLastStrafe = vecMoveOriginal;
		angViewDeltaStrafe = (angView - angViewOld);
	}
	appliedStrafeLast = false;
	angViewOld = angView;

	for (int t = 0; t < 4; t++) {
		static int lastType = 0;
		if (lastType) {
			t = lastType;
			lastType = 0;
		}
		memory->restoreEntityToPredictedFrame(0, commandsPredicted - 1);
		if (buttonsOriginal& UserCmd::IN_DUCK&& t < 2)
			t = 2;
		bool applyStrafe = !(t % 2);
		bool applyDuck = t > 1;

		cmd->viewangles = angViewLastStrafe;
		cmd->buttons = buttonsOriginal;
		cmd->sidemove = vecMoveLastStrafe.x;
		cmd->forwardmove = vecMoveLastStrafe.y;

		for (int i = 0; i < config->misc.edgeBugPredAmnt; i++) {
			if (applyDuck)
				cmd->buttons |= UserCmd::IN_DUCK;
			else
				cmd->buttons &= ~UserCmd::IN_DUCK;
			if (applyStrafe) {
				cmd->viewangles += angViewDeltaStrafe;
				cmd->viewangles.normalize();
			} else {
				cmd->sidemove = 0.f;
				cmd->forwardmove = 0.f;
			}
			EnginePrediction::run(cmd);
			shouldEdgebug = zVelBackup < -bugSpeed && std::round(localPlayer->velocity().z) == -std::round(bugSpeed) && localPlayer->moveType() != MoveType::LADDER;
			zVelBackup = localPlayer->velocity().z;
			if (shouldEdgebug) {
				edgebugButtons = cmd->buttons;
				if (applyStrafe) {
					appliedStrafeLast = true;
					angView = (angViewLastStrafe + angViewDeltaStrafe);
					angView.normalize();
					angViewLastStrafe = angView;
					cmd->sidemove = vecMoveLastStrafe.x;
					cmd->forwardmove = vecMoveLastStrafe.y;
				}
				cmd->viewangles = angCmdViewOriginal;
				lastType = t;
				memory->restoreEntityToPredictedFrame(0, interfaces->prediction->split->commandsPredicted - 1);
				EnginePrediction::run(cmd);
				return;
			}

			if (localPlayer->flags() & 1 || localPlayer->moveType() == MoveType::LADDER)
				break;
		}
	}
	cmd->viewangles = angCmdViewOriginal;
	angView = angViewOriginal;
	cmd->buttons = buttonsOriginal;
	cmd->sidemove = vecMoveOriginal.x;
	cmd->forwardmove = vecMoveOriginal.y;

	memory->restoreEntityToPredictedFrame(0, interfaces->prediction->split->commandsPredicted - 1);
	EnginePrediction::run(cmd);
}

void Misc::prePrediction(UserCmd* cmd) noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	zVelBackup = localPlayer->velocity().z;

	if (shouldEdgebug)
		cmd->buttons = edgebugButtons;

	static auto gravity = interfaces->cvar->findVar("sv_gravity");
	bugSpeed = (gravity->getFloat() * 0.5f * memory->globalVars->intervalPerTick);

	shouldEdgebug = zVelBackup < -bugSpeed && std::round(localPlayer->velocity().z) == -std::round(bugSpeed) && localPlayer->moveType() != MoveType::LADDER;
}

void Misc::drawPlayerList() noexcept
{
	if (!config->misc.playerList.enabled)
		return;

	if (config->misc.playerList.pos != ImVec2{}) {
		ImGui::SetNextWindowPos(config->misc.playerList.pos);
		config->misc.playerList.pos = {};
	}

	static bool changedName = true;
	static std::string nameToChange = "";

	if (!changedName && nameToChange != "")
		changedName = changeName(false, (nameToChange + '\x1').c_str(), 1.0f);

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (!gui->isOpen()) {
		windowFlags |= ImGuiWindowFlags_NoInputs;
		return;
	}

	GameData::Lock lock;
	if ((GameData::players().empty()) && !gui->isOpen())
		return;

	ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_Once);

	if (ImGui::Begin("Player List", nullptr, windowFlags)) {
		if (ImGui::beginTable("", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 120.0f);
			ImGui::TableSetupColumn("Steam ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Wins", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Armor", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Money", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetColumnEnabled(2, config->misc.playerList.steamID);
			ImGui::TableSetColumnEnabled(3, config->misc.playerList.rank);
			ImGui::TableSetColumnEnabled(4, config->misc.playerList.wins);
			ImGui::TableSetColumnEnabled(5, config->misc.playerList.health);
			ImGui::TableSetColumnEnabled(6, config->misc.playerList.armor);
			ImGui::TableSetColumnEnabled(7, config->misc.playerList.money);

			ImGui::TableHeadersRow();

			std::vector<std::reference_wrapper<const PlayerData>> playersOrdered{ GameData::players().begin(), GameData::players().end() };
			std::ranges::sort(playersOrdered, [](const PlayerData& a, const PlayerData& b) {
				// enemies first
				if (a.enemy != b.enemy)
					return a.enemy && !b.enemy;

				return a.handle < b.handle;
				});

			ImGui::PushFont(gui->getUnicodeFont());

			for (const PlayerData& player : playersOrdered) {
				ImGui::TableNextRow();
				ImGui::PushID(ImGui::TableGetRowIndex());

				if (ImGui::TableNextColumn())
					ImGui::Text("%d", player.userId);

				if (ImGui::TableNextColumn()) {
					ImGui::Image(player.getAvatarTexture(), { ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
					ImGui::SameLine();
					ImGui::textEllipsisInTableCell(player.name.c_str());
				}

				if (ImGui::TableNextColumn() && ImGui::smallButtonFullWidth("Copy", player.steamID == 0))
					ImGui::SetClipboardText(std::to_string(player.steamID).c_str());

				if (ImGui::TableNextColumn()) {
					ImGui::Image(player.getRankTexture(), { 2.45f /* -> proportion 49x20px */ * ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::PushFont(nullptr);
						ImGui::TextUnformatted(player.getRankName().data());
						ImGui::PopFont();
						ImGui::EndTooltip();
					}

				}

				if (ImGui::TableNextColumn())
					ImGui::Text("%d", player.competitiveWins);

				if (ImGui::TableNextColumn()) {
					if (!player.alive)
						ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", "Dead");
					else
						ImGui::Text("%d HP", player.health);
				}

				if (ImGui::TableNextColumn())
					ImGui::Text("%d", player.armor);

				if (ImGui::TableNextColumn())
					ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "$%d", player.money);

				if (ImGui::TableNextColumn()) {
					if (ImGui::smallButtonFullWidth("...", false))
						ImGui::OpenPopup("");

					if (ImGui::BeginPopup("")) {
						if (ImGui::Button("Steal name")) {
							changedName = changeName(false, (std::string{ player.name } + '\x1').c_str(), 1.0f);
							nameToChange = player.name;

							if (PlayerInfo playerInfo; interfaces->engine->isInGame() && localPlayer
								&& interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (playerInfo.name == std::string{ "?empty" } || playerInfo.name == std::string{ "\n\xAD\xAD\xAD" }))
								changedName = false;
						}

						if (ImGui::Button("Steal clantag"))
							memory->setClanTag(player.clanTag.c_str(), player.clanTag.c_str());

						if (GameData::local().exists && player.team == GameData::local().team && player.steamID != 0) {
							if (ImGui::Button("Kick")) {
								const std::string cmd = "callvote kick " + std::to_string(player.userId);
								interfaces->engine->clientCmdUnrestricted(cmd.c_str());
							}
						}

						ImGui::EndPopup();
					}
				}

				ImGui::PopID();
			}

			ImGui::PopFont();
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

static int blockTargetHandle = 0;

void Misc::blockBot(UserCmd* cmd) noexcept
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (!config->misc.blockBot || !config->misc.blockBotKey.isActive()) {
		blockTargetHandle = 0;
		return;
	}

	float best = 1024.0f;
	if (!blockTargetHandle) {
		for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
			Entity* entity = interfaces->entityList->getEntity(i);

			if (!entity || !entity->isPlayer() || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive())
				continue;

			const auto distance = entity->getAbsOrigin().distTo(localPlayer->getAbsOrigin());
			if (distance < best) {
				best = distance;
				blockTargetHandle = entity->handle();
			}
		}
	}

	const auto target = interfaces->entityList->getEntityFromHandle(blockTargetHandle);
	if (target && target->isPlayer() && target != localPlayer.get() && !target->isDormant() && target->isAlive()) {
		const auto targetVec = (target->getAbsOrigin() + target->velocity() * memory->globalVars->intervalPerTick - localPlayer->getAbsOrigin());
		const auto z1 = target->getAbsOrigin().z - localPlayer->getEyePosition().z;
		const auto z2 = target->getEyePosition().z - localPlayer->getAbsOrigin().z;
		if (z1 >= 0.0f || z2 <= 0.0f) {
			Vector fwd = Vector::fromAngle2D(cmd->viewangles.y);
			Vector side = fwd.crossProduct(Vector{ 0.0f, 0.0f, 1.0f });
			Vector move = Vector{ fwd.dotProduct2D(targetVec), side.dotProduct2D(targetVec), 0.0f };
			move *= 45.0f;

			const float l = move.length2D();
			if (l > 450.0f)
				move *= 450.0f / l;

			cmd->forwardmove = move.x;
			cmd->sidemove = move.y;
		} else {
			Vector fwd = Vector::fromAngle2D(cmd->viewangles.y);
			Vector side = fwd.crossProduct(Vector{ 0.0f, 0.0f, 1.0f });
			Vector tar = (targetVec / targetVec.length2D()).crossProduct(Vector{ 0.0f, 0.0f, 1.0f });
			tar = tar.snapTo4();
			tar *= tar.dotProduct2D(targetVec);
			Vector move = Vector{ fwd.dotProduct2D(tar), side.dotProduct2D(tar), 0.0f };
			move *= 45.0f;

			const float l = move.length2D();
			if (l > 450.0f)
				move *= 450.0f / l;

			cmd->forwardmove = move.x;
			cmd->sidemove = move.y;
		}
	}
}

void Misc::visualizeBlockBot(ImDrawList* drawList) noexcept
{
	if (!config->misc.blockBotVisualize.enabled)
		return;

	GameData::Lock lock;
	const auto& local = GameData::local();

	if (!local.exists || !local.alive)
		return;

	auto target = GameData::playerByHandle(blockTargetHandle);
	if (!target || target->dormant || !target->alive)
		return;

	Vector max = target->obbMaxs + target->origin;
	Vector min = target->obbMins + target->origin;
	const auto z = target->origin.z;

	ImVec2 points[4];
	const auto color = Helpers::calculateColor(config->misc.blockBotVisualize);

	bool draw = Helpers::worldToScreen(Vector{ max.x, max.y, z }, points[0]);
	draw = draw && Helpers::worldToScreen(Vector{ max.x, min.y, z }, points[1]);
	draw = draw && Helpers::worldToScreen(Vector{ min.x, min.y, z }, points[2]);
	draw = draw && Helpers::worldToScreen(Vector{ min.x, max.y, z }, points[3]);

	if (draw) {
		drawList->AddLine(points[0], points[1], color, config->misc.blockBotVisualize.thickness);
		drawList->AddLine(points[1], points[2], color, config->misc.blockBotVisualize.thickness);
		drawList->AddLine(points[2], points[3], color, config->misc.blockBotVisualize.thickness);
		drawList->AddLine(points[3], points[0], color, config->misc.blockBotVisualize.thickness);
	}
}

static int buttons = 0;

void Misc::runFreeCam(UserCmd* cmd, Vector viewAngles) noexcept
{
	static Vector currentViewAngles = Vector{ };
	static Vector realViewAngles = Vector{ };
	static bool wasCrouching = false;
	static bool wasHoldingAttack = false;
	static bool wasHoldingUse = false;
	static bool hasSetAngles = false;

	buttons = cmd->buttons;
	if (!config->visuals.freeCam || !config->visuals.freeCamKey.isActive()) {
		if (hasSetAngles) {
			interfaces->engine->setViewAngles(realViewAngles);
			cmd->viewangles = currentViewAngles;
			if (wasCrouching)
				cmd->buttons |= UserCmd::IN_DUCK;
			if (wasHoldingAttack)
				cmd->buttons |= UserCmd::IN_ATTACK;
			if (wasHoldingUse)
				cmd->buttons |= UserCmd::IN_USE;
			wasCrouching = false;
			wasHoldingAttack = false;
			wasHoldingUse = false;
			hasSetAngles = false;
		}
		currentViewAngles = Vector{};
		return;
	}

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (currentViewAngles.null()) {
		currentViewAngles = cmd->viewangles;
		realViewAngles = viewAngles;
		wasCrouching = cmd->buttons & UserCmd::IN_DUCK;
	}

	cmd->forwardmove = 0;
	cmd->sidemove = 0;
	cmd->buttons = 0;
	if (wasCrouching)
		cmd->buttons |= UserCmd::IN_DUCK;
	if (wasHoldingAttack)
		cmd->buttons |= UserCmd::IN_ATTACK;
	if (wasHoldingUse)
		cmd->buttons |= UserCmd::IN_USE;
	cmd->viewangles = currentViewAngles;
	hasSetAngles = true;
}

void Misc::freeCam(ViewSetup* setup) noexcept
{
	static Vector newOrigin = Vector{ };

	if (!config->visuals.freeCam || !config->visuals.freeCamKey.isActive()) {
		newOrigin = Vector{ };
		return;
	}

	if (!localPlayer || !localPlayer->isAlive())
		return;

	float freeCamSpeed = std::abs(static_cast<float>(config->visuals.freeCamSpeed));

	if (newOrigin.null())
		newOrigin = setup->origin;

	Vector forward, right, up;
	setup->angles.fromAngle(forward, right, up);

	const bool backBtn = buttons & UserCmd::IN_BACK;
	const bool forwardBtn = buttons & UserCmd::IN_FORWARD;
	const bool rightBtn = buttons & UserCmd::IN_MOVERIGHT;
	const bool leftBtn = buttons & UserCmd::IN_MOVELEFT;
	const bool shiftBtn = buttons & UserCmd::IN_SPEED;
	const bool duckBtn = buttons & UserCmd::IN_DUCK;
	const bool jumpBtn = buttons & UserCmd::IN_JUMP;

	if (duckBtn)
		freeCamSpeed *= 0.45f;

	if (shiftBtn)
		freeCamSpeed *= 1.65f;

	if (forwardBtn)
		newOrigin += forward * freeCamSpeed;

	if (rightBtn)
		newOrigin += right * freeCamSpeed;

	if (leftBtn)
		newOrigin -= right * freeCamSpeed;

	if (backBtn)
		newOrigin -= forward * freeCamSpeed;

	if (jumpBtn)
		newOrigin += up * freeCamSpeed;

	setup->origin = newOrigin;
}

void Misc::viewModelChanger(ViewSetup* setup) noexcept
{
	if (!localPlayer)
		return;

	constexpr auto setViewmodel = [](Entity* viewModel, const Vector& angles) constexpr noexcept {
		if (viewModel) {
			Vector forward = Vector::fromAngle(angles);
			Vector up = Vector::fromAngle(angles - Vector{ 90.0f, 0.0f, 0.0f });
			Vector side = forward.crossProduct(up);
			Vector offset = side * config->visuals.viewModel.x + forward * config->visuals.viewModel.y + up * config->visuals.viewModel.z;
			memory->setAbsOrigin(viewModel, viewModel->getRenderOrigin() + offset);
			memory->setAbsAngle(viewModel, angles + Vector{ 0.0f, 0.0f, config->visuals.viewModel.roll });
		}
	};

	if (localPlayer->isAlive()) {
		if (config->visuals.viewModel.enabled && !localPlayer->isScoped() && !memory->input->isCameraInThirdPerson)
			setViewmodel(interfaces->entityList->getEntityFromHandle(localPlayer->viewModel()), setup->angles);
	} else if (auto observed = localPlayer->getObserverTarget(); observed && localPlayer->getObserverMode() == ObsMode::InEye) {
		if (config->visuals.viewModel.enabled && !observed->isScoped())
			setViewmodel(interfaces->entityList->getEntityFromHandle(observed->viewModel()), setup->angles);
	}
}

static Vector peekPosition{};

void Misc::drawAutoPeek(ImDrawList* drawList) noexcept
{
	if (!config->misc.autoPeek.enabled)
		return;

	if (peekPosition.notNull()) {
		constexpr float pi2 = std::numbers::pi_v<float> * 2.f;
		constexpr float step = pi2 / 20.f;
		std::vector<ImVec2> points;
		for (float lat = 0.f; lat <= pi2; lat += step) {
			const auto& point3d = Vector{ std::sin(lat), std::cos(lat), 0.f } * 15.f;
			ImVec2 point2d;
			if (Helpers::worldToScreen(peekPosition + point3d, point2d))
				points.push_back(point2d);
		}

		const ImU32 color = (Helpers::calculateColor(config->misc.autoPeek));
		auto flags_backup = drawList->Flags;
		drawList->Flags |= ImDrawListFlags_AntiAliasedFill;
		drawList->AddConvexPolyFilled(points.data(), points.size(), color);
		drawList->AddPolyline(points.data(), points.size(), color, true, 2.f);
		drawList->Flags = flags_backup;
	}
}

void Misc::autoPeek(UserCmd* cmd, Vector currentViewAngles) noexcept
{
	static bool hasShot = false;

	if (!config->misc.autoPeek.enabled) {
		hasShot = false;
		peekPosition = Vector{};
		return;
	}

	if (!localPlayer)
		return;

	if (!localPlayer->isAlive()) {
		hasShot = false;
		peekPosition = Vector{};
		return;
	}

	if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP || !(localPlayer->flags() & 1))
		return;

	if (config->misc.autoPeekKey.isActive()) {
		if (peekPosition.null())
			peekPosition = localPlayer->getRenderOrigin();

		if (cmd->buttons & UserCmd::IN_ATTACK)
			hasShot = true;

		if (hasShot) {
			const float yaw = currentViewAngles.y;
			const auto difference = localPlayer->getRenderOrigin() - peekPosition;

			if (difference.length2D() > 5.0f) {
				const auto velocity = Vector{
					difference.x * std::cos(yaw / 180.0f * std::numbers::pi_v<float>) + difference.y * std::sin(yaw / 180.0f * std::numbers::pi_v<float>),
					difference.y * std::cos(yaw / 180.0f * std::numbers::pi_v<float>) - difference.x * std::sin(yaw / 180.0f * std::numbers::pi_v<float>),
					difference.z };

				cmd->forwardmove = -velocity.x * 20.f;
				cmd->sidemove = velocity.y * 20.f;
			} else {
				hasShot = false;
				peekPosition = Vector{};
				if (config->misc.autoPeekKey.keyMode == Toggle)
					config->misc.autoPeekKey.setToggleTo(false);
			}
		}
	} else {
		hasShot = false;
		peekPosition = Vector{};
	}
}

void Misc::forceRelayCluster() noexcept
{
	constexpr std::array dataCentersList = { "", "syd", "vie", "gru", "scl", "dxb", "par", "fra", "hkg",
	"maa", "bom", "tyo", "lux", "ams", "limc", "man", "waw", "sgp", "jnb",
	"mad", "sto", "lhr", "atl", "eat", "ord", "lax", "mwh", "okc", "sea", "iad" };

	*memory->relayCluster = dataCentersList[config->misc.forceRelayCluster];
}

void Misc::jumpBug(UserCmd* cmd) noexcept
{
	if (!config->misc.jumpBug || !config->misc.jumpBugKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP)
		return;

	if (!(EnginePrediction::getFlags() & 1) && (localPlayer->flags() & 1)) {
		if (config->misc.fastDuck)
			cmd->buttons &= ~UserCmd::IN_BULLRUSH;
		cmd->buttons |= UserCmd::IN_DUCK;
	}

	if (localPlayer->flags() & 1)
		cmd->buttons &= ~UserCmd::IN_JUMP;
}

std::vector<conCommandBase*> dev;
std::vector<conCommandBase*> hidden;

void Misc::initHiddenCvars() noexcept
{
	conCommandBase* iterator = **reinterpret_cast<conCommandBase***>(interfaces->cvar + 0x34);

	for (auto c = iterator->next; c != nullptr; c = c->next) {
		conCommandBase* cmd = c;

		if (cmd->flags & DEVELOPMENTONLY)
			dev.push_back(cmd);

		if (cmd->flags & HIDDEN)
			hidden.push_back(cmd);

	}
}

void Misc::unlockHiddenCvars() noexcept
{
	static bool toggle = true;

	if (config->misc.unhideConvars == toggle)
		return;

	if (config->misc.unhideConvars) {
		for (unsigned x = 0; x < dev.size(); x++)
			dev.at(x)->flags &= ~DEVELOPMENTONLY;

		for (unsigned x = 0; x < hidden.size(); x++)
			hidden.at(x)->flags &= ~HIDDEN;

	}
	if (!config->misc.unhideConvars) {
		for (unsigned x = 0; x < dev.size(); x++)
			dev.at(x)->flags |= DEVELOPMENTONLY;

		for (unsigned x = 0; x < hidden.size(); x++)
			hidden.at(x)->flags |= HIDDEN;
	}

	toggle = config->misc.unhideConvars;

}

void Misc::fakeDuck(UserCmd* cmd, bool& sendPacket) noexcept
{
	if (!config->misc.fakeduck || !config->misc.fakeduckKey.isActive())
		return;

	if (const auto gameRules = *memory->gameRules; gameRules) {
		static auto gameType = interfaces->cvar->findVar("game_type");
		static auto gameMode = interfaces->cvar->findVar("game_mode");
		if ((gameType->getInt() != 0 || gameMode->getInt() != 1) && gameRules->isValveDS())
			return;
	}

	if (!localPlayer || !localPlayer->isAlive() || !(localPlayer->flags() & 1))
		return;

	const auto netChannel = interfaces->engine->getNetworkChannel();
	if (!netChannel)
		return;

	sendPacket = false;
	static bool down = false;
	switch (cmd->tickCount % 14) {
	case 0:
		down = true;
		break;
	case 6:
		sendPacket = true;
		break;
	case 7:
		down = false;
		break;
	}
	if (netChannel->chokedPackets > 8)
		cmd->buttons &= ~UserCmd::IN_ATTACK;
	if (down)
		cmd->buttons |= UserCmd::IN_DUCK;
	else
		cmd->buttons &= ~UserCmd::IN_DUCK;
}


void Misc::edgejump(UserCmd* cmd) noexcept
{
	if (!config->misc.edgeJump || !config->misc.edgeJumpKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP)
		return;

	if ((EnginePrediction::getFlags() & 1) && !(localPlayer->flags() & 1))
		cmd->buttons |= UserCmd::IN_JUMP;
}

void Misc::slowwalk(UserCmd* cmd) noexcept
{
	if (!config->misc.slowwalk || !config->misc.slowwalkKey.isActive())
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon)
		return;

	const auto weaponData = activeWeapon->getWeaponData();
	if (!weaponData)
		return;

	const float maxSpeed = config->misc.slowwalkAmnt ? config->misc.slowwalkAmnt : (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

	if (cmd->forwardmove && cmd->sidemove) {
		const float maxSpeedRoot = maxSpeed * static_cast<float>(M_SQRT1_2);
		cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
		cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
	} else if (cmd->forwardmove) {
		cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeed : maxSpeed;
	} else if (cmd->sidemove) {
		cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeed : maxSpeed;
	}
}

void Misc::inverseRagdollGravity() noexcept
{
	static auto ragdollGravity = interfaces->cvar->findVar("cl_ragdoll_gravity");
	ragdollGravity->setValue(config->visuals.inverseRagdollGravity ? -600 : 600);
}

void Misc::updateClanTag(bool tagChanged) noexcept
{
	static std::string clanTag;
	static std::string origTag;
	static bool flip = false;
	static std::size_t count = 0;

	if (tagChanged) {
		clanTag = config->misc.clanTag;
		origTag = config->misc.clanTag;
		count = 0;
		flip = false;
		if (!origTag.empty() && origTag.front() != ' ' && origTag.back() != ' ') {
			origTag.push_back(' ');
		}
		return;
	}

	static auto lastTime = 0.0f;

	if (config->misc.customClanTag) {
		if ((memory->globalVars->realtime - interfaces->engine->getNetworkChannel()->getLatency(0) - lastTime < config->misc.tagUpdateInterval) && (!tagChanged || origTag.empty()) && config->misc.tagType != 5)
			return;

		const int offset = Helpers::utf8SeqLen(origTag[0]);

		switch (config->misc.tagType) {
		case 1: // Normal animation
			if (offset != -1 && static_cast<std::size_t>(offset) <= clanTag.length())
				std::ranges::rotate(clanTag, clanTag.begin() + offset);
			break;

		case 2: // Auto reverse animation
			if (origTag.length() > clanTag.length() && !flip)
				clanTag = origTag.substr(0, clanTag.length() + offset);
			else if (clanTag.length() == 0)
				flip = false;
			else if (origTag.length() <= clanTag.length() || flip) {
				flip = true;
				clanTag = origTag.substr(0, clanTag.length() - offset);
			}
			break;

		case 3: // Reverse auto reverse animation?
			if (origTag.length() > clanTag.length() && !flip)
				clanTag = origTag.substr(0, clanTag.length() + offset);
			else if (clanTag.length() == 0) {
				flip = false;
				count = 0;
			} else if (origTag.length() <= clanTag.length() || flip) {
				flip = true;
				clanTag = origTag.substr(0 + (offset * ++count), origTag.length());
			}
			break;

		case 4: // Custom animation
			clanTag = config->misc.tagAnimationSteps[count];
			if (count < config->misc.tagAnimationSteps.size() - 1u)
				count++;
			else
				count = 0;
			break;

		case 5: // Clock
			if (memory->globalVars->realtime - lastTime < 1.0f)
				return;

			const auto time = std::time(nullptr);
			const auto localTime = std::localtime(&time);

			clanTag = std::format("[{:02d}:{:02d}:{:02d}]", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
			break;
		}

		lastTime = memory->globalVars->realtime;
		memory->setClanTag(clanTag.c_str(), clanTag.c_str());
	} else {
		memory->setClanTag("", "");
	}
}

const bool anyActiveKeybinds() noexcept
{
	const bool rageBot = config->ragebotKey.canShowKeybind();
	const bool minDamageOverride = config->minDamageOverrideKey.canShowKeybind();
	const bool fakeAngle = config->invert.canShowKeybind();
	const bool antiAimAutoDirection = config->autoDirection.canShowKeybind();
	const bool antiAimManualForward = config->manualForward.canShowKeybind();
	const bool antiAimManualBackward = config->manualBackward.canShowKeybind();
	const bool antiAimManualRight = config->manualRight.canShowKeybind();
	const bool antiAimManualLeft = config->manualLeft.canShowKeybind();
	const bool legitAntiAim = config->legitAntiAim.enabled && config->legitAntiAim.invert.canShowKeybind();
	const bool doubletap = config->tickbase.doubletap.canShowKeybind();
	const bool hideshots = config->tickbase.hideshots.canShowKeybind();
	const bool legitBot = config->legitbotKey.canShowKeybind();
	const bool triggerBot = config->triggerbotKey.canShowKeybind();
	const bool glow = config->glowKey.canShowKeybind();
	const bool chams = config->chamsKey.canShowKeybind();
	const bool esp = config->streamProofESP.key.canShowKeybind();

	const bool zoom = config->visuals.zoom && config->visuals.zoomKey.canShowKeybind();
	const bool thirdperson = config->visuals.thirdperson && config->visuals.thirdpersonKey.canShowKeybind();
	const bool freeCam = config->visuals.freeCam && config->visuals.freeCamKey.canShowKeybind();

	const bool blockbot = config->misc.blockBot && config->misc.blockBotKey.canShowKeybind();
	const bool edgejump = config->misc.edgeJump && config->misc.edgeJumpKey.canShowKeybind();
	const bool minijump = config->misc.miniJump && config->misc.miniJumpKey.canShowKeybind();
	const bool jumpBug = config->misc.jumpBug && config->misc.jumpBugKey.canShowKeybind();
	const bool edgebug = config->misc.edgeBug && config->misc.edgeBugKey.canShowKeybind();
	const bool autoPixelSurf = config->misc.autoPixelSurf && config->misc.autoPixelSurfKey.canShowKeybind();
	const bool slowwalk = config->misc.slowwalk && config->misc.slowwalkKey.canShowKeybind();
	const bool fakeduck = config->misc.fakeduck && config->misc.fakeduckKey.canShowKeybind();
	const bool autoPeek = config->misc.autoPeek.enabled && config->misc.autoPeekKey.canShowKeybind();
	const bool prepareRevolver = config->misc.prepareRevolver && config->misc.prepareRevolverKey.canShowKeybind();

	return rageBot || minDamageOverride || fakeAngle || antiAimAutoDirection || antiAimManualForward || antiAimManualBackward || antiAimManualRight || antiAimManualLeft
		|| doubletap || hideshots
		|| legitAntiAim || legitBot || triggerBot || chams || glow || esp
		|| zoom || thirdperson || freeCam || blockbot || edgejump || minijump || jumpBug || edgebug || autoPixelSurf || slowwalk || fakeduck || autoPeek || prepareRevolver;
}

void Misc::showKeybinds() noexcept
{
	if (!config->misc.keybindList.enabled)
		return;

	if (!anyActiveKeybinds() && !gui->isOpen())
		return;

	if (config->misc.keybindList.pos != ImVec2{}) {
		ImGui::SetNextWindowPos(config->misc.keybindList.pos);
		config->misc.keybindList.pos = {};
	}

	ImGui::SetNextWindowSize({ 250.f, 0.f }, ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints({ 250.f, 0.f }, { 250.f, FLT_MAX });

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (!gui->isOpen())
		windowFlags |= ImGuiWindowFlags_NoInputs;

	if (config->misc.keybindList.noTitleBar)
		windowFlags |= ImGuiWindowFlags_NoTitleBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, { 0.5f, 0.5f });
	ImGui::Begin("Keybind list", nullptr, windowFlags);
	ImGui::PopStyleVar();

	config->ragebotKey.showKeybind();
	config->minDamageOverrideKey.showKeybind();
	config->invert.showKeybind();

	config->autoDirection.showKeybind();
	config->manualForward.showKeybind();
	config->manualBackward.showKeybind();
	config->manualRight.showKeybind();
	config->manualLeft.showKeybind();

	config->tickbase.doubletap.showKeybind();
	config->tickbase.hideshots.showKeybind();

	if (config->legitAntiAim.enabled)
		config->legitAntiAim.invert.showKeybind();

	config->legitbotKey.showKeybind();
	config->triggerbotKey.showKeybind();
	config->chamsKey.showKeybind();
	config->glowKey.showKeybind();
	config->streamProofESP.key.showKeybind();

	if (config->visuals.zoom)
		config->visuals.zoomKey.showKeybind();
	if (config->visuals.thirdperson)
		config->visuals.thirdpersonKey.showKeybind();
	if (config->visuals.freeCam)
		config->visuals.freeCamKey.showKeybind();

	if (config->misc.blockBot)
		config->misc.blockBotKey.showKeybind();
	if (config->misc.edgeJump)
		config->misc.edgeJumpKey.showKeybind();
	if (config->misc.miniJump)
		config->misc.miniJumpKey.showKeybind();
	if (config->misc.jumpBug)
		config->misc.jumpBugKey.showKeybind();
	if (config->misc.edgeBug)
		config->misc.edgeBugKey.showKeybind();
	if (config->misc.autoPixelSurf)
		config->misc.autoPixelSurfKey.showKeybind();
	if (config->misc.slowwalk)
		config->misc.slowwalkKey.showKeybind();
	if (config->misc.fakeduck)
		config->misc.fakeduckKey.showKeybind();
	if (config->misc.autoPeek.enabled)
		config->misc.autoPeekKey.showKeybind();
	if (config->misc.prepareRevolver)
		config->misc.prepareRevolverKey.showKeybind();

	ImGui::End();
}

void Misc::spectatorList() noexcept
{
	if (!config->misc.spectatorList.enabled)
		return;

	GameData::Lock lock;

	const auto& observers = GameData::observers();

	if (std::ranges::none_of(observers, [](const auto& obs) { return obs.targetIsLocalPlayer; }) && !gui->isOpen())
		return;

	if (config->misc.spectatorList.pos != ImVec2{}) {
		ImGui::SetNextWindowPos(config->misc.spectatorList.pos);
		config->misc.spectatorList.pos = {};
	}

	ImGui::SetNextWindowSize({ 250.f, 0.f }, ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints({ 250.f, 0.f }, { 250.f, FLT_MAX });

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (!gui->isOpen())
		windowFlags |= ImGuiWindowFlags_NoInputs;

	if (config->misc.spectatorList.noTitleBar)
		windowFlags |= ImGuiWindowFlags_NoTitleBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, { 0.5f, 0.5f });
	ImGui::Begin("Spectator list", nullptr, windowFlags);
	ImGui::PopStyleVar();

	for (const auto& observer : observers) {
		if (!observer.targetIsLocalPlayer)
			continue;

		if (const auto it = std::ranges::find(GameData::players(), observer.playerHandle, &PlayerData::handle); it != GameData::players().cend()) {

			auto obsMode{ "" };

			switch (it->observerMode) {
			case ObsMode::InEye:
				obsMode = "1st";
				break;
			case ObsMode::Chase:
				obsMode = "3rd";
				break;
			case ObsMode::Roaming:
				obsMode = "Freecam";
				break;
			default:
				continue;
			}

			if (const auto texture = it->getAvatarTexture()) {
				const auto textSize = ImGui::CalcTextSize(it->name.c_str());
				ImGui::Image(texture, ImVec2(textSize.y, textSize.y), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.3f));
				ImGui::SameLine();
				ImGui::TextWrapped("%s - %s", it->name.c_str(), obsMode);
			}
		}
	}

	ImGui::End();
}

void Misc::noscopeCrosshair() noexcept
{
	static auto nozoom_crosshair = interfaces->cvar->findVar("weapon_debug_spread_show");

	GameData::Lock lock;
	if (const auto& local = GameData::local(); !config->misc.noscopeCrosshair || !local.exists || !local.alive || !local.noScope)
		nozoom_crosshair->setValue(0);
	else
		nozoom_crosshair->setValue(3);
}

void Misc::recoilCrosshair() noexcept
{
	static auto recoilCrosshair = interfaces->cvar->findVar("cl_crosshair_recoil");
	recoilCrosshair->setValue(config->misc.recoilCrosshair ? 1 : 0);
}

void Misc::watermark() noexcept
{
	if (!config->misc.watermark.enabled)
		return;

	if (config->misc.watermark.pos != ImVec2{}) {
		ImGui::SetNextWindowPos(config->misc.watermark.pos);
		config->misc.watermark.pos = {};
	}

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
	if (!gui->isOpen())
		windowFlags |= ImGuiWindowFlags_NoInputs;

	ImGui::SetNextWindowBgAlpha(0.3f);
	ImGui::Begin("Watermark", nullptr, windowFlags);

	static auto frame_rate = 1.0f;
	frame_rate = 0.9f * frame_rate + 0.1f * memory->globalVars->absoluteFrameTime;
	GameData::Lock lock;
	const auto& [exists, alive, inReload, shooting, noScope, nextWeaponAttack, fov, handle, flashDuration, aimPunch, origin, inaccuracy, team, velocityModifier] { GameData::local() };
	ImGui::Text("Osiris%s | %d fps | %d ms%s | %s%s%s",
#ifdef _DEBUG
		" [DEBUG]",
#else
		"",
#endif
		frame_rate != 0.0f ? static_cast<int>(1 / frame_rate) : 0,
		GameData::getNetOutgoingLatency(),
		Fakelag::latest_choked_packets != 0 ? std::format(" ({}t choke)", Fakelag::latest_choked_packets).c_str() : "",
		!interfaces->engine->isInGame() ? "NONE" : team == Team::Spectators ? "SPEC" : team == Team::TT ? "T" : team == Team::CT ? "CT" : "NONE",
		inReload ? " | RELOADING" : "",
		noScope ? " | NO SCOPE" : "");
	ImGui::End();
}

void Misc::prepareRevolver(UserCmd* cmd) noexcept
{
	if (!config->misc.prepareRevolver || !config->misc.prepareRevolverKey.isActive())
		return;

	if (!localPlayer)
		return;

	if (cmd->buttons & UserCmd::IN_ATTACK)
		return;

	constexpr float revolverPrepareTime{ 0.234375f };

	if (auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver) {
		const auto time = memory->globalVars->serverTime();

		if (localPlayer->nextAttack() > time)
			return;

		cmd->buttons &= ~UserCmd::IN_ATTACK2;

		static auto readyTime = time + revolverPrepareTime;
		if (activeWeapon->nextPrimaryAttack() <= time) {
			if (readyTime <= time) {
				if (activeWeapon->nextSecondaryAttack() <= time)
					readyTime = time + revolverPrepareTime;
				else
					cmd->buttons |= UserCmd::IN_ATTACK2;
			} else
				cmd->buttons |= UserCmd::IN_ATTACK;
		} else
			readyTime = time + revolverPrepareTime;
	}
}

void Misc::fastPlant(UserCmd* cmd) noexcept
{
	if (!config->misc.fastPlant)
		return;

	static auto plantAnywhere = interfaces->cvar->findVar("mp_plant_c4_anywhere");

	if (plantAnywhere->getInt())
		return;

	if (!localPlayer || !localPlayer->isAlive() || (localPlayer->inBombZone() && localPlayer->flags() & 1))
		return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || activeWeapon->getClientClass()->classId != ClassId::C4)
		return;

	cmd->buttons &= ~UserCmd::IN_ATTACK;

	constexpr auto doorRange = 200.0f;

	Trace trace;
	const auto startPos = localPlayer->getEyePosition();
	const auto endPos = startPos + Vector::fromAngle(cmd->viewangles) * doorRange;
	interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);

	if (!trace.entity || trace.entity->getClientClass()->classId != ClassId::PropDoorRotating)
		cmd->buttons &= ~UserCmd::IN_USE;
}

void Misc::fastStop(UserCmd* cmd) noexcept
{
	if (!config->misc.fastStop)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER || !(localPlayer->flags() & 1) || cmd->buttons & UserCmd::IN_JUMP)
		return;

	if (cmd->buttons & (UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT | UserCmd::IN_FORWARD | UserCmd::IN_BACK))
		return;

	const auto& velocity = localPlayer->velocity();
	const auto speed = velocity.length2D();
	if (speed < 15.0f)
		return;

	Vector direction = velocity.toAngle();
	direction.y = cmd->viewangles.y - direction.y;

	const auto negatedDirection = Vector::fromAngle(direction) * -speed;
	cmd->forwardmove = negatedDirection.x;
	cmd->sidemove = negatedDirection.y;
}

void Misc::drawBombTimer() noexcept
{
	if (!config->misc.bombTimer.enabled)
		return;

	if (!localPlayer)
		return;

	GameData::Lock lock;
	const auto& plantedC4 = GameData::plantedC4();
	if (plantedC4.blowTime == 0.0f && !gui->isOpen())
		return;

	if (!gui->isOpen()) {
		ImGui::SetNextWindowBgAlpha(0.3f);
	}

	static float windowWidth = 200.0f;
	ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 200.0f) / 2.0f, 60.0f }, ImGuiCond_Once);
	ImGui::SetNextWindowSize({ windowWidth, 0 }, ImGuiCond_Once);

	if (!gui->isOpen())
		ImGui::SetNextWindowSize({ windowWidth, 0 });

	ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
	ImGui::Begin("Bomb Timer", nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration));

	std::ostringstream ss; ss << "Bomb on " << (!plantedC4.bombsite ? 'A' : 'B') << " : " << std::fixed << std::showpoint << std::setprecision(3) << std::max(plantedC4.blowTime - memory->globalVars->currenttime, 0.0f) << " s";

	ImGui::textUnformattedCentered(ss.str().c_str());

	bool drawDamage = true;

	auto targetEntity = !localPlayer->isAlive() ? localPlayer->getObserverTarget() : localPlayer.get();
	auto bombEntity = interfaces->entityList->getEntityFromHandle(plantedC4.bombHandle);

	if (!bombEntity || bombEntity->isDormant() || bombEntity->getClientClass()->classId != ClassId::PlantedC4)
		drawDamage = false;

	if (!targetEntity || targetEntity->isDormant())
		drawDamage = false;

	if (drawDamage) {
		constexpr double defaultBombDamage = 500.; // This is not constant on every map, and so it might not work
		constexpr double defaultBombRadius = defaultBombDamage * 3.5; // This is not constant either
		constexpr double sigma = defaultBombRadius / 3.;

		const double distanceToLocalPlayer = ((bombEntity->viewOffset() + bombEntity->origin()) - (targetEntity->viewOffset() + targetEntity->origin())).length();
		const double gaussianFalloff = std::exp(-distanceToLocalPlayer * distanceToLocalPlayer / (2. * sigma * sigma));

		float bombDamage = static_cast<float>(defaultBombDamage * gaussianFalloff);

		if (const int armorValue = targetEntity->armor(); armorValue && armorValue > 0)
			AimbotFunction::calculateArmorDamage(.5f, armorValue, targetEntity->hasHeavyArmor(), bombDamage);

		const int health = targetEntity->health();
		const int finalBombDamage = static_cast<int>(std::floor(bombDamage));
		if (health && health <= finalBombDamage) {
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
			ImGui::textUnformattedCentered("Lethal");
			ImGui::PopStyleColor();
		} else {
			std::ostringstream text; text << "Damage: " << std::clamp(finalBombDamage, 0, health - 1);
			const auto color = Helpers::healthColor(std::clamp(1.f - static_cast<float>(finalBombDamage) / health, 0.f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::textUnformattedCentered(text.str().c_str());
			ImGui::PopStyleColor();
		}
	}

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Helpers::calculateColor(config->misc.bombTimer));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f });
	ImGui::progressBarFullWidth((plantedC4.blowTime - memory->globalVars->currenttime) / plantedC4.timerLength, 5.0f);

	if (plantedC4.defuserHandle != -1) {
		const bool canDefuse = plantedC4.blowTime >= plantedC4.defuseCountDown;

		if (plantedC4.defuserHandle == GameData::local().handle) {
			if (canDefuse) {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
				ImGui::textUnformattedCentered("You can defuse!");
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				ImGui::textUnformattedCentered("You can not defuse!");
			}
			ImGui::PopStyleColor();
		} else if (const auto defusingPlayer = GameData::playerByHandle(plantedC4.defuserHandle)) {
			std::ostringstream ss; ss << defusingPlayer->name << " is defusing: " << std::fixed << std::showpoint << std::setprecision(3) << std::max(plantedC4.defuseCountDown - memory->globalVars->currenttime, 0.0f) << " s";

			ImGui::textUnformattedCentered(ss.str().c_str());

			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, canDefuse ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
			ImGui::progressBarFullWidth((plantedC4.defuseCountDown - memory->globalVars->currenttime) / plantedC4.defuseLength, 5.0f);
			ImGui::PopStyleColor();
		}
	}

	windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;

	ImGui::PopStyleColor(2);
	ImGui::End();
}

void Misc::hurtIndicator() noexcept
{
	if (!config->misc.hurtIndicator.enabled)
		return;

	GameData::Lock lock;
	const auto& local = GameData::local();
	if ((!local.exists || !local.alive) && !gui->isOpen())
		return;

	if (local.velocityModifier >= 0.99f && !gui->isOpen())
		return;

	if (!gui->isOpen()) {
		ImGui::SetNextWindowBgAlpha(0.3f);
	}

	static float windowWidth = 140.0f;
	ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 140.0f) / 2.0f, 260.0f }, ImGuiCond_Once);
	ImGui::SetNextWindowSize({ windowWidth, 0 }, ImGuiCond_Once);

	if (!gui->isOpen())
		ImGui::SetNextWindowSize({ windowWidth, 0 });

	ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
	ImGui::Begin("Hurt Indicator", nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration));

	std::ostringstream ss; ss << "Slowed down " << static_cast<int>(local.velocityModifier * 100.f) << "%";
	ImGui::textUnformattedCentered(ss.str().c_str());

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Helpers::calculateColor(config->misc.hurtIndicator));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f });
	ImGui::progressBarFullWidth(local.velocityModifier, 1.0f);

	windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;

	ImGui::PopStyleColor(2);
	ImGui::End();
}

void Misc::yawIndicator(ImDrawList* drawList) noexcept
{
	if (!config->misc.yawIndicator.enabled)
		return;
	{
		GameData::Lock lock;
		if (const auto& [exists, alive, inReload, shooting, noScope, nextWeaponAttack, fov, handle, flashDuration, aimPunch, origin, inaccuracy, team, velocityModifier] { GameData::local() }; !exists || !alive)
			return;
	}
	const ImVec2 pos{ ImGui::GetIO().DisplaySize / 2 };
	const ImU32 col{ Helpers::calculateColor(static_cast<Color4>(config->misc.yawIndicator)) };
	if (config->manualForward.isToggled())
		drawList->AddTriangleFilled(pos + ImVec2{ -20, -20 }, pos + ImVec2{ 20, -20 }, pos + ImVec2{ 0, -50 }, col);
	if (config->manualBackward.isToggled())
		drawList->AddTriangleFilled(pos + ImVec2{ -20, 20 }, pos + ImVec2{ 20, 20 }, pos + ImVec2{ 0, 50 }, col);
	if (config->manualRight.isToggled() || (AntiAim::auto_direction_yaw == 1 && config->autoDirection.isToggled()))
		drawList->AddTriangleFilled(pos + ImVec2{ 20, 20 }, pos + ImVec2{ 20, -20 }, pos + ImVec2{ 50, 0 }, col);
	if (config->manualLeft.isToggled() || (AntiAim::auto_direction_yaw == -1 && config->autoDirection.isToggled()))
		drawList->AddTriangleFilled(pos + ImVec2{ -20, 20 }, pos + ImVec2{ -20, -20 }, pos + ImVec2{ -50, 0 }, col);
}

void Misc::stealNames() noexcept
{
	if (!config->misc.nameStealer)
		return;

	if (!localPlayer)
		return;

	static std::vector<int> stolenIds;

	for (int i = 1; i <= memory->globalVars->maxClients; ++i) {
		const auto entity = interfaces->entityList->getEntity(i);

		if (!entity || entity == localPlayer.get())
			continue;

		PlayerInfo playerInfo;
		if (!interfaces->engine->getPlayerInfo(entity->index(), playerInfo))
			continue;

		if (playerInfo.fakeplayer || std::find(stolenIds.cbegin(), stolenIds.cend(), playerInfo.userId) != stolenIds.cend())
			continue;

		if (changeName(false, (std::string{ playerInfo.name } + '\x1').c_str(), 1.0f))
			stolenIds.push_back(playerInfo.userId);

		return;
	}
	stolenIds.clear();
}

void Misc::disablePanoramablur() noexcept
{
	static auto blur = interfaces->cvar->findVar("@panorama_disable_blur");
	blur->setValue(config->misc.disablePanoramablur);
}

bool Misc::changeName(bool reconnect, const char* newName, float delay) noexcept
{
	static auto exploitInitialized{ false };

	static auto name{ interfaces->cvar->findVar("name") };

	if (reconnect) {
		exploitInitialized = false;
		return false;
	}

	if (!exploitInitialized && interfaces->engine->isInGame()) {
		if (PlayerInfo playerInfo; localPlayer && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (!strcmp(playerInfo.name, "?empty") || !strcmp(playerInfo.name, "\n\xAD\xAD\xAD"))) {
			exploitInitialized = true;
		} else {
			name->onChangeCallbacks.size = 0;
			name->setValue("\n\xAD\xAD\xAD");
			return false;
		}
	}

	static auto nextChangeTime{ 0.0f };
	if (nextChangeTime <= memory->globalVars->realtime) {
		name->setValue(newName);
		nextChangeTime = memory->globalVars->realtime + delay;
		return true;
	}
	return false;
}

void Misc::bunnyHop(UserCmd* cmd) noexcept
{
	if (config->misc.jumpBug && config->misc.jumpBugKey.isActive())
		return;

	static auto lastJumped = false;
	static auto shouldFake = false;

	if (!localPlayer || !localPlayer->isAlive() || !config->misc.bunnyHop)
		return;

	if (localPlayer->moveType() == MoveType::LADDER || localPlayer->moveType() == MoveType::NOCLIP)
		return;

	if (!lastJumped && shouldFake) {
		shouldFake = false;
		cmd->buttons |= UserCmd::IN_JUMP;
	} else if (cmd->buttons & UserCmd::IN_JUMP) {
		if (localPlayer->flags() & 1) {
			lastJumped = true;
			shouldFake = true;
		} else {
			cmd->buttons &= ~UserCmd::IN_JUMP;
			lastJumped = false;
		}
	} else {
		lastJumped = false;
		shouldFake = false;
	}
}

void Misc::fixTabletSignal() noexcept
{
	if (config->misc.fixTabletSignal && localPlayer) {
		if (auto activeWeapon{ localPlayer->getActiveWeapon() }; activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet)
			activeWeapon->tabletReceptionIsBlocked() = false;
	}
}

void Misc::killfeedChanger(GameEvent& event) noexcept
{
	if (!config->misc.killfeedChanger.enabled)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
		return;

	if (config->misc.killfeedChanger.headshot)
		event.setInt("headshot", 1);

	if (config->misc.killfeedChanger.dominated)
		event.setInt("Dominated", 1);

	if (config->misc.killfeedChanger.revenge)
		event.setInt("Revenge", 1);

	if (config->misc.killfeedChanger.penetrated)
		event.setInt("penetrated", 1);

	if (config->misc.killfeedChanger.noscope)
		event.setInt("noscope", 1);

	if (config->misc.killfeedChanger.thrusmoke)
		event.setInt("thrusmoke", 1);

	if (config->misc.killfeedChanger.attackerblind)
		event.setInt("attackerblind", 1);
}

void Misc::killMessage(GameEvent& event) noexcept
{
	if (!config->misc.killMessage)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
		return;

	std::string cmd = "say \"";
	cmd += config->misc.killMessages[std::uniform_int_distribution<int>(0, config->misc.killMessages.size() - 1)(PCG::generator)];
	cmd += '"';
	interfaces->engine->clientCmdUnrestricted(cmd.c_str());
}

void Misc::fixMovement(UserCmd* cmd, float yaw) noexcept
{
	float oldYaw = yaw + (yaw < 0.0f ? 360.0f : 0.0f);
	float newYaw = cmd->viewangles.y + (cmd->viewangles.y < 0.0f ? 360.0f : 0.0f);
	float yawDelta = newYaw < oldYaw ? std::abs(newYaw - oldYaw) : 360.0f - std::abs(newYaw - oldYaw);
	yawDelta = 360.0f - yawDelta;

	const float forwardmove = cmd->forwardmove;
	const float sidemove = cmd->sidemove;
	cmd->forwardmove = std::cos(Helpers::deg2rad(yawDelta)) * forwardmove + std::cos(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;
	cmd->sidemove = std::sin(Helpers::deg2rad(yawDelta)) * forwardmove + std::sin(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;
}

void Misc::antiAfkKick(UserCmd* cmd) noexcept
{
	if (config->misc.antiAfkKick && cmd->commandNumber % 2)
		cmd->buttons |= 1 << 27;
}

void Misc::fixAnimationLOD(FrameStage stage) noexcept
{
	if (stage != FrameStage::RENDER_START)
		return;

	if (!localPlayer)
		return;

	for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
		Entity* entity = interfaces->entityList->getEntity(i);
		if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive())
			continue;
		*reinterpret_cast<int*>(entity + 0xA28) = 0;
		*reinterpret_cast<int*>(entity + 0xA30) = memory->globalVars->framecount;
	}
}

void Misc::autoPistol(UserCmd* cmd) noexcept
{
	if (config->misc.autoPistol && localPlayer) {
		const auto activeWeapon = localPlayer->getActiveWeapon();
		if (activeWeapon && activeWeapon->isPistol() && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime()) {
			if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
				cmd->buttons &= ~UserCmd::IN_ATTACK2;
			else
				cmd->buttons &= ~UserCmd::IN_ATTACK;
		}
	}
}

void Misc::autoReload(UserCmd* cmd) noexcept
{
	if (config->misc.autoReload && localPlayer) {
		const auto activeWeapon = localPlayer->getActiveWeapon();
		if (activeWeapon && getWeaponIndex(activeWeapon->itemDefinitionIndex2()) && !activeWeapon->clip())
			cmd->buttons &= ~(UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2);
	}
}

void Misc::revealRanks(UserCmd* cmd) noexcept
{
	if (config->misc.revealRanks && cmd->buttons & UserCmd::IN_SCORE)
		interfaces->client->dispatchUserMessage(50, 0, 0, nullptr);
}

void Misc::autoStrafe(UserCmd* cmd, Vector& currentViewAngles) noexcept
{
	if (!config->misc.autoStrafe)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if ((EnginePrediction::getFlags() & 1) || localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
		return;

	const float speed = localPlayer->velocity().length2D();
	if (speed < 5.0f)
		return;

	constexpr auto angleDiffRad = [](float a1, float a2) noexcept {
		constexpr float pi = std::numbers::pi_v<float>;
		float delta = std::isfinite(a1 - a2) ? std::remainder(a1 - a2, 360.0f) : 0.0f;
		if (a1 > a2) {
			if (delta >= pi)
				delta -= pi * 2;
		} else {
			if (delta <= -pi)
				delta += pi * 2;
		}

		return delta;
	};

	constexpr auto perfectDelta = [](float speed) noexcept {
		static auto speedVar = interfaces->cvar->findVar("sv_maxspeed");
		static auto airVar = interfaces->cvar->findVar("sv_airaccelerate");
		static auto wishVar = interfaces->cvar->findVar("sv_air_max_wishspeed");

		const auto term = wishVar->getFloat() / airVar->getFloat() / speedVar->getFloat() * 100.0f / speed;

		if (term < 1.0f && term > -1.0f)
			return std::acos(term);

		return 0.0f;
	};

	if (const float pDelta = perfectDelta(speed); pDelta) {
		const float yaw = Helpers::deg2rad(cmd->viewangles.y);
		const float velDir = std::atan2(EnginePrediction::getVelocity().y, EnginePrediction::getVelocity().x) - yaw;
		const float wishAng = std::atan2(-cmd->sidemove, cmd->forwardmove);
		const float delta = angleDiffRad(velDir, wishAng);

		float moveDir = delta < 0.0f ? velDir + pDelta : velDir - pDelta;

		cmd->forwardmove = std::cos(moveDir) * 450.0f;
		cmd->sidemove = -std::sin(moveDir) * 450.0f;
	}
}

void Misc::removeCrouchCooldown(UserCmd* cmd) noexcept
{
	if (const auto gameRules = *memory->gameRules; gameRules) {
		static auto gameType = interfaces->cvar->findVar("game_type");
		static auto gameMode = interfaces->cvar->findVar("game_mode");
		if ((gameType->getInt() != 0 || gameMode->getInt() != 1) && gameRules->isValveDS())
			return;
	}

	if (config->misc.fastDuck)
		cmd->buttons |= UserCmd::IN_BULLRUSH;
}

void Misc::moonwalk(UserCmd* cmd, bool& send_packet) noexcept
{
	const auto net_channel{ interfaces->engine->getNetworkChannel() };
	if (!net_channel)
		return;
	if (config->misc.moonwalk && localPlayer && localPlayer->moveType() != MoveType::LADDER)
		if (!config->misc.leg_break || !send_packet)
			cmd->buttons ^= UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT;
		else if (std::abs(cmd->forwardmove) > 3.f)
			cmd->buttons |= UserCmd::IN_BACK;
		else return;
}

void Misc::playHitSound(GameEvent& event) noexcept
{
	if (!config->misc.hitSound)
		return;

	if (!localPlayer)
		return;

	if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
		return;

	constexpr std::array hitSounds{
		"play physics/metal/metal_solid_impact_bullet2",
			"play buttons/arena_switch_press_02",
			"play training/timer_bell",
			"play physics/glass/glass_impact_bullet1"
	};

	if (static_cast<std::size_t>(config->misc.hitSound - 1) < hitSounds.size())
		interfaces->engine->clientCmdUnrestricted(hitSounds[config->misc.hitSound - 1]);
	else if (config->misc.hitSound == 5)
		interfaces->engine->clientCmdUnrestricted(("play " + config->misc.customHitSound).c_str());
}

void Misc::killSound(GameEvent& event) noexcept
{
	if (!config->misc.killSound)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
		return;

	constexpr std::array killSounds{
		"play physics/metal/metal_solid_impact_bullet2",
			"play buttons/arena_switch_press_02",
			"play training/timer_bell",
			"play physics/glass/glass_impact_bullet1"
	};

	if (static_cast<std::size_t>(config->misc.killSound - 1) < killSounds.size())
		interfaces->engine->clientCmdUnrestricted(killSounds[config->misc.killSound - 1]);
	else if (config->misc.killSound == 5)
		interfaces->engine->clientCmdUnrestricted(("play " + config->misc.customKillSound).c_str());
}

void Misc::autoBuy(GameEvent* event) noexcept
{
	constexpr std::array primary = {
		"",
		"mac10;buy mp9;",
		"mp7;",
		"ump45;",
		"p90;",
		"bizon;",
		"galilar;buy famas;",
		"ak47;buy m4a1;",
		"ssg08;",
		"sg556;buy aug;",
		"awp;",
		"g3sg1; buy scar20;",
		"nova;",
		"xm1014;",
		"sawedoff;buy mag7;",
		"m249; ",
		"negev;"
	};
	constexpr std::array secondary = {
		"",
		"glock;buy hkp2000;",
		"elite;",
		"p250;",
		"tec9;buy fiveseven;",
		"deagle;buy revolver;"
	};
	constexpr std::array armor = {
		"",
		"vest;",
		"vesthelm;",
	};
	constexpr std::array utility = {
		"defuser;",
		"taser;"
	};
	constexpr std::array nades = {
		"hegrenade;",
		"smokegrenade;",
		"molotov;buy incgrenade;",
		"flashbang;buy flashbang;",
		"decoy;"
	};

	if (!config->misc.autoBuy.enabled)
		return;

	std::string cmd = "";
	if (event) {
		if (config->misc.autoBuy.primaryWeapon)
			cmd += std::format("buy {}", primary[config->misc.autoBuy.primaryWeapon]);
		if (config->misc.autoBuy.secondaryWeapon)
			cmd += std::format("buy {}", secondary[config->misc.autoBuy.secondaryWeapon]);
		if (config->misc.autoBuy.armor)
			cmd += std::format("buy {}", armor[config->misc.autoBuy.armor]);

		for (size_t i = 0; i < utility.size(); i++) {
			if ((config->misc.autoBuy.utility & 1 << i) == 1 << i)
				cmd += std::format("buy {}", utility[i]);
		}

		for (size_t i = 0; i < nades.size(); i++) {
			if ((config->misc.autoBuy.grenades & 1 << i) == 1 << i)
				cmd += std::format("buy {}", nades[i]);
		}

		interfaces->engine->clientCmdUnrestricted(cmd.c_str());
	}
}

void Misc::purchaseList(GameEvent* event) noexcept
{
	static std::mutex mtx;
	std::scoped_lock _{ mtx };

	struct PlayerPurchases {
		int totalCost;
		std::unordered_map<std::string, int> items;
	};

	static std::unordered_map<int, PlayerPurchases> playerPurchases;
	static std::unordered_map<std::string, int> purchaseTotal;
	static int totalCost;

	static auto freezeEnd = 0.0f;

	if (event) {
		switch (fnv::hashRuntime(event->getName())) {
		case fnv::hash("item_purchase"):
		{
			const auto player = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(event->getInt("userid")));

			if (player && localPlayer && memory->isOtherEnemy(player, localPlayer.get())) {
				if (const auto definition = memory->itemSystem()->getItemSchema()->getItemDefinitionByName(event->getString("weapon"))) {
					auto& purchase = playerPurchases[player->handle()];
					if (const auto weaponInfo = memory->weaponSystem->getWeaponInfo(definition->getWeaponId())) {
						purchase.totalCost += weaponInfo->price;
						totalCost += weaponInfo->price;
					}
					const std::string weapon = interfaces->localize->findAsUTF8(definition->getItemBaseName());
					++purchaseTotal[weapon];
					++purchase.items[weapon];
				}
			}
			break;
		}
		case fnv::hash("round_start"):
			freezeEnd = 0.0f;
			playerPurchases.clear();
			purchaseTotal.clear();
			totalCost = 0;
			break;
		case fnv::hash("round_freeze_end"):
			freezeEnd = memory->globalVars->realtime;
			break;
		}
	} else {
		if (!config->misc.purchaseList.enabled)
			return;

		static const auto mp_buytime = interfaces->cvar->findVar("mp_buytime");

		if ((!interfaces->engine->isInGame() || freezeEnd != 0.0f && memory->globalVars->realtime > freezeEnd + (!config->misc.purchaseList.onlyDuringFreezeTime ? mp_buytime->getFloat() : 0.0f) || playerPurchases.empty() || purchaseTotal.empty()) && !gui->isOpen())
			return;

		if (config->misc.purchaseList.pos != ImVec2{}) {
			ImGui::SetNextWindowPos(config->misc.purchaseList.pos);
			config->misc.purchaseList.pos = {};
		}

		ImGui::SetNextWindowSize({ 200.0f, 200.0f }, ImGuiCond_Once);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
		if (!gui->isOpen())
			windowFlags |= ImGuiWindowFlags_NoInputs;
		if (config->misc.purchaseList.noTitleBar)
			windowFlags |= ImGuiWindowFlags_NoTitleBar;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, { 0.5f, 0.5f });
		ImGui::Begin("Purchases", nullptr, windowFlags);
		ImGui::PopStyleVar();

		if (config->misc.purchaseList.mode == PurchaseList::Details) {
			GameData::Lock lock;

			for (const auto& [handle, purchases] : playerPurchases) {
				std::string s;
				s.reserve(std::accumulate(purchases.items.begin(), purchases.items.end(), 0, [](int length, const auto& p) { return length + p.first.length() + 2; }));
				for (const auto& purchasedItem : purchases.items) {
					if (purchasedItem.second > 1)
						s += std::to_string(purchasedItem.second) + "x ";
					s += purchasedItem.first + ", ";
				}

				if (s.length() >= 2)
					s.erase(s.length() - 2);

				if (const auto player = GameData::playerByHandle(handle)) {
					if (config->misc.purchaseList.showPrices)
						ImGui::TextWrapped("%s $%d: %s", player->name.c_str(), purchases.totalCost, s.c_str());
					else
						ImGui::TextWrapped("%s: %s", player->name.c_str(), s.c_str());
				}
			}
		} else if (config->misc.purchaseList.mode == PurchaseList::Summary) {
			for (const auto& purchase : purchaseTotal)
				ImGui::TextWrapped("%d x %s", purchase.second, purchase.first.c_str());

			if (config->misc.purchaseList.showPrices && totalCost > 0) {
				ImGui::Separator();
				ImGui::TextWrapped("Total: $%d", totalCost);
			}
		}
		ImGui::End();
	}
}

void Misc::oppositeHandKnife(FrameStage stage) noexcept
{
	if (!config->misc.oppositeHandKnife)
		return;

	if (!localPlayer)
		return;

	if (stage != FrameStage::RENDER_START && stage != FrameStage::RENDER_END)
		return;

	static const auto cl_righthand = interfaces->cvar->findVar("cl_righthand");
	static bool original;

	if (stage == FrameStage::RENDER_START) {
		original = cl_righthand->getInt();

		if (const auto activeWeapon = localPlayer->getActiveWeapon()) {
			if (const auto classId = activeWeapon->getClientClass()->classId; classId == ClassId::Knife || classId == ClassId::KnifeGG)
				cl_righthand->setValue(!original);
		}
	} else {
		cl_righthand->setValue(original);
	}
}

static std::vector<std::uint64_t> reportedPlayers;
static int reportbotRound;

void Misc::runReportbot() noexcept
{
	if (!config->misc.reportbot.enabled)
		return;

	if (!localPlayer)
		return;

	static auto lastReportTime = 0.0f;

	if (lastReportTime + config->misc.reportbot.delay > memory->globalVars->realtime)
		return;

	if (reportbotRound >= config->misc.reportbot.rounds)
		return;

	for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
		const auto entity = interfaces->entityList->getEntity(i);

		if (!entity || entity == localPlayer.get())
			continue;

		if (config->misc.reportbot.target != 2 && (entity->isOtherEnemy(localPlayer.get()) ? config->misc.reportbot.target != 0 : config->misc.reportbot.target != 1))
			continue;

		PlayerInfo playerInfo;
		if (!interfaces->engine->getPlayerInfo(i, playerInfo))
			continue;

		if (playerInfo.fakeplayer || std::find(reportedPlayers.cbegin(), reportedPlayers.cend(), playerInfo.xuid) != reportedPlayers.cend())
			continue;

		std::string report;

		if (config->misc.reportbot.textAbuse)
			report += "textabuse,";
		if (config->misc.reportbot.griefing)
			report += "grief,";
		if (config->misc.reportbot.wallhack)
			report += "wallhack,";
		if (config->misc.reportbot.aimbot)
			report += "aimbot,";
		if (config->misc.reportbot.other)
			report += "speedhack,";

		if (!report.empty()) {
			memory->submitReport(std::to_string(playerInfo.xuid).c_str(), report.c_str());
			lastReportTime = memory->globalVars->realtime;
			reportedPlayers.push_back(playerInfo.xuid);
		}
		return;
	}

	reportedPlayers.clear();
	++reportbotRound;
}

void Misc::resetReportbot() noexcept
{
	reportbotRound = 0;
	reportedPlayers.clear();
}

void Misc::preserveKillfeed(bool roundStart) noexcept
{
	if (!config->misc.preserveKillfeed.enabled)
		return;

	static auto nextUpdate = 0.0f;

	if (roundStart) {
		nextUpdate = memory->globalVars->realtime + 10.0f;
		return;
	}

	if (nextUpdate > memory->globalVars->realtime)
		return;

	nextUpdate = memory->globalVars->realtime + 2.0f;

	const auto deathNotice = std::uintptr_t(memory->findHudElement(memory->hud, "CCSGO_HudDeathNotice"));
	if (!deathNotice)
		return;

	const auto deathNoticePanel = (*(UIPanel**)(*reinterpret_cast<std::uintptr_t*>(deathNotice - 20 + 88) + sizeof(std::uintptr_t)));

	const auto childPanelCount = deathNoticePanel->getChildCount();

	for (int i = 0; i < childPanelCount; ++i) {
		const auto child = deathNoticePanel->getChild(i);
		if (!child)
			continue;

		if (child->hasClass("DeathNotice_Killer") && (!config->misc.preserveKillfeed.onlyHeadshots || child->hasClass("DeathNoticeHeadShot")))
			child->setAttributeFloat("SpawnTime", memory->globalVars->currenttime);
	}
}

// TODO: Display map name on change map vote

static short yesVoteCountTT = 0;
static short noVoteCountTT = 0;
static short yesVoteCountCT = 0;
static short noVoteCountCT = 0;

void Misc::voteRevealer(GameEvent& event) noexcept
{
	if (!config->misc.revealVotes)
		return;

	const auto entity = interfaces->entityList->getEntity(event.getInt("entityid"));
	if (!entity || !entity->isPlayer())
		return;

	const auto votedYes = event.getInt("vote_option") == 0;
	const auto isLocal = localPlayer && entity == localPlayer.get();
	const auto team = entity->getTeamNumber();
	const char color = votedYes ? '\x06' : '\x07';

	short* const yes = team == Team::TT ? &yesVoteCountTT : &yesVoteCountCT;
	short* const no = team == Team::TT ? &noVoteCountTT : &noVoteCountCT;

	(votedYes ? *yes : *no)++; // cpp.cn/tip/1222

	memory->clientMode->getHudChat()->printf(0, " \x0C\u2022Osiris\u2022 %c%s\x01 voted %c%s\x01 (%hd/%hd)", isLocal ? '\x01' : color, isLocal ? "You" : entity->getPlayerName().c_str(), color, votedYes ? "Yes" : "No", *yes, *no);
}

void Misc::onVoteStart(void* data, int length) noexcept
{
	if (!config->misc.revealVotes)
		return;

	/*message CCSUsrMsg_VoteStart {
		int32 team = 1;
		int32 ent_idx = 2;
		int32 vote_type = 3;
		string disp_str = 4;
		string details_str = 5;
		string other_team_str = 6;
		bool is_yes_no_vote = 7;
		int32 entidx_target = 8;
	}*/

	constexpr auto voteName = [](int index) {
		switch (index) {
		case 0: return "kick";
		case 1: return "change the map";
		case 6: return "surrender";
		case 13: return "start a timeout";
		default: return "?";
		}
	};

	ProtoWriter msg(data, length, 8);
	if (msg.has(2) && msg.has(3)) {
		const auto ent_idx = msg.get(2).UInt32();
		const auto vote_type = msg.get(3).UInt32();
		const auto targetEntityIndex = msg.has(8) ? msg.get(8).UInt32() : 0;
		const auto targetEntity = targetEntityIndex ? interfaces->entityList->getEntity(targetEntityIndex) : nullptr;

		const auto entity = interfaces->entityList->getEntity(ent_idx);
		const auto team = entity->getTeamNumber();
		const auto isLocal = localPlayer && entity == localPlayer.get();

		(team == Team::TT ? yesVoteCountTT : yesVoteCountCT) = 0;
		(team == Team::TT ? noVoteCountTT : noVoteCountCT) = 0;

		memory->clientMode->getHudChat()->printf(0, " \x0C\u2022Osiris\u2022 %c%s\x01 called a vote to \x06%s\x01 %s", isLocal ? '\x01' : '\x06', isLocal ? "You" : entity->getPlayerName().c_str(), voteName(vote_type), targetEntity ? targetEntity->getPlayerName().c_str() : "");
	}
}

void Misc::onVotePass(void* data, int length) noexcept
{
	if (!config->misc.revealVotes)
		return;

	/*message CCSUsrMsg_VotePass {
		optional int32 team = 1;
		optional int32 vote_type = 2;
		optional string disp_str = 3;
		optional string details_str = 4;
	}*/

	ProtoWriter msg(data, length, 4);
	if (msg.has(1)) {
		const auto team = static_cast<Team>(msg.get(1).UInt32());
		const short yes = team == Team::TT ? yesVoteCountTT : yesVoteCountCT;
		const short no = team == Team::TT ? noVoteCountTT : noVoteCountCT;
		memory->clientMode->getHudChat()->printf(0, " \x0C\u2022Osiris\u2022\x01 Vote\x06 PASSED\x01 (%hd/%hd)", yes, no);
	}
}

void Misc::onVoteFail(void* data, int length) noexcept
{
	if (!config->misc.revealVotes)
		return;

	/*message CCSUsrMsg_VoteFailed {
		optional int32 team = 1;
		optional int32 reason = 2;
	}*/

	ProtoWriter msg(data, length, 2);
	if (msg.has(1)) {
		const auto team = static_cast<Team>(msg.get(1).UInt32());
		const short yes = team == Team::TT ? yesVoteCountTT : yesVoteCountCT;
		const short no = team == Team::TT ? noVoteCountTT : noVoteCountCT;
		memory->clientMode->getHudChat()->printf(0, " \x0C\u2022Osiris\u2022\x01 Vote\x07 FAILED\x01 (%hd/%hd)", yes, no);
	}
}

// ImGui::ShadeVertsLinearColorGradientKeepAlpha() modified to do interpolation in HSV
static void shadeVertsHSVColorGradientKeepAlpha(ImDrawList* draw_list, int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1)
{
	ImVec2 gradient_extent = gradient_p1 - gradient_p0;
	float gradient_inv_length2 = 1.0f / ImLengthSqr(gradient_extent);
	ImDrawVert* vert_start = draw_list->VtxBuffer.Data + vert_start_idx;
	ImDrawVert* vert_end = draw_list->VtxBuffer.Data + vert_end_idx;

	ImVec4 col0HSV = ImGui::ColorConvertU32ToFloat4(col0);
	ImVec4 col1HSV = ImGui::ColorConvertU32ToFloat4(col1);
	ImGui::ColorConvertRGBtoHSV(col0HSV.x, col0HSV.y, col0HSV.z, col0HSV.x, col0HSV.y, col0HSV.z);
	ImGui::ColorConvertRGBtoHSV(col1HSV.x, col1HSV.y, col1HSV.z, col1HSV.x, col1HSV.y, col1HSV.z);
	ImVec4 colDelta = col1HSV - col0HSV;

	for (ImDrawVert* vert = vert_start; vert < vert_end; vert++) {
		float d = ImDot(vert->pos - gradient_p0, gradient_extent);
		float t = ImClamp(d * gradient_inv_length2, 0.0f, 1.0f);

		float h = col0HSV.x + colDelta.x * t;
		float s = col0HSV.y + colDelta.y * t;
		float v = col0HSV.z + colDelta.z * t;

		ImVec4 rgb;
		ImGui::ColorConvertHSVtoRGB(h, s, v, rgb.x, rgb.y, rgb.z);
		vert->col = (ImGui::ColorConvertFloat4ToU32(rgb) & ~IM_COL32_A_MASK) | (vert->col & IM_COL32_A_MASK);
	}
}

void Misc::drawOffscreenEnemies(ImDrawList* drawList) noexcept
{
	if (!config->misc.offscreenEnemies.enabled)
		return;

	const auto yaw = Helpers::deg2rad(interfaces->engine->getViewAngles().y);

	GameData::Lock lock;
	for (auto& player : GameData::players()) {
		if ((player.dormant && player.fadingAlpha() == 0.0f) || !player.alive || !player.enemy || player.inViewFrustum)
			continue;

		const auto positionDiff = GameData::local().origin - player.origin;

		auto x = std::cos(yaw) * positionDiff.y - std::sin(yaw) * positionDiff.x;
		auto y = std::cos(yaw) * positionDiff.x + std::sin(yaw) * positionDiff.y;
		if (const auto len = std::sqrt(x * x + y * y); len != 0.0f) {
			x /= len;
			y /= len;
		}

		constexpr auto avatarRadius = 13.0f;
		constexpr auto triangleSize = 10.0f;

		const auto pos = ImGui::GetIO().DisplaySize / 2 + ImVec2{ x, y } *200;
		const auto trianglePos = pos + ImVec2{ x, y } *(avatarRadius + (config->misc.offscreenEnemies.healthBar.enabled ? 5 : 3));

		Helpers::setAlphaFactor(player.fadingAlpha());
		const auto white = Helpers::calculateColor(255, 255, 255, 255);
		const auto background = Helpers::calculateColor(0, 0, 0, 80);
		const auto color = Helpers::calculateColor(config->misc.offscreenEnemies);
		const auto healthBarColor = config->misc.offscreenEnemies.healthBar.type == HealthBar::HealthBased ? Helpers::healthColor(std::clamp(player.health / 100.0f, 0.0f, 1.0f)) : Helpers::calculateColor(config->misc.offscreenEnemies.healthBar);
		Helpers::setAlphaFactor(1.0f);

		const ImVec2 trianglePoints[]{
			trianglePos + ImVec2{  0.4f * y, -0.4f * x } *triangleSize,
			trianglePos + ImVec2{  1.0f * x,  1.0f * y } *triangleSize,
			trianglePos + ImVec2{ -0.4f * y,  0.4f * x } *triangleSize
		};

		drawList->AddConvexPolyFilled(trianglePoints, 3, color);
		drawList->AddCircleFilled(pos, avatarRadius + 1, white & IM_COL32_A_MASK, 40);

		const auto texture = player.getAvatarTexture();

		const bool pushTextureId = drawList->_TextureIdStack.empty() || texture != drawList->_TextureIdStack.back();
		if (pushTextureId)
			drawList->PushTextureID(texture);

		const int vertStartIdx = drawList->VtxBuffer.Size;
		drawList->AddCircleFilled(pos, avatarRadius, white, 40);
		const int vertEndIdx = drawList->VtxBuffer.Size;
		ImGui::ShadeVertsLinearUV(drawList, vertStartIdx, vertEndIdx, pos - ImVec2{ avatarRadius, avatarRadius }, pos + ImVec2{ avatarRadius, avatarRadius }, { 0, 0 }, { 1, 1 }, true);

		if (pushTextureId)
			drawList->PopTextureID();

		if (config->misc.offscreenEnemies.healthBar.enabled) {
			const auto radius = avatarRadius + 2;
			const auto healthFraction = std::clamp(player.health / 100.0f, 0.0f, 1.0f);

			drawList->AddCircle(pos, radius, background, 40, 3.0f);

			const int vertStartIdx = drawList->VtxBuffer.Size;
			if (healthFraction == 1.0f) { // sometimes PathArcTo is missing one top pixel when drawing a full circle, so draw it with AddCircle
				drawList->AddCircle(pos, radius, healthBarColor, 40, 2.0f);
			} else {
				constexpr float pi = std::numbers::pi_v<float>;
				drawList->PathArcTo(pos, radius - 0.5f, pi / 2 - pi * healthFraction, pi / 2 + pi * healthFraction, 40);
				drawList->PathStroke(healthBarColor, false, 2.0f);
			}
			const int vertEndIdx = drawList->VtxBuffer.Size;

			if (config->misc.offscreenEnemies.healthBar.type == HealthBar::Gradient)
				shadeVertsHSVColorGradientKeepAlpha(drawList, vertStartIdx, vertEndIdx, pos - ImVec2{ 0.0f, radius }, pos + ImVec2{ 0.0f, radius }, IM_COL32(0, 255, 0, 255), IM_COL32(255, 0, 0, 255));
		}
	}
}

void Misc::autoAccept(const char* soundEntry) noexcept
{
	if (!config->misc.autoAccept)
		return;

	if (std::strcmp(soundEntry, "UIPanorama.popup_accept_match_beep"))
		return;

	if (const auto idx = memory->registeredPanoramaEvents->find(memory->makePanoramaSymbol("MatchAssistedAccept")); idx != -1) {
		if (const auto eventPtr = memory->registeredPanoramaEvents->memory[idx].value.makeEvent(nullptr))
			interfaces->panoramaUIEngine->accessUIEngine()->dispatchEvent(eventPtr);
	}

	auto window = FindWindowW(L"Valve001", NULL);
	FLASHWINFO flash{ sizeof(FLASHWINFO), window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
	FlashWindowEx(&flash);
	ShowWindow(window, SW_RESTORE);
}

void Misc::updateEventListeners(bool forceRemove) noexcept
{
	class PurchaseEventListener : public GameEventListener {
	public:
		void fireGameEvent(GameEvent* event) { purchaseList(event); }
	};

	static PurchaseEventListener listener;
	static bool listenerRegistered = false;

	if (config->misc.purchaseList.enabled && !listenerRegistered) {
		interfaces->gameEventManager->addListener(&listener, "item_purchase");
		listenerRegistered = true;
	} else if ((!config->misc.purchaseList.enabled || forceRemove) && listenerRegistered) {
		interfaces->gameEventManager->removeListener(&listener);
		listenerRegistered = false;
	}
}

void Misc::updateInput() noexcept
{
	config->misc.blockBotKey.handleToggle();
	config->misc.edgeJumpKey.handleToggle();
	config->misc.miniJumpKey.handleToggle();
	config->misc.jumpBugKey.handleToggle();
	config->misc.edgeBugKey.handleToggle();
	config->misc.autoPixelSurfKey.handleToggle();
	config->misc.slowwalkKey.handleToggle();
	config->misc.fakeduckKey.handleToggle();
	config->misc.autoPeekKey.handleToggle();
	config->misc.prepareRevolverKey.handleToggle();
}

void Misc::reset(int resetType) noexcept
{
	if (resetType == 1) {
		static auto ragdollGravity = interfaces->cvar->findVar("cl_ragdoll_gravity");
		static auto blur = interfaces->cvar->findVar("@panorama_disable_blur");
		ragdollGravity->setValue(600);
		blur->setValue(0);
	}
	jumpStatsCalculations = JumpStatsCalculations{ };
}
