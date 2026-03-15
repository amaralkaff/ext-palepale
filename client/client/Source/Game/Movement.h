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
            Sleep(10); // ~100Hz with timeBeginPeriod(1)

            ULONG64 base = self->neoBaseAddress.load();
            if (!base) continue;

            bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
            bool d = (GetAsyncKeyState('D') & 0x8000) != 0;

            if (!space) continue;

            // Jump
            KMem::Write<int>(base + Offsets::in_jump + 0x8, 5);

            // Tap strafe — only A/D, don't touch W
            if (a) KMem::Write<int>(base + Offsets::in_moveleft + 0x8, 5);
            if (d) KMem::Write<int>(base + Offsets::in_moveright + 0x8, 5);

            // Brief hold then release
            Sleep(5); // 5ms hold

            KMem::Write<int>(base + Offsets::in_jump + 0x8, 4);
            if (a) KMem::Write<int>(base + Offsets::in_moveleft + 0x8, 4);
            if (d) KMem::Write<int>(base + Offsets::in_moveright + 0x8, 4);
        }

        timeEndPeriod(1);
        return 0;
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
