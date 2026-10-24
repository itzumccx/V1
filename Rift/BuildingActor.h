#pragma once
#include "framework.h"

// =====================================================================
// Rift Gameserver - v17.30
// BuildingActor.h - Harvesting material drops when hitting world buildings
// =====================================================================

namespace BuildingActor
{
    // -----------------------------------------------------------------
    // OnDamageServer
    // When a player pickaxe-hits a non-player-placed building, grant
    // the appropriate material type and count to the attacker.
    // -----------------------------------------------------------------
    void (*OnDamageServerOG)(
        ABuildingActor*            This,
        float                      Damage,
        FGameplayTagContainer&     DamageTags,
        FVector&                   Momentum,
        FHitResult&                HitInfo,
        AController*               InstigatedBy,
        AActor*                    DamageCauser,
        FGameplayEffectContextHandle& EffectContext);

    void OnDamageServer(
        ABuildingActor*            This,
        float                      Damage,
        FGameplayTagContainer&     DamageTags,
        FVector&                   Momentum,
        FHitResult&                HitInfo,
        AController*               InstigatedBy,
        AActor*                    DamageCauser,
        FGameplayEffectContextHandle& EffectContext)
    {
        // Only handle pickaxe hits by real players on world buildings
        if (!This
            || !InstigatedBy
            || !InstigatedBy->IsA(AFortPlayerControllerAthena::StaticClass())
            || !DamageCauser->IsA(AFortWeapon::StaticClass())
            || !((AFortWeapon*)DamageCauser)->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())
            || This->bPlayerPlaced)
        {
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
        }

        ABuildingSMActor*           BuildingSMActor = (ABuildingSMActor*)This;
        AFortPlayerControllerAthena* PC             = (AFortPlayerControllerAthena*)InstigatedBy;

        if (!PC->Pawn)
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;

        int MaterialCount = (int)(Damage / (float)(UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(6, 12)));

        PC->ClientReportDamagedResourceBuilding(
            BuildingSMActor,
            BuildingSMActor->ResourceType,
            MaterialCount,
            BuildingSMActor->bDestroyed,
            (Damage == 100.f));

        UFortResourceItemDefinition* ResourceItemDefinition =
            UFortKismetLibrary::K2_GetResourceItemDefinition(BuildingSMActor->ResourceType);

        if (!ResourceItemDefinition)
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

        // Spawn the material pickup directly at the player and auto-collect
        AFortPickup* Pickup = SpawnPickup(
            ResourceItemDefinition, MaterialCount, 0,
            PC->K2_GetActorLocation(),
            EFortPickupSourceTypeFlag::Player,
            EFortPickupSpawnSource::Unset,
            true, PC->MyFortPawn);

        Pawn->ServerHandlePickup(Pickup, 0.3f, FVector(), true);

        return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
    }

    // -----------------------------------------------------------------
    // HookAll
    // -----------------------------------------------------------------
    void HookAll()
    {
        MH_CreateHook((LPVOID)(ImageBase + 0x515FEA4), OnDamageServer, (LPVOID*)&OnDamageServerOG);
        Log("BuildingActor hooked!");
    }
}
