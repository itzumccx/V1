#pragma once

// =====================================================================
// Rift Gameserver - v17.30
// framework.h - Core includes, function pointers, utilities
// =====================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <intrin.h>
#include <sstream>
#include <array>
#include <tlhelp32.h>
#include <future>
#include <set>
#include <variant>
#include <unordered_set>
#include <ctime>
#include <format>

#include "minhook/MinHook.h"
#include "SDK/SDK.hpp"
#include "Globals.h"

#pragma comment(lib, "minhook/minhook.lib")

using namespace SDK;

// =====================================================================
// Image base + core function pointers (all offsets for v17.30)
// =====================================================================

static auto ImageBase = InSDKUtils::GetImageBase();

// -- Building item definitions (used when granting default items)
static UFortBuildingItemDefinition* Floor = UObject::FindObject<UFortBuildingItemDefinition>("FortBuildingItemDefinition BuildingItemData_Floor.BuildingItemData_Floor");
static UFortBuildingItemDefinition* Roof  = UObject::FindObject<UFortBuildingItemDefinition>("FortBuildingItemDefinition BuildingItemData_RoofS.BuildingItemData_RoofS");
static UFortBuildingItemDefinition* Stair = UObject::FindObject<UFortBuildingItemDefinition>("FortBuildingItemDefinition BuildingItemData_Stair_W.BuildingItemData_Stair_W");
static UFortBuildingItemDefinition* Wall  = UObject::FindObject<UFortBuildingItemDefinition>("FortBuildingItemDefinition BuildingItemData_Wall.BuildingItemData_Wall");
static UFortEditToolItemDefinition* EditTool = UObject::FindObject<UFortEditToolItemDefinition>("FortEditToolItemDefinition EditTool.EditTool");

// -- Replication
void (*ServerReplicateActors)(UReplicationDriver* This, float DeltaSeconds) = decltype(ServerReplicateActors)(ImageBase + 0x3DDF340);

// -- Net driver / listen
static UNetDriver* (*CreateNetDriver)(UEngine* This, UWorld* InWorld, FName NetDriverDefinition) = decltype(CreateNetDriver)(ImageBase + 0x4DD092C);
static bool (*InitListen)(UNetDriver* This, UWorld* InWorld, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) = decltype(InitListen)(ImageBase + 0x3A01BF4);
static void (*SetWorld)(UNetDriver* This, UWorld* InWorld) = decltype(SetWorld)(ImageBase + 0x176E8D8);
static bool (*InitHost)(UObject* Beacon) = decltype(InitHost)(ImageBase + 0x3A0186C);
static void (*PauseBeaconRequests)(UObject* Beacon, bool bPause) = decltype(PauseBeaconRequests)(ImageBase + 0x4D716DC);

// -- Ability system
static void (*GiveAbilityOG)(UAbilitySystemComponent* This, FGameplayAbilitySpecHandle* Handle, FGameplayAbilitySpec Spec) = decltype(GiveAbilityOG)(ImageBase + 0x1343068);
static void (*AbilitySpecConstructor)(FGameplayAbilitySpec*, UGameplayAbility*, int, int, UObject*) = decltype(AbilitySpecConstructor)(ImageBase + 0x1342F9C);
static bool (*InternalTryActivateAbility)(UAbilitySystemComponent* AbilitySystemComp, FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey InPredictionKey, UGameplayAbility** OutInstancedAbility, void* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData) = decltype(InternalTryActivateAbility)(ImageBase + 0x3892DB8);
static FGameplayAbilitySpecHandle (*GiveAbilityAndActivateOnce)(UAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec) = decltype(GiveAbilityAndActivateOnce)(ImageBase + 0x389156C);

// -- Supply drop
static FVector* (*PickSupplyDropLocationOG)(AFortAthenaMapInfo* MapInfo, FVector* outLocation, __int64 Center, float Radius) = decltype(PickSupplyDropLocationOG)(ImageBase + 0x473D79C);

// -- Building
static ABuildingSMActor* (*ReplaceBuildingActor)(ABuildingSMActor* BuildingSMActor, unsigned int a2, UObject* a3, unsigned int a4, int a5, bool bMirrored, AFortPlayerControllerAthena* PC) = decltype(ReplaceBuildingActor)(ImageBase + 0x49A9AAC);
static __int64 (*CantBuild)(UWorld*, UObject*, FVector, FRotator, char, TArray<ABuildingSMActor*>*, char*) = decltype(CantBuild)(ImageBase + 0x4bbeb18);

// -- Character customization
static void* (*ApplyCharacterCustomization)(AFortPlayerStateAthena* a1, APawn* a2) = decltype(ApplyCharacterCustomization)(ImageBase + 0x4F32C40);

// -- Alive player tracking
static void (*RemoveFromAlivePlayers)(AFortGameModeAthena*, AFortPlayerControllerAthena*, APlayerState*, AFortPlayerPawn*, UFortWeaponItemDefinition*, uint8_t DeathCause, char) = decltype(RemoveFromAlivePlayers)(ImageBase + 0x478B2E0);
static void (*AddToAlivePlayers)(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* Player) = decltype(AddToAlivePlayers)(ImageBase + 0x4765A74);

// -- Object management
static void* (*StaticFindObjectOG)(UClass*, UObject* Package, const wchar_t* OrigInName, bool ExactClass) = decltype(StaticFindObjectOG)(ImageBase + 0xE3DF60);
static void* (*StaticLoadObjectOG)(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, uint32_t LoadFlags, UObject* Sandbox, bool bAllowObjectReconciliation, void*) = decltype(StaticLoadObjectOG)(ImageBase + 0x136DFB0);

// -- Zipline
static void (*OnRep_ZiplineState)(AFortPlayerPawn* a1) = decltype(OnRep_ZiplineState)(ImageBase + 0x4EB6DD0);

// =====================================================================
// Net mode enum
// =====================================================================

enum class ENetMode
{
    Standalone,
    DedicatedServer,
    ListenServer,
    Client,
    MAX,
};

// =====================================================================
// Trivial hook stubs
// =====================================================================

int IsTrue()  { return TRUE;  }
int IsFalse() { return FALSE; }
void nullFunc() {}

// =====================================================================
// Global actor / cosmetic caches
// =====================================================================

static std::vector<UAthenaCharacterItemDefinition*>   Characters{};
static std::vector<UAthenaPickaxeItemDefinition*>     Pickaxes{};
static std::vector<UAthenaBackpackItemDefinition*>    Backpacks{};
static std::vector<UAthenaGliderItemDefinition*>      Gliders{};
static std::vector<UAthenaSkyDiveContrailItemDefinition*> Contrails{};
inline std::vector<UAthenaDanceItemDefinition*>       Dances{};

UAthenaNavSystem*          AthenaNavSystem = nullptr;
AFortAthenaMutator_Bots*   BotMutator      = nullptr;
TArray<FVector>            PickedSupplyDropLocations;
TArray<APlayerController*> GivenLootPlayers;

static TArray<AActor*> BuildingFoundations;
static TArray<AActor*> PlayerStarts;

// =====================================================================
// Logging
// =====================================================================

void Log(const std::string& msg)
{
    static bool firstCall = true;

    if (firstCall)
    {
        std::ofstream logFile("Rift_Server_log.txt", std::ios::trunc);
        if (logFile.is_open())
        {
            logFile << "[RIFT]: Log file initialized!\n";
            logFile.close();
        }
        firstCall = false;
    }

    std::ofstream logFile("Rift_Server_log.txt", std::ios::app);
    if (logFile.is_open())
    {
        logFile << "[RIFT]: " << msg << "\n";
        logFile.close();
    }

    std::cout << "[RIFT]: " << msg << "\n";
}

// =====================================================================
// VTable hook helper
// =====================================================================

void HookVTable(void* Base, int Idx, void* Detour, void** OG)
{
    DWORD oldProtection;
    void** VTable = *(void***)Base;

    if (OG)
        *OG = VTable[Idx];

    VirtualProtect(&VTable[Idx], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtection);
    VTable[Idx] = Detour;
    VirtualProtect(&VTable[Idx], sizeof(void*), oldProtection, NULL);
}

// =====================================================================
// Math helpers
// =====================================================================

inline FQuat RotatorToQuat(FRotator Rotation)
{
    FQuat Quat;
    constexpr float DEG_TO_RAD  = 3.14159f / 180.0f;
    constexpr float DIVIDE_BY_2 = DEG_TO_RAD / 2.0f;

    float SP = sinf(Rotation.Pitch * DIVIDE_BY_2);
    float CP = cosf(Rotation.Pitch * DIVIDE_BY_2);
    float SY = sinf(Rotation.Yaw   * DIVIDE_BY_2);
    float CY = cosf(Rotation.Yaw   * DIVIDE_BY_2);
    float SR = sinf(Rotation.Roll  * DIVIDE_BY_2);
    float CR = cosf(Rotation.Roll  * DIVIDE_BY_2);

    Quat.X =  CR * SP * SY - SR * CP * CY;
    Quat.Y = -CR * SP * CY - SR * CP * SY;
    Quat.Z =  CR * CP * SY - SR * SP * CY;
    Quat.W =  CR * CP * CY + SR * SP * SY;

    return Quat;
}

