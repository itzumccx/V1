#pragma once
#include "framework.h"

// =====================================================================
// Rift Gameserver - v17.30
// Looting.h - Loot tier data resolution, container searching, floor loot
// =====================================================================

namespace Looting
{
    // -----------------------------------------------------------------
    // Weighted random selection from a list of LootTierData rows
    // -----------------------------------------------------------------
    static FFortLootTierData* GetLootTierData(std::vector<FFortLootTierData*>& LootTierData)
    {
        float TotalWeight = 0.f;
        for (auto Item : LootTierData)
            TotalWeight += Item->Weight;

        float RandomNumber = UKismetMathLibrary::RandomFloatInRange(0.f, TotalWeight);
        FFortLootTierData* SelectedItem = nullptr;

        for (auto Item : LootTierData)
        {
            if (RandomNumber <= Item->Weight)
            {
                SelectedItem = Item;
                break;
            }
            RandomNumber -= Item->Weight;
        }

        if (!SelectedItem)
            return GetLootTierData(LootTierData);

        return SelectedItem;
    }

    // -----------------------------------------------------------------
    // Weighted random selection from a list of LootPackage rows
    // -----------------------------------------------------------------
    static FFortLootPackageData* GetLootPackage(std::vector<FFortLootPackageData*>& LootPackages)
    {
        float TotalWeight = 0.f;
        for (auto Item : LootPackages)
            TotalWeight += Item->Weight;

        float RandomNumber = UKismetMathLibrary::RandomFloatInRange(0.f, TotalWeight);
        FFortLootPackageData* SelectedItem = nullptr;

        for (auto Item : LootPackages)
        {
            if (RandomNumber <= Item->Weight)
            {
                SelectedItem = Item;
                break;
            }
            RandomNumber -= Item->Weight;
        }

        if (!SelectedItem)
            return GetLootPackage(LootPackages);

        return SelectedItem;
    }

    // -----------------------------------------------------------------
    // Get the clip size for a ranged weapon definition via its data table
    // -----------------------------------------------------------------
    int GetClipSize(UFortItemDefinition* ItemDef)
    {
        if (auto RangedDef = Cast<UFortWeaponRangedItemDefinition>(ItemDef))
        {
            auto DataTable = RangedDef->WeaponStatHandle.DataTable;
            auto RowName   = RangedDef->WeaponStatHandle.RowName;

            if (DataTable && RowName.ComparisonIndex)
            {
                auto& RowMap = *(TMap<FName, FFortRangedWeaponStats*>*)(__int64(DataTable) + 0x30);

                for (auto& Pair : RowMap)
                {
                    FName                 CurrentRowName = Pair.Key();
                    FFortRangedWeaponStats* PackageData  = Pair.Value();

                    if (CurrentRowName == RowName && PackageData)
                        return PackageData->ClipSize;
                }
            }
        }
        return 0;
    }

