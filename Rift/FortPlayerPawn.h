#pragma once
#include "framework.h"
#include "FortInventory.h"

// =====================================================================
// Rift Gameserver - v17.30
// FortPlayerPawn.h - Pickup handling, reload, damage cue sync
// =====================================================================

namespace FortPlayerPawn
{
    // -----------------------------------------------------------------
    // ServerHandlePickup
    // Sets the pickup's flight data so the client animates it correctly.
    // Actual item granting happens in CompletePickupAnimation below.
    // -----------------------------------------------------------------
    void ServerHandlePickup(
        AFortPlayerPawnAthena* Pawn,
        AFortPickup*           PickUp,
        float                  InFlyTime,
        FVector&               InStartDirection,
        bool                   bPlayPickupSound)
    {
        if (!Pawn || !PickUp || PickUp->bPickedUp) return;

        PickUp->PickupLocationData.PickupGuid    = PickUp->PrimaryPickupItemEntry.ItemGuid;
        PickUp->PickupLocationData.PickupTarget  = Pawn;
        PickUp->PickupLocationData.ItemOwner     = Pawn;
        PickUp->PickupLocationData.FlyTime       = 0.4f;
        PickUp->PickupLocationData.bPlayPickupSound = true;
        PickUp->OnRep_PickupLocationData();

        PickUp->bPickedUp = true;
        PickUp->OnRep_bPickedUp();
    }

