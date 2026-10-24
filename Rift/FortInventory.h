#pragma once
#include "framework.h"

// =====================================================================
// Rift Gameserver - v17.30
// FortInventory.h - Player inventory management
// =====================================================================

namespace FortInventory
{
    // -----------------------------------------------------------------
    // GUID comparison
    // -----------------------------------------------------------------
    bool CompareGuids(FGuid Guid1, FGuid Guid2)
    {
        return Guid1.A == Guid2.A
            && Guid1.B == Guid2.B
            && Guid1.C == Guid2.C
            && Guid1.D == Guid2.D;
    }

    // -----------------------------------------------------------------
    // Mark the inventory dirty and force replication
    // -----------------------------------------------------------------
    void Update(AFortPlayerController* PC, FFortItemEntry* ItemEntry = nullptr)
    {
        PC->HandleWorldInventoryLocalUpdate();
        PC->WorldInventory->HandleInventoryLocalUpdate();
        PC->WorldInventory->bRequiresLocalUpdate = true;
        PC->WorldInventory->ForceNetUpdate();

        if (ItemEntry == nullptr)
            PC->WorldInventory->Inventory.MarkArrayDirty();
        else
            PC->WorldInventory->Inventory.MarkItemDirty(*ItemEntry);
    }

    // -----------------------------------------------------------------
    // Add an item to the player's inventory
    // -----------------------------------------------------------------
    void GiveItem(AFortPlayerController* PC, UFortItemDefinition* Def, int Count, int LoadedAmmo, bool bShouldAddToExistingStack = false)
    {
        if (!PC || !Def) return;

        if (bShouldAddToExistingStack)
        {
            for (FFortItemEntry& ItemEntry : PC->WorldInventory->Inventory.ReplicatedEntries)
            {
                if (Def == ItemEntry.ItemDefinition)
                {
                    ItemEntry.Count += Count;
                    PC->WorldInventory->Inventory.MarkItemDirty(ItemEntry);
                    Update(PC);
                    return;
                }
            }
            return;
        }

        UFortWorldItem* Item = Cast<UFortWorldItem>(Def->CreateTemporaryItemInstanceBP(Count, 0));
        if (!Item) return;

        Item->SetOwningControllerForTemporaryItem(PC);
        Item->OwnerInventory          = PC->WorldInventory;
        Item->ItemEntry.LoadedAmmo    = LoadedAmmo;

        if (Item->ItemEntry.ItemDefinition)
        {
            FFortItemEntryStateValue Value{};
            Value.IntValue  = true;
            Value.StateType = EFortItemEntryState::ShouldShowItemToast;
            Item->ItemEntry.StateValues.Add(Value);
        }

        PC->WorldInventory->Inventory.ReplicatedEntries.Add(Item->ItemEntry);
        PC->WorldInventory->Inventory.ItemInstances.Add(Item);
        PC->WorldInventory->Inventory.MarkItemDirty(Item->ItemEntry);
        PC->WorldInventory->HandleInventoryLocalUpdate();
    }

    // -----------------------------------------------------------------
    // Remove an item (or reduce its count) from the inventory
    // -----------------------------------------------------------------
    void RemoveItem(AFortPlayerController* PC, UFortItemDefinition* Def, int Count = INT_MAX)
    {
        if (!PC || !Def) return;

        for (int i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
        {
            FFortItemEntry& ItemEntry = PC->WorldInventory->Inventory.ReplicatedEntries[i];
            if (Def == ItemEntry.ItemDefinition)
            {
                ItemEntry.Count -= Count;
                if (ItemEntry.Count <= 0)
                {
                    PC->WorldInventory->Inventory.ReplicatedEntries[i].StateValues.Free();
                    PC->WorldInventory->Inventory.ReplicatedEntries.Remove(i);

                    for (int j = 0; j < PC->WorldInventory->Inventory.ItemInstances.Num(); j++)
                    {
                        UFortWorldItem* WorldItem = PC->WorldInventory->Inventory.ItemInstances[j];
                        if (WorldItem->ItemEntry.ItemDefinition == Def)
                        {
                            PC->WorldInventory->Inventory.ItemInstances.Remove(j);
                            break;
                        }
                    }
                }
                else
                {
                    Update(PC, &ItemEntry);
                }
                break;
            }
        }
        Update(PC);
    }

    // -----------------------------------------------------------------
    // Find an item entry by definition pointer
    // -----------------------------------------------------------------
    FFortItemEntry* FindItemEntry(AFortPlayerControllerAthena* PC, UFortItemDefinition* ItemDef)
    {
        if (!PC || !ItemDef) return nullptr;

        for (FFortItemEntry& ItemEntry : PC->WorldInventory->Inventory.ReplicatedEntries)
        {
            if (ItemDef == ItemEntry.ItemDefinition)
                return &ItemEntry;
        }
        return nullptr;
    }

    // -----------------------------------------------------------------
    // Find an item entry by GUID
    // -----------------------------------------------------------------
    FFortItemEntry* FindItemEntry(AFortPlayerControllerAthena* PC, FGuid Guid)
    {
        if (!PC) return nullptr;

        for (FFortItemEntry& ItemEntry : PC->WorldInventory->Inventory.ReplicatedEntries)
        {
            if (CompareGuids(ItemEntry.ItemGuid, Guid))
                return &ItemEntry;
        }
        return nullptr;
    }

    // -----------------------------------------------------------------
    // Determine which quickbar slot category an item belongs to
    // -----------------------------------------------------------------
    static EFortQuickBars GetQuickBars(UFortItemDefinition* ItemDefinition)
    {
        if (!ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())  &&
            !ItemDefinition->IsA(UFortEditToolItemDefinition::StaticClass())       &&
            !ItemDefinition->IsA(UFortBuildingItemDefinition::StaticClass())       &&
            !ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass())           &&
            !ItemDefinition->IsA(UFortResourceItemDefinition::StaticClass())       &&
            !ItemDefinition->IsA(UFortTrapItemDefinition::StaticClass()))
            return EFortQuickBars::Primary;

        return EFortQuickBars::Secondary;
    }

    // -----------------------------------------------------------------
    // Check whether the primary weapon slots are full (max 5 weapons)
    // -----------------------------------------------------------------
    static bool IsInventoryFull(AFortPlayerController* PC)
    {
        if (!PC || !PC->WorldInventory) return true;

        static constexpr int Max = 5;
        int Count = 0;

        for (int i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
        {
            FFortItemEntry* Entry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
            if (!Entry || !Entry->ItemDefinition) continue;

            if (GetQuickBars(Entry->ItemDefinition) == EFortQuickBars::Primary)
            {
                ++Count;
                if (Count >= Max) return true;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------
    // Get the max stack size for a definition (curve table aware)
    // -----------------------------------------------------------------
    float GetMaxStackSize(UFortItemDefinition* Def)
    {
        if (!Def->MaxStackSize.Curve.CurveTable)
            return Def->MaxStackSize.Value;

        EEvaluateCurveTableResult Result;
        float Ret = 1.f;
        ((UDataTableFunctionLibrary*)UDataTableFunctionLibrary::StaticClass()->DefaultObject)
            ->EvaluateCurveTableRow(Def->MaxStackSize.Curve.CurveTable, Def->MaxStackSize.Curve.RowName, 0, &Result, &Ret, FString());
        return Ret;
    }
}
