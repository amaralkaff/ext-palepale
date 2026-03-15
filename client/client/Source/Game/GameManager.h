#pragma once
#include <Windows.h>
#include <vector>
#include "Offsets.h"
#include "Structs.h"
#include "Math.h"
#include "../../Library/kmem.h"

class GameManager
{
public:
    ULONG64 BaseAddress = 0;
    UINT32 ProcessId = 0;
    ULONG ImageSize = 0;

    ULONG64 LocalPlayerPtr = 0;
    int LocalTeamId = -1;
    FVector LocalPosition;
    float TimeBase = 0;
    float CurTime = 0;
    FVector PunchAngles;
    float BulletSpeed = 0;
    float BulletScale = 0;

    float ViewMatrix[16] = {};
    std::vector<ESPPlayer> Players;
    // Glow
    bool GlowEnabled = true;
    int GlowedCount = 0;
    int FrameCount = 0;

    bool Init(UINT32 pid, ULONG64 base)
    {
        ProcessId = pid;
        BaseAddress = base;
        IMAGE_DOS_HEADER dos = KMem::Read<IMAGE_DOS_HEADER>(base);
        IMAGE_NT_HEADERS64 nt = KMem::Read<IMAGE_NT_HEADERS64>(base + dos.e_lfanew);
        ImageSize = nt.OptionalHeader.SizeOfImage;
        return true;
    }

    bool UpdatePointers()
    {
        LocalPlayerPtr = KMem::Read<ULONG64>(BaseAddress + Offsets::LocalPlayer);
        if (!LocalPlayerPtr || LocalPlayerPtr < 0x10000)
            return false;

        return true;
    }

    void UpdateCamera()
    {
        // Double dereference: ViewRender -> pointer at ViewMatrix offset -> read 16 floats
        ULONG64 viewRender = KMem::Read<ULONG64>(BaseAddress + Offsets::ViewRender);
        if (!viewRender) return;
        ULONG64 viewMatrixPtr = KMem::Read<ULONG64>(viewRender + Offsets::ViewMatrix);
        if (!viewMatrixPtr || viewMatrixPtr < 0x10000) return;
        KMem::ReadMemory(viewMatrixPtr, ViewMatrix, sizeof(ViewMatrix));
    }

    void UpdateLocalPlayer()
    {
        if (!LocalPlayerPtr) return;
        LocalTeamId = KMem::Read<int>(LocalPlayerPtr + Offsets::m_iTeamNum);
        LocalPosition = KMem::Read<FVector>(LocalPlayerPtr + Offsets::m_vecAbsOrigin);
        TimeBase = KMem::Read<float>(LocalPlayerPtr + Offsets::m_timeBase);
        PunchAngles = KMem::Read<FVector>(LocalPlayerPtr + Offsets::m_vecPunchAngle);

        // Cache global curtime for visibility checks
        ULONG64 globalVars = KMem::Read<ULONG64>(BaseAddress + Offsets::GlobalVars);
        if (globalVars > 0x10000)
            CurTime = KMem::Read<float>(globalVars + 0x10);
        if (CurTime <= 0) CurTime = TimeBase;
    }

    void UpdateWeaponInfo()
    {
        if (!LocalPlayerPtr) return;

        // Read active weapon handle
        int weaponHandle = KMem::Read<int>(LocalPlayerPtr + Offsets::m_latestPrimaryWeapons);
        if (weaponHandle <= 0) return;

        // Convert handle to entity pointer
        int weaponIdx = weaponHandle & Offsets::ENT_ENTRY_MASK;
        ULONG64 entityListBase = BaseAddress + Offsets::EntityList;
        ULONG64 weaponPtr = KMem::Read<ULONG64>(entityListBase + (ULONG64)weaponIdx * Offsets::ENTITY_STRIDE);
        if (!weaponPtr || weaponPtr < 0x10000) return;

        BulletSpeed = KMem::Read<float>(weaponPtr + Offsets::m_bulletSpeed);
        BulletScale = KMem::Read<float>(weaponPtr + Offsets::m_bulletScale);
    }

