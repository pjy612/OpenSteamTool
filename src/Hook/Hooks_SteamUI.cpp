#include "HookManager.h"
#include "HookMacros.h"

namespace {
    HOOK_FUNC(LoadModuleWithPath, HMODULE, const char* path, bool flags) {
        LOG_INFO("LoadModuleWithPath called with path: {} , flags: {}", path, flags);
        HMODULE h = oLoadModuleWithPath(path, flags);
        if (!strcmp(path, "steamclient64.dll"))
            h = diversion_hMdoule;
        return h;
    }
}

namespace SteamUI {
    void CoreHook() {
        HOOK_BEGIN();
        INSTALL_HOOK(GetModuleHandleA("steamui.dll"), LoadModuleWithPath);
        HOOK_END();
    }

    void CoreUnhook() {
        UNHOOK_BEGIN();
        UNINSTALL_HOOK(LoadModuleWithPath);
        UNHOOK_END();
    }
}