// Alias kept for compatibility with spots that use FRotToQuat
static inline FQuat FRotToQuat(FRotator Rotation) { return RotatorToQuat(Rotation); }

// =====================================================================
// Object helpers
// =====================================================================

template<typename T>
static inline T* StaticFindObject(std::wstring ObjectName)
{
    return (T*)StaticFindObjectOG(T::StaticClass(), nullptr, ObjectName.c_str(), false);
}

template<typename T>
inline T* Cast(UObject* Object)
{
    if (!Object || !Object->IsA(T::StaticClass()))
        return nullptr;
    return (T*)Object;
}

template<typename T = UObject>
static inline T* StaticLoadObject(const std::string& Name)
{
    auto ConvName = std::wstring(Name.begin(), Name.end());
    T* Object = StaticFindObject<T>(ConvName);
    if (!Object)
        Object = (T*)StaticLoadObjectOG(T::StaticClass(), nullptr, ConvName.c_str(), nullptr, 0, nullptr, false, nullptr);
    return Object;
}

template<typename T>
T* GetDefaultObject()
{
    return (T*)T::StaticClass()->DefaultObject;
}

// =====================================================================
// Actor spawning helpers
// =====================================================================

template<typename T>
inline T* SpawnActor(
    FVector Loc,
    FRotator Rot   = FRotator(),
    AActor*  Owner = nullptr,
    SDK::UClass* Class = T::StaticClass(),
    ESpawnActorCollisionHandlingMethod Handle = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn)
{
    FTransform Transform{};
    Transform.Scale3D    = FVector{ 1, 1, 1 };
    Transform.Translation = Loc;
    Transform.Rotation   = FRotToQuat(Rot);

    return (T*)UGameplayStatics::FinishSpawningActor(
        UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), Class, Transform, Handle, Owner),
        Transform);
}

template<typename T>
static inline T* SpawnActorClass(
    FVector Loc     = { 0, 0, 0 },
    FRotator Rot    = { 0, 0, 0 },
    UClass*  Class  = nullptr,
    AActor*  Owner  = nullptr)
{
    FTransform Transform{};
    Transform.Scale3D    = { 1, 1, 1 };
    Transform.Translation = Loc;
    Transform.Rotation   = RotatorToQuat(Rot);

    AActor* NewActor = UGameplayStatics::BeginDeferredActorSpawnFromClass(
        UWorld::GetWorld(), Class, Transform,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, Owner);
    return (T*)UGameplayStatics::FinishSpawningActor(NewActor, Transform);
}

// Convenience alias
template<typename T>
T* Actors(UClass* Class = T::StaticClass(), FVector Loc = {}, FRotator Rot = {}, AActor* Owner = nullptr)
{
    return SpawnActor<T>(Loc, Rot, Owner, Class);
}

// =====================================================================
// Pickup helpers
// =====================================================================

AFortPickup* SpawnPickup(
    UFortItemDefinition*      ItemDef,
    int                       OverrideCount,
    int                       LoadedAmmo,
    FVector                   Loc,
    EFortPickupSourceTypeFlag SourceType,
    EFortPickupSpawnSource    Source,
    bool                      bShouldCombine = false,
    AFortPawn*                Pawn           = nullptr)
{
    auto SpawnedPickup = Actors<AFortPickup>(AFortPickup::StaticClass(), Loc);
    SpawnedPickup->bRandomRotation = true;

    auto& PickupEntry                = SpawnedPickup->PrimaryPickupItemEntry;
    PickupEntry.ItemDefinition       = ItemDef;
    PickupEntry.Count                = OverrideCount;
    PickupEntry.LoadedAmmo           = LoadedAmmo;
    PickupEntry.ReplicationKey++;
    SpawnedPickup->OnRep_PrimaryPickupItemEntry();
    SpawnedPickup->PawnWhoDroppedPickup = Pawn;

    SpawnedPickup->TossPickup(Loc, Pawn, -1, true, bShouldCombine, SourceType, Source);
    SpawnedPickup->SetReplicateMovement(true);
    SpawnedPickup->MovementComponent = (UProjectileMovementComponent*)
        GetDefaultObject<UGameplayStatics>()->SpawnObject(UProjectileMovementComponent::StaticClass(), SpawnedPickup);

    if (SourceType == EFortPickupSourceTypeFlag::Container)
    {
        SpawnedPickup->bTossedFromContainer = true;
        SpawnedPickup->OnRep_TossedFromContainer();
    }

    return SpawnedPickup;
}

