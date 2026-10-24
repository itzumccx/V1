#pragma once
#include "framework.h"

// =====================================================================
// Rift Gameserver - v17.30
// Misc.h - Miscellaneous hooks (kick, context checks, session, etc.)
// =====================================================================

namespace Misc
{
    // -----------------------------------------------------------------
    // KickPlayer - no-op stub (prevent engine from booting players)
    // -----------------------------------------------------------------
    static void KickPlayer(AGameSession* GameSession, AController* Controller)
    {
        if (Controller && Controller->PlayerState)
            Log(std::format("KickPlayer suppressed for: {}", Controller->PlayerState->GetPlayerName().ToString()));
    }

    // -----------------------------------------------------------------
    // CanCreateInCurrentContext - always allow actor creation
    // -----------------------------------------------------------------
    bool CanCreateInCurrentContext()
    {
        return true;
    }

    // -----------------------------------------------------------------
    // DispatchRequest - force request type 3 (server mode)
    // -----------------------------------------------------------------
    void (*DispatchRequestOG)(__int64 a1, unsigned __int64* a2, int a3);
    void DispatchRequest(__int64 a1, unsigned __int64* a2, int a3)
    {
        return DispatchRequestOG(a1, a2, 3);
    }

    // -----------------------------------------------------------------
    // HookAll - install all misc hooks
    // -----------------------------------------------------------------
    void HookAll()
    {
        // Suppress kick
        MH_CreateHook((LPVOID)(ImageBase + 0x4D68614), KickPlayer, nullptr);

        // Suppress ChangeGameSessionId
        MH_CreateHook((LPVOID)(ImageBase + 0x28932FC), nullFunc, nullptr);

        // Patch DispatchRequest to always use type 3
        MH_CreateHook((LPVOID)(ImageBase + 0x15A07D8), DispatchRequest, (LPVOID*)&DispatchRequestOG);

        // Allow actor/navmesh creation in server context
        HookVTable(AActor::GetDefaultObj(),           0x1B, CanCreateInCurrentContext, nullptr);
        HookVTable(AAthenaAIDirector::GetDefaultObj(), 0x1B, CanCreateInCurrentContext, nullptr);
        HookVTable(AAthenaNavMesh::GetDefaultObj(),    0x1B, CanCreateInCurrentContext, nullptr);

        // Suppress various engine callbacks that cause issues in server mode
        MH_CreateHook((LPVOID)(ImageBase + 0x1371e94), nullFunc, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0x2f327b0), nullFunc, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0x28932fc), nullFunc, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0x4efb7dc), nullFunc, nullptr);

        // Force dedicated server checks to return true
        MH_CreateHook((LPVOID)(ImageBase + 0x4d79308), IsTrue, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0x46CF05C), IsTrue, nullptr);

        Log("Misc hooked!");
    }
}
