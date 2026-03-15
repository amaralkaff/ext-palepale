#pragma once

// Apex Legends Offsets — from UC "Apex Reversal, Structs and Offsets" page 60
// No encryption (EAC only, no pointer obfuscation)

namespace Offsets
{
    // === Globals (RVA from r5apex.exe base) ===
    constexpr uintptr_t EntityList       = 0x644d138;   // cl_entitylist
    constexpr uintptr_t LocalPlayer      = 0x282d298;   // LocalPlayer
    constexpr uintptr_t NameList         = 0x8e47640;   // NameList
    constexpr uintptr_t ViewRender       = 0x3f372e0;   // ViewRender
    constexpr uintptr_t ViewMatrix       = 0x11a350;    // VP matrix from ViewRender (double-deref, matches ext)
    constexpr uintptr_t LevelName        = 0x1f7b9f4;   // LevelName
    constexpr uintptr_t GlobalVars       = 0x1f6f6e0;   // GlobalVars

    constexpr int NUM_ENT_ENTRIES        = 0x10000;     // max entity slots
    constexpr int ENT_ENTRY_MASK         = 0xFFFF;
    constexpr int ENTITY_STRIDE          = 0x20;        // bytes per entity slot

    // === Entity / Player Netvars ===
    constexpr uintptr_t m_vecAbsOrigin   = 0x017c;     // m_vecAbsOrigin
    constexpr uintptr_t m_iHealth        = 0x0324;      // m_iHealth
    constexpr uintptr_t m_iMaxHealth     = 0x0468;      // m_iMaxHealth
    constexpr uintptr_t m_iTeamNum       = 0x0334;      // m_iTeamNum
    constexpr uintptr_t m_lifeState      = 0x0690;      // m_lifeState (>0 = dead)
    constexpr uintptr_t m_bleedoutState  = 0x27c0;      // m_bleedoutState (>0 = knocked)
    constexpr uintptr_t m_shieldHealth   = 0x01a0;      // m_shieldHealth
    constexpr uintptr_t m_shieldHealthMax= 0x01a4;      // m_shieldHealthMax
    constexpr uintptr_t m_iName          = 0x0479;      // m_iName
    constexpr uintptr_t m_iSignifierName = 0x0470;      // m_iSignifierName
    constexpr uintptr_t m_vecAbsVelocity = 0x0170;      // m_vecAbsVelocity
    constexpr uintptr_t m_fFlags         = 0x00c8;      // m_fFlags

    // === Bones ===
    constexpr uintptr_t m_nForceBone     = 0x0db8;      // m_nForceBone
    constexpr uintptr_t m_boneMatrix     = 0x0db8 + 0x48; // bone matrix base (m_nForceBone + 0x48)
    constexpr int BONE_STRIDE            = 0x30;         // sizeof(matrix3x4_t) = 48 bytes
    constexpr uintptr_t m_pStudioHdr     = 0x1000;      // studioHdr

    // === Weapons ===
    constexpr uintptr_t m_latestPrimaryWeapons = 0x19c4; // m_latestPrimaryWeapons
    constexpr uintptr_t m_latestNonOffhand     = 0x19d4; // m_latestNonOffhandWeapons (ext: OFF_WEAPON_HANDLE)
    constexpr uintptr_t m_weaponNameIndex      = 0x1850; // m_weaponNameIndex
    constexpr uintptr_t m_ammoInClip     = 0x1610;      // m_ammoInClip
    constexpr uintptr_t m_curZoomFOV     = 0x1660 + 0x00c0; // m_playerData + m_curZoomFOV
    constexpr uintptr_t m_bulletSpeed    = 0x0d80;      // WeaponSettings.projectile_launch_speed (ext)
    constexpr uintptr_t m_bulletScale    = 0x0d88;      // WeaponSettings.projectile_gravity_scale (ext)

    // === Visibility ===
    constexpr uintptr_t m_lastVisibleTime       = 0x1a64; // lastVisibleTime
    constexpr uintptr_t m_lastCrosshairTarget   = 0x1a64 + 0x8; // lastCrosshairTargetTime

    // === Camera / View ===
    constexpr uintptr_t CameraPosition   = 0x1fc4;      // camera_origin
    constexpr uintptr_t m_viewAngles     = 0x2600;      // ViewAngles (ext)
    constexpr uintptr_t m_breathAngles   = 0x2600 - 0x10; // viewAngles - 0x10
    constexpr uintptr_t m_vecPunchAngle  = 0x2518;      // m_vecPunchWeapon_Angle
    constexpr uintptr_t m_timeBase       = 0x2168;      // timeBase