    // =========================================================================
    // Glow: write highlight properties directly to entity memory
    // The game engine renders the glow - no overlay needed
    // =========================================================================

    bool ForceGlowRefresh = false;

    BYTE GlowStyleId = 65;

    // Queue: 1 write per frame to avoid burst crash
    struct GlowWrite { ULONG64 entity; ULONG64 addr; int value; int size; }; // size: 1=byte, 4=int
    std::vector<GlowWrite> GlowQueue;
    int GlowQueueIdx = 0;

    void QueueEntityGlow(ULONG64 entity)
    {
        GlowQueue.push_back({ entity, entity + 0x298, GlowStyleId, 1 });
        GlowQueue.push_back({ entity, entity + 0x28C, 7, 4 });
        GlowQueue.push_back({ entity, entity + 0x26c, 2, 4 });
        GlowQueue.push_back({ entity, entity + 0x292, 16256, 4 });
        GlowQueue.push_back({ entity, entity + 0x30c, 1193322764, 4 });
    }

    void UpdateGlow()
    {
        if (!LocalPlayerPtr || !GlowEnabled) return;

        // Process 1 queued write per call
        if (!GlowQueue.empty())
        {
            if (GlowQueueIdx < (int)GlowQueue.size())
            {
                auto& w = GlowQueue[GlowQueueIdx];
                // Validate entity still alive every 5th write (saves 4/5 health reads)
                bool alive = true;
                if (GlowQueueIdx % 5 == 0)
                {
                    int hp = KMem::Read<int>(w.entity + Offsets::m_iHealth);
                    alive = (hp > 0 && hp < 500);
                }
                if (alive)
                {
                    if (w.size == 1)
                        KMem::Write<BYTE>(w.addr, (BYTE)w.value);
                    else
                        KMem::Write<int>(w.addr, w.value);
                }
                GlowQueueIdx++;
            }
            else
            {
                GlowQueue.clear();
                GlowQueueIdx = 0;
            }
            return; // Only 1 write per frame
        }

        // Queue is empty — check if we need to glow new entities
        if (Players.empty()) return;

        for (const auto& player : Players)
        {
            if (!ForceGlowRefresh)
            {
                int cur = KMem::Read<int>(player.Address + 0x28C);
                if (cur == 7) continue; // Already glowed
            }
            QueueEntityGlow(player.Address);
        }
        GlowQueueIdx = 0;
        ForceGlowRefresh = false;
    }

    // Bulk entity read: read 0x700 bytes per entity in one IOCTL, then extract fields
    bool FiringRangeMode = false;

    void UpdatePlayers(float maxDistance, bool needBones)
    {
        Players.clear();
        if (!LocalPlayerPtr) return;

        ULONG64 entityListBase = BaseAddress + Offsets::EntityList;
        int maxSlots = FiringRangeMode ? 10000 : 100;

        constexpr int BATCH = 128;
        BYTE rawBuf[BATCH * 0x20];

        for (int base = 0; base < maxSlots; base += BATCH)
        {
            int count = (base + BATCH > maxSlots) ? (maxSlots - base) : BATCH;
            if (!KMem::ReadMemory(entityListBase + (ULONG64)base * 0x20, rawBuf, count * 0x20))
                continue;

            for (int j = 0; j < count; j++)
            {
                ULONG64 entityPtr = *(ULONG64*)(rawBuf + j * 0x20);
                if (!entityPtr || entityPtr == LocalPlayerPtr)
                    continue;
                if (entityPtr < 0x10000 || entityPtr > 0x7FFFFFFFFFFF)
                    continue;

                ProcessEntity(entityPtr, maxDistance, needBones);
            }
        }

    }

