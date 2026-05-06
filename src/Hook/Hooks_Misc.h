#pragma once

#include "dllmain.h"

// Catch-all for the lightweight info-capture int3 traps that don't fit a
// dedicated category — currently:
//   * GetAppIDForCurrentPipe  -> captures the SteamEngine pointer
//   * InitialRunningGame      -> captures the running AppId early in startup
namespace Hooks_Misc {
    void Install();
    void Uninstall();

    // Returns the AppId for the current Steam pipe via the captured engine
    // pointer, or 0 if we haven't yet observed the host calling
    // GetAppIDForCurrentPipe.
    AppId_t GetAppIDForCurrentPipe();

    // Returns the AppId captured from the InitialRunningGame call site.
    // Available earlier in the Steam startup sequence than
    // GetAppIDForCurrentPipe.
    AppId_t GetAppIDFromInitialRunningGame();

    // Grow a CUtlBuffer to at least 'size' bytes and set m_Put = size.
    // Uses CUtlBuffer::EnsureCapacity from steamclient, resolved on first call.
    void EnsureBufferSize(CUtlBuffer* pWrite, int32 size);
}
