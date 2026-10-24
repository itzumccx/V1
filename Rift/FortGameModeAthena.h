#pragma once
#include "framework.h"
#include "FortInventory.h"
#include "AbilitySystemComponent.h"

// =====================================================================
// Rift Gameserver - v17.30
// FortGameModeAthena.h - Match lifecycle hooks
//   ReadyToStartMatch, SpawnDefaultPawnFor, aircraft phases, safe zones,
//   team assignment
// =====================================================================

namespace FortGameModeAthena
{
    // -----------------------------------------------------------------
    // ReadyToStartMatch
    // Runs every tick while in the lobby/warmup phase.
    // Sets up playlist, bot manager, networking, and warmup timer.
    // -----------------------------------------------------------------
    bool ReadyToStartMatch(AFortGameModeAthena* GameMode)
    {
        static bool bSetupPlaylist = false;
        static bool bInitialized   = false;
        static bool bListening     = false;

        if (!GameMode) return false;

        AFortGameStateAthena* GameState = (AFortGameStateAthena*)GameMode->GameState;
        if (!GameState) return false;

        float CurrentTime = UGameplayStatics::GetDefaultObj()->GetTimeSeconds(UWorld::GetWorld());
        float WarmupTime  = 60.f;

        // ---- 1. Setup playlist ----
        if (!bSetupPlaylist)
        {
            bSetupPlaylist = true;

            UFortPlaylistAthena* Playlist = nullptr;
            if (Globals::bEventEnabled)
                Playlist = StaticLoadObject<UFortPlaylistAthena>("/BuffetPlaylist/Playlist/Playlist_Buffet.Playlist_Buffet");
            else
                Playlist = StaticLoadObject<UFortPlaylistAthena>("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");

            if (!Playlist) { Log("ReadyToStartMatch: Playlist not found!"); return false; }

            GameState->CurrentPlaylistInfo.BasePlaylist     = Playlist;
            GameState->CurrentPlaylistInfo.OverridePlaylist = Playlist;
            GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
            GameState->CurrentPlaylistInfo.MarkArrayDirty();
            GameState->OnRep_CurrentPlaylistInfo();

            GameMode->CurrentPlaylistName         = Playlist->PlaylistName;
            GameState->CurrentPlaylistId          = Playlist->PlaylistId;
            GameState->OnRep_CurrentPlaylistId();

            GameState->WarmupCountdownEndTime      = CurrentTime + WarmupTime;
            GameMode->WarmupCountdownDuration      = WarmupTime;
            GameState->WarmupCountdownStartTime    = CurrentTime;
            GameMode->WarmupEarlyCountdownDuration = WarmupTime;

            GameMode->GameSession->MaxPlayers                 = Playlist->MaxPlayers;
            GameMode->GameSession->MaxSpectators              = 0;
            GameMode->GameSession->MaxPartySize               = Playlist->MaxSquadSize;
            GameMode->GameSession->MaxSplitscreensPerConnection = 2;
            GameMode->GameSession->bRequiresPushToTalk        = false;
            GameMode->GameSession->SessionName                = UKismetStringLibrary::Conv_StringToName(FString(L"GameSession"));

            Globals::MaxPlayersPerTeam = Playlist->MaxSquadSize;
            Globals::NextTeamIndex     = Playlist->DefaultFirstTeam;

            Log("Playlist set up: " + Playlist->GetName());
        }

        if (!GameState->MapInfo) return false;

        // ---- 2. One-time initialisation ----
        if (!bInitialized)
        {
            bInitialized = true;

            GameState->OnRep_CurrentPlaylistId();
            GameState->OnRep_CurrentPlaylistInfo();

            GameMode->bAllowSpectateAfterDeath             = true;
            GameMode->MinRespawnDelay                      = 5.0f;
            GameMode->WarmupRequiredPlayerCount            = 1;
            GameState->DefaultParachuteDeployTraceForGroundDistance = 10000;

            // ---- Bot manager ----
            if (auto BotManager = (UFortServerBotManagerAthena*)
                    UGameplayStatics::SpawnObject(UFortServerBotManagerAthena::StaticClass(), GameMode))
            {
                GameMode->AISettings             = GameState->CurrentPlaylistInfo.BasePlaylist->AISettings;
                GameMode->AISettings->AIServices[1] = UAthenaAIServicePlayerBots::StaticClass();
                GameMode->ServerBotManager       = BotManager;
                BotManager->CachedGameState      = GameState;
                BotManager->CachedGameMode       = GameMode;

                *(bool*)(__int64(GameMode->ServerBotManager) + 0x458) = true;
                GameMode->ServerBotManager->CachedAIPopulationTracker =
                    ((UAthenaAISystem*)UWorld::GetWorld()->AISystem)->AIPopulationTracker;

                if (!GameMode->SpawningPolicyManager)
                    GameMode->SpawningPolicyManager = SpawnActor<AFortAthenaSpawningPolicyManager>({}, {});

                GameMode->SpawningPolicyManager->GameModeAthena  = GameMode;
                GameMode->SpawningPolicyManager->GameStateAthena = GameState;

                // Locate or create bot mutator
                BotMutator = GameMode->ServerBotManager->CachedBotMutator;
                if (!BotMutator)
                    BotMutator = (AFortAthenaMutator_Bots*)GameMode->GetMutatorByClass(GameMode, AFortAthenaMutator_Bots::StaticClass());
                if (!BotMutator)
                    BotMutator = (AFortAthenaMutator_Bots*)GameMode->GetMutatorByClass(GameMode->GameState, AFortAthenaMutator_Bots::StaticClass());
                if (!BotMutator)
                    BotMutator = SpawnActor<AFortAthenaMutator_Bots>({});

                BotManager->CachedBotMutator  = BotMutator;
                BotMutator->CachedGameMode    = GameMode;
                BotMutator->CachedGameState   = GameState;

                // AI director
                GameMode->AIDirector = SpawnActor<AAthenaAIDirector>({});
                if (GameMode->AIDirector)
                    GameMode->AIDirector->Activate();
                else
                    Log("No AIDirector spawned!");

                if (!GameMode->AIGoalManager)
                    GameMode->AIGoalManager = SpawnActor<AFortAIGoalManager>({});

                UAISystem::GetDefaultObj()->AILoggingVerbose();

                // ---- Cache cosmetic definitions for bot randomisation ----
                for (size_t i = 0; i < UObject::GObjects->Num(); i++)
                {
                    UObject* Obj = UObject::GObjects->GetByIndex(i);
                    if (!Obj) continue;

                    if (Obj->IsA(UAthenaCharacterItemDefinition::StaticClass()))
                    {
                        std::string Name = ((UAthenaCharacterItemDefinition*)Obj)->Name.ToString();
                        if (Name.contains("Athena_Commando") || Name.contains("CID_Character")
                            || !Name.contains("CID_NPC") || !Name.contains("CID_VIP") || !Name.contains("CID_TBD"))
                            Characters.push_back((UAthenaCharacterItemDefinition*)Obj);
                    }
                    if (Obj->IsA(UAthenaBackpackItemDefinition::StaticClass()))
                        Backpacks.push_back((UAthenaBackpackItemDefinition*)Obj);
                    if (Obj->IsA(UAthenaPickaxeItemDefinition::StaticClass()))
                        Pickaxes.push_back((UAthenaPickaxeItemDefinition*)Obj);
                    if (Obj->IsA(UAthenaDanceItemDefinition::StaticClass()))
                    {
                        std::string Name = ((UAthenaDanceItemDefinition*)Obj)->Name.ToString();
                        if (Name.contains("EID") || !Name.contains("Sync") || !Name.contains("Owned"))
                            Dances.push_back((UAthenaDanceItemDefinition*)Obj);
                    }
                    if (Obj->IsA(UAthenaGliderItemDefinition::StaticClass()))
                        Gliders.push_back((UAthenaGliderItemDefinition*)Obj);
                }

                Log("Bots initialised!");
            }
            else
            {
                Log("BotManager is nullptr!");
            }

            // ---- Stream additional playlist levels ----
            for (auto& Level : GameState->CurrentPlaylistInfo.BasePlaylist->AdditionalLevels)
            {
                bool Success = false;
                ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString());
                if (Success)
                {
                    FAdditionalLevelStreamed lvl{};
                    lvl.bIsServerOnly = false;
                    lvl.LevelName     = Level.ObjectID.AssetPathName;
                    GameState->AdditionalPlaylistLevelsStreamed.Add(lvl);
                }
            }
            for (auto& Level : GameState->CurrentPlaylistInfo.BasePlaylist->AdditionalLevelsServerOnly)
            {
                bool Success = false;
                ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString());
                if (Success)
                {
                    FAdditionalLevelStreamed lvl{};
                    lvl.bIsServerOnly = true;
                    lvl.LevelName     = Level.ObjectID.AssetPathName;
                    GameState->AdditionalPlaylistLevelsStreamed.Add(lvl);
                }
            }
            GameState->OnRep_AdditionalPlaylistLevelsStreamed();
            GameState->OnFinishedStreamingAdditionalPlaylistLevel();

