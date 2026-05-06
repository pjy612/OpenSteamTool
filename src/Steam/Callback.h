#pragma once

// ── ISteamUser callbacks (base = 100) ───────────────────────────────

constexpr int k_iSteamUserCallbacks = 100;

//-----------------------------------------------------------------------------
// Purpose: Result from RequestEncryptedAppTicket (async)
//-----------------------------------------------------------------------------
struct EncryptedAppTicketResponse_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 54 };

	EResult m_eResult;
};
