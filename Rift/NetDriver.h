#pragma once
#include "framework.h"
#include "FortGameModeAthena.h"

// =====================================================================
// Rift Gameserver - v17.30
// NetDriver.h - TickFlush (main server loop), tick rate, net mode
// =====================================================================

namespace NetDriver
{
    // -----------------------------------------------------------------
    // Map the current game phase + state into a fine-grained phase step
    // -----------------------------------------------------------------
    EAthenaGamePhaseStep GetCurrentGamePhaseStep(AFortGameModeAthena* GameMode, AFortGameStateAthena* GameState)
    {
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        switch (GameState->GamePhase)
        {
        case EAthenaGamePhase::Setup:
            return EAthenaGamePhaseStep::Setup;

        case EAthenaGamePhase::Warmup:
            return (GameState->WarmupCountdownEndTime > CurrentTime + 10.f)
                ? EAthenaGamePhaseStep::Warmup
                : EAthenaGamePhaseStep::GetReady;

        case EAthenaGamePhase::Aircraft:
            return GameState->bAircraftIsLocked
                ? EAthenaGamePhaseStep::BusLocked
                : EAthenaGamePhaseStep::BusFlying;

        case EAthenaGamePhase::SafeZones:
            if (!GameState->SafeZoneIndicator)        return EAthenaGamePhaseStep::StormForming;
            if (GameState->SafeZoneIndicator->bPaused) return EAthenaGamePhaseStep::StormHolding;
            return EAthenaGamePhaseStep::StormShrinking;

        case EAthenaGamePhase::EndGame:
            return EAthenaGamePhaseStep::EndGame;

        case EAthenaGamePhase::Count:
            return EAthenaGamePhaseStep::Count;

        default:
            return EAthenaGamePhaseStep::EAthenaGamePhaseStep_MAX;
        }
    }

    // -----------------------------------------------------------------
    // TickFlush - fires every network tick
    // Handles: replication, phase step sync, warmup countdown,
    //          bot spawning logic, bot ticking.
    // -----------------------------------------------------------------
    void (*TickFlushOG)(UNetDriver* This, float DeltaSeconds);
    void TickFlush(UNetDriver* This, float DeltaSeconds)
    {
        if (!This) return;

        AFortGameModeAthena*  GameMode  = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        // Replicate to connected clients
        if (This->ClientConnections.Num() > 0)
            ServerReplicateActors(This->ReplicationDriver, DeltaSeconds);

        if (GameMode && GameState)
        {
            // ---- Phase step sync ----
            EAthenaGamePhaseStep CurrentStep = GetCurrentGamePhaseStep(GameMode, GameState);
            if (CurrentStep != GameState->GamePhaseStep)
                GameState->GamePhaseStep = CurrentStep;

            // ---- Warmup countdown expiry → start aircraft ----
            float TimeLeft = GameState->WarmupCountdownEndTime - UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
            if (TimeLeft <= 0.f && GameState->GamePhase == EAthenaGamePhase::Warmup)
                FortGameModeAthena::StartAircraftPhase(GameMode, false);

            // ---- Bot fill during warmup ----
            if (GameState->GamePhase == EAthenaGamePhase::Warmup
                && GameMode->AlivePlayers.Num() > 0
                && (GameMode->AlivePlayers.Num() + GameMode->AliveBots.Num()) < GameMode->GameSession->MaxPlayers
                && GameMode->AliveBots.Num() < Globals::MaxBotsToSpawn
                && Globals::bBotsEnabled)
            {
                // Staggered random spawning (~4.5% chance per tick) to avoid frame spikes
                if (UKismetMathLibrary::GetDefaultObj()->RandomBoolWithWeight(0.045f))
                {
                    // Bot spawner integration point - left for the bots module
                    // BotsSpawner::SpawnPlayerBot();
                }
            }
        }

        return TickFlushOG(This, DeltaSeconds);
    }

    // -----------------------------------------------------------------
    // Cap server tick rate to 30 Hz
    // -----------------------------------------------------------------
    float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing = true)
    {
        return 30.f;
    }

    // -----------------------------------------------------------------
    // Always report dedicated server net mode
    // -----------------------------------------------------------------
    int GetNetMode()
    {
        return (int)ENetMode::DedicatedServer;
    }

    // -----------------------------------------------------------------
    // HookAll
    // -----------------------------------------------------------------
    void HookAll()
    {
        // Main replication/tick loop
        MH_CreateHook((LPVOID)(ImageBase + 0xE4043C), TickFlush, (LPVOID*)&TickFlushOG);

        // Tick rate cap
        MH_CreateHook((LPVOID)(ImageBase + 0x104914C), GetMaxTickRate, nullptr);

        // Net mode overrides (UWorld, AActor, UNetDriver)
        MH_CreateHook((LPVOID)(ImageBase + 0xCD2164), GetNetMode, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0xE44930), GetNetMode, nullptr);
        MH_CreateHook((LPVOID)(ImageBase + 0xE433AC), GetNetMode, nullptr);

        Log("NetDriver hooked!");
    }
}