            Log("Initialised!");
        }

        // ---- 3. Start listening for connections ----
        if (!bListening)
        {
            bListening = true;

            auto Beacon = SpawnActor<AFortOnlineBeaconHost>({});
            Beacon->ListenPort = 7777;
            InitHost(Beacon);
            PauseBeaconRequests(Beacon, false);

            UWorld::GetWorld()->NetDriver               = Beacon->NetDriver;
            UWorld::GetWorld()->NetDriver->World        = UWorld::GetWorld();
            UWorld::GetWorld()->NetDriver->NetDriverName =
                UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"GameNetDriver");

            FString Error{};
            FURL URL{};
            URL.Port = 7777;

            if (InitListen(UWorld::GetWorld()->NetDriver, UWorld::GetWorld(), URL, true, Error))
                Log("InitListen successful!");
            else
                Log("InitListen failed: " + Error.ToString());

            SetWorld(UWorld::GetWorld()->NetDriver, UWorld::GetWorld());

            for (int i = 0; i < UWorld::GetWorld()->LevelCollections.Num(); i++)
                UWorld::GetWorld()->LevelCollections[i].NetDriver = UWorld::GetWorld()->NetDriver;

            GameMode->bWorldIsReady = true;
            SetConsoleTitleA("Rift v17.30 | Listening on :7777");
            Log("Server is listening on port 7777!");
        }

        // ---- 4. Allow match to start once at least one player has joined ----
        if (GameState->PlayersLeft > 0)
        {
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), ABuildingFoundation::StaticClass(), &BuildingFoundations);
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerStartWarmup::StaticClass(), &PlayerStarts);
            return true;
        }

        // Reset warmup timer while nobody is connected
        GameState->WarmupCountdownEndTime      = CurrentTime + WarmupTime;
        GameMode->WarmupCountdownDuration      = WarmupTime;
        GameState->WarmupCountdownStartTime    = CurrentTime;
        GameMode->WarmupEarlyCountdownDuration = WarmupTime;

        return false;
    }

    // -----------------------------------------------------------------
    // SpawnDefaultPawnFor
    // Called when the engine needs to create a pawn for a joining player.
    // Gives the default pickaxe, starting items, and ability set.
    // -----------------------------------------------------------------
    APawn* (*SpawnDefaultPawnForOG)(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot);
    APawn* SpawnDefaultPawnFor(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot)
    {
        Log("SpawnDefaultPawnFor called");

        AFortPlayerPawnAthena* Pawn      = (AFortPlayerPawnAthena*)GameMode->SpawnDefaultPawnAtTransform(NewPlayer, StartSpot->GetTransform());
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)NewPlayer->PlayerState;

        // XP component registration
        NewPlayer->XPComponent->bRegisteredWithQuestManager = true;
        NewPlayer->XPComponent->OnRep_bRegisteredWithQuestManager();

        PlayerState->SeasonLevelUIDisplay = NewPlayer->XPComponent->CurrentLevel;
        PlayerState->OnRep_SeasonLevelUIDisplay();

        NewPlayer->GetQuestManager(ESubGame::Athena)->InitializeQuestAbilities(NewPlayer->Pawn);

        // Apply cosmetics
        UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(PlayerState);
        PlayerState->OnRep_CharacterData();

        // Give pickaxe
        FFortAthenaLoadout& Loadout = NewPlayer->CosmeticLoadoutPC;
        UAthenaPickaxeItemDefinition* PickDef = Loadout.Pickaxe
            ? Loadout.Pickaxe
            : StaticLoadObject<UAthenaPickaxeItemDefinition>(
                "/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");

        if (PickDef)
            FortInventory::GiveItem(NewPlayer, PickDef->WeaponDefinition, 1, 0);
        else
            Log("SpawnDefaultPawnFor: pickaxe definition not found!");

        // Give playlist starting items
        for (size_t i = 0; i < GameMode->StartingItems.Num(); i++)
        {
            if (GameMode->StartingItems[i].Count > 0)
                FortInventory::GiveItem(NewPlayer, GameMode->StartingItems[i].Item, GameMode->StartingItems[i].Count, 0);
        }

        // Grant ability set
        AbilitySystemComponent::GiveAbilitySet(
            StaticLoadObject<UFortAbilitySet>("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer"),
            PlayerState);

        // Apply character customization visuals
        ApplyCharacterCustomization(PlayerState, Pawn);

        return Pawn;
    }

    // -----------------------------------------------------------------
    // StartAircraftPhase
    // -----------------------------------------------------------------
    __int64 (*StartAircraftPhaseOG)(AFortGameModeAthena* GameMode, bool bUseAircraftCountdown);
    __int64 StartAircraftPhase(AFortGameModeAthena* GameMode, bool bUseAircraftCountdown)
    {
        Log("StartAircraftPhase called");
        return StartAircraftPhaseOG(GameMode, bUseAircraftCountdown);
    }

    // -----------------------------------------------------------------
    // OnAircraftExitedDropZone
    // -----------------------------------------------------------------
    static inline void (*OriginalOnAircraftExitedDropZone)(AFortGameModeAthena* GameMode, AFortAthenaAircraft* Aircraft);
    void OnAircraftExitedDropZone(AFortGameModeAthena* GameMode, AFortAthenaAircraft* Aircraft)
    {
        Log("OnAircraftExitedDropZone called");
        return OriginalOnAircraftExitedDropZone(GameMode, Aircraft);
    }

    // -----------------------------------------------------------------
    // OnAircraftEnteredDropZone
    // -----------------------------------------------------------------
    void (*OnAircraftEnteredDropZoneOG)(AFortGameModeAthena* GameMode, AFortAthenaAircraft* Aircraft);
    void OnAircraftEnteredDropZone(AFortGameModeAthena* GameMode, AFortAthenaAircraft* Aircraft)
    {
        Log("OnAircraftEnteredDropZone called");
        return OnAircraftEnteredDropZoneOG(GameMode, Aircraft);
    }

    // -----------------------------------------------------------------
    // StartNewSafeZonePhase
    // Read wait/shrink times from the game data curve table and apply them.
    // -----------------------------------------------------------------
    static inline void (*StartNewSafeZonePhaseOG)(AFortGameModeAthena* GameMode, int32 ZoneIndex);
    static void StartNewSafeZonePhase(AFortGameModeAthena* GameMode, int32 ZoneIndex)
    {
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)GameMode->GameState;
        Log("SafeZonePhase: " + std::to_string(GameMode->SafeZonePhase));

        static UCurveTable* AthenaGameData = StaticLoadObject<UCurveTable>(
            UKismetStringLibrary::Conv_NameToString(GameState->CurrentPlaylistInfo.BasePlaylist->GameData.ObjectID.AssetPathName).ToString());
        if (!AthenaGameData)
            AthenaGameData = StaticLoadObject<UCurveTable>("/Game/Athena/Balance/DataTables/AthenaGameData.AthenaGameData");

        // --- Wait time ---
        float WaitTime = 30.f;
        EEvaluateCurveTableResult WaitResult;
        UDataTableFunctionLibrary::EvaluateCurveTableRow(
            AthenaGameData,
            UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.WaitTime"),
            (GameMode->SafeZonePhase + 1),
            &WaitResult, &WaitTime, FString());

        if (WaitResult == EEvaluateCurveTableResult::RowNotFound)
            Log("SafeZone WaitTime row not found - using default 30s");
        else
            GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime =
                UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + WaitTime;

        // --- Shrink time ---
        float ShrinkTime = 30.f;
        EEvaluateCurveTableResult ShrinkResult;
        UDataTableFunctionLibrary::EvaluateCurveTableRow(
            AthenaGameData,
            UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.ShrinkTime"),
            (GameMode->SafeZonePhase + 1),
            &ShrinkResult, &ShrinkTime, FString());

        if (ShrinkResult == EEvaluateCurveTableResult::RowNotFound)
            Log("SafeZone ShrinkTime row not found - using default 30s");
        else
            GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime =
                GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + ShrinkTime;

        GameState->OnRep_SafeZoneIndicator();
        GameState->OnRep_SafeZonePhase();

        return StartNewSafeZonePhaseOG(GameMode, ZoneIndex);
    }

    // -----------------------------------------------------------------
    // PickTeam
    // Round-robin team assignment for real players.
    // -----------------------------------------------------------------
    __int64 (*PickTeamOG)(AFortGameModeAthena* a1, unsigned __int8 a2, AFortPlayerControllerAthena* a3);
    __int64 __fastcall PickTeam(AFortGameModeAthena* a1, unsigned __int8 a2, AFortPlayerControllerAthena* a3)
    {
        int Ret = Globals::NextTeamIndex;
        ++Globals::CurrentPlayersOnTeam;

        if (Globals::CurrentPlayersOnTeam >= Globals::MaxPlayersPerTeam)
        {
            Globals::NextTeamIndex++;
            Globals::CurrentPlayersOnTeam = 0;
        }

        return Ret;
    }

    // -----------------------------------------------------------------
    // HookAll
    // -----------------------------------------------------------------
    void HookAll()
    {
        MH_CreateHook((LPVOID)(ImageBase + 0x478A48C), ReadyToStartMatch, nullptr);

        MH_CreateHook((LPVOID)(ImageBase + 0x4793EFC), SpawnDefaultPawnFor, (LPVOID*)&SpawnDefaultPawnForOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x4796A0C), StartAircraftPhase, (LPVOID*)&StartAircraftPhaseOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x5294FE0), OnAircraftExitedDropZone, (LPVOID*)&OriginalOnAircraftExitedDropZone);

        MH_CreateHook((LPVOID)(ImageBase + 0x5294F44), OnAircraftEnteredDropZone, (LPVOID*)&OnAircraftEnteredDropZoneOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x4799688), StartNewSafeZonePhase, (LPVOID*)&StartNewSafeZonePhaseOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x4785400), PickTeam, (LPVOID*)&PickTeamOG);

        Log("FortGameModeAthena hooked!");
    }
}
