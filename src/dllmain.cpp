#include "dllmain.h"
#include "Hook/HookManager.h"

// Load diversion.dll and prepare key runtime paths.
bool LoadDiversion()
{
    if (!GetCurrentDirectoryA(MAX_PATH, SteamInstallPath)) {
        return false;
    }
    sprintf_s(SteamclientPath, MAX_PATH, "%s\\steamclient64.dll",  SteamInstallPath);
    sprintf_s(DiversionPath,   MAX_PATH, "%s\\bin\\diversion64.dll", SteamInstallPath);
    sprintf_s(LuaDir,          MAX_PATH, "%s\\config\\stplug-in",        SteamInstallPath);
    // copy steamclient64.dll to diversion.dll
    CopyFileA(SteamclientPath, DiversionPath, FALSE);
    diversion_hMdoule = LoadLibraryA(DiversionPath);
    return diversion_hMdoule != nullptr;
}

// All initialisation that touches the filesystem, calls LoadLibrary, scans
// memory, or installs detours runs here on a worker thread — we MUST NOT do
// any of that from inside DllMain (loader lock).
static DWORD WINAPI InitThread(LPVOID param) {
    HMODULE selfModule = static_cast<HMODULE>(param);
    Log::Init(selfModule);
    LOG_INFO("OpenSteamTool init thread started");

    if (!LoadDiversion()) {
        LOG_ERROR("LoadDiversion failed");
        return 1;
    }

    LuaConfig::ParseDirectory(std::string(LuaDir));
    SteamUI::CoreHook();
    SteamClient::CoreHook();
    LOG_INFO("OpenSteamTool init complete");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        // Hand off all real work to a worker thread to avoid running file I/O,
        // LoadLibrary, and detour transactions under the loader lock.
        HANDLE h = CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        SteamUI::CoreUnhook();
        SteamClient::CoreUnhook();
    }

    return TRUE;
}