// Overload from FFortItemEntry
static AFortPickup* SpawnPickup(
    FFortItemEntry            ItemEntry,
    FVector                   Location,
    EFortPickupSourceTypeFlag PickupSource = EFortPickupSourceTypeFlag::Other,
    EFortPickupSpawnSource    SpawnSource  = EFortPickupSpawnSource::Unset)
{
    return SpawnPickup(ItemEntry.ItemDefinition, ItemEntry.Count, ItemEntry.LoadedAmmo, Location, PickupSource, SpawnSource);
}

// =====================================================================
// Building foundation helper
// =====================================================================

inline void ShowFoundation(ABuildingFoundation* BuildingFoundation)
{
    if (!BuildingFoundation) return;

    BuildingFoundation->bServerStreamedInLevel = true;
    BuildingFoundation->DynamicFoundationType  = EDynamicFoundationType::Static;
    BuildingFoundation->OnRep_ServerStreamedInLevel();

    BuildingFoundation->FoundationEnabledState                           = EDynamicFoundationEnabledState::Enabled;
    BuildingFoundation->DynamicFoundationRepData.EnabledState            = EDynamicFoundationEnabledState::Enabled;
    BuildingFoundation->DynamicFoundationTransform                       = BuildingFoundation->GetTransform();
    BuildingFoundation->OnRep_DynamicFoundationRepData();
}

// =====================================================================
// Supply drop location picker
// =====================================================================

FVector PickSupplyDropLocation(AFortAthenaMapInfo* MapInfo, FVector Center, float Radius)
{
    if (!PickSupplyDropLocationOG) return FVector(0, 0, 0);

    const float MinDistance = 10000.0f;

    for (int i = 0; i < 20; i++)
    {
        FVector loc = FVector(0, 0, 0);
        PickSupplyDropLocationOG(MapInfo, &loc, (__int64)&Center, Radius);

        bool bTooClose = false;
        for (const auto& other : PickedSupplyDropLocations)
        {
            float dx = loc.X - other.X;
            float dy = loc.Y - other.Y;
            float dz = loc.Z - other.Z;
            if ((dx * dx + dy * dy + dz * dz) < MinDistance * MinDistance)
            {
                bTooClose = true;
                break;
            }
        }

        if (!bTooClose)
        {
            PickedSupplyDropLocations.Add(loc);
            return loc;
        }
    }

    return FVector(0, 0, 0);
}

// =====================================================================
// UObject iteration helpers
// =====================================================================

template<typename T>
inline std::vector<T*> GetAllObjectsOfClass(UClass* Class = T::StaticClass())
{
    std::vector<T*> Objects{};

    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Object = UObject::GObjects->GetByIndex(i);
        if (!Object) continue;
        if (Object->GetFullName().contains("Default")) continue;
        if (Object->GetFullName().contains("Test"))    continue;
        if (Object->IsA(Class) && !Object->IsDefaultObject())
            Objects.push_back((T*)Object);
    }

    return Objects;
}

template<class T>
TArray<T*> GetAllActorsOfClass()
{
    TArray<T*> ResultActors;
    if (UWorld* World = UWorld::GetWorld())
    {
        TArray<AActor*> OutActors;
        UGameplayStatics::GetAllActorsOfClass(World, T::StaticClass(), &OutActors);
        for (AActor* Actor : OutActors)
        {
            if (T* CastedActor = Cast<T>(Actor))
                ResultActors.Add(CastedActor);
        }
    }
    return ResultActors;
}

// =====================================================================
// Misc utilities
// =====================================================================

AFortPlayerControllerAthena* GetPCFromId(FUniqueNetIdRepl& ID)
{
    for (auto& PlayerState : UWorld::GetWorld()->GameState->PlayerArray)
    {
        auto PlayerStateAthena = Cast<AFortPlayerStateAthena>(PlayerState);
        if (!PlayerStateAthena) continue;
        if (PlayerStateAthena->AreUniqueIDsIdentical(ID, PlayerState->UniqueId))
            return Cast<AFortPlayerControllerAthena>(PlayerState->Owner);
    }
    return nullptr;
}

FName ConvFName(FString Name)
{
    return UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(Name);
}

namespace TextManipUtils
{
    std::vector<std::string> SplitWhitespace(std::string const& input)
    {
        std::istringstream buffer(input);
        std::vector<std::string> ret;
        std::copy(std::istream_iterator<std::string>(buffer),
                  std::istream_iterator<std::string>(),
                  std::back_inserter(ret));
        return ret;
    }
}
