#pragma once
#include "Steam/Types.h"

namespace Hooks_Manifest {
    void Install();
    void Uninstall();

    // Fetch a manifest request code from online providers.
    // Thread-safe — serialises access to the underlying WinHTTP connection.
    // Returns true and sets *outRequestCode on success.
    bool FetchManifestRequestCode(uint64 manifestGid, uint64* outRequestCode);
}
