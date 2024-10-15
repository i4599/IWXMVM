#include "StdInclude.hpp"
#include "ControlBar.hpp"

#include "Components/Playback.hpp"
#include "Components/Rewinding.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "Types/Dvar.hpp"
#include "UI/Blur.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    std::optional<Types::Dvar> timescale;

    void ControlBar::Render(ImVec2 keyframeManagerPos)
    {
        if (keyframeManagerPos.x == 0.0f && keyframeManagerPos.y == 0.0f)
        {
            return;
        }

        if (!timescale.has_value())
        {
            timescale = Mod::GetGameInterface()->GetDvar("timescale");
            return;
        }

        const auto demoInfo = Mod::GetGameInterface()->GetDemoInfo();
        const auto currentTick = Components::Playback::GetTimelineTick();


        float width = Manager::GetWindowSizeX() / 2.4f;
        float height = Manager::GetWindowSizeY() / 8.8f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        float X = keyframeManagerPos.x;
        float Y = keyframeManagerPos.y - Manager::GetFontSize() * 0.5f - height;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {});

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Control Bar", nullptr, flags))
        {
            Blur::RenderToWindow(size, pos);

            float barWidth = size.x * (6.0f / 7.0f);
            ImVec2 barPos = {(size.x - barWidth) / 2.0f, Manager::GetFontSize() * 2.1f};
            ImGui::SetNextItemWidth(barWidth);
            ImGui::SetCursorPos(barPos);

            static std::int32_t tick;
            const bool draggedProgressBar = ImGui::SliderInt(
                "##Timeline slider", &tick, 0, static_cast<int>(demoInfo.endTick), "", ImGuiSliderFlags_None);

            if (draggedProgressBar && !Components::Rewinding::IsRewinding())
            {
                Components::Playback::SetTickDelta(tick - static_cast<std::int32_t>(currentTick));
            }
            else
            {
                tick = static_cast<std::int32_t>(currentTick);
            }

            ImVec2 startTickPos = {barPos.x + Manager::GetFontSize() / 8.0f,
                                   barPos.y + Manager::GetFontSize() * (7.0f / 6.0f)};
            ImGui::SetCursorPos(startTickPos);
            ImGui::Text("%d", 0);

            char endTickText[32] = {};
            snprintf(endTickText, sizeof(endTickText), "%u", demoInfo.endTick);
            ImVec2 endTickSize = ImGui::CalcTextSize(endTickText);

            ImVec2 endTickPos = {barPos.x + barWidth - endTickSize.x - Manager::GetFontSize() / 8.0f,
                                 barPos.y + Manager::GetFontSize() * (7.0f / 6.0f)};
            ImGui::SetCursorPos(endTickPos);
            ImGui::Text("%u", demoInfo.endTick);

            char currentTickText[32] = {};
            snprintf(currentTickText, sizeof(currentTickText), "%u", currentTick);
            ImVec2 currentTickSize = ImGui::CalcTextSize(currentTickText);

            ImVec2 currentTickPos = {(barPos.x + barWidth - currentTickSize.x) / 2.0f,
                                     barPos.y + Manager::GetFontSize() * (7.0f / 6.0f)};
            ImGui::SetCursorPos(currentTickPos);
            ImGui::Text("%d", tick);

            ImVec2 playButtonPos = {barPos.x + Manager::GetIconsFontSize() / 8.0f,
                                    barPos.y - Manager::GetIconsFontSize() * (7.0f / 4.0f)};
            ImGui::SetCursorPos(playButtonPos);
            ImVec2 playButtonSize = {};
            if (Components::Playback::IsPaused())
            {
                ImGui::Text(ICON_FA_PLAY);
                playButtonSize = ImGui::CalcTextSize(ICON_FA_PLAY);
            }
            else
            {
                ImGui::Text(ICON_FA_PAUSE);
                playButtonSize = ImGui::CalcTextSize(ICON_FA_PAUSE);
            }
            // if (ImGui::Button(ICON_FA_PLAY, playButtonSize))
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                Components::Playback::TogglePaused();
            }

            float timescaleSliderWidth = playButtonSize.x * 10.0f;
            float timescaleSliderHeight = Manager::GetFontSize() / 2.0f;
            float oldFontSize = ImGui::GetIO().Ctx->FontSize;
            ImGui::GetIO().Ctx->FontSize = timescaleSliderHeight;

            ImVec2 timescaleSliderPos = {playButtonPos.x + playButtonSize.x + Manager::GetFontSize() / 4.0f,
                                         playButtonPos.y + (playButtonSize.y - timescaleSliderHeight) / 2.0f};
            ImGui::SetCursorPos(timescaleSliderPos);

            ImGui::SetNextItemWidth(playButtonSize.x * 10.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.12f, 0.12f, 0.12f, 1.00f});
            ImGui::SliderFloat("##Timescale", &timescale.value().value->floating_point, 0.01f, 10.0f, "",
                               ImGuiSliderFlags_Logarithmic);
            ImGui::PopStyleColor(1);

            ImGui::GetIO().Ctx->FontSize = oldFontSize;

            ImGui::PushFont(Manager::GetTQFont());
            ImVec2 timescalePos = {timescaleSliderPos.x + timescaleSliderWidth + Manager::GetFontSize() / 4.0f,
                                   playButtonPos.y + playButtonSize.y / 2.0f - Manager::GetTQFontSize() / 2.0f};
            ImGui::SetCursorPos(timescalePos);
            ImGui::Text("%.3f", timescale.value().value->floating_point);
            ImGui::PopFont();

            const char* demoName = demoInfo.name.c_str();
            ImVec2 demoNameSize = ImGui::CalcTextSize(demoName);
            ImVec2 demoNamePos = {
                barPos.x + barWidth - demoNameSize.x - Manager::GetIconsFontSize() / 8.0f,
                playButtonPos.y + playButtonSize.y / 2.0f - Manager::GetFontSize() / 2.0f,
            };
            ImGui::SetCursorPos(demoNamePos);
            ImGui::Text("%s", demoName);

            // ImGui::PopFont();
        }
        ImGui::End();
    }
}  // namespace IWXMVM::UI