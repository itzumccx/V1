// =====================================================================
// Rift Gameserver - v17.30
// dllmain.cpp - DLL entry point
//
// Injection order (matches 17.30-Gameserver-main reference):
//   1. Allocate console + log file
//   2. Init MinHook
//   3. Wait for UEngine
//   4. Wait for Frontend InProgress (logged in)
//   5. Install ALL hooks + MH_EnableHook
//   6. Flip GIsClient / GIsServer
//   7. Sleep 2s then open Apollo_Terrain
//   8. Wait for Apollo world to be fully loaded
//   9. Remove local player
// =====================================================================

#include "framework.h"
#include "FortGameModeAthena.h"
#include "FortPlayerControllerAthena.h"
#include "FortPlayerPawn.h"
#include "AbilitySystemComponent.h"
#include "Looting.h"
#include "BuildingActor.h"
#include "NetDriver.h"
#include "Misc.h"

// -----------------------------------------------------------------------
// InitConsole
// -----------------------------------------------------------------------
static void InitConsole()
{
    AllocConsole();
    FILE* fptr = nullptr;
    freopen_s(&fptr, "CONOUT$", "w+", stdout);
    SetConsoleTitleA("Rift v17.30 | Starting...");
    Log("Welcome to Rift v17.30 Gameserver!");
}

// -----------------------------------------------------------------------
// HookAll - create all hooks then enable in one shot
// -----------------------------------------------------------------------
static void HookAll()
{
    Misc::HookAll();
    FortGameModeAthena::HookAll();
    FortPlayerControllerAthena::HookAll();
    FortPlayerPawn::HookAll();
    AbilitySystemComponent::HookAll();
    Looting::HookAll();
    BuildingActor::HookAll();
    NetDriver::HookAll();

    MH_EnableHook(MH_ALL_HOOKS);
    Log("All hooks installed and enabled!");
}

// -----------------------------------------------------------------------
// WaitForLogin - spin until Frontend map is InProgress
// -----------------------------------------------------------------------
static void WaitForLogin()
{
    Log("Waiting for login...");

    FName Frontend   = UKismetStringLibrary::Conv_StringToName(L"Frontend");
    FName InProgress = UKismetStringLibrary::Conv_StringToName(L"InProgress");

    while (true)
    {
        UWorld* World = UWorld::GetWorld();
        if (World && World->Name == Frontend)
        {
            auto GM = (AGameMode*)World->AuthorityGameMode;
            if (GM && GM->GetMatchState() == InProgress)
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Log("Logged in!");
}

// -----------------------------------------------------------------------
// WaitForApollo - spin until Apollo_Terrain is the active world
// -----------------------------------------------------------------------
static void WaitForApollo()
{
    Log("Waiting for Apollo_Terrain to load...");

    FName ApolloName = UKismetStringLibrary::Conv_StringToName(L"Apollo_Terrain");

    while (true)
    {
        UWorld* World = UWorld::GetWorld();
        if (World && World->Name == ApolloName)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Extra settle time - let all actors spawn before we start touching them
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    Log("Apollo_Terrain loaded!");
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------
static DWORD WINAPI Main(LPVOID)
{
    InitConsole();

    if (MH_Initialize() != MH_OK)
    {
        Log("MinHook init FAILED!");
        return 1;
    }
    Log("MinHook initialised!");

    // Wait for engine
    while (UEngine::GetEngine() == nullptr)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    Log("UEngine found!");

    WaitForLogin();

    // Install hooks while still on Frontend - safe because we hook
    // function addresses, not vtable slots of world-owned objects
    HookAll();

    // Flip server flags
    *(bool*)(ImageBase + 0x973E49B) = false; // GIsClient
    *(bool*)(ImageBase + 0x973E49C) = true;  // GIsServer
    Log("Server flags set!");

    Sleep(2000);

    // Open the battle royale map
    Log("Opening Apollo_Terrain...");
    UWorld* FrontendWorld = UWorld::GetWorld();
    if (FrontendWorld)
    {
        // Remove local player BEFORE the map transition
        // (safer than after, world pointer is still valid here)
        if (FrontendWorld->OwningGameInstance &&
            FrontendWorld->OwningGameInstance->LocalPlayers.Num() > 0)
        {
            FrontendWorld->OwningGameInstance->LocalPlayers.Remove(0);
            Log("Local player removed!");
        }

        UKismetSystemLibrary::ExecuteConsoleCommand(
            FrontendWorld, L"open Apollo_Terrain", nullptr);
    }

    // Wait for Apollo to finish loading before doing anything else
    WaitForApollo();

    SetConsoleTitleA("Rift v17.30 | Apollo loaded - waiting for players...");

    return 0;
}

// -----------------------------------------------------------------------
// DllMain
// -----------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        MH_Uninitialize();
        break;

    default:
        break;
    }
    return TRUE;
}
