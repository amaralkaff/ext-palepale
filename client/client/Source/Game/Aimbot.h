#pragma once
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include <cmath>
#include <vector>
#include <cfloat>
#include <random>
#include "Structs.h"
#include "Offsets.h"
#include "../../Library/kmem.h"

class Aimbot
{
public:
    FVector GetTargetBone(const ESPPlayer& player, int bone)
    {
        FVector t = player.Position;
        switch (bone)
        {
        case 0: t.Z += 60.0f; break;
        case 1: t.Z += 50.0f; break;
        case 2: t.Z += 35.0f; break;
        case 3: t.Z += 20.0f; break;
        case 4: t.Z += 10.0f; break;
        default: t.Z += 35.0f; break;
        }
        return t;
    }

    FVector CalcAngles(const FVector& src, const FVector& dst)
    {
        FVector d = src - dst;
        float hyp = sqrtf(d.X * d.X + d.Y * d.Y);
        if (hyp < 0.001f) hyp = 0.001f;
        FVector a;
        a.X = atanf(d.Z / hyp) * (180.0f / 3.14159265f);
        a.Y = atan2f(d.Y, d.X) * (180.0f / 3.14159265f) + 180.0f;
        a.Z = 0.0f;
        while (a.Y > 180.0f) a.Y -= 360.0f;
        while (a.Y < -180.0f) a.Y += 360.0f;
        return a;
    }

    float GetFov(const FVector& view, const FVector& target)
    {
        float dp = NormalizeAngle(view.X - target.X);
        float dy = NormalizeAngle(view.Y - target.Y);
        return sqrtf(dp * dp + dy * dy);
    }

    float NormalizeAngle(float a)
    {
        while (a > 180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    bool JitterEnabled = false;
    float JitterAmount = 0.02f;
    float RCSStrength = 0.50f;
    HANDLE jitterThread = NULL;
    std::atomic<bool> jitterRunning{ false };
    std::atomic<ULONG64> jitterLocalPlayer{ 0 };

    // Jitter + auto anti-recoil via punch angle tracking
    // Reads m_vecPunchWeapon_Angle delta each tick and counters it
    // Works for ALL weapons automatically — no per-weapon config needed
    static DWORD WINAPI JitterThread(LPVOID param)
    {
        Aimbot* self = (Aimbot*)param;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        timeBeginPeriod(1);

        FVector lastPunch = {};
        bool wasShooting = false;
        int step = 0;

        while (self->jitterRunning.load())
        {
            Sleep(3); // ~333Hz with timeBeginPeriod(1), close enough to 300Hz

            if (!self->JitterEnabled) continue;

            ULONG64 lp = self->jitterLocalPlayer.load();
            if (!lp) continue;

            bool shooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            bool ads = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

            if (!shooting || !ads)
            {
                lastPunch = {};
                wasShooting = false;
                step = 0;
                continue;
            }

            // Read current punch angles (recoil displacement)
            FVector punch = KMem::Read<FVector>(lp + Offsets::m_vecPunchAngle);
            FVector va = KMem::Read<FVector>(lp + Offsets::m_viewAngles);

            if (!wasShooting)
            {
                // First frame shooting — init tracking
                lastPunch = punch;
                wasShooting = true;
                continue;
            }

            // Punch delta = how much recoil moved this tick
            float dPitch = punch.X - lastPunch.X;
            float dYaw = punch.Y - lastPunch.Y;
            lastPunch = punch;

            // Counter recoil: subtract 85% of punch delta
            // Not 100% — leaves a tiny bit so it looks natural
            va.X -= dPitch * self->RCSStrength;
            va.Y -= dYaw * self->RCSStrength;

            // Jitter pattern on top
            float j = self->JitterAmount;
            float jY = (step % 2 == 0) ? -j : j;
            float jX = (step % 2 == 0) ? -j * 0.5f : j * 0.5f;
            va.X += jX;
            va.Y += jY;
            step++;

            KMem::Write<FVector>(lp + Offsets::m_viewAngles, va);
        }
        timeEndPeriod(1);
        return 0;
    }

    void StartJitter() {
        if (jitterThread) return;
        jitterRunning.store(true);
        jitterThread = CreateThread(NULL, 0, JitterThread, this, 0, NULL);
    }
    void StopJitter() {
        if (!jitterThread) return;
        jitterRunning.store(false);
        WaitForSingleObject(jitterThread, 2000);
        CloseHandle(jitterThread); jitterThread = NULL;
    }

    void Update(const std::vector<ESPPlayer>& players, const float viewMatrix[16],
        const AimbotConfig& config, const FVector& localPos,
        const FVector& punchAngles, float bulletSpeed, float bulletScale,
        float screenW, float screenH, ULONG64 localPlayerPtr)
    {
        if (!localPlayerPtr) return;
        bool shooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        // Jitter handled by dedicated 300Hz thread (JitterThread)

        if (!config.Enabled || !shooting) return;
        bool ads = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        FVector viewAngles = KMem::Read<FVector>(localPlayerPtr + Offsets::m_viewAngles);
        FVector camPos = KMem::Read<FVector>(localPlayerPtr + Offsets::CameraPosition);
        if (camPos.X == 0 && camPos.Y == 0 && camPos.Z == 0) camPos = localPos;

        const ESPPlayer* best = nullptr;
        float bestFov = config.FOV;
        FVector bestPoint;

        for (const auto& p : players)
        {
            if (p.Health <= 0) continue;
            FVector aim = GetTargetBone(p, config.Bone);

            // Movement prediction: lead the target based on velocity + distance
            if (config.Prediction)
            {
                float dist = camPos.Distance(aim);
                // Lead time: distance / avg bullet speed (30000 units/sec for most weapons)
                // Scaled by PredictionAmount (0-1)
                float leadTime = (dist / 30000.0f) * config.PredictionAmount;
                if (leadTime > 0.3f) leadTime = 0.3f;  // cap at 300ms

                // Only predict if enemy is actually moving
                float speed = p.Velocity.Length();
                if (speed > 50.0f)  // moving faster than 50 units/sec
                {
                    aim.X += p.Velocity.X * leadTime;
                    aim.Y += p.Velocity.Y * leadTime;
                    aim.Z += p.Velocity.Z * leadTime;
                }
            }

            float fov = GetFov(viewAngles, CalcAngles(camPos, aim));
            if (fov < bestFov) { bestFov = fov; best = &p; bestPoint = aim; }
        }

        if (!best) return;

        FVector desired = CalcAngles(camPos, bestPoint);
        float dx = NormalizeAngle(desired.X - viewAngles.X);
        float dy = NormalizeAngle(desired.Y - viewAngles.Y);

        if (sqrtf(dx * dx + dy * dy) < config.Deadzone) return;

        float t = 1.0f - (config.Smooth / 30.0f);
        if (t < 0.05f) t = 0.05f;
        if (t > 1.0f) t = 1.0f;

        FVector out;
        out.X = viewAngles.X + dx * t;
        out.Y = NormalizeAngle(viewAngles.Y + dy * t);
        out.Z = 0;

        if (out.X > 89.0f) out.X = 89.0f;
        if (out.X < -89.0f) out.X = -89.0f;

        KMem::Write<FVector>(localPlayerPtr + Offsets::m_viewAngles, out);
    }
};
