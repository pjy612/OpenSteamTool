#include "Hooks_SteamUI.h"
#include "HookManager.h"
#include "HookMacros.h"
#include "dllmain.h"
#include "steam_messages.pb.h"
#include "Utils/VehCommon.h"

namespace {

    CAPTURE_THIS_FUNC(GetAppByID, CSteamApp*, g_pController,void* pThis, AppId_t appId, bool bCreate);
    CAPTURE_THIS_FUNC(MarkAppChange,void*,g_pAppChangeSource,void* pThis,AppId_t appId, EAppChangeFlags changeFlags);

    HOOK_FUNC(FillInAppOverview,void*,void* pThis,void* pAppOverview,CSteamApp* pApp)
    {
        if (pApp && LuaConfig::HasDepot(pApp->nAppID,false)) {
            uint32_t t = LuaConfig::GetPurchaseTime(pApp->nAppID);
            if(t) {
                pApp->PurchasedTime = t;
                LOG_STEAMUI_TRACE("FillInAppOverview: set PurchasedTime={} for appId={}",
                                  pApp->PurchasedTime, pApp->nAppID);
            }
        }
        return oFillInAppOverview(pThis, pAppOverview, pApp);
    }
}

namespace Hooks_SteamUI {
    void Install() {

        ARM_CAPTURE_U(GetAppByID);
        ARM_CAPTURE_U(MarkAppChange);

        HOOK_BEGIN();
        INSTALL_HOOK_U(FillInAppOverview);
        HOOK_END();

    }

    void Uninstall() {
        UNHOOK_BEGIN();
        UNINSTALL_HOOK(FillInAppOverview);
        UNHOOK_END();
    }

    void RemoveAppAndSendChange(AppId_t appId) {
        // skip on owned apps
        if(LuaConfig::IsOwned(appId)){
            LOG_STEAMUI_WARN("RemoveAppAndSendChange: appId={} is owned, skipping", appId);
            return;
        }
        if(CAPTURE_READY(GetAppByID) && CAPTURE_READY(MarkAppChange)) {
            CSteamApp* pApp = oGetAppByID(g_pController, appId, false);
            if(pApp) {
                pApp->OwnershipFlags = k_EAppOwnershipFlags_None;
                LOG_STEAMUI_DEBUG("RemoveAppAndSendChange: cleared owned flag for appId={}", appId);
                oMarkAppChange(g_pAppChangeSource, appId, EAppChangeFlags::AddedOrCreated);
            } else {
                LOG_STEAMUI_WARN("RemoveAppAndSendChange: appId={} not found in GetAppByID", appId);
            }
        }
    }

}
