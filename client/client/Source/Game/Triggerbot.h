#pragma once
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include <cmath>
#include <vector>
#include <mutex>
#include <atomic>
#include "Structs.h"
#include "Math.h"
#include "Offsets.h"
#include "../../Library/kmem.h"

extern std::mutex g_EntityMutex;
extern std::vector<ESPPlayer> g_Entities;

class Triggerbot
{
public:
    HANDLE tbThread = NULL;
    std::atomic<bool> tbRunning{ false };
    TriggerbotConfig* configPtr = nullptr;
    float screenW = 1920, screenH = 1080;
    ULONG64 baseAddress = 0;

    bool IsCrosshairOnTarget(const ESPPlayer& player, const float vm[16], float sw, float sh)
    {
        float cx = sw / 2.0f;
        float cy = sh / 2.0f;

        float heights[] = { 60.0f, 50.0f, 35.0f, 20.0f, 10.0f };
        float radMult[] = { 0.7f, 0.8f, 1.2f, 1.3f, 1.0f };

        float baseRadius = 20.0f;
        if (player.Distance > 10.0f)
            baseRadius = 20.0f * (10.0f / player.Distance);
        if (baseRadius < 4.0f) baseRadius = 4.0f;

        for (int i = 0; i < 5; i++)
        {
            FVector pos = player.Position;
            pos.Z += heights[i];
            FVector2D scr;
            if (!GameMath::WorldToScreenVM(pos, vm, sw, sh, scr)) continue;
            float d = sqrtf((scr.X - cx) * (scr.X - cx) + (scr.Y - cy) * (scr.Y - cy));
            if (d <= baseRadius * radMult[i]) return true;
        }
        return false;
    }

    static DWORD WINAPI TriggerThread(LPVOID param)
    {
        Triggerbot* self = (Triggerbot*)param;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        timeBeginPeriod(1);

        while (self->tbRunning.load())
        {
            if (!self->configPtr || !self->configPtr->Enabled || !self->baseAddress)
            {
                Sleep(5);
                continue;
            }

            if (!(GetAsyncKeyState(self->configPtr->TriggerKey) & 0x8000))
            {
                Sleep(5);
                continue;
            }

            // Read ViewMatrix FRESH (not cached) — eliminates crosshair lag
            float vm[16] = {};
            ULONG64 vr = KMem::Read<ULONG64>(self->baseAddress + Offsets::ViewRender);
            if (vr)
            {
                ULONG64 vmPtr = KMem::Read<ULONG64>(vr + Offsets::ViewMatrix);
                if (vmPtr > 0x10000)
                    KMem::ReadMemory(vmPtr, vm, sizeof(vm));
            }

            // Get entity snapshot
            std::vector<ESPPlayer> players;
            {
                std::lock_guard<std::mutex> lock(g_EntityMutex);
                players = g_Entities;
            }

            bool onTarget = false;
            for (const auto& p : players)
            {
                if (p.Health <= 0) continue;
                if (self->configPtr->VisibleOnly && !p.IsVisible) continue;
                if (self->IsCrosshairOnTarget(p, vm, self->screenW, self->screenH))
                {
                    onTarget = true;
                    break;
                }
            }

            if (onTarget)
            {
                // Write in_attack to memory — instant, can't be blocked by anti-cheat input hooks
                KMem::Write<int>(self->baseAddress + Offsets::in_attack + 0x8, 5);
                Sleep(16); // hold for 1 frame
                KMem::Write<int>(self->baseAddress + Offsets::in_attack + 0x8, 4);
            }

            Sleep(3);
        }

        timeEndPeriod(1);
        return 0;
    }

    void Start(TriggerbotConfig* cfg, float sw, float sh, ULONG64 base)
    {
        if (tbThread) return;
        configPtr = cfg;
        screenW = sw;
        screenH = sh;
        baseAddress = base;
        tbRunning.store(true);
        tbThread = CreateThread(NULL, 0, TriggerThread, this, 0, NULL);
    }

    void Stop()
    {
        if (!tbThread) return;
        tbRunning.store(false);
        WaitForSingleObject(tbThread, 2000);
        CloseHandle(tbThread);
        tbThread = NULL;
    }

    void Update(const std::vector<ESPPlayer>&, const float[16],
        const TriggerbotConfig&, const FVector&, float, float, float, float) {}
};