    // -----------------------------------------------------------------
    // Resolve a loot tier group name into a list of item entries
    // -----------------------------------------------------------------
    std::vector<FFortItemEntry> PickLootDrops(FName TierGroupName, int recursive = 0)
    {
        std::vector<FFortItemEntry> LootDrops;

        if (recursive >= 5) return LootDrops;

        static std::vector<UDataTable*> LTDTables;
        static std::vector<UDataTable*> LPTables;
        static bool First = false;

        if (!First)
        {
            First = true;

            auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

            UDataTable* MainLTD = StaticLoadObject<UDataTable>(
                UKismetStringLibrary::Conv_NameToString(GameState->CurrentPlaylistInfo.BasePlaylist->LootTierData.ObjectID.AssetPathName).ToString());
            UDataTable* MainLP = StaticLoadObject<UDataTable>(
                UKismetStringLibrary::Conv_NameToString(GameState->CurrentPlaylistInfo.BasePlaylist->LootPackages.ObjectID.AssetPathName).ToString());

            if (!MainLTD)
                MainLTD = StaticLoadObject<UDataTable>("/Game/Items/DataTables/AthenaLootTierData_Client.AthenaLootTierData_Client");
            if (!MainLP)
                MainLP = StaticLoadObject<UDataTable>("/Game/Items/DataTables/AthenaLootPackages_Client.AthenaLootPackages_Client");

            LTDTables.push_back(MainLTD);
            LPTables.push_back(MainLP);
        }

        // --- Gather matching LTD rows
        std::vector<FFortLootTierData*> TierGroupLTDs;
        for (auto LTD : LTDTables)
        {
            for (int i = 0; i < LTD->RowMap.Elements.Num(); i++)
            {
                auto TierData = (FFortLootTierData*)LTD->RowMap.Elements[i].Value();
                if (TierGroupName == TierData->TierGroup && TierData->Weight != 0)
                    TierGroupLTDs.push_back(TierData);
            }
        }

        FFortLootTierData* ChosenLTD = GetLootTierData(TierGroupLTDs);

        if (ChosenLTD->NumLootPackageDrops <= 0)
            return PickLootDrops(TierGroupName, ++recursive);

        if (ChosenLTD->LootPackageCategoryMinArray.Num() != ChosenLTD->LootPackageCategoryWeightArray.Num() ||
            ChosenLTD->LootPackageCategoryMinArray.Num() != ChosenLTD->LootPackageCategoryMaxArray.Num())
            return PickLootDrops(TierGroupName, ++recursive);

        // Weight sanity check
        int SumWeight = 0, SumMin = 0;
        for (int i = 0; i < ChosenLTD->LootPackageCategoryWeightArray.Num(); i++)
        {
            if (ChosenLTD->LootPackageCategoryWeightArray[i] > 0 && ChosenLTD->LootPackageCategoryMaxArray[i] < 0)
                SumWeight += ChosenLTD->LootPackageCategoryWeightArray[i];
        }
        for (int i = 0; i < ChosenLTD->LootPackageCategoryMinArray.Num(); i++)
        {
            if (ChosenLTD->LootPackageCategoryMinArray[i] > 0 && ChosenLTD->LootPackageCategoryMaxArray[i] < 0)
                SumMin += ChosenLTD->LootPackageCategoryMinArray[i];
        }
        if (SumWeight > SumMin)
            return PickLootDrops(TierGroupName, ++recursive);

        // --- Gather matching LP rows
        std::vector<FFortLootPackageData*> TierGroupLPs;
        for (auto LP : LPTables)
        {
            for (int i = 0; i < LP->RowMap.Elements.Num(); i++)
            {
                auto LootPackage = (FFortLootPackageData*)LP->RowMap.Elements[i].Value();
                if (!LootPackage) continue;
                if (LootPackage->LootPackageID == ChosenLTD->LootPackage && LootPackage->Weight != 0)
                    TierGroupLPs.push_back(LootPackage);
            }
        }

        bool  bIsWorldList      = ChosenLTD->LootPackage.ToString().contains("WorldList");
        float NumPackageDrops   = std::floor(ChosenLTD->NumLootPackageDrops);
        LootDrops.reserve((size_t)NumPackageDrops);

        for (float i = 0; i < NumPackageDrops; i++)
        {
            if (i >= TierGroupLPs.size()) break;

            auto TierGroupLP    = TierGroupLPs.at((size_t)i);
            auto TierGroupLPStr = TierGroupLP->LootPackageCall.ToString();

            if (TierGroupLPStr.contains(".Empty"))
            {
                NumPackageDrops++;
                continue;
            }

            std::vector<FFortLootPackageData*> lootPackageCalls;

            if (bIsWorldList)
            {
                for (auto& lp : TierGroupLPs)
                {
                    if (lp->Weight != 0)
                        lootPackageCalls.push_back(lp);
                }
            }
            else
            {
                for (auto LP : LPTables)
                {
                    for (int j = 0; j < LP->RowMap.Elements.Num(); j++)
                    {
                        auto LootPackage = (FFortLootPackageData*)LP->RowMap.Elements[j].Value();
                        if (LootPackage->LootPackageID.ToString() == TierGroupLPStr && LootPackage->Weight != 0)
                            lootPackageCalls.push_back(LootPackage);
                    }
                }
            }

            if (lootPackageCalls.empty()) { NumPackageDrops++; continue; }

            FFortLootPackageData* LootPackageCall = GetLootPackage(lootPackageCalls);
            if (!LootPackageCall) continue;

            auto ItemDef = LootPackageCall->ItemDefinition.Get();
            if (!ItemDef) { NumPackageDrops++; continue; }

            FFortItemEntry Entry{};
            Entry.ItemDefinition = ItemDef;
            Entry.LoadedAmmo     = GetClipSize(Cast<UFortWeaponItemDefinition>(ItemDef));
            Entry.Count          = LootPackageCall->Count;
            LootDrops.push_back(Entry);
        }

        return LootDrops;
    }

