#pragma once

#include <memory>
#include <type_traits>
#include <d3d9.h>
#include <Windows.h>

#include "Hooks/MinHook.h"

#include "SDK/Platform.h"


struct SoundInfo;

using HookType = MinHook;

class Hooks {
public:
	Hooks(HMODULE moduleHandle) noexcept;

	WNDPROC originalWndProc;
	std::add_pointer_t<HRESULT __stdcall(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*)> originalPresent;
	std::add_pointer_t<HRESULT __stdcall(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)> originalReset;

	void install() noexcept;
	void uninstall() noexcept;

	std::add_pointer_t<int __fastcall(SoundInfo&)> originalDispatchSound;

	HookType buildTransformations;
	HookType doExtraBoneProcessing;
	HookType shouldSkipAnimationFrame;
	HookType standardBlendingRules;
	HookType updateClientSideAnimation;
	HookType checkForSequenceChange;

	HookType modifyEyePosition;
	HookType calculateView;
	HookType updateState;

	HookType resetState;
	HookType setupVelocity;
	HookType setupMovement;
	HookType setupAliveloop;
	HookType setupWeaponAction;

	HookType sendDatagram;

	HookType setupBones;
	HookType eyeAngles;
	HookType calcViewBob;

	HookType postDataUpdate;

	HookType clSendMove;
	HookType clMove;

	HookType getColorModulation;
	HookType isUsingStaticPropDebugModes;
	HookType traceFilterForHeadCollision;
	HookType performScreenOverlay;
	HookType postNetworkDataReceived;
	HookType isDepthOfFieldEnabled;
	HookType getClientModelRenderable;
	HookType physicsSimulate;
	HookType updateFlashBangEffect;

	HookType newFunctionClientDLL;
	HookType newFunctionEngineDLL;
	HookType newFunctionStudioRenderDLL;
	HookType newFunctionMaterialSystemDLL;

	HookType fileSystem;
	HookType bspQuery;
	HookType client;
	HookType clientMode;
	HookType clientState;
	HookType engine;
	HookType gameMovement;
	HookType modelRender;
	HookType sound;
	HookType surface;
	HookType viewRender;
	HookType svCheats;
private:
	HMODULE moduleHandle;
	HWND window;
};

inline std::unique_ptr<Hooks> hooks;
