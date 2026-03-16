#pragma once
#include "../imgui/imgui.h"
#include "../Game/Structs.h"
#include "../Game/Math.h"
#include "../Game/Config.h"
#include <cstdio>

class Renderer
{
public:
    static void DrawESP(const std::vector<ESPPlayer>& players, const float viewMatrix[16], const Settings& settings, float screenW, float screenH)
    {
        return; // ESP removed — using glow instead

        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (const auto& player : players)
        {
            FVector2D footScreen, headScreen;

            if (!GameMath::WorldToScreenVM(player.Position, viewMatrix, screenW, screenH, footScreen))
                continue;

            FVector headTop = player.HeadPosition;
            headTop.Z += 10.0f;
            if (!GameMath::WorldToScreenVM(headTop, viewMatrix, screenW, screenH, headScreen))
                continue;

            float boxHeight = fabsf(footScreen.Y - headScreen.Y);
            float boxWidth = boxHeight * 0.5f;

            if (boxHeight < 4.0f) continue;

            float boxLeft = footScreen.X - boxWidth / 2.0f;
            float boxTop = headScreen.Y;
            float boxRight = footScreen.X + boxWidth / 2.0f;
            float boxBottom = footScreen.Y;

            ImU32 color;
            if (player.IsKnocked)
                color = IM_COL32(255, 165, 0, 255);
            else if (player.IsVisible)
                color = IM_COL32(0, 255, 0, 255);
            else
                color = IM_COL32(255, 0, 0, 255);

            // Box ESP
            if (settings.ShowBox)
            {
                draw->AddRect(ImVec2(boxLeft, boxTop), ImVec2(boxRight, boxBottom), color, 0.0f, 0, 1.5f);
                draw->AddRect(ImVec2(boxLeft - 1, boxTop - 1), ImVec2(boxRight + 1, boxBottom + 1), IM_COL32(0, 0, 0, 150), 0.0f, 0, 1.0f);
            }

            // Health bar (left side)
            if (settings.ShowHealth)
            {
                float healthPercent = player.Health / player.HealthMax;
                if (healthPercent > 1.0f) healthPercent = 1.0f;
                if (healthPercent < 0.0f) healthPercent = 0.0f;
                float barHeight = boxHeight * healthPercent;
                float barLeft = boxLeft - 5.0f;
                float barRight = boxLeft - 2.0f;

                draw->AddRectFilled(ImVec2(barLeft, boxTop), ImVec2(barRight, boxBottom), IM_COL32(0, 0, 0, 150));
                ImU32 healthColor;
                if (healthPercent > 0.5f)
                    healthColor = IM_COL32(0, 255, 0, 255);
                else if (healthPercent > 0.25f)
                    healthColor = IM_COL32(255, 255, 0, 255);
                else
                    healthColor = IM_COL32(255, 0, 0, 255);

                draw->AddRectFilled(ImVec2(barLeft, boxBottom - barHeight), ImVec2(barRight, boxBottom), healthColor);
            }

            // Shield bar (right side)
            if (settings.ShowShield && player.ShieldMax > 0)
            {
                float shieldPercent = player.Shield / player.ShieldMax;
                if (shieldPercent > 1.0f) shieldPercent = 1.0f;
                if (shieldPercent < 0.0f) shieldPercent = 0.0f;
                float barHeight = boxHeight * shieldPercent;
                float barLeft = boxRight + 2.0f;
                float barRight = boxRight + 5.0f;

                draw->AddRectFilled(ImVec2(barLeft, boxTop), ImVec2(barRight, boxBottom), IM_COL32(0, 0, 0, 150));

                ImU32 shieldColor;
                switch (player.ArmorTier)
                {
                case 1:  shieldColor = IM_COL32(200, 200, 200, 255); break;
                case 2:  shieldColor = IM_COL32(39,  120, 255, 255); break;
                case 3:  shieldColor = IM_COL32(160, 32,  240, 255); break;
                case 4:  shieldColor = IM_COL32(255, 215, 0,   255); break;
                case 5:  shieldColor = IM_COL32(255, 40,  40,  255); break;
                default: shieldColor = IM_COL32(150, 150, 150, 255); break;
                }

                draw->AddRectFilled(ImVec2(barLeft, boxBottom - barHeight), ImVec2(barRight, boxBottom), shieldColor);
            }

            // Distance text
            if (settings.ShowDistance)
            {
                char distText[32];
                snprintf(distText, sizeof(distText), "%.0fm", player.Distance);
                ImVec2 textSize = ImGui::CalcTextSize(distText);
                float textX = footScreen.X - textSize.x / 2.0f;
                float textY = boxBottom + 2.0f;
                draw->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 200), distText);
                draw->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), distText);
            }

            // Head circle
            if (settings.ShowHeadCircle)
            {
                FVector2D headCenter;
                if (GameMath::WorldToScreenVM(player.HeadPosition, viewMatrix, screenW, screenH, headCenter))
                {
                    float radius = boxHeight * 0.08f;
                    if (radius < 3.0f) radius = 3.0f;
                    draw->AddCircle(ImVec2(headCenter.X, headCenter.Y), radius, color, 12, 1.5f);
                }
            }

            // Bone ESP
            if (settings.ShowBones && player.HasBones && player.BoneCount > 0)
            {
                DrawBones(draw, player, viewMatrix, screenW, screenH, color);
            }
        }
    }

    static void DrawBones(ImDrawList* draw, const ESPPlayer& player, const float viewMatrix[16],
        float screenW, float screenH, ImU32 color)
    {
        // Define bone connections
        struct BoneLink { int from; int to; };
        static const BoneLink links[] = {
            { Offsets::Bones::Head, Offsets::Bones::Neck },
            { Offsets::Bones::Neck, Offsets::Bones::UpperChest },
            { Offsets::Bones::UpperChest, Offsets::Bones::LowerChest },
            { Offsets::Bones::LowerChest, Offsets::Bones::Stomach },
            { Offsets::Bones::Stomach, Offsets::Bones::Pelvis },
            // Left arm
            { Offsets::Bones::UpperChest, Offsets::Bones::LeftShoulder },
            { Offsets::Bones::LeftShoulder, Offsets::Bones::LeftElbow },
            { Offsets::Bones::LeftElbow, Offsets::Bones::LeftHand },
            // Right arm
            { Offsets::Bones::UpperChest, Offsets::Bones::RightShoulder },
            { Offsets::Bones::RightShoulder, Offsets::Bones::RightElbow },
            { Offsets::Bones::RightElbow, Offsets::Bones::RightHand },
            // Left leg
            { Offsets::Bones::Pelvis, Offsets::Bones::LeftThigh },
            { Offsets::Bones::LeftThigh, Offsets::Bones::LeftKnee },
            { Offsets::Bones::LeftKnee, Offsets::Bones::LeftFoot },
            // Right leg
            { Offsets::Bones::Pelvis, Offsets::Bones::RightThigh },
            { Offsets::Bones::RightThigh, Offsets::Bones::RightKnee },
            { Offsets::Bones::RightKnee, Offsets::Bones::RightFoot },
        };

        for (const auto& link : links)
        {
            if (link.from >= player.BoneCount || link.to >= player.BoneCount) continue;

            FVector2D fromScreen, toScreen;
            if (GameMath::WorldToScreenVM(player.BonePositions[link.from], viewMatrix, screenW, screenH, fromScreen) &&
                GameMath::WorldToScreenVM(player.BonePositions[link.to], viewMatrix, screenW, screenH, toScreen))
            {
                draw->AddLine(ImVec2(fromScreen.X, fromScreen.Y), ImVec2(toScreen.X, toScreen.Y), color, 1.5f);
            }
        }
    }

    static void DrawAimbotFOV(const Settings& settings, float screenW, float screenH)
    {
        if (!settings.Aimbot.Enabled || !settings.Aimbot.ShowFOV) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        // FOV circle — same as ext: tan(fov/2) * screenH
        float fovRad = settings.Aimbot.FOV * 3.14159265f / 180.0f;
        float radius = tanf(fovRad * 0.5f) * screenH;
        if (radius < 10.0f) radius = 10.0f;
        if (radius > screenH * 0.5f) radius = screenH * 0.5f;
        draw->AddCircle(ImVec2(screenW / 2.0f, screenH / 2.0f), radius, IM_COL32(255, 255, 255, 60), 64, 1.0f);
    }

    static void DrawMenu(Settings& settings)
    {
        if (!settings.MenuOpen) return;

        ImGui::SetNextWindowSize(ImVec2(380, 420), ImGuiCond_FirstUseEver);
        ImGui::Begin("palepale", &settings.MenuOpen, ImGuiWindowFlags_NoCollapse);

        // Tab buttons
        const char* tabs[] = { "Glow", "Aimbot", "Triggerbot", "Misc" };
        for (int i = 0; i < 4; i++)
        {
            if (i > 0) ImGui::SameLine();
            bool selected = (settings.MenuTab == i);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(tabs[i], ImVec2(80, 25)))
                settings.MenuTab = i;
            if (selected) ImGui::PopStyleColor();
        }
        ImGui::Separator();

        switch (settings.MenuTab)
        {
        case 0: DrawGlowTab(settings); break;
        case 1: DrawAimbotTab(settings); break;
        case 2: DrawTriggerbotTab(settings); break;
        case 3: DrawMiscTab(settings); break;
        }

        ImGui::End();
    }

