#pragma once
#include "framework.h"
#include "FortInventory.h"
#include "Looting.h"

// =====================================================================
// Rift Gameserver - v17.30
// FortPlayerControllerAthena.h - Player controller server RPCs
// =====================================================================

namespace FortPlayerControllerAthena
{
    // -----------------------------------------------------------------
    // ServerAcknowledgePossession
    // Store the acknowledged pawn reference before calling original.
    // -----------------------------------------------------------------
    void (*ServerAcknowledgePossessionOG)(AFortPlayerControllerAthena* This, AFortPlayerPawnAthena* Pawn);
    void ServerAcknowledgePossession(AFortPlayerControllerAthena* This, AFortPlayerPawnAthena* Pawn)
    {
        Log("ServerAcknowledgePossession called");
        This->AcknowledgedPawn = Pawn;
        return ServerAcknowledgePossessionOG(This, Pawn);
    }

    // -----------------------------------------------------------------
    // ServerReadyToStartMatch
    // First player ready: spawn floor loot then let engine proceed.
    // -----------------------------------------------------------------
    void (*ServerReadyToStartMatchOG)(AFortPlayerControllerAthena* PC);
    void ServerReadyToStartMatch(AFortPlayerControllerAthena* PC)
    {
        if (!PC)
        {
            Log("ServerReadyToStartMatch: No PC!");
            return;
        }

        static bool bSetupWorld = false;
        if (!bSetupWorld)
        {
            bSetupWorld = true;
            Looting::SpawnFloorLoot();
            Log("World setup complete!");
        }

        return ServerReadyToStartMatchOG(PC);
    }

    // -----------------------------------------------------------------
    // ServerExecuteInventoryItem
    // Equip the item the player selected from their inventory bar.
    // -----------------------------------------------------------------
    void (*ServerExecuteInventoryItemOG)(AFortPlayerControllerAthena* PC, FGuid& ItemGuid);
    void ServerExecuteInventoryItem(AFortPlayerControllerAthena* PC, FGuid& ItemGuid)
    {
        if (!PC || PC->IsInAircraft())
            return ServerExecuteInventoryItemOG(PC, ItemGuid);

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;
        FFortItemEntry* ItemEntry   = FortInventory::FindItemEntry(PC, ItemGuid);

        if (ItemEntry)
        {
            Pawn->EquipWeaponDefinition(
                (UFortWeaponItemDefinition*)ItemEntry->ItemDefinition,
                ItemEntry->ItemGuid, ItemEntry->TrackerGuid, false);
        }

        return ServerExecuteInventoryItemOG(PC, ItemGuid);
    }

