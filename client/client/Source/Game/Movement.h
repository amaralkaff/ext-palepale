#pragma once
#include <Windows.h>
#include <atomic>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include "../../Library/kmem.h"

// Neo Strafe: hold Space = auto-jump on every landing
// YOU control WASD direction manually — the lurch triggers naturally
// from your direction key presses while airborne

class Movement
{
public:
    HANDLE neoThread = NULL;
    std::atomic<bool> neoRunning{ false };
    std::atomic<ULONG64> neoLocalPlayer{ 0 };

    std::atomic<ULONG64> neoBaseAddress{ 0 };

    static DWORD WINAPI NeoStrafeThread(LPVOID param)
    {
        Movement* self = (Movement*)param;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        timeBeginPeriod(1);

        while (self->neoRunning.load())
        {
            Sleep(10);

            ULONG64 base = self->neoBaseAddress.load();
            if (!base) continue;

            bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (!space) continue;

            bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
            bool d = (GetAsyncKeyState('D') & 0x8000) != 0;

            // Only write jump + left/right for lurch
            // Don't touch forward/backward — your physical W/S keys handle those
            KMem::Write<int>(base + Offsets::in_jump + 0x8, 5);
            if (a) KMem::Write<int>(base + Offsets::in_moveleft + 0x8, 5);
            if (d) KMem::Write<int>(base + Offsets::in_moveright + 0x8, 5);

            Sleep(5);

            KMem::Write<int>(base + Offsets::in_jump + 0x8, 4);
            if (a) KMem::Write<int>(base + Offsets::in_moveleft + 0x8, 4);
            if (d) KMem::Write<int>(base + Offsets::in_moveright + 0x8, 4);
        }

        timeEndPeriod(1);
        return 0;
    }

    // ====== Superglide ======
    // Triggered on key press: jump → wait 1 frame → crouch, all via memory writes
    HANDLE sgThread = NULL;
    std::atomic<bool> sgRunning{ false };
    std::atomic<int> sgKey{ 'C' };

    // Auto superglide: when traversal ends (progress hits ~1.0), fire jump+crouch
    static DWORD WINAPI SuperglideThread(LPVOID param)
    {
        Movement* self = (Movement*)param;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        timeBeginPeriod(1);
        bool fired = false;

        while (self->sgRunning.load())
        {
            ULONG64 base = self->neoBaseAddress.load();
            ULONG64 lp = self->neoLocalPlayer.load();
            if (!base || !lp) { Sleep(5); continue; }

            float progress = KMem::Read<float>(lp + Offsets::m_traversalProgress);
            float startTime = KMem::Read<float>(lp + Offsets::m_traversalStartTime);

            // Reset when not climbing
            if (progress < 0.1f) { fired = false; }

            // Fire at exactly 99% — the very last moment of the climb
            if (!fired && progress > 0.99f)
            {
                KMem::Write<int>(base + Offsets::in_jump + 0x8, 5);
                Sleep(3);
                KMem::Write<int>(base + Offsets::in_jump + 0x8, 4);
                fired = true;
                Sleep(300);
            }
            Sleep(1);
        }

        timeEndPeriod(1);
        return 0;
    }

    void StartSuperglide() {
        if (sgThread) return;
        sgRunning.store(true);
        sgThread = CreateThread(NULL, 0, SuperglideThread, this, 0, NULL);
    }
    void StopSuperglide() {
        if (!sgThread) return;
        sgRunning.store(false);
        WaitForSingleObject(sgThread, 2000);
        CloseHandle(sgThread); sgThread = NULL;
    }

    void StartNeoStrafe() {
        if (neoThread) return;
        neoRunning.store(true);
        neoThread = CreateThread(NULL, 0, NeoStrafeThread, this, 0, NULL);
    }
    void StopNeoStrafe() {
        if (!neoThread) return;
        neoRunning.store(false);
        WaitForSingleObject(neoThread, 2000);
        CloseHandle(neoThread); neoThread = NULL;
    }
};
