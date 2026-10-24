#pragma once

// =====================================================================
// Rift Gameserver - v17.30
// Globals.h - Server-wide configuration and state variables
// =====================================================================

namespace Globals
{
    // -- Server mode flags
    bool bIsProdServer = false;

    bool bCreativeEnabled = false;
    bool bSTWEnabled      = false;
    bool bEventEnabled    = false;

    // -- Bot configuration
    bool bBotsEnabled               = true;
    bool bBotsShouldUseManualTicking = false;

    int MaxBotsToSpawn       = 95;
    int MinPlayersForEarlyStart = 95;

    // -- Team tracking (real players)
    static int NextTeamIndex        = 0;
    static int CurrentPlayersOnTeam = 0;
    static int MaxPlayersPerTeam    = 1;
}
