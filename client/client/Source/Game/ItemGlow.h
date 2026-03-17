#pragma once
#include <map>
#include "Offsets.h"
#include "Structs.h"
#include "../../Library/kmem.h"

namespace ItemGlow
{
    enum ItemRarity { COMMON, RARE, EPIC, LEGENDARY, HEIRLOOM };

    struct ItemInfo { const char* name; ItemRarity rarity; };

    static const std::map<int, ItemInfo> ITEM_MAP = {
        // Weapons - Heirloom tier
        {1,   {"Kraber",             HEIRLOOM}},
        {44,  {"G7 Scout",           HEIRLOOM}},
        {183, {"Car SMG",            HEIRLOOM}},
        // Weapons - Legendary
        {6,   {"Mastiff Gold",       LEGENDARY}},
        {57,  {"Volt Gold",          LEGENDARY}},
        // Heals
        {221, {"Phoenix Kit",        EPIC}},
        {222, {"Med Kit",            RARE}},
        {224, {"Shield Battery",     RARE}},
        // Armor
        {245, {"Body Armor L3",      EPIC}},
        {246, {"Body Armor L4",      HEIRLOOM}},
        {369, {"Evo Cache",          LEGENDARY}},
        // Knockdown
        {187, {"Knockdown L4",       LEGENDARY}},
        // Backpack
        {254, {"Backpack L3",        EPIC}},
        {255, {"Backpack L4",        LEGENDARY}},
        // Optics
        {199, {"1x Digital Threat",  LEGENDARY}},
        {269, {"4x-10x Digi Sniper", LEGENDARY}},
        // Hop-ups
        {234, {"Turbocharger",       LEGENDARY}},
        {249, {"Boosted Loader",     LEGENDARY}},
        // Misc
        {397, {"Patrol Kit",         LEGENDARY}},
    };

    inline int GetGlowId(ItemRarity r)
    {
        switch (r)
        {
            case HEIRLOOM:  return 32; // Gold
            case LEGENDARY: return 32; // Gold
            case EPIC:      return 53; // Purple
            case RARE:      return 37; // White
            default:        return 4;  // Cyan
        }
    }

    // Queued writes — 1 per frame, same pattern as player glow
    struct ItemWrite { ULONG64 addr; BYTE value; };
    inline std::vector<ItemWrite> Queue;
    inline int QueueIdx = 0;

    inline void Update(ULONG64 baseAddress, const FVector& localPos, float maxDistance)
    {
        // Process 1 queued write per call
        if (!Queue.empty())
        {
            if (QueueIdx < (int)Queue.size())
            {
                auto& w = Queue[QueueIdx];
                KMem::Write<BYTE>(w.addr, w.value);
                QueueIdx++;
            }
            else
            {
                Queue.clear();
                QueueIdx = 0;
            }
            return; // Only 1 write per frame
        }

        // Scan entity list for items
        ULONG64 entityListBase = baseAddress + Offsets::EntityList;
        constexpr int BATCH = 128;
        BYTE rawBuf[BATCH * 0x20];

        // Items tend to be in higher entity slots; scan a wider range
        int maxSlots = 10000;

        for (int base = 0; base < maxSlots; base += BATCH)
        {
            int count = (base + BATCH > maxSlots) ? (maxSlots - base) : BATCH;
            if (!KMem::ReadMemory(entityListBase + (ULONG64)base * 0x20, rawBuf, count * 0x20))
                continue;

            for (int j = 0; j < count; j++)
            {
                ULONG64 entityPtr = *(ULONG64*)(rawBuf + j * 0x20);
                if (!entityPtr || entityPtr < 0x10000 || entityPtr > 0x7FFFFFFFFFFF)
                    continue;

                // Read position
                FVector position = KMem::Read<FVector>(entityPtr + Offsets::m_vecAbsOrigin);
                if (position.X == 0 && position.Y == 0 && position.Z == 0)
                    continue;

                float dist = localPos.Distance(position) / 39.37f;
                if (dist > maxDistance || dist < 1.0f)
                    continue;

                // Read item ID (m_customScriptInt)
                int itemId = KMem::Read<int>(entityPtr + Offsets::m_itemId);
                if (itemId <= 0)
                    continue;

                auto it = ITEM_MAP.find(itemId);
                if (it == ITEM_MAP.end())
                    continue;

                // Check if already glowed
                BYTE curId = KMem::Read<BYTE>(entityPtr + Offsets::m_glowHighlightId);
                BYTE glowId = (BYTE)GetGlowId(it->second.rarity);
                if (curId == glowId)
                    continue;

                // Queue the glow write
                Queue.push_back({ entityPtr + Offsets::m_glowHighlightId, glowId });
            }
        }
        QueueIdx = 0;
    }
}
