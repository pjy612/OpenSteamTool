#include "Hooks_Misc.h"
#include "HookMacros.h"
#include "Utils/VehCommon.h"


namespace {
    using GetAppIDForCurrentPipe_t = AppId_t(*)(void* pSteamEngine);

    GetAppIDForCurrentPipe_t g_oGetAppIDForCurrentPipe   = nullptr;
    void*                    g_steamEngine               = nullptr;
    uint8_t*                 g_initialRunningGameTarget  = nullptr;
    AppId_t                  g_runningAppId              = 0;
    PVOID                    g_vehHandle                 = nullptr;

    // VEH handler scoped to this module's int3 sites only. Foreign RIP ->
    // EXCEPTION_CONTINUE_SEARCH so other VEH handlers (e.g. the access-token
    // one) still get their turn.
    LONG CALLBACK VehHandler(PEXCEPTION_POINTERS pExInfo) {
        PCONTEXT ctx = pExInfo->ContextRecord;

        if (pExInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
            if (ctx->Rip == reinterpret_cast<uint64_t>(g_oGetAppIDForCurrentPipe)) {
                g_steamEngine = reinterpret_cast<void*>(ctx->Rcx);
                // One-shot capture: restore the original byte permanently.
                *reinterpret_cast<uint8_t*>(g_oGetAppIDForCurrentPipe) = 0x48;
                LOG_MISC_INFO("Captured SteamEngine pointer: 0x{:X}",
                         reinterpret_cast<uint64_t>(g_steamEngine));
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (ctx->Rip == reinterpret_cast<uint64_t>(g_initialRunningGameTarget)) {
                // [rsp+0x28] holds a pointer to the RunningGame AppId.
                uint64_t pAppId = *reinterpret_cast<uint64_t*>(ctx->Rsp + 0x28);
                g_runningAppId = *reinterpret_cast<uint32_t*>(pAppId);
                *g_initialRunningGameTarget = 0x48;
                ctx->EFlags |= 0x100;  // arm TF for re-arming
                LOG_MISC_INFO("Captured Running AppId: {} from 0x{:X}", g_runningAppId, pAppId);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }

        if (pExInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP
            && ctx->Rip == reinterpret_cast<uint64_t>(g_initialRunningGameTarget + 5)) {
            *g_initialRunningGameTarget = 0xCC;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }
}

namespace Hooks_Misc {
    void Install() {
        if (g_vehHandle) return;

        if (auto* p = FIND_SIG(diversion_hMdoule, GetAppIDForCurrentPipe)) {
            g_oGetAppIDForCurrentPipe = reinterpret_cast<GetAppIDForCurrentPipe_t>(p);
            VehCommon::ArmInt3(p);
        }

        if (auto* p = static_cast<uint8_t*>(FIND_SIG(diversion_hMdoule, InitialRunningGame))) {
            g_initialRunningGameTarget = p;
            VehCommon::ArmInt3(g_initialRunningGameTarget);
        }

        if (g_oGetAppIDForCurrentPipe || g_initialRunningGameTarget) {
            g_vehHandle = AddVectoredExceptionHandler(1, VehHandler);
        }
    }

    void Uninstall() {
        if (g_vehHandle) {
            RemoveVectoredExceptionHandler(g_vehHandle);
            g_vehHandle = nullptr;
        }
        // GetAppIDForCurrentPipe is self-restoring; only the running-game
        // site might still be armed.
        if (g_initialRunningGameTarget && *g_initialRunningGameTarget == 0xCC) {
            VehCommon::RestoreByte(g_initialRunningGameTarget, 0x48);
        }
        g_oGetAppIDForCurrentPipe   = nullptr;
        g_steamEngine               = nullptr;
        g_initialRunningGameTarget  = nullptr;
    }

    AppId_t GetAppIDForCurrentPipe() {
        if (!g_steamEngine || !g_oGetAppIDForCurrentPipe) {
            LOG_MISC_WARN("GetAppIDForCurrentPipe called before capture — returning 0");
            return 0;
        }
        auto appid =  g_oGetAppIDForCurrentPipe(g_steamEngine);
        LOG_MISC_INFO("GetAppIDForCurrentPipe: AppId={}", appid);
        return appid;
    }

    AppId_t GetAppIDFromInitialRunningGame() {
        if (!g_initialRunningGameTarget) {
            LOG_MISC_WARN("GetAppIDFromInitialRunningGame called before capture — returning 0");
            return 0;
        }
        LOG_MISC_INFO("GetAppIDFromInitialRunningGame: returning AppId={}", g_runningAppId);
        return g_runningAppId;
    }
}
