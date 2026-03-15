#pragma once
#include "Structs.h"
#include <cmath>

namespace GameMath
{
    constexpr float PI = 3.14159265358979323846f;

    // =========================================================================
    // ViewMatrix-based WorldToScreen (Source Engine / Apex Legends)
    // matrix = flat float[16] column-major view-projection matrix
    // =========================================================================
    inline bool WorldToScreenVM(const FVector& worldPos, const float matrix[16], float screenW, float screenH, FVector2D& out)
    {
        float w = matrix[12] * worldPos.X + matrix[13] * worldPos.Y + matrix[14] * worldPos.Z + matrix[15];
        if (w < 0.001f)
            return false;

        float x = matrix[0] * worldPos.X + matrix[1] * worldPos.Y + matrix[2] * worldPos.Z + matrix[3];
        float y = matrix[4] * worldPos.X + matrix[5] * worldPos.Y + matrix[6] * worldPos.Z + matrix[7];

        float invW = 1.0f / w;
        x *= invW;
        y *= invW;

        out.X = (screenW * 0.5f) + (x * screenW * 0.5f);
        out.Y = (screenH * 0.5f) - (y * screenH * 0.5f);

        return true;
    }

    // =========================================================================
    // Get bone world position from Source Engine 4x3 bone matrix
    // The bone matrix already contains the world-space transform
    // =========================================================================
    inline FVector GetBonePositionApex(const matrix3x4_t& bone)
    {
        return FVector(bone.m[0][3], bone.m[1][3], bone.m[2][3]);
    }

    // =========================================================================
    // UE4-style WorldToScreen (kept for reference / other games)
    // =========================================================================
    inline FMatrix RotatorToMatrix(FRotator rot)
    {
        float radPitch = rot.Pitch * PI / 180.0f;
        float radYaw = rot.Yaw * PI / 180.0f;
        float radRoll = rot.Roll * PI / 180.0f;

        float sp = sinf(radPitch), cp = cosf(radPitch);
        float sy = sinf(radYaw), cy = cosf(radYaw);
        float sr = sinf(radRoll), cr = cosf(radRoll);

        FMatrix m = {};
        m.M[0][0] = cp * cy;
        m.M[0][1] = cp * sy;
        m.M[0][2] = sp;
        m.M[0][3] = 0.0f;

        m.M[1][0] = sr * sp * cy - cr * sy;
        m.M[1][1] = sr * sp * sy + cr * cy;
        m.M[1][2] = -sr * cp;
        m.M[1][3] = 0.0f;

        m.M[2][0] = -(cr * sp * cy + sr * sy);
        m.M[2][1] = cy * sr - cr * sp * sy;
        m.M[2][2] = cr * cp;
        m.M[2][3] = 0.0f;

        m.M[3][0] = 0.0f;
        m.M[3][1] = 0.0f;
        m.M[3][2] = 0.0f;
        m.M[3][3] = 1.0f;

        return m;
    }

    inline bool WorldToScreen(const FVector& worldPos, const FVector& cameraPos, const FRotator& cameraRot, float fov, float screenW, float screenH, FVector2D& out)
    {
        FMatrix rotMatrix = RotatorToMatrix(cameraRot);

        FVector delta = worldPos - cameraPos;

        FVector transform;
        transform.X = delta.X * rotMatrix.M[1][0] + delta.Y * rotMatrix.M[1][1] + delta.Z * rotMatrix.M[1][2];
        transform.Y = delta.X * rotMatrix.M[2][0] + delta.Y * rotMatrix.M[2][1] + delta.Z * rotMatrix.M[2][2];
        transform.Z = delta.X * rotMatrix.M[0][0] + delta.Y * rotMatrix.M[0][1] + delta.Z * rotMatrix.M[0][2];

        if (transform.Z < 0.1f)
            return false;

        float fovRad = fov * PI / 360.0f;
        float screenCenterX = screenW / 2.0f;
        float screenCenterY = screenH / 2.0f;

        out.X = screenCenterX + transform.X * (screenCenterX / tanf(fovRad)) / transform.Z;
        out.Y = screenCenterY - transform.Y * (screenCenterX / tanf(fovRad)) / transform.Z;

        return true;
    }

    inline FVector GetBoneWorldPosition(const FTransform& bone, const FTransform& componentToWorld)
    {
        FVector bonePos = bone.Translation;

        float qx = componentToWorld.Rotation[0];
        float qy = componentToWorld.Rotation[1];
        float qz = componentToWorld.Rotation[2];
        float qw = componentToWorld.Rotation[3];

        float xx = qx * qx, yy = qy * qy, zz = qz * qz;
        float xy = qx * qy, xz = qx * qz, yz = qy * qz;
        float wx = qw * qx, wy = qw * qy, wz = qw * qz;

        FVector rotated;
        rotated.X = (1.0f - 2.0f * (yy + zz)) * bonePos.X + 2.0f * (xy - wz) * bonePos.Y + 2.0f * (xz + wy) * bonePos.Z;
        rotated.Y = 2.0f * (xy + wz) * bonePos.X + (1.0f - 2.0f * (xx + zz)) * bonePos.Y + 2.0f * (yz - wx) * bonePos.Z;
        rotated.Z = 2.0f * (xz - wy) * bonePos.X + 2.0f * (yz + wx) * bonePos.Y + (1.0f - 2.0f * (xx + yy)) * bonePos.Z;

        rotated.X *= componentToWorld.Scale3D.X;
        rotated.Y *= componentToWorld.Scale3D.Y;
        rotated.Z *= componentToWorld.Scale3D.Z;

        return rotated + componentToWorld.Translation;
    }
}