private:
    static void DrawGlowTab(Settings& settings)
    {
        ImGui::Checkbox("Enable Glow", &settings.EnableGlow);
        if (settings.EnableGlow)
        {
            ImGui::Checkbox("Through Walls", &settings.GlowThroughWalls);
            ImGui::Separator();
            ImGui::SliderFloat("Max Distance (m)", &settings.MaxDistance, 50.0f, 1000.0f, "%.0f");
        }
    }

    static void DrawAimbotTab(Settings& settings)
    {
        AimbotConfig& aim = settings.Aimbot;

        ImGui::Text("RCS (Recoil Control System)");
        ImGui::Checkbox("Enable RCS", &settings.RCS);
        if (settings.RCS)
        {
            float strengthPct = settings.RCSStrength * 100.0f;
            if (ImGui::SliderFloat("Strength", &strengthPct, 0.0f, 100.0f, "%.0f%%"))
                settings.RCSStrength = strengthPct / 100.0f;
            ImGui::SliderFloat("Jitter", &settings.RCSJitter, 0.005f, 0.1f, "%.3f deg");
        }

        ImGui::Separator();
        ImGui::Checkbox("Enable Aimbot", &aim.Enabled);
        if (!aim.Enabled) return;

        const char* bones[] = { "Head", "Neck", "Upper Chest", "Lower Chest", "Stomach" };
        ImGui::Combo("Bone", &aim.Bone, bones, 5);
        ImGui::SliderFloat("FOV", &aim.FOV, 1.0f, 30.0f, "%.0f");
        ImGui::SliderFloat("Smooth", &aim.Smooth, 1.0f, 30.0f, "%.0f");
        ImGui::SliderFloat("Deadzone", &aim.Deadzone, 0.0f, 2.0f, "%.1f");
        ImGui::Checkbox("Show FOV", &aim.ShowFOV);
        ImGui::Checkbox("Prediction", &aim.Prediction);
        if (aim.Prediction)
            ImGui::SliderFloat("Lead Amount", &aim.PredictionAmount, 0.1f, 1.0f, "%.1f");
    }

    static void DrawTriggerbotTab(Settings& settings)
    {
        TriggerbotConfig& trig = settings.Triggerbot;

        ImGui::Checkbox("Enable Triggerbot", &trig.Enabled);
        if (!trig.Enabled) return;

        ImGui::Separator();

        // Trigger key selection
        const char* trigKeys[] = { "Space", "Mouse4", "Mouse5", "Right Mouse", "Shift", "Ctrl" };
        int trigKeyValues[] = { VK_SPACE, VK_XBUTTON1, VK_XBUTTON2, VK_RBUTTON, VK_SHIFT, VK_CONTROL };
        int currentKey = 0;
        for (int i = 0; i < 6; i++) {
            if (trig.TriggerKey == trigKeyValues[i]) { currentKey = i; break; }
        }
        if (ImGui::Combo("Trigger Key", &currentKey, trigKeys, 6))
            trig.TriggerKey = trigKeyValues[currentKey];

        const char* delayLabels[] = { "None", "Low", "Medium", "High", "Very High" };
        ImGui::Combo("Delay", &trig.DelayLevel, delayLabels, 5);

        ImGui::Checkbox("Prediction", &trig.Prediction);
        ImGui::Checkbox("Visible Only", &trig.VisibleOnly);
    }

    static void DrawMiscTab(Settings& settings)
    {
        ImGui::Checkbox("Show Spectators", &settings.ShowSpectators);
        ImGui::Checkbox("Firing Range Mode", &settings.FiringRangeMode);
        ImGui::Separator();
        ImGui::Text("Movement");
        ImGui::Checkbox("Neo Strafe (hold Space)", &settings.NeoStrafe);
        ImGui::Separator();
        ImGui::Text("Hotkeys:");
        ImGui::BulletText("WIN+G - Toggle Menu");
        ImGui::BulletText("INSERT - Toggle Menu (alt)");
        ImGui::BulletText("END - Exit");
        ImGui::Separator();
        ImGui::Text("Status:");
        ImGui::BulletText("Glow: %s", settings.EnableGlow ? "ON" : "OFF");
        ImGui::BulletText("Aimbot: %s", settings.Aimbot.Enabled ? "ON" : "OFF");
        ImGui::BulletText("Triggerbot: %s", settings.Triggerbot.Enabled ? "ON" : "OFF");
        ImGui::Separator();
        if (ImGui::Button("Save Config", ImVec2(150, 25)))
            Config::Save(settings);
        ImGui::SameLine();
        if (ImGui::Button("Load Config", ImVec2(150, 25)))
            Config::Load(settings);
    }
};
