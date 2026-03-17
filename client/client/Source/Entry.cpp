#include <Windows.h>
#include <tlhelp32.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

#include "../Library/kmem.h"
#include "Game/Structs.h"
#include "Game/GameManager.h"
#include "Game/Aimbot.h"
#include "Game/Triggerbot.h"
#include "Game/SpectatorList.h"
#include "Game/Movement.h"
#include "Game/Config.h"
#include "Game/ItemGlow.h"
#include "Overlay/Overlay.h"
#include "Overlay/Renderer.h"

UINT32 LookupProcessId(const wchar_t* processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snapshot || snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (0 == _wcsicmp(entry.szExeFile, processName)) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

GameManager g_Game;
Settings g_Settings;
Aimbot g_Aimbot;
Triggerbot g_Triggerbot;
SpectatorList g_SpectatorList;
Movement g_Movement;
std::atomic<bool> g_Running{ true };

std::mutex g_EntityMutex;
std::vector<ESPPlayer> g_Entities;
float g_ViewMatrix[16] = {};
FVector g_LocalPos;
FVector g_PunchAngles;
float g_BulletSpeed = 0;
float g_BulletScale = 0;
float g_ScreenWidth = 1920.0f;
float g_ScreenHeight = 1080.0f;

void MemoryLoop()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    while (g_Running)
    {
        if (!g_Game.UpdatePointers())
        {
            Sleep(100);
            continue;
        }

        g_Game.UpdateLocalPlayer();

        bool needBones = false;
        g_Game.FiringRangeMode = g_Settings.FiringRangeMode;

        // Always update players if any feature needs them (ESP, aimbot, triggerbot, glow)
        bool needPlayers = g_Settings.Aimbot.Enabled ||
                           g_Settings.Triggerbot.Enabled || g_Settings.EnableGlow;

        if (needPlayers)
        {
            // Only read camera when aimbot or triggerbot need it (they use viewMatrix)
            if (g_Settings.Aimbot.Enabled || g_Settings.Triggerbot.Enabled)
                g_Game.UpdateCamera();
            // Only read weapon info when prediction is enabled (bullet speed/scale)
            if (g_Settings.Aimbot.Prediction || g_Settings.Triggerbot.Prediction)
                g_Game.UpdateWeaponInfo();
            g_Game.UpdatePlayers(g_Settings.MaxDistance, needBones);

            {
                std::lock_guard<std::mutex> lock(g_EntityMutex);
                g_Entities = g_Game.Players;
                memcpy(g_ViewMatrix, g_Game.ViewMatrix, sizeof(g_ViewMatrix));
                g_LocalPos = g_Game.LocalPosition;
                g_PunchAngles = g_Game.PunchAngles;
                g_BulletSpeed = g_Game.BulletSpeed;
                g_BulletScale = g_Game.BulletScale;
            }

            g_Aimbot.JitterEnabled = g_Settings.RCS;
            g_Aimbot.JitterAmount = g_Settings.RCSJitter;
            g_Aimbot.RCSStrength = g_Settings.RCSStrength;
            g_Aimbot.jitterLocalPlayer.store(g_Game.LocalPlayerPtr);
            if (g_Settings.RCS && !g_Aimbot.jitterThread)
                g_Aimbot.StartJitter();
            else if (!g_Settings.RCS && g_Aimbot.jitterThread)
                g_Aimbot.StopJitter();
            g_Aimbot.Update(g_Game.Players, g_Game.ViewMatrix, g_Settings.Aimbot,
                g_Game.LocalPosition, g_Game.PunchAngles,
                g_Game.BulletSpeed, g_Game.BulletScale,
                g_ScreenWidth, g_ScreenHeight, g_Game.LocalPlayerPtr);

            g_Triggerbot.Update(g_Game.Players, g_Game.ViewMatrix, g_Settings.Triggerbot,
                g_Game.LocalPosition, g_Game.BulletSpeed, g_Game.BulletScale,
                g_ScreenWidth, g_ScreenHeight);
        }

        // Safe glow — only writes to entity addresses (no global pointer writes)
        g_Game.GlowEnabled = g_Settings.EnableGlow;
        if (g_Game.GlowEnabled)
            g_Game.UpdateGlow();

        // Item glow — scan every 30 frames to save IOCTLs
        if (g_Settings.ItemGlow && (g_Game.FrameCount % 30 == 0))
            ItemGlow::Update(g_Game.BaseAddress, g_Game.LocalPosition, g_Settings.MaxDistance);

        if (g_Settings.ShowSpectators && (g_Game.FrameCount % 100 == 0))
            g_SpectatorList.Update(g_Game.LocalPlayerPtr, g_Game.BaseAddress);

        // Movement
        g_Movement.neoLocalPlayer.store(g_Game.LocalPlayerPtr);
        g_Movement.neoBaseAddress.store(g_Game.BaseAddress);
        if (g_Settings.NeoStrafe && !g_Movement.neoThread)
            g_Movement.StartNeoStrafe();
        else if (!g_Settings.NeoStrafe && g_Movement.neoThread)
            g_Movement.StopNeoStrafe();

        // Adaptive sleep
        float closestDist = 9999.0f;
        for (const auto& p : g_Game.Players)
            if (p.Distance < closestDist) closestDist = p.Distance;

        int sleepMs = (closestDist < 50.0f) ? 16 : (closestDist < 100.0f) ? 20 : (closestDist < 200.0f) ? 25 : 33;
        g_Game.FrameCount++;
        Sleep(sleepMs);
    }
}