    void ProcessEntity(ULONG64 entity, float maxDistance, bool needBones)
    {
        // Bulk read 0x170-0x340: covers velocity, origin, shield, health, team
        constexpr uintptr_t BUF_START = 0x170;
        constexpr size_t BUF_SIZE = 0x340 - 0x170;
        BYTE entityBuf[BUF_SIZE];
        if (!KMem::ReadMemory(entity + BUF_START, entityBuf, BUF_SIZE))
            return;

        auto ReadBuf = [&](uintptr_t offset) -> void* {
            uintptr_t rel = offset - BUF_START;
            if (rel >= BUF_SIZE) return nullptr;
            return entityBuf + rel;
        };

        FVector position = *(FVector*)ReadBuf(Offsets::m_vecAbsOrigin);
        if (position.X == 0 && position.Y == 0 && position.Z == 0) return;

        float distance = LocalPosition.Distance(position) / 39.37f;
        if (distance > maxDistance || distance < 1.0f) return;

        int health = *(int*)ReadBuf(Offsets::m_iHealth);
        int teamId = *(int*)ReadBuf(Offsets::m_iTeamNum);

        if (health <= 0) return;
        if (!FiringRangeMode)
        {
            if (teamId <= 0 || teamId > 100) return;  // Raised from 50 to 100
            if (teamId == LocalTeamId) return;
        }

        int lifeState = KMem::Read<int>(entity + Offsets::m_lifeState);
        if (lifeState > 0) return;

        // Velocity
        FVector velocity = *(FVector*)ReadBuf(Offsets::m_vecAbsVelocity);

        // Shield
        int shield = *(int*)ReadBuf(Offsets::m_shieldHealth);
        int shieldMax = *(int*)ReadBuf(Offsets::m_shieldHealthMax);

        // Bleedout state (outside buffer, read separately)
        int bleedout = KMem::Read<int>(entity + Offsets::m_bleedoutState);
        int armorTier = KMem::Read<int>(entity + Offsets::armorType);

        // Visibility
        // Visibility: compare lastVisibleTime with cached CurTime
        float lastVisible = KMem::Read<float>(entity + Offsets::m_lastVisibleTime);
        bool isVisible = (lastVisible > 0 && CurTime > 0 && (CurTime - lastVisible) < 0.3f);

        FVector headPos = position;
        headPos.Z += 72.0f;

        ESPPlayer player = {};
        player.Address = entity;
        player.Position = position;
        player.HeadPosition = headPos;
        player.Velocity = velocity;
        player.Health = (float)health;
        player.HealthMax = 100.0f;
        player.Shield = (float)shield;
        player.ShieldMax = (float)shieldMax;
        player.ArmorTier = armorTier;
        player.TeamId = teamId;
        player.Distance = distance;
        player.IsVisible = isVisible;
        player.IsKnocked = (bleedout > 0);
        player.HasBones = false;
        player.BoneCount = 0;

        // Read bones if needed (for aimbot/triggerbot)
        if (needBones)
        {
            ULONG64 boneBase = KMem::Read<ULONG64>(entity + Offsets::m_boneMatrix);
            if (boneBase && boneBase > 0x10000)
            {
                constexpr int MAX_BONES = 80;
                matrix3x4_t bones[MAX_BONES];
                // Read all bones in one bulk read
                if (KMem::ReadMemory(boneBase, bones, sizeof(bones)))
                {
                    player.HasBones = true;
                    player.BoneCount = MAX_BONES;
                    for (int b = 0; b < MAX_BONES; b++)
                    {
                        player.BonePositions[b] = GameMath::GetBonePositionApex(bones[b]);
                    }
                    // Use actual bone head position if available
                    if (player.BonePositions[Offsets::Bones::Head].Z > 0)
                        player.HeadPosition = player.BonePositions[Offsets::Bones::Head];
                }
            }
        }

        Players.push_back(player);
    }

    void Update(const Settings& settings)
    {
        if (!UpdatePointers()) return;
        UpdateLocalPlayer();

        bool needBones = settings.Aimbot.Enabled || settings.Triggerbot.Enabled || settings.ShowBones;

        if (settings.Aimbot.Enabled || settings.Triggerbot.Enabled)
        {
            UpdateCamera();
            UpdateWeaponInfo();
            UpdatePlayers(settings.MaxDistance, needBones);
        }

        FrameCount++;
    }
};
