#include "Hooks_IPC_ISteamUtils.h"
#include "Hooks_IPC.h"
#include "Hooks_IPC_ISteamUser.h"
#include "Steam/Callback.h"
#include "Utils/Log.h"

namespace {

    constexpr uint32 HASH_IClientUtils_GetAppID              = 0x09607EC4;
    constexpr uint32 HASH_IClientUtils_GetAPICallResult      = 0x2D3D3947;

    // ── GetAPICallResult request layout (after 10-byte header) ────
    //  offset 10: hSteamAPICall      (uint64)
    //  offset 18: cubCallback        (uint32)
    //  offset 22: iCallbackExpected  (uint32)
    constexpr int GACR_OFF_HASYNCCALL   = OFFSET_ARGS;
    constexpr int GACR_OFF_CUBCALLBACK  = OFFSET_ARGS + 8;
    constexpr int GACR_OFF_CALLBACKID   = OFFSET_ARGS + 12;
    constexpr int GACR_ARGS_SIZE        = 16;  // 8+4+4

    // ── Helper: write the GetAPICallResult response boilerplate ───
    //  Response layout: prefix(1) + retval(1) + CallbackT + pbFailed(1)
    //
    //  Uses a lambda Fill(CallbackT&) to populate the struct, keeping
    //  per-callback logic concise.  Returns false when pWrite is too small.
    template<typename CallbackT, typename F>
    bool WriteCallbackResponse(CUtlBuffer* pWrite, F&& fill)
    {
        constexpr int32 total = 1 + 1 + sizeof(CallbackT) + 1;
        if (pWrite->m_Put < total) return false;

        uint8* base = pWrite->m_Memory.m_pMemory;
        base[0] = RESPONSE_PREFIX;
        base[1] = 1;                           // retval = true
        base[2 + sizeof(CallbackT)] = 0;       // *pbFailed = false

        auto* cb = reinterpret_cast<CallbackT*>(base + 2);
        fill(*cb);
        return true;
    }

    // ── Handler: IClientUtils::GetAppID ───────────────────────────
    //  Request:  no args
    //  Response: [uint8 prefix=0x0B][uint32 AppID]  (5 bytes)
    //
    //  Passthrough for now - the real Steam already returns the
    //  correct AppID resolved from the pipe.
    void Handler_IClientUtils_GetAppID(CUtlBuffer*, const uint8*, int32, AppId_t)
    {
        // No spoofing needed for now.
    }

    // ════════════════════════════════════════════════════════════════
    //  GetAPICallResult per-callback handlers
    //
    //  Each handler receives the already-parsed fields plus the
    //  pWrite buffer.  Return true when the response was spoofed.
    // ════════════════════════════════════════════════════════════════

    bool HandleCallback_EncryptedAppTicketResponse(
        CUtlBuffer* pWrite, uint64 hAsyncCall, int32 cubCallback)
    {
        AppId_t appId = Hooks_IPC_ISteamUser::LookupEticketAsyncCall(hAsyncCall);
        if (!appId) return false;

        LOG_IPC_DEBUG("GetAPICallResult: EncryptedAppTicketResponse hAsyncCall=0x{:016X} "
                  "AppId={} - injecting k_EResultOK", hAsyncCall, appId);

        if (!WriteCallbackResponse<EncryptedAppTicketResponse_t>(pWrite, [](auto& cb) {
            cb.m_eResult = k_EResultOK;
        })) return false;

        Hooks_IPC_ISteamUser::EraseEticketAsyncCall(hAsyncCall);
        return true;
    }

    // ── Dispatch table entry ──────────────────────────────────────
    struct GacrDispatchEntry {
        int32    callbackId;
        bool   (*handler)(CUtlBuffer* pWrite, uint64 hAsyncCall, int32 cubCallback);
    };

    constexpr GacrDispatchEntry g_GacrDispatch[] = {
        { EncryptedAppTicketResponse_t::k_iCallback, HandleCallback_EncryptedAppTicketResponse },
    };

    // ── Handler: IClientUtils::GetAPICallResult ───────────────────
    void Handler_IClientUtils_GetAPICallResult(
        CUtlBuffer* pWrite, const uint8* reqData, int32 reqSize, AppId_t appId)
    {
        if (reqSize < OFFSET_ARGS + GACR_ARGS_SIZE) return;

        const uint64 hAsyncCall = *reinterpret_cast<const uint64*>(reqData + GACR_OFF_HASYNCCALL);
        const uint32 iCallback  = *reinterpret_cast<const uint32*>(reqData + GACR_OFF_CALLBACKID);
        const int32  cubCallback = static_cast<int32>(*reinterpret_cast<const uint32*>(reqData + GACR_OFF_CUBCALLBACK));
        LOG_IPC_DEBUG("GetAPICallResult: hAsyncCall=0x{:016X} AppId={} iCallback={} cubCallback={}",
                  hAsyncCall, appId, iCallback, cubCallback);
        for (auto& entry : g_GacrDispatch) {
            if (entry.callbackId == iCallback) {
                entry.handler(pWrite, hAsyncCall, cubCallback);
                return;
            }
        }
    }

    const Hooks_IPC::IpcHandlerEntry g_Entries[] = {
        { EIPCInterface::IClientUtils, HASH_IClientUtils_GetAppID,
          "IClientUtils::GetAppID",
          Handler_IClientUtils_GetAppID },
        { EIPCInterface::IClientUtils, HASH_IClientUtils_GetAPICallResult,
          "IClientUtils::GetAPICallResult",
          Handler_IClientUtils_GetAPICallResult },
    };

} // namespace

namespace Hooks_IPC_ISteamUtils {
    void Register() {
        Hooks_IPC::RegisterHandlers(g_Entries, std::size(g_Entries));
    }
}
