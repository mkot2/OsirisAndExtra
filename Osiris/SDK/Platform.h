#pragma once

#define LINUX_ARGS(...)
#define RETURN_ADDRESS() std::uintptr_t(_ReturnAddress())
#define FRAME_ADDRESS() (std::uintptr_t(_AddressOfReturnAddress()) - sizeof(std::uintptr_t))
#define IS_WIN32() true
#define WIN32_LINUX(win32, linux) win32

const auto CLIENT_DLL = xorstr_("client");
const auto SERVER_DLL = xorstr_("server");
const auto ENGINE_DLL = xorstr_("engine");
const auto FILESYSTEM_DLL = xorstr_("filesystem_stdio");
const auto INPUTSYSTEM_DLL = xorstr_("inputsystem");
const auto LOCALIZE_DLL = xorstr_("localize");
const auto MATERIALSYSTEM_DLL = xorstr_("materialsystem");
const auto PANORAMA_DLL = xorstr_("panorama");
const auto SOUNDEMITTERSYSTEM_DLL = xorstr_("soundemittersystem");
const auto STEAMNETWORKINGSOCKETS_DLL = xorstr_("steamnetworkingsockets");
const auto STUDIORENDER_DLL = xorstr_("studiorender");
const auto TIER0_DLL = xorstr_("tier0");
const auto DATACACHE_DLL = xorstr_("datacache");
const auto VGUI2_DLL = xorstr_("vgui2");
const auto VGUIMATSURFACE_DLL = xorstr_("vguimatsurface");
const auto VPHYSICS_DLL = xorstr_("vphysics");
const auto VSTDLIB_DLL = xorstr_("vstdlib");