    // -----------------------------------------------------------------
    // ServerAttemptAircraftJump
    // Spawn the pawn and begin skydiving when the player jumps off the bus.
    // -----------------------------------------------------------------
    void ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* Comp, FRotator ClientRotation)
    {
        auto PC = (AFortPlayerControllerAthena*)Comp->GetOwner();
        UWorld::GetWorld()->AuthorityGameMode->RestartPlayer(PC);

        if (PC->MyFortPawn)
        {
            PC->ClientSetRotation(ClientRotation, true);
            PC->MyFortPawn->BeginSkydiving(true);
            PC->MyFortPawn->SetHealth(100);
            PC->MyFortPawn->SetShield(0);
        }
    }

    // -----------------------------------------------------------------
    // ServerAttemptInventoryDrop
    // Drop an item from the inventory onto the ground.
    // -----------------------------------------------------------------
    void ServerAttemptInventoryDrop(AFortPlayerControllerAthena* PC, FGuid ItemGuid, int Count, bool bTrash)
    {
        if (!PC || !PC->Pawn) return;

        FFortItemEntry* Entry = FortInventory::FindItemEntry(PC, ItemGuid);
        if (!Entry) return;

        AFortPlayerPawn* Pawn = (AFortPlayerPawn*)PC->Pawn;
        SpawnPickup(Entry->ItemDefinition, Count, Entry->LoadedAmmo,
                    PC->Pawn->K2_GetActorLocation(),
                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
        FortInventory::RemoveItem(PC, Entry->ItemDefinition, Count);
    }

    // -----------------------------------------------------------------
    // ServerCheat
    // Development console commands - disabled on production servers.
    // -----------------------------------------------------------------
    void ServerCheat(AFortPlayerControllerAthena* PC, FString& Msg)
    {
        if (Globals::bIsProdServer) return;
        if (!PC || !PC->Pawn)      return;

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;
        std::string Command         = Msg.ToString();
        Log("ServerCheat: " + Command);

        if (Command == "GodMode")
        {
            PC->MyFortPawn->bIsInvulnerable = !PC->MyFortPawn->bIsInvulnerable;
            Log(std::string("GodMode: ") + (PC->MyFortPawn->bIsInvulnerable ? "ON" : "OFF"));
        }
        else if (Command == "DumpLoc")
        {
            FVector Loc = PC->Pawn->K2_GetActorLocation();
            Log(std::format("Location: X={:.1f} Y={:.1f} Z={:.1f}", Loc.X, Loc.Y, Loc.Z));
        }
        else if (Command.starts_with("Teleport "))
        {
            std::vector<std::string> args = TextManipUtils::SplitWhitespace(Command);
            if (args.size() >= 4)
            {
                FVector TeleportLoc{};
                TeleportLoc.X = std::stof(args[1]);
                TeleportLoc.Y = std::stof(args[2]);
                TeleportLoc.Z = std::stof(args[3]);

                if (!PC->Pawn->K2_TeleportTo(TeleportLoc, PC->Pawn->K2_GetActorRotation()))
                {
                    FHitResult HitResult{};
                    Pawn->K2_SetActorLocation(TeleportLoc, false, &HitResult, true);
                }
                Log(std::format("Teleported to X={} Y={} Z={}", args[1], args[2], args[3]));
            }
        }
        else if (Command == "startaircraft")
        {
            UKismetSystemLibrary::GetDefaultObj()->ExecuteConsoleCommand(UWorld::GetWorld(), TEXT("startaircraft"), nullptr);
        }
        else if (Command == "pausesafezone")
        {
            UKismetSystemLibrary::GetDefaultObj()->ExecuteConsoleCommand(UWorld::GetWorld(), TEXT("pausesafezone"), nullptr);
        }
    }

    // -----------------------------------------------------------------
    // ServerCreateBuildingActor
    // Validate, deduct materials, spawn the building piece.
    // -----------------------------------------------------------------
    void (*ServerCreateBuildingActorOG)(AFortPlayerControllerAthena* PC, FCreateBuildingActorData& CreateBuildingData);
    void ServerCreateBuildingActor(AFortPlayerControllerAthena* PC, FCreateBuildingActorData& CreateBuildingData)
    {
        if (!PC) return;

        UClass* BuildingClass = PC->BroadcastRemoteClientInfo->RemoteBuildableClass.Get();

        TArray<ABuildingSMActor*> BuildingsToRemove;
        char BuildRestrictionFlag = 0;
        if (CantBuild(UWorld::GetWorld(), BuildingClass,
                      CreateBuildingData.BuildLoc, CreateBuildingData.BuildRot,
                      CreateBuildingData.bMirrored, &BuildingsToRemove, &BuildRestrictionFlag))
        {
            Log("ServerCreateBuildingActor: CantBuild returned true");
            return;
        }

        auto ResourceItemDef = UFortKismetLibrary::GetDefaultObj()->K2_GetResourceItemDefinition(
            ((ABuildingSMActor*)BuildingClass->DefaultObject)->ResourceType);
        FortInventory::RemoveItem(PC, ResourceItemDef, 10);

        ABuildingSMActor* PlacedBuilding = SpawnActor<ABuildingSMActor>(
            CreateBuildingData.BuildLoc, CreateBuildingData.BuildRot, PC, BuildingClass);
        PlacedBuilding->bPlayerPlaced = true;
        PlacedBuilding->InitializeKismetSpawnedBuildingActor(PlacedBuilding, PC, true, nullptr);
        PlacedBuilding->TeamIndex = ((AFortPlayerStateAthena*)PC->PlayerState)->TeamIndex;
        PlacedBuilding->Team      = EFortTeam(PlacedBuilding->TeamIndex);

        for (size_t i = 0; i < BuildingsToRemove.Num(); i++)
            BuildingsToRemove[i]->K2_DestroyActor();
        BuildingsToRemove.Free();
    }

    // -----------------------------------------------------------------
    // ServerBeginEditingBuildingActor
    // Mark the building as being edited and equip the edit tool.
    // -----------------------------------------------------------------
    void (*ServerBeginEditingBuildingActorOG)(AFortPlayerControllerAthena* PC, ABuildingSMActor* BuildingActorToEdit);
    void ServerBeginEditingBuildingActor(AFortPlayerControllerAthena* PC, ABuildingSMActor* BuildingActorToEdit)
    {
        if (!BuildingActorToEdit || !BuildingActorToEdit->bPlayerPlaced || !PC->MyFortPawn) return;

        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)PC->PlayerState;
        BuildingActorToEdit->SetNetDormancy(ENetDormancy::DORM_Awake);
        BuildingActorToEdit->EditingPlayer = PlayerState;

        for (int i = 0; i < PC->WorldInventory->Inventory.ItemInstances.Num(); i++)
        {
            auto Item = PC->WorldInventory->Inventory.ItemInstances[i];
            if (Item->GetItemDefinitionBP()->IsA(UFortEditToolItemDefinition::StaticClass()))
            {
                PC->MyFortPawn->EquipWeaponDefinition(
                    (UFortWeaponItemDefinition*)Item->GetItemDefinitionBP(),
                    Item->GetItemGuid(), Item->GetTrackerGuid(), false);
                break;
            }
        }

        if (!PC->MyFortPawn->CurrentWeapon || !PC->MyFortPawn->CurrentWeapon->IsA(AFortWeap_EditingTool::StaticClass()))
            return;

        AFortWeap_EditingTool* EditToolWeapon = (AFortWeap_EditingTool*)PC->MyFortPawn->CurrentWeapon;
        EditToolWeapon->EditActor = BuildingActorToEdit;
        EditToolWeapon->OnRep_EditActor();

        return ServerBeginEditingBuildingActorOG(PC, BuildingActorToEdit);
    }

    // -----------------------------------------------------------------
    // ServerEndEditingBuildingActor
    // Remove the editing lock and re-equip the edit tool.
    // -----------------------------------------------------------------
    void ServerEndEditingBuildingActor(AFortPlayerControllerAthena* PC, ABuildingSMActor* BuildingActorToStopEditing)
    {
        if (!BuildingActorToStopEditing || !PC->MyFortPawn
            || BuildingActorToStopEditing->bDestroyed == 1
            || BuildingActorToStopEditing->EditingPlayer != PC->PlayerState)
            return;

        BuildingActorToStopEditing->SetNetDormancy(ENetDormancy::DORM_DormantAll);
        BuildingActorToStopEditing->EditingPlayer = nullptr;

        for (size_t i = 0; i < PC->WorldInventory->Inventory.ItemInstances.Num(); i++)
        {
            auto Item = PC->WorldInventory->Inventory.ItemInstances[i];
            if (Item->GetItemDefinitionBP()->IsA(UFortEditToolItemDefinition::StaticClass()))
            {
                PC->MyFortPawn->EquipWeaponDefinition(
                    (UFortWeaponItemDefinition*)Item->GetItemDefinitionBP(),
                    Item->GetItemGuid(), Item->GetTrackerGuid(), false);
                break;
            }
        }

        if (!PC->MyFortPawn->CurrentWeapon || !PC->MyFortPawn->CurrentWeapon->WeaponData
            || !PC->MyFortPawn->CurrentWeapon->IsA(AFortWeap_EditingTool::StaticClass()))
            return;

        AFortWeap_EditingTool* EditToolWeapon = (AFortWeap_EditingTool*)PC->MyFortPawn->CurrentWeapon;
        EditToolWeapon->EditActor = nullptr;
        EditToolWeapon->OnRep_EditActor();
    }

    // -----------------------------------------------------------------
    // ServerEditBuildingActor
    // Replace a building piece with a new shape/rotation.
    // -----------------------------------------------------------------
    void ServerEditBuildingActor(
        AFortPlayerControllerAthena* PC,
        ABuildingSMActor*             BuildingActorToEdit,
        TSubclassOf<ABuildingSMActor> NewBuildingClass,
        uint8                         RotationIterations,
        bool                          bMirrored)
    {
        if (!BuildingActorToEdit
            || BuildingActorToEdit->EditingPlayer != PC->PlayerState
            || !NewBuildingClass.Get()
            || BuildingActorToEdit->bDestroyed == 1)
            return;

        BuildingActorToEdit->SetNetDormancy(ENetDormancy::DORM_DormantAll);
        BuildingActorToEdit->EditingPlayer = nullptr;

        ABuildingSMActor* EditedBuilding = ReplaceBuildingActor(
            BuildingActorToEdit, 1, NewBuildingClass.Get(), 0, RotationIterations, bMirrored, PC);
        if (EditedBuilding)
            EditedBuilding->bPlayerPlaced = true;
    }

    // -----------------------------------------------------------------
    // ServerRepairBuildingActor
    // Restore building HP in exchange for materials.
    // -----------------------------------------------------------------
    void ServerRepairBuildingActor(AFortPlayerControllerAthena* PC, ABuildingSMActor* BuildingActorToRepair)
    {
        if (!BuildingActorToRepair || BuildingActorToRepair->EditingPlayer) return;

        float BuildingHealthPercentLost = 1.0f - BuildingActorToRepair->GetHealthPercent();
        float RepairCostUnrounded       = (10.f * BuildingHealthPercentLost) * 0.75f;
        float RepairCost = std::floor(RepairCostUnrounded > 0
            ? (RepairCostUnrounded < 1 ? 1 : RepairCostUnrounded)
            : 0);
        if (RepairCost < 0) return;

        auto ResourceDef = UFortKismetLibrary::GetDefaultObj()->K2_GetResourceItemDefinition(BuildingActorToRepair->ResourceType);
        if (!ResourceDef) return;

        if (!PC->bBuildFree)
            FortInventory::RemoveItem(PC, ResourceDef, (int)RepairCost);

        BuildingActorToRepair->RepairBuilding(PC, (int)RepairCost);
    }

    // -----------------------------------------------------------------
    // HookAll
    // -----------------------------------------------------------------
    void HookAll()
    {
        HookVTable(AFortPlayerControllerAthena::GetDefaultObj(), 0x114,
                   ServerAcknowledgePossession, (LPVOID*)&ServerAcknowledgePossessionOG);

        HookVTable(AFortPlayerControllerAthena::GetDefaultObj(), 0x278,
                   ServerReadyToStartMatch, (LPVOID*)&ServerReadyToStartMatchOG);

        HookVTable(AFortPlayerControllerAthena::GetDefaultObj(), 0x215,
                   ServerExecuteInventoryItem, (LPVOID*)&ServerExecuteInventoryItemOG);

        HookVTable(UFortControllerComponent_Aircraft::GetDefaultObj(), 0x94,
                   ServerAttemptAircraftJump, nullptr);

        HookVTable(AFortPlayerControllerAthena::GetDefaultObj(), 0x225,
                   ServerAttemptInventoryDrop, nullptr);

        HookVTable(AFortPlayerControllerAthena::GetDefaultObj(), 0x1CF,
                   ServerCheat, nullptr);

        HookVTable(AAthena_PlayerController_C::GetDefaultObj(), 0x239,
                   ServerCreateBuildingActor, (LPVOID*)&ServerCreateBuildingActorOG);

        HookVTable(AAthena_PlayerController_C::GetDefaultObj(), 0x240,
                   ServerBeginEditingBuildingActor, (LPVOID*)&ServerBeginEditingBuildingActorOG);

        HookVTable(AAthena_PlayerController_C::GetDefaultObj(), 0x23E,
                   ServerEndEditingBuildingActor, nullptr);

        HookVTable(AAthena_PlayerController_C::GetDefaultObj(), 0x23B,
                   ServerEditBuildingActor, nullptr);

        HookVTable(AAthena_PlayerController_C::GetDefaultObj(), 0x235,
                   ServerRepairBuildingActor, nullptr);

        Log("FortPlayerControllerAthena hooked!");
    }
}