int main(int argc, char* argv[])
{
    printf("[>] palepale v1.0\n\n");

    if (Config::Load(g_Settings))
        printf("[+] Config loaded\n");


    printf("[*] Connecting to driver...\n");
    if (!KMem::Init())
    {
        printf("[!] Load driver first\n"); getchar();
        return 1;
    }
    printf("[+] Driver connected\n");

    printf("[*] Waiting for Apex...\n");
    UINT32 pid = 0;
    for (int i = 0; i < 60 && !pid; i++)
    {
        pid = LookupProcessId(L"r5apex_dx12.exe");
        if (!pid) pid = LookupProcessId(L"r5apex.exe");
        if (!pid) Sleep(1000);
    }
    if (!pid)
    {
        printf("[!] Apex not found\n"); getchar();
        return 1;
    }
    printf("[+] PID: %u\n", pid);

    printf("[*] Attaching...\n");
    if (!KMem::AttachToProcess(pid))
    {
        printf("[!] Failed to attach\n"); getchar();
        return 1;
    }
    printf("[+] Base: 0x%llX\n", KMem::TargetBase);
    printf("[+] Mode: %s\n", KMem::UsePhysical ? "Physical" : "Virtual");

    UINT64 baseAddress = KMem::TargetBase;
    UINT64 header = KMem::Read<UINT64>(baseAddress);
    printf("[+] MZ: %s\n", (header & 0xFFFF) == 0x5A4D ? "OK" : "FAIL");
    if ((header & 0xFFFF) != 0x5A4D)
    {
        printf("[!] MZ check failed\n"); getchar();
        return 1;
    }

    g_Game.Init(pid, baseAddress);

    HWND gameWindow = FindWindowA("Respawn001", NULL);
    if (!gameWindow) gameWindow = FindWindow(nullptr, L"Apex Legends");
    if (!gameWindow) gameWindow = GetDesktopWindow();

    Overlay overlay;
    if (!overlay.Init(gameWindow))
    {
        printf("[!] Overlay failed\n"); getchar();
        return 1;
    }

    g_ScreenWidth = (float)overlay.Width;
    g_ScreenHeight = (float)overlay.Height;
    printf("[+] Overlay: %dx%d\n", overlay.Width, overlay.Height);
    printf("[+] INSERT=menu END=exit\n\n");

    // Global hotkey: INSERT works from ANY window, even fullscreen Apex
    RegisterHotKey(NULL, 1, 0, VK_INSERT);

    // Start memory thread
    std::thread memThread(MemoryLoop);

    // Start triggerbot thread (1000Hz)
    g_Triggerbot.Start(&g_Settings.Triggerbot, g_ScreenWidth, g_ScreenHeight, baseAddress);

    while (overlay.Running && g_Running)
    {
        // Global INSERT hotkey — works from any screen
        MSG hotMsg;
        while (PeekMessage(&hotMsg, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE))
        {
            if (hotMsg.wParam == 1)
                g_Settings.MenuOpen = !g_Settings.MenuOpen;
        }

        if (!overlay.ProcessMessages()) break;
        if (GetAsyncKeyState(VK_END) & 1) { g_Running = false; break; }
        if (GetAsyncKeyState(VK_F5) & 1) g_Game.ForceGlowRefresh = true;  // F5 = refresh glow

        LONG_PTR ex = GetWindowLongPtr(overlay.Window, GWL_EXSTYLE);
        SetWindowLongPtr(overlay.Window, GWL_EXSTYLE,
            g_Settings.MenuOpen ? (ex & ~WS_EX_TRANSPARENT) : (ex | WS_EX_TRANSPARENT));

        // Minimal frame when menu closed and no visual features active
        bool needFullRender = g_Settings.MenuOpen || g_Settings.ShowSpectators ||
                              g_Settings.Aimbot.Enabled; // FOV circle
        if (needFullRender)
        {
            overlay.BeginFrame();
            Renderer::DrawAimbotFOV(g_Settings, (float)overlay.Width, (float)overlay.Height);
            if (g_Settings.ShowSpectators)
                g_SpectatorList.Render();
            Renderer::DrawMenu(g_Settings);
            overlay.EndFrame();
        }
        else
        {
            // Minimal transparent frame — keeps window responsive, minimal GPU cost
            overlay.BeginFrame();
            overlay.EndFrame();
        }
    }

    g_Running = false;
    g_Triggerbot.Stop();
    g_Aimbot.StopJitter();
    g_Movement.StopNeoStrafe();
    if (memThread.joinable()) memThread.join();
    UnregisterHotKey(NULL, 1);
    overlay.Cleanup();
    return 0;
}
