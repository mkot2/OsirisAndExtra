#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <Windows.h>

#include "SDK/Platform.h"

class BaseFileSystem;
class Client;
class Cvar;
class DebugOverlay;
class Engine;
class EngineSound;
class EngineTrace;
class EntityList;
class FileSystem;
class GameEventManager;
class GameMovement;
class GameUI;
class InputSystem;
class Localize;
class MaterialSystem;
class MDLCache;
class ModelInfo;
class ModelRender;
class NetworkStringTableContainer;
class PanoramaUIEngine;
class PhysicsSurfaceProps;
class Prediction;
class RenderView;
class Surface;
class Server;
class SoundEmitter;
class StudioRender;

class Interfaces {
public:
#define GAME_INTERFACE(type, name, moduleName, version) \
type* name = reinterpret_cast<type*>(find(moduleName, version));

	GAME_INTERFACE(BaseFileSystem, baseFileSystem, FILESYSTEM_DLL, xorstr_("VBaseFileSystem011"))
		GAME_INTERFACE(Client, client, CLIENT_DLL, xorstr_("VClient018"))
		GAME_INTERFACE(Cvar, cvar, VSTDLIB_DLL, xorstr_("VEngineCvar007"))
		GAME_INTERFACE(DebugOverlay, debugOverlay, ENGINE_DLL, xorstr_("VDebugOverlay004"))
		GAME_INTERFACE(Engine, engine, ENGINE_DLL, xorstr_("VEngineClient014"))
		GAME_INTERFACE(EngineTrace, engineTrace, ENGINE_DLL, xorstr_("EngineTraceClient004"))
		GAME_INTERFACE(EntityList, entityList, CLIENT_DLL, xorstr_("VClientEntityList003"))
		GAME_INTERFACE(FileSystem, fileSystem, FILESYSTEM_DLL, xorstr_("VFileSystem017"))
		GAME_INTERFACE(GameEventManager, gameEventManager, ENGINE_DLL, xorstr_("GAMEEVENTSMANAGER002"))
		GAME_INTERFACE(GameMovement, gameMovement, CLIENT_DLL, xorstr_("GameMovement001"))
		GAME_INTERFACE(GameUI, gameUI, CLIENT_DLL, xorstr_("GameUI011"))
		GAME_INTERFACE(InputSystem, inputSystem, INPUTSYSTEM_DLL, xorstr_("InputSystemVersion001"))
		GAME_INTERFACE(Localize, localize, LOCALIZE_DLL, xorstr_("Localize_001"))
		GAME_INTERFACE(MaterialSystem, materialSystem, MATERIALSYSTEM_DLL, xorstr_("VMaterialSystem080"))
		GAME_INTERFACE(MDLCache, mdlCache, DATACACHE_DLL, xorstr_("MDLCache004"))
		GAME_INTERFACE(ModelInfo, modelInfo, ENGINE_DLL, xorstr_("VModelInfoClient004"))
		GAME_INTERFACE(ModelRender, modelRender, ENGINE_DLL, xorstr_("VEngineModel016"))
		GAME_INTERFACE(NetworkStringTableContainer, networkStringTableContainer, ENGINE_DLL, xorstr_("VEngineClientStringTable001"))
		GAME_INTERFACE(PanoramaUIEngine, panoramaUIEngine, PANORAMA_DLL, xorstr_("PanoramaUIEngine001"))
		GAME_INTERFACE(PhysicsSurfaceProps, physicsSurfaceProps, VPHYSICS_DLL, xorstr_("VPhysicsSurfaceProps001"))
		GAME_INTERFACE(Prediction, prediction, CLIENT_DLL, xorstr_("VClientPrediction001"))
		GAME_INTERFACE(RenderView, renderView, ENGINE_DLL, xorstr_("VEngineRenderView014"))
		GAME_INTERFACE(Surface, surface, VGUIMATSURFACE_DLL, xorstr_("VGUI_Surface031"))
		GAME_INTERFACE(EngineSound, sound, ENGINE_DLL, xorstr_("IEngineSoundClient003"))
		GAME_INTERFACE(Server, server, SERVER_DLL, xorstr_("ServerGameDLL005"))
		GAME_INTERFACE(SoundEmitter, soundEmitter, SOUNDEMITTERSYSTEM_DLL, xorstr_("VSoundEmitter003"))
		GAME_INTERFACE(StudioRender, studioRender, STUDIORENDER_DLL, xorstr_("VStudioRender026"))

#undef GAME_INTERFACE
private:
	static void* find(const char* moduleName, const char* name) noexcept
	{
		if (const auto createInterface = reinterpret_cast<std::add_pointer_t<void* __cdecl(const char* name, int* returnCode)>>(
			GetProcAddress(GetModuleHandleA(moduleName), xorstr_("CreateInterface"))
			)) {
			if (void* foundInterface = createInterface(name, nullptr))
				return foundInterface;
		}

		MessageBoxA(nullptr, (xorstr_("Failed to find ") + std::string{ name } + xorstr_(" interface!")).c_str(), xorstr_("Osiris"), MB_OK | MB_ICONERROR);
		std::exit(EXIT_FAILURE);
	}
};

inline std::unique_ptr<const Interfaces> interfaces;
