#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <limits>
#include <string_view>
#include <utility>
#include <Windows.h>
#include <Psapi.h>

#include <mem/module.h>
#include <mem/pattern.h>

#include "SDK/LocalPlayer.h"

#include "Interfaces.h"
#include "Memory.h"

template <typename T>
static constexpr auto relativeToAbsolute(uintptr_t address) noexcept
{
	return (T)(address + 4 + *reinterpret_cast<std::int32_t*>(address));
}

static std::uintptr_t findPattern(const char* moduleName, std::string_view patternString, bool reportNotFound = true) noexcept
{
	if (const auto module = mem::module::named(moduleName); module != mem::module()) {
		mem::pattern pattern(patternString.data());
		mem::default_scanner scanner(pattern);

		if (const auto address = scanner.scan(module).as<std::uintptr_t>(); address)
			return address;
	}

	if (reportNotFound)
		MessageBoxA(nullptr, std::format("Failed to find pattern: {}", patternString).c_str(), "Osiris", MB_OK | MB_ICONWARNING);
	return 0;
}

Memory::Memory() noexcept
{
	present = findPattern(OVERLAY_DLL, "FF 15 ? ? ? ? 8B F0 85 FF") + 2;
	reset = findPattern(OVERLAY_DLL, "C7 45 ? ? ? ? ? FF 15 ? ? ? ? 8B D8") + 9;

	clientMode = **reinterpret_cast<ClientMode***>((*reinterpret_cast<uintptr_t**>(interfaces->client))[10] + 5);
	input = *reinterpret_cast<Input**>((*reinterpret_cast<uintptr_t**>(interfaces->client))[16] + 1);
	globalVars = **reinterpret_cast<GlobalVars***>((*reinterpret_cast<uintptr_t**>(interfaces->client))[11] + 10);
	glowObjectManager = *reinterpret_cast<GlowObjectManager**>(findPattern(CLIENT_DLL, "0F 11 05 ? ? ? ? 83 C8 01") + 3);
	disablePostProcessing = *reinterpret_cast<bool**>(findPattern(CLIENT_DLL, "83 EC 4C 80 3D") + 5);
	loadSky = relativeToAbsolute<decltype(loadSky)>(findPattern(ENGINE_DLL, "E8 ? ? ? ? 84 C0 74 2D A1") + 1);
	setClanTag = reinterpret_cast<decltype(setClanTag)>(findPattern(ENGINE_DLL, "53 56 57 8B DA 8B F9 FF 15"));
	lineGoesThroughSmoke = relativeToAbsolute<decltype(lineGoesThroughSmoke)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B 4C 24 30 33 D2") + 1);
	cameraThink = findPattern(CLIENT_DLL, "85 C0 75 30 38 87");
	getSequenceActivity = reinterpret_cast<decltype(getSequenceActivity)>(findPattern(CLIENT_DLL, "55 8B EC 53 8B 5D 08 56 8B F1 83"));
	isOtherEnemy = relativeToAbsolute<decltype(isOtherEnemy)>(findPattern(CLIENT_DLL, "8B CE E8 ? ? ? ? 02 C0") + 3);
	hud = *reinterpret_cast<std::uintptr_t*>(findPattern(CLIENT_DLL, "B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08") + 1);
	findHudElement = reinterpret_cast<decltype(findHudElement)>(findPattern(CLIENT_DLL, "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39"));
	clearHudWeapon = relativeToAbsolute<decltype(clearHudWeapon)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B F0 C6 44 24 ? ? C6 44 24") + 1);
	itemSystem = relativeToAbsolute<decltype(itemSystem)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 0F B7 0F") + 1);
	setAbsOrigin = relativeToAbsolute<decltype(setAbsOrigin)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? EB 19 8B 07") + 1);
	insertIntoTree = findPattern(CLIENT_DLL, "56 52 FF 50 18") + 5;
	dispatchSound = reinterpret_cast<int*>(findPattern(ENGINE_DLL, "74 0B E8 ? ? ? ? 8B 3D") + 3);
	traceToExit = findPattern(CLIENT_DLL, "55 8B EC 83 EC 4C F3 0F 10 75");
	viewRender = **reinterpret_cast<ViewRender***>(findPattern(CLIENT_DLL, "8B 0D ? ? ? ? FF 75 0C 8B 45 08") + 2);
	viewRenderBeams = *reinterpret_cast<ViewRenderBeams**>(findPattern(CLIENT_DLL, "B9 ? ? ? ? 0F 11 44 24 ? C7 44 24 ? ? ? ? ? F3 0F 10 84 24") + 1);
	drawScreenEffectMaterial = relativeToAbsolute<uintptr_t>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 83 C4 0C 8D 4D F8") + 1);
	submitReportFunction = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 83 EC 28 8B 4D 08");
	const auto tier0 = GetModuleHandleW(L"tier0");
	debugMsg = reinterpret_cast<decltype(debugMsg)>(GetProcAddress(tier0, "Msg"));
	conColorMsg = reinterpret_cast<decltype(conColorMsg)>(GetProcAddress(tier0, "?ConColorMsg@@YAXABVColor@@PBDZZ"));
	vignette = *reinterpret_cast<float**>(findPattern(CLIENT_DLL, "0F 11 05 ? ? ? ? F3 0F 7E 87") + 3) + 1;
	equipWearable = reinterpret_cast<decltype(equipWearable)>(findPattern(CLIENT_DLL, "55 8B EC 83 EC 10 53 8B 5D 08 57 8B F9"));
	predictionRandomSeed = *reinterpret_cast<int**>(findPattern(CLIENT_DLL, "8B 0D ? ? ? ? BA ? ? ? ? E8 ? ? ? ? 83 C4 04") + 2);
	moveData = **reinterpret_cast<MoveData***>(findPattern(CLIENT_DLL, "A1 ? ? ? ? F3 0F 59 CD") + 1);
	moveHelper = **reinterpret_cast<MoveHelper***>(findPattern(CLIENT_DLL, "8B 0D ? ? ? ? 8B 45 ? 51 8B D4 89 02 8B 01") + 2);
	keyValuesFromString = relativeToAbsolute<decltype(keyValuesFromString)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 83 C4 04 89 45 D8") + 1);
	keyValuesFindKey = relativeToAbsolute<decltype(keyValuesFindKey)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? F7 45") + 1);
	keyValuesSetString = relativeToAbsolute<decltype(keyValuesSetString)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 89 77 38") + 1);
	weaponSystem = *reinterpret_cast<WeaponSystem**>(findPattern(CLIENT_DLL, "8B 35 ? ? ? ? FF 10 0F B7 C0") + 2);
	getPlayerViewmodelArmConfigForPlayerModel = relativeToAbsolute<decltype(getPlayerViewmodelArmConfigForPlayerModel)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 89 87 ? ? ? ? 6A") + 1);
	getEventDescriptor = relativeToAbsolute<decltype(getEventDescriptor)>(findPattern(ENGINE_DLL, "E8 ? ? ? ? 8B D8 85 DB 75 27") + 1);
	activeChannels = *reinterpret_cast<ActiveChannels**>(findPattern(ENGINE_DLL, "8B 1D ? ? ? ? 89 5C 24 48") + 2);
	channels = *reinterpret_cast<Channel**>(findPattern(ENGINE_DLL, "81 C2 ? ? ? ? 8B 72 54") + 2);
	playerResource = *reinterpret_cast<PlayerResource***>(findPattern(CLIENT_DLL, "74 30 8B 35 ? ? ? ? 85 F6") + 4);
	getDecoratedPlayerName = relativeToAbsolute<decltype(getDecoratedPlayerName)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 66 83 3E") + 1);
	scopeDust = findPattern(CLIENT_DLL, "FF 50 3C 8B 4C 24 20") + 3;
	scopeArc = findPattern(CLIENT_DLL, "8B 0D ? ? ? ? FF B7 ? ? ? ? 8B 01 FF 90 ? ? ? ? 8B 7C 24 1C");
	demoOrHLTV = findPattern(CLIENT_DLL, "84 C0 75 09 38 05");
	money = findPattern(CLIENT_DLL, "84 C0 75 0C 5B");
	demoFileEndReached = findPattern(CLIENT_DLL, "8B C8 85 C9 74 1F 80 79 10");
	plantedC4s = *reinterpret_cast<decltype(plantedC4s)*>(findPattern(CLIENT_DLL, "7E 2C 8B 15") + 4);
	gameRules = *reinterpret_cast<Entity***>(findPattern(CLIENT_DLL, "8B EC 8B 0D ? ? ? ? 85 C9 74 07") + 4);
	setOrAddAttributeValueByNameFunction = relativeToAbsolute<decltype(setOrAddAttributeValueByNameFunction)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B 8D ? ? ? ? 85 C9 74 10") + 1);
	registeredPanoramaEvents = reinterpret_cast<decltype(registeredPanoramaEvents)>(*reinterpret_cast<std::uintptr_t*>(findPattern(CLIENT_DLL, "E8 ? ? ? ? A1 ? ? ? ? A8 01 75 21") + 6) - 36);
	makePanoramaSymbolFn = relativeToAbsolute<decltype(makePanoramaSymbolFn)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 0F B7 45 0E 8D 4D 0E") + 1);

	localPlayer.init(*reinterpret_cast<Entity***>(findPattern(CLIENT_DLL, "A1 ? ? ? ? 89 45 BC 85 C0") + 1));

	shouldDrawFogReturnAddress = relativeToAbsolute<std::uintptr_t>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B 0D ? ? ? ? 0F B6 D0") + 1) + 82;

	// Custom
	clientState = **reinterpret_cast<ClientState***>(findPattern(ENGINE_DLL, "A1 ? ? ? ? 8B 80 ? ? ? ? C3") + 1); //52
	memalloc = *reinterpret_cast<MemAlloc**>(GetProcAddress(tier0, "g_pMemAlloc"));
	setAbsAngle = reinterpret_cast<decltype(setAbsAngle)>(reinterpret_cast<DWORD*>(findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1")));
	createState = findPattern(CLIENT_DLL, "55 8B EC 56 8B F1 B9 ? ? ? ? C7");
	updateState = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 F3");
	resetState = findPattern(CLIENT_DLL, "56 6A 01 68 ? ? ? ? 8B F1");
	invalidateBoneCache = findPattern(CLIENT_DLL, "80 3D ? ? ? ? ? 74 16 A1 ? ? ? ? 48 C7 81");

	setupVelocityAddress = *(reinterpret_cast<void**>(findPattern(CLIENT_DLL, "84 C0 75 38 8B 0D ? ? ? ? 8B 01 8B 80")));
	accumulateLayersAddress = *(reinterpret_cast<void**>(findPattern(CLIENT_DLL, "84 C0 75 0D F6 87")));
	standardBlendingRules = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F0 B8 F8 10");

	buildTransformations = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B F9 8B 0D ? ? ? ? 89 7C 24 28 8B");
	doExtraBoneProcessing = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 56 8B F1 57 89 74 24 1C 80");
	shouldSkipAnimationFrame = findPattern(CLIENT_DLL, "57 8B F9 8B 07 8B 80 ? ? ? ? FF D0 84 C0 75 02");
	updateClientSideAnimation = findPattern(CLIENT_DLL, "55 8B EC 51 56 8B F1 80 BE ? ? ? ? ? 74 36");

	checkForSequenceChange = findPattern(CLIENT_DLL, "55 8B EC 51 53 8B 5D 08 56 8B F1 57 85");

	sendDatagram = findPattern(ENGINE_DLL, "55 8B EC 83 E4 F0 B8 ? ? ? ? E8 ? ? ? ? 56 57 8B F9 89 7C 24 14");

	modifyEyePosition = relativeToAbsolute<decltype(modifyEyePosition)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B 06 8B CE FF 90 ? ? ? ? 85 C0 74 50") + 1);

	lookUpBone = reinterpret_cast<decltype(lookUpBone)>(findPattern(CLIENT_DLL, "55 8B EC 53 56 8B F1 57 83 BE ? ? ? ? ? 75 14"));
	getBonePos = relativeToAbsolute<decltype(getBonePos)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8D 14 24") + 1);

	setCollisionBounds = relativeToAbsolute<decltype(setCollisionBounds)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 0F BF 87 ? ? ? ?") + 1);
	calculateView = findPattern(CLIENT_DLL, "55 8B EC 83 EC 14 53 56 57 FF 75 18");

	setupVelocity = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 83 EC 30 56 57 8B 3D");
	setupMovement = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 81 EC ? ? ? ? 56 57 8B 3D ? ? ? ? 8B F1");
	setupAliveloop = findPattern(CLIENT_DLL, "55 8B EC 51 53 56 57 8B F9 8B 77 60");

	getWeaponPrefix = reinterpret_cast<decltype(getWeaponPrefix)>(findPattern(CLIENT_DLL, "53 56 57 8B F9 33 F6 8B 4F 60 8B 01 FF"));
	getLayerActivity = reinterpret_cast<decltype(getLayerActivity)>(findPattern(CLIENT_DLL, "55 8B EC 83 EC 08 53 56 8B 35 ? ? ? ? 57 8B F9 8B CE 8B 06 FF 90 ? ? ? ? 8B 7F 60 83"));

	lookUpSequence = relativeToAbsolute<decltype(lookUpSequence)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 5E 83 F8 FF") + 1);
	seqdesc = relativeToAbsolute<decltype(seqdesc)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 03 40 04") + 1);
	getFirstSequenceAnimTag = relativeToAbsolute<decltype(getFirstSequenceAnimTag)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? F3 0F 11 86 ? ? ? ? 0F 57 DB") + 1);
	getSequenceLinearMotion = relativeToAbsolute<decltype(getSequenceLinearMotion)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? F3 0F 10 4D ? 83 C4 08 F3 0F 10 45 ? F3 0F 59 C0") + 1);
	sequenceDuration = relativeToAbsolute<decltype(sequenceDuration)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? F3 0F 11 86 ? ? ? ? 51") + 1);
	lookUpPoseParameter = relativeToAbsolute<decltype(lookUpPoseParameter)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 85 C0 79 08") + 1);
	studioSetPoseParameter = relativeToAbsolute<decltype(studioSetPoseParameter)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 0F 28 D8 83 C4 04") + 1);
	calcAbsoluteVelocity = relativeToAbsolute<decltype(calcAbsoluteVelocity)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 83 7B 30 00") + 1);
	utilPlayerByIndex = reinterpret_cast<decltype(utilPlayerByIndex)>(findPattern(SERVER_DLL, "85 C9 7E 32 A1 ? ? ? ?"));
	drawServerHitboxes = findPattern(SERVER_DLL, "55 8B EC 81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 8B D9 57 8B CE");
	postDataUpdate = findPattern(CLIENT_DLL, "55 8B EC 53 56 8B F1 57 80 ? ? ? ? ? ? 74 0A");

	setupBones = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F0 B8 D8");

	restoreEntityToPredictedFrame = reinterpret_cast<decltype(restoreEntityToPredictedFrame)>(findPattern(CLIENT_DLL, "55 8B EC 8B 4D ? 56 E8 ? ? ? ? 8B 75"));

	markSurroundingBoundsDirty = relativeToAbsolute<decltype(markSurroundingBoundsDirty)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 83 FB 01 75 41") + 1);
	isBreakableEntity = reinterpret_cast<decltype(isBreakableEntity)>(findPattern(CLIENT_DLL, "55 8B EC 51 56 8B F1 85 F6 74 68"));

	clSendMove = findPattern(ENGINE_DLL, "55 8B EC 8B 4D 04 81 EC ? ? ? ?");
	clMsgMoveSetData = relativeToAbsolute<decltype(clMsgMoveSetData)>(findPattern(ENGINE_DLL, "E8 ? ? ? ? 8D 7E 18") + 1);
	clMsgMoveDescontructor = relativeToAbsolute<decltype(clMsgMoveDescontructor)>(findPattern(ENGINE_DLL, "E8 ? ? ? ? 5F 5E 5B 8B E5 5D C3 CC CC CC CC CC CC CC CC CC 55 8B EC 81 EC ? ? ? ?") + 1);
	clMove = findPattern(ENGINE_DLL, "55 8B EC 81 EC ? ? ? ? 53 56 8A F9");
	chokeLimit = findPattern(ENGINE_DLL, "B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC") + 1;
	relayCluster = *reinterpret_cast<std::string**>(findPattern(STEAMNETWORKINGSOCKETS_DLL, "B8 ? ? ? ? B9 ? ? ? ? 0F 43") + 1);
	unlockInventory = findPattern(CLIENT_DLL, "84 C0 75 05 B0 01 5F");
	getColorModulation = findPattern(MATERIALSYSTEM_DLL, "55 8B EC 83 EC ? 56 8B F1 8A 46");
	isUsingStaticPropDebugModes = relativeToAbsolute<decltype(isUsingStaticPropDebugModes)>(findPattern(ENGINE_DLL, "E8 ? ? ? ? 84 C0 8B 45 08") + 1);
	traceFilterForHeadCollision = findPattern(CLIENT_DLL, "55 8B EC 56 8B 75 0C 57 8B F9 F7 C6 ? ? ? ?");
	performScreenOverlay = findPattern(CLIENT_DLL, "55 8B EC 51 A1 ? ? ? ? 53 56 8B D9");
	postNetworkDataReceived = relativeToAbsolute<decltype(postNetworkDataReceived)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 33 F6 6A 02") + 1);
	saveData = relativeToAbsolute<decltype(saveData)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 6B 45 08 34") + 1);
	isDepthOfFieldEnabled = findPattern(CLIENT_DLL, "8B 0D ? ? ? ? 56 8B 01 FF 50 34 8B F0 85 F6 75 04");
	eyeAngles = findPattern(CLIENT_DLL, "56 8B F1 85 F6 74 32");
	eyePositionAndVectors = findPattern(CLIENT_DLL, "8B 55 0C 8B C8 E8 ? ? ? ? 83 C4 08 5E 8B E5");
	calcViewBob = findPattern(CLIENT_DLL, "55 8B EC A1 ? ? ? ? 83 EC 10 56 8B F1 B9 ? ? ? ?");
	getClientModelRenderable = findPattern(CLIENT_DLL, "56 8B F1 80 BE ? ? ? ? ? 0F 84 ? ? ? ? 80 BE");
	physicsSimulate = findPattern(CLIENT_DLL, "56 8B F1 8B ? ? ? ? ? 83 F9 FF 74 23");
	updateFlashBangEffect = findPattern(CLIENT_DLL, "55 8B EC 8B 55 04 56 8B F1 57 8D 8E ? ? ? ?");
	writeUsercmd = findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D");
	reevauluateAnimLODAddress = relativeToAbsolute<decltype(reevauluateAnimLODAddress)>(findPattern(CLIENT_DLL, "E8 ? ? ? ? 8B CE E8 ? ? ? ? 8B 8F ? ? ? ?") + 0x2B);
	physicsRunThink = reinterpret_cast<decltype(physicsRunThink)>(findPattern(CLIENT_DLL, "55 8B EC 83 EC 10 53 56 57 8B F9 8B 87"));
	checkHasThinkFunction = reinterpret_cast<decltype(checkHasThinkFunction)>(findPattern(CLIENT_DLL, "55 8B EC 56 57 8B F9 8B B7 ? ? ? ? 8B C6 C1 E8"));
	postThinkVPhysics = reinterpret_cast<decltype(postThinkVPhysics)>(findPattern(CLIENT_DLL, "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 8B D9 56 57 83 BB ? ? ? ? ? 0F 84"));
	simulatePlayerSimulatedEntities = reinterpret_cast<decltype(simulatePlayerSimulatedEntities)>(findPattern(CLIENT_DLL, "56 8B F1 57 8B BE ? ? ? ? 83 EF 01 78 74"));

	predictionPlayer = *reinterpret_cast<int**>(findPattern(CLIENT_DLL, "89 35 ? ? ? ? F3 0F 10 48") + 0x2);

	newFunctionClientDLL = findPattern(CLIENT_DLL, "55 8B EC 56 8B F1 33 C0 57 8B 7D 08");
	newFunctionEngineDLL = findPattern(ENGINE_DLL, "55 8B EC 56 8B F1 33 C0 57 8B 7D 08");
	newFunctionStudioRenderDLL = findPattern(STUDIORENDER_DLL, "55 8B EC 56 8B F1 33 C0 57 8B 7D 08");
	newFunctionMaterialSystemDLL = findPattern(MATERIALSYSTEM_DLL, "55 8B EC 56 8B F1 33 C0 57 8B 7D 08");
	transferData = reinterpret_cast<decltype(transferData)>(findPattern(CLIENT_DLL, "55 8B EC 8B 45 10 53 56 8B F1 57"));
	findLoggingChannel = reinterpret_cast<decltype(findLoggingChannel)>(GetProcAddress(tier0, "LoggingSystem_FindChannel"));
	logDirect = reinterpret_cast<decltype(logDirect)>(GetProcAddress(tier0, "LoggingSystem_LogDirect"));
	getGameModeNameFn = findPattern(CLIENT_DLL, "55 8B EC 8B 0D ? ? ? ? 53 57 8B 01");

	createSimpleThread = reinterpret_cast<decltype(createSimpleThread)>(GetProcAddress(tier0, "CreateSimpleThread"));
	releaseThreadHandle = reinterpret_cast<decltype(releaseThreadHandle)>(GetProcAddress(tier0, "ReleaseThreadHandle"));
}