    // -----------------------------------------------------------------
    // Called when a player searches a building container
    // -----------------------------------------------------------------
    char SpawnLoot(ABuildingContainer* BuildingContainer)
    {
        static auto Bars = StaticLoadObject<UFortItemDefinition>("/Game/Items/ResourcePickups/Athena_WadsItemData.Athena_WadsItemData");

        std::string ClassName = BuildingContainer->Class->GetName();

        auto SearchLootTierGroup = BuildingContainer->SearchLootTierGroup;
        EFortPickupSpawnSource    SpawnSource        = EFortPickupSpawnSource::Unset;
        EFortPickupSourceTypeFlag PickupSourceFlags  = EFortPickupSourceTypeFlag::Container;

        static auto Loot_Treasure             = UKismetStringLibrary::Conv_StringToName(L"Loot_Treasure");
        static auto Loot_Ammo                 = UKismetStringLibrary::Conv_StringToName(L"Loot_Ammo");
        static auto Loot_AthenaFloorLoot      = UKismetStringLibrary::Conv_StringToName(L"Loot_AthenaFloorLoot");
        static auto Loot_AthenaFloorLoot_Warm = UKismetStringLibrary::Conv_StringToName(L"Loot_AthenaFloorLoot_Warmup");

        if (SearchLootTierGroup == Loot_AthenaFloorLoot || SearchLootTierGroup == Loot_AthenaFloorLoot_Warm)
            PickupSourceFlags = EFortPickupSourceTypeFlag::FloorLoot;

        BuildingContainer->bAlreadySearched = true;
        BuildingContainer->SearchBounceData.SearchAnimationCount++;
        BuildingContainer->OnRep_bAlreadySearched();

        if (SearchLootTierGroup == Loot_Treasure)
        {
            SearchLootTierGroup = UKismetStringLibrary::Conv_StringToName(L"Loot_AthenaTreasure");
            SpawnSource         = EFortPickupSpawnSource::Chest;
        }

        if (SearchLootTierGroup == Loot_Ammo)
        {
            SearchLootTierGroup = UKismetStringLibrary::Conv_StringToName(L"Loot_AthenaAmmoLarge");
            SpawnSource         = EFortPickupSpawnSource::AmmoBox;
        }

        auto LootDrops = PickLootDrops(SearchLootTierGroup);
        FVector CorrectLocation = BuildingContainer->K2_GetActorLocation()
            + BuildingContainer->GetActorForwardVector() * BuildingContainer->LootSpawnLocation_Athena.X
            + BuildingContainer->GetActorRightVector()   * BuildingContainer->LootSpawnLocation_Athena.Y
            + BuildingContainer->GetActorUpVector()      * BuildingContainer->LootSpawnLocation_Athena.Z;

        if (ClassName.contains("Tiered_Chest"))
        {
            for (auto& LootDrop : LootDrops)
                SpawnPickup(LootDrop.ItemDefinition, LootDrop.Count, LootDrop.LoadedAmmo, CorrectLocation, PickupSourceFlags, SpawnSource);

            static auto Wood  = StaticLoadObject<UFortItemDefinition>("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
            static auto Metal = StaticLoadObject<UFortItemDefinition>("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
            static auto Stone = StaticLoadObject<UFortItemDefinition>("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");

            UFortItemDefinition* Mats = (rand() % 40 > 20) ? ((rand() % 20 > 10) ? Wood : Stone) : Metal;
            SpawnPickup(Mats,  30, 0, CorrectLocation, PickupSourceFlags, SpawnSource);
            SpawnPickup(Bars, UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(1, 30), 0, CorrectLocation, PickupSourceFlags, SpawnSource);
        }
        else
        {
            for (auto& LootDrop : LootDrops)
            {
                if (LootDrop.Count <= 0) continue;

                SpawnPickup(LootDrop.ItemDefinition, LootDrop.Count, LootDrop.LoadedAmmo, CorrectLocation, PickupSourceFlags, SpawnSource);

                if (SearchLootTierGroup == Loot_AthenaFloorLoot || SearchLootTierGroup == Loot_AthenaFloorLoot_Warm)
                {
                    if (LootDrop.ItemDefinition->GetName() == "WID_Athena_HappyGhost")
                        return 0;

                    auto AmmoDef = (UFortAmmoItemDefinition*)((UFortWeaponRangedItemDefinition*)LootDrop.ItemDefinition)->GetAmmoWorldItemDefinition_BP();
                    if (AmmoDef && LootDrop.ItemDefinition != AmmoDef && AmmoDef->DropCount > 0)
                        SpawnPickup(AmmoDef, AmmoDef->DropCount, 0, CorrectLocation, PickupSourceFlags, SpawnSource);
                }
            }
        }

        return true;
    }

    // -----------------------------------------------------------------
    // Remove floor loot spawner actors so we handle loot ourselves
    // -----------------------------------------------------------------
    void SpawnFloorLoot()
    {
        auto Statics = (UGameplayStatics*)UGameplayStatics::StaticClass()->DefaultObject;
        TArray<AActor*> FloorLootSpawners;

        // Destroy warmup floor loot spawners
        UClass* SpawnerClass = StaticLoadObject<UClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C");
        Statics->GetAllActorsOfClass(UWorld::GetWorld(), SpawnerClass, &FloorLootSpawners);
        for (size_t i = 0; i < FloorLootSpawners.Num(); i++)
            FloorLootSpawners[i]->K2_DestroyActor();
        FloorLootSpawners.Free();

        // Destroy regular floor loot spawners
        SpawnerClass = StaticLoadObject<UClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");
        Statics->GetAllActorsOfClass(UWorld::GetWorld(), SpawnerClass, &FloorLootSpawners);
        for (size_t i = 0; i < FloorLootSpawners.Num(); i++)
            FloorLootSpawners[i]->K2_DestroyActor();
        FloorLootSpawners.Free();

        Log("Floor loot spawners cleared!");
    }

    // -----------------------------------------------------------------
    // Hook container search function
    // -----------------------------------------------------------------
    void HookAll()
    {
        MH_CreateHook(reinterpret_cast<void*>(ImageBase + 0x498C198), SpawnLoot, nullptr);
        Log("Looting hooked!");
    }
}
