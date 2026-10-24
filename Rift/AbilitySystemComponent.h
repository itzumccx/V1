#pragma once
#include "framework.h"

// =====================================================================
// Rift Gameserver - v17.30
// AbilitySystemComponent.h - Gameplay ability granting & activation
// =====================================================================

namespace AbilitySystemComponent
{
    // -----------------------------------------------------------------
    // Give a single ability to a player state's ASC
    // -----------------------------------------------------------------
    void GiveAbility(UFortGameplayAbility* Ability, AFortPlayerState* PS)
    {
        if (!Ability || !PS) return;

        FGameplayAbilitySpec Spec{};
        AbilitySpecConstructor(&Spec, Ability, 1, -1, nullptr);
        GiveAbilityOG(PS->AbilitySystemComponent, &Spec.Handle, Spec);
    }

    // -----------------------------------------------------------------
    // Give every ability and effect in an ability set
    // -----------------------------------------------------------------
    void GiveAbilitySet(UFortAbilitySet* AbilitySet, AFortPlayerState* PS)
    {
        if (!AbilitySet || !PS)
        {
            Log("GiveAbilitySet: NULL parameter - skipping");
            return;
        }

        // Grant abilities
        for (int i = 0; i < AbilitySet->GameplayAbilities.Num(); i++)
        {
            GiveAbility(
                (UFortGameplayAbility*)AbilitySet->GameplayAbilities[i].Get()->DefaultObject,
                PS);
        }

        // Apply granted gameplay effects
        for (int i = 0; i < AbilitySet->GrantedGameplayEffects.Num(); i++)
        {
            UClass* GameplayEffect = AbilitySet->GrantedGameplayEffects[i].GameplayEffect.Get();
            float   Level          = AbilitySet->GrantedGameplayEffects[i].Level;

            if (!GameplayEffect) continue;

            PS->AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(
                GameplayEffect, Level, FGameplayEffectContextHandle());
        }
    }

    // -----------------------------------------------------------------
    // Find an ability spec by handle in the ASC's activatable list
    // Ref: UE source AbilitySystemComponent_Abilities.cpp L584
    // -----------------------------------------------------------------
    FGameplayAbilitySpec* FindAbilitySpecFromHandle(
        UAbilitySystemComponent*  AbilitySystemComponent,
        FGameplayAbilitySpecHandle Handle)
    {
        for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->ActivatableAbilities.Items)
        {
            if (Spec.Handle.Handle == Handle.Handle)
                return &Spec;
        }
        return nullptr;
    }

    // -----------------------------------------------------------------
    // Server-side internal ability activation
    // Ref: UE source AbilitySystemComponent_Abilities.cpp L1445
    // -----------------------------------------------------------------
    void InternalServerTryActiveAbility(
        UAbilitySystemComponent*   AbilitySystemComponent,
        FGameplayAbilitySpecHandle Handle,
        bool                       InputPressed,
        const FPredictionKey&      PredictionKey,
        const FGameplayEventData*  TriggerEventData)
    {
        FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilitySystemComponent, Handle);
        if (!Spec)
        {
            AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
            return;
        }

        UGameplayAbility* InstancedAbility = nullptr;
        Spec->InputPressed = true;

        if (!InternalTryActivateAbility(AbilitySystemComponent, Handle, PredictionKey,
                                        &InstancedAbility, nullptr, TriggerEventData))
        {
            AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
            Spec->InputPressed = false;
            AbilitySystemComponent->ActivatableAbilities.MarkItemDirty(*Spec);
        }
    }

    // -----------------------------------------------------------------
    // Hook the internal try-activate vtable slot on all ASC types
    // -----------------------------------------------------------------
    void HookAll()
    {
        constexpr int InternalServerTryActiveAbilityIndex = 0xFE;

        HookVTable(UAbilitySystemComponent::GetDefaultObj(),        InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);
        HookVTable(UFortAbilitySystemComponent::GetDefaultObj(),     InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);
        HookVTable(UFortAbilitySystemComponentAthena::GetDefaultObj(), InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);

        Log("AbilitySystemComponent hooked!");
    }
}