    // === Observer ===
    constexpr uintptr_t m_iObserverMode  = 0x35f4;      // m_iObserverMode
    constexpr uintptr_t m_hViewModels    = 0x2e00;      // m_hViewModels

    // === Armor ===
    constexpr uintptr_t armorType        = 0x4814;      // m_armorType
    constexpr uintptr_t m_nSkin          = 0x0d68;      // m_nSkin

    // === Skydive ===
    constexpr uintptr_t m_skydiveState   = 0x4878;      // m_skydiveState
    constexpr uintptr_t m_skydiveSpeed   = 0x4898;      // m_skydiveSpeed

    // === Movement ===
    constexpr uintptr_t m_duckState      = 0x2abc;      // m_duckState
    constexpr uintptr_t m_wallRunStartTime  = 0x370c;   // m_wallRunStartTime
    constexpr uintptr_t m_wallRunClearTime  = 0x3710;   // m_wallRunClearTime
    constexpr uintptr_t m_traversalStartTime = 0x2bd4;  // m_traversalStartTime
    constexpr uintptr_t m_traversalProgress  = 0x2bcc;  // m_traversalProgress

    // === Grapple ===
    constexpr uintptr_t m_grapple        = 0x2d20;      // m_grapple
    constexpr uintptr_t m_grappleActive  = 0x2da8;      // m_grappleActive

    // === Input (RVA from base) ===
    constexpr uintptr_t in_jump          = 0x3f3a460;
    constexpr uintptr_t in_duck          = 0x3f3a558;
    constexpr uintptr_t in_attack        = 0x3f39bd0;
    constexpr uintptr_t in_forward       = 0x3f3a598;
    constexpr uintptr_t in_backward      = 0x3f3a5c0;
    constexpr uintptr_t in_moveleft      = 0x3f3a588;
    constexpr uintptr_t in_moveright     = 0x3f3a5b0;
    constexpr uintptr_t in_speed         = 0x3f39b38;

    // === Glow / Highlight ===
    constexpr uintptr_t m_glowEnable     = 0x28C;       // m_highlightServerActiveStates (7=on, 2=off)
    constexpr uintptr_t m_glowThroughWalls = 0x26c;     // m_highlightVisibilityType (2=on, 5=off)
    constexpr uintptr_t m_glowFix        = 0x278;       // Default fix
    constexpr uintptr_t m_glowHighlightId = 0x298;      // Highlight ID
    constexpr uintptr_t m_glowDistance   = 0x294;       // Highlight_SetFarFadeDist
    constexpr uintptr_t m_highlightGenericContexts = 0x0299; // m_highlightGenericContexts
    constexpr uintptr_t m_highlightFunctionBits = 0x02f0;    // m_highlightFunctionBits
    constexpr uintptr_t m_highlightSettings = 0x6b96e40; // HighlightSettings global (RVA)
    constexpr int HIGHLIGHT_TYPE_SIZE    = 0x34;         // Size per highlight context

    // === Observer List ===
    constexpr uintptr_t ObserverList     = 0x644f158;
    constexpr uintptr_t ObserverListInArray = 0x964;

    // === Misc ===
    constexpr uintptr_t thirdperson_override = 0x266a160;
    constexpr uintptr_t m_itemId         = 0x15e4;      // m_customScriptInt

    // === Bone indices (Apex Source Engine skeleton) ===
    namespace Bones
    {
        constexpr int Head          = 8;
        constexpr int Neck          = 7;
        constexpr int UpperChest    = 5;
        constexpr int LowerChest    = 3;
        constexpr int Stomach       = 2;
        constexpr int Pelvis        = 0;
        constexpr int LeftShoulder  = 11;
        constexpr int RightShoulder = 39;
        constexpr int LeftElbow     = 12;
        constexpr int RightElbow    = 40;
        constexpr int LeftHand      = 14;
        constexpr int RightHand     = 42;
        constexpr int LeftThigh     = 63;
        constexpr int RightThigh    = 70;
        constexpr int LeftKnee      = 64;
        constexpr int RightKnee     = 71;
        constexpr int LeftFoot      = 65;
        constexpr int RightFoot     = 72;
    }
}
