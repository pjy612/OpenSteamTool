// xinput1_4.dll HiJack Project

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <detours.h>

#pragma comment(linker, "/EXPORT:DllMain=XINPUT1_4.DllMain,@1")
#pragma comment(linker, "/EXPORT:XInputEnable=XINPUT1_4.XInputEnable,@5")
#pragma comment(linker, "/EXPORT:XInputGetAudioDeviceIds=XINPUT1_4.XInputGetAudioDeviceIds,@10")
#pragma comment(linker, "/EXPORT:XInputGetBatteryInformation=XINPUT1_4.XInputGetBatteryInformation,@7")
#pragma comment(linker, "/EXPORT:XInputGetCapabilities=XINPUT1_4.XInputGetCapabilities,@4")
#pragma comment(linker, "/EXPORT:XInputGetKeystroke=XINPUT1_4.XInputGetKeystroke,@8")
#pragma comment(linker, "/EXPORT:XInputGetState=XINPUT1_4.XInputGetState,@2")
#pragma comment(linker, "/EXPORT:XInputSetState=XINPUT1_4.XInputSetState,@3")
#pragma comment(linker, "/EXPORT:#100=XINPUT1_4.#100,@100,NONAME")
#pragma comment(linker, "/EXPORT:#101=XINPUT1_4.#101,@101,NONAME")
#pragma comment(linker, "/EXPORT:#102=XINPUT1_4.#102,@102,NONAME")
#pragma comment(linker, "/EXPORT:#103=XINPUT1_4.#103,@103,NONAME")
#pragma comment(linker, "/EXPORT:#104=XINPUT1_4.#104,@104,NONAME")
#pragma comment(linker, "/EXPORT:#108=XINPUT1_4.#108,@108,NONAME")

// Our own module handle — returned to callers who try to load xinput1_4.dll at runtime.
static HMODULE g_OldModule = nullptr;

// ─── Detours: intercept LoadLibraryExW ───────────────────────────
typedef HMODULE(WINAPI* LoadLibraryExW_t)(LPCWSTR, HANDLE, DWORD);
static LoadLibraryExW_t oLoadLibraryExW = nullptr;

HMODULE WINAPI hkLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    if (lpLibFileName)
    {
        char buf[512];
        sprintf(buf, "[xinput1_4] hkLoadLibraryExW called: %ls (flags=0x%lx)", lpLibFileName, dwFlags);
        OutputDebugStringA(buf);

        if (g_OldModule && _wcsicmp(lpLibFileName, L"xinput1_4.dll") == 0)
        {
            OutputDebugStringA("[xinput1_4] -> hijacked, returning g_OldModule");
            return g_OldModule;
        }
    }
    return oLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

static void InstallHook()
{
    oLoadLibraryExW = reinterpret_cast<LoadLibraryExW_t>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryExW"));
    if (!oLoadLibraryExW)
    {
        OutputDebugStringA("[xinput1_4] GetProcAddress(LoadLibraryExW) failed");
        return;
    }

    char buf[256];
    sprintf(buf, "[xinput1_4] oLoadLibraryExW = %p, hkLoadLibraryExW = %p, g_OldModule = %p",
        reinterpret_cast<void*>(oLoadLibraryExW),
        reinterpret_cast<void*>(hkLoadLibraryExW),
        g_OldModule);
    OutputDebugStringA(buf);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&oLoadLibraryExW),
                 reinterpret_cast<PVOID>(hkLoadLibraryExW));
    LONG err = DetourTransactionCommit();
    sprintf(buf, "[xinput1_4] DetourTransactionCommit returned %ld (0=success)", err);
    OutputDebugStringA(buf);
}

static void UninstallHook()
{
    if (!oLoadLibraryExW)
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<PVOID*>(&oLoadLibraryExW),
                 reinterpret_cast<PVOID>(hkLoadLibraryExW));
    DetourTransactionCommit();
    oLoadLibraryExW = nullptr;
}

// Only inject when the host process is steam.exe (case-insensitive).
// LoadLibraryA itself guarantees that OpenSteamTool.dll's DllMain
// runs at most once per process, so multiple hijack DLLs can safely
// call this without additional synchronisation.
BOOL OpenSteamToolLoad()
{
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH))
    {
        const char* exeName = strrchr(exePath, '\\');
        exeName = exeName ? exeName + 1 : exePath;
        if (_stricmp(exeName, "steam.exe") != 0)
            return TRUE;   // not Steam — let the proxy load, but don't inject
    }
    return LoadLibraryA("OpenSteamTool.dll") != NULL;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hModule);

            // Save our own handle so the hook can return it.
            g_OldModule = hModule;

            // Pin ourselves in memory — prevents FreeLibrary from unloading us
            // and breaking the hook. FROM_ADDRESS resolves to this DLL.
            HMODULE pinned = nullptr;
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(&hkLoadLibraryExW),
                &pinned);

            InstallHook();

            if (!OpenSteamToolLoad())
                return FALSE;
            break;
        }
    case DLL_PROCESS_DETACH:
        UninstallHook();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
