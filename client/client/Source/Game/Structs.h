#pragma once
#include <cmath>
#include <vector>
#include <string>

struct FVector
{
    float X, Y, Z;

    FVector() : X(0), Y(0), Z(0) {}
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}

    FVector operator-(const FVector& other) const { return { X - other.X, Y - other.Y, Z - other.Z }; }
    FVector operator+(const FVector& other) const { return { X + other.X, Y + other.Y, Z + other.Z }; }
    FVector operator*(float scalar) const { return { X * scalar, Y * scalar, Z * scalar }; }

    float Length() const { return sqrtf(X * X + Y * Y + Z * Z); }
    float Length2D() const { return sqrtf(X * X + Y * Y); }
    float Distance(const FVector& other) const { return (*this - other).Length(); }

    FVector Normalized() const {
        float len = Length();
        if (len < 0.0001f) return FVector(0, 0, 0);
        return FVector(X / len, Y / len, Z / len);
    }
};

struct FVector2D
{
    float X, Y;
    FVector2D() : X(0), Y(0) {}
    FVector2D(float x, float y) : X(x), Y(y) {}

    float Distance(const FVector2D& other) const {
        float dx = X - other.X;
        float dy = Y - other.Y;
        return sqrtf(dx * dx + dy * dy);
    }
};

struct FRotator
{
    float Pitch, Yaw, Roll;
    FRotator() : Pitch(0), Yaw(0), Roll(0) {}
    FRotator(float p, float y, float r) : Pitch(p), Yaw(y), Roll(r) {}
};

struct FMatrix
{
    float M[4][4];
};

struct FTransform
{
    float Rotation[4];   // Quaternion
    FVector Translation;
    float pad0;
    FVector Scale3D;
    float pad1;
};

// Source Engine 4x3 bone matrix (matrix3x4_t)
struct matrix3x4_t
{
    float m[3][4];
};

template<typename T>
struct TArray
{
    uintptr_t Data;
    int Count;
    int Max;
};

struct ESPPlayer
{
    uintptr_t Address;
    FVector Position;
    FVector HeadPosition;
    FVector Velocity;
    float Health;
    float HealthMax;
    float Shield;
    float ShieldMax;
    int ArmorTier;       // 0=none, 1=white, 2=blue, 3=purple, 4=gold, 5=red
    int TeamId;
    float Distance;
    bool IsVisible;
    bool IsKnocked;
    // Bone data for aimbot/triggerbot
    bool HasBones;
    FVector BonePositions[80]; // Up to 80 bones
    int BoneCount;
};

// Aimbot configuration
struct AimbotConfig
{
    bool Enabled = true;
    float FOV = 5.0f;
    float Smooth = 27.0f;
    int Bone = 2;
    float Deadzone = 0.5f;
    bool ShowFOV = true;
    bool Prediction = false;
    float PredictionAmount = 0.5f;
};

// Triggerbot configuration
struct TriggerbotConfig
{
    bool Enabled = true;
    int TriggerKey = VK_SPACE;    // Space
    int DelayLevel = 0;           // 0=instant, 1-4 adds delay
    bool Prediction = false;
    bool VisibleOnly = true;
};

struct Settings
{
    bool EnableGlow = true;
    bool GlowThroughWalls = true;
    bool ShowBox = true;
    bool ShowHealth = true;
    bool ShowShield = true;
    bool ShowDistance = true;
    bool ShowHeadCircle = true;
    bool ShowBones = false;
    bool ShowName = false;
    float MaxDistance = 500.0f;
    bool MenuOpen = true;
    int MenuTab = 0;              // 0=ESP, 1=Aimbot, 2=Triggerbot, 3=Misc

    bool ShowSpectators = true;
    bool FiringRangeMode = false;

    AimbotConfig Aimbot;
    TriggerbotConfig Triggerbot;

    // RCS (Recoil Control System)
    bool RCS = true;
    float RCSStrength = 0.50f;     // 0-1 how much recoil to compensate
    float RCSJitter = 0.02f;       // jitter amount (calibrated for 1200dpi / 2.75sens)

    // Movement
    bool NeoStrafe = true;

    // Item Glow
    bool ItemGlow = false;  // Off by default (performance)
};