    // -----------------------------------------------------------------
    // CompletePickupAnimation
    // After the fly animation finishes, add the item to the inventory
    // and handle weapon-slot swapping / stack overflow correctly.
    // -----------------------------------------------------------------
    __int64 (*CompletePickupAnimationOG)(AFortPickup* Pickup);
    __int64 CompletePickupAnimation(AFortPickup* Pickup)
    {
        if (!Pickup) return CompletePickupAnimationOG(Pickup);

        FFortPickupLocationData& PickupLocationData = Pickup->PickupLocationData;
        FFortItemEntry&          PickupEntry        = Pickup->PrimaryPickupItemEntry;

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PickupLocationData.PickupTarget;
        if (!Pawn) return CompletePickupAnimationOG(Pickup);

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC) return CompletePickupAnimationOG(Pickup);

        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)PC->PlayerState;
        if (!PlayerState) return CompletePickupAnimationOG(Pickup);

        // Bots handle their own pickup
        if (PlayerState->bIsABot) return CompletePickupAnimationOG(Pickup);

        UFortItemDefinition* PickupItemDefinition = PickupEntry.ItemDefinition;
        int   PickupCount        = PickupEntry.Count;
        int   PickupLoadedAmmo   = PickupEntry.LoadedAmmo;
        float PickupMaxStackSize = FortInventory::GetMaxStackSize(PickupItemDefinition);

        if (!PC->WorldInventory) return CompletePickupAnimationOG(Pickup);

        FFortItemEntry* ItemEntry    = FortInventory::FindItemEntry(PC, PickupItemDefinition);
        AFortWeapon*    CurrentWeapon = Pawn->CurrentWeapon;

        if (!CurrentWeapon || !CurrentWeapon->WeaponData)
            return CompletePickupAnimationOG(Pickup);

        FVector Drop = Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 100.f;

        if (ItemEntry)
        {
            // ---- Item already in inventory ----
            if (PickupItemDefinition->IsStackable())
            {
                bool bCanPickup = (ItemEntry->Count + PickupCount) <= PickupMaxStackSize;
                if (PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass()))
                    bCanPickup = true;

                if (bCanPickup)
                {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo, true);
                }
                else
                {
                    int Space    = (int)PickupMaxStackSize - ItemEntry->Count;
                    int AddAmt   = UKismetMathLibrary::GetDefaultObj()->Min(Space, PickupCount);
                    int LeftOver = PickupCount - AddAmt;

                    if (AddAmt > 0)
                    {
                        FortInventory::GiveItem(PC, PickupItemDefinition, AddAmt, 0, true);
                        SpawnPickup(PickupItemDefinition, LeftOver, PickupLoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                    }
                    else if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary)
                    {
                        FFortItemEntry* CurEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);
                        SpawnPickup(CurrentWeapon->WeaponData, CurEntry->Count, CurEntry->LoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                        FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                    else
                    {
                        SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                    }
                }
            }
            else
            {
                // Non-stackable duplicate - swap with current weapon if it's a primary
                if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary)
                {
                    if (FortInventory::IsInventoryFull(PC))
                    {
                        FFortItemEntry* CurEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);
                        SpawnPickup(CurrentWeapon->WeaponData, CurEntry->Count, CurEntry->LoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                        FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                    else
                    {
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                }
                else
                {
                    SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop,
                                EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                }
            }
        }
        else
        {
            // ---- New item ----
            if (PickupItemDefinition->IsStackable())
            {
                if (PickupItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass())   ||
                    PickupItemDefinition->IsA(UFortResourceItemDefinition::StaticClass()) ||
                    PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass()))
                {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                }
                else
                {
                    if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary)
                    {
                        if (FortInventory::IsInventoryFull(PC))
                        {
                            FFortItemEntry* CurEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);
                            SpawnPickup(CurrentWeapon->WeaponData, CurEntry->Count, CurEntry->LoadedAmmo, Drop,
                                        EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                            FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                        }
                        else
                        {
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                        }
                    }
                    else
                    {
                        if (FortInventory::IsInventoryFull(PC))
                            SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop,
                                        EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                        else
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                }
            }
            else
            {
                // Non-stackable, slot not occupied
                if (PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass()))
                {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                }
                else if (FortInventory::IsInventoryFull(PC))
                {
                    if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary)
                    {
                        FFortItemEntry* CurEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);
                        SpawnPickup(CurrentWeapon->WeaponData, CurEntry->Count, CurEntry->LoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                        FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                    }
                    else
                    {
                        SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop,
                                    EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, false, Pawn);
                    }
                }
                else
                {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                }
            }
        }

        FortInventory::Update(PC);
        Pickup->K2_DestroyActor();
        return CompletePickupAnimationOG(Pickup);
    }

    // -----------------------------------------------------------------
    // NetMulticast_Athena_BatchedDamageCues
    // Sync ammo count in the inventory when a weapon fires.
    // Skip for bot controllers (PhoebePlayerController).
    // -----------------------------------------------------------------
    void (*NetMulticast_Athena_BatchedDamageCuesOG)(
        AFortPlayerPawnAthena*                      Pawn,
        FAthenaBatchedDamageGameplayCues_Shared     SharedData,
        FAthenaBatchedDamageGameplayCues_NonShared  NonSharedData);

    void NetMulticast_Athena_BatchedDamageCues(
        AFortPlayerPawnAthena*                     Pawn,
        FAthenaBatchedDamageGameplayCues_Shared    SharedData,
        FAthenaBatchedDamageGameplayCues_NonShared NonSharedData)
    {
        if (!Pawn || Pawn->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()))
            return;

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC)
            return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);

        // Keep loaded ammo in sync after shooting
        if (Pawn->CurrentWeapon)
        {
            FFortItemEntry* ItemEntry = FortInventory::FindItemEntry(PC, Pawn->CurrentWeapon->ItemEntryGuid);
            if (ItemEntry)
            {
                ItemEntry->LoadedAmmo = Pawn->CurrentWeapon->AmmoCount;
                FortInventory::Update(PC, ItemEntry);
            }
        }

        return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);
    }

    // -----------------------------------------------------------------
    // OnReload
    // Deduct ammo from inventory when the weapon reloads.
    // -----------------------------------------------------------------
    void (*OnReloadOG)(AFortWeapon* a1, int RemoveCount);
    void OnReload(AFortWeapon* a1, int RemoveCount)
    {
        if (!a1) return OnReloadOG(a1, RemoveCount);

        AFortPlayerPawn* Pawn = (AFortPlayerPawn*)a1->GetOwner();
        if (!Pawn || !Pawn->Controller) return OnReloadOG(a1, RemoveCount);

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC) return OnReloadOG(a1, RemoveCount);

        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)Pawn->PlayerState;
        if (!PlayerState || PlayerState->bIsABot) return OnReloadOG(a1, RemoveCount);

        FFortItemEntry* WeaponItemEntry = FortInventory::FindItemEntry(PC, a1->ItemEntryGuid);
        if (!WeaponItemEntry || !WeaponItemEntry->ItemDefinition) return OnReloadOG(a1, RemoveCount);

        UFortWorldItemDefinition* AmmoItemDef =
            a1->WeaponData ? a1->WeaponData->GetAmmoWorldItemDefinition_BP() : nullptr;

        if (!AmmoItemDef) return OnReloadOG(a1, RemoveCount);

        FFortItemEntry* AmmoItemEntry = FortInventory::FindItemEntry(PC, (UFortItemDefinition*)AmmoItemDef);
        if (AmmoItemEntry)
        {
            FortInventory::RemoveItem(PC, AmmoItemEntry->ItemDefinition, RemoveCount);
        }
        else
        {
            // No separate ammo stack - deduct from the weapon's own stack if stackable
            int MaxStackSize = WeaponItemEntry->ItemDefinition->MaxStackSize.Value;
            if (MaxStackSize > 1)
                FortInventory::RemoveItem(PC, WeaponItemEntry->ItemDefinition, RemoveCount);
        }

        WeaponItemEntry->LoadedAmmo = a1->AmmoCount;
        FortInventory::Update(PC, WeaponItemEntry);

        return OnReloadOG(a1, RemoveCount);
    }

    // -----------------------------------------------------------------
    // HookAll
    // -----------------------------------------------------------------
    void HookAll()
    {
        // ServerHandlePickup - vtable slot 0x208 on APlayerPawn_Athena_C
        HookVTable(APlayerPawn_Athena_C::GetDefaultObj(), 0x208, ServerHandlePickup, nullptr);

        // CompletePickupAnimation
        MH_CreateHook((LPVOID)(ImageBase + 0x1BA62B4), CompletePickupAnimation, (LPVOID*)&CompletePickupAnimationOG);

        // BatchedDamageCues - vtable slot 0x11B
        HookVTable(AFortPlayerPawnAthena::GetDefaultObj(), 0x11B,
                   NetMulticast_Athena_BatchedDamageCues, (LPVOID*)&NetMulticast_Athena_BatchedDamageCuesOG);

        // OnReload
        MH_CreateHook((LPVOID)(ImageBase + 0x512FB08), OnReload, (LPVOID*)&OnReloadOG);

        Log("FortPlayerPawn hooked!");
    }
}
