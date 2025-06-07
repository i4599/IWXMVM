#include "StdInclude.hpp"
#include "ControlBar.hpp"

#include "Components/Playback.hpp"
#include "Components/Rewinding.hpp"
#include "Input.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "Types/Dvar.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
	float playbackSpeed = 1.0f;

    std::optional<Types::Dvar> timescale;

    void SmartSetTickDelta(int32_t value)
    {
        // Skip forward/backward by the desired amount of ticks, while snapping to the closest capturing marker if 
        // there is one between where we are and where we want to go
        // added skip forward/backward to frozen tick marker
        
        auto& captureSettings = Components::CaptureManager::Get().GetCaptureSettings();
        auto currentTick = Components::Playback::GetTimelineTick();
        auto frozenTick = Components::Playback::GetFrozenTick();
        auto targetTick = currentTick + value;

        if (value > 0)
        {
            // skipping forward
            if (captureSettings.startTick > currentTick && captureSettings.startTick < targetTick)
            {
                Components::Playback::SetTickDelta(captureSettings.startTick - currentTick);
                return;
            }
            else if (captureSettings.endTick > currentTick && captureSettings.endTick < targetTick)
            {
                Components::Playback::SetTickDelta(captureSettings.endTick - currentTick);
                return;
            }

            if (frozenTick.has_value() && frozenTick.value() > currentTick && frozenTick.value() < targetTick)
            {
                Components::Playback::SetTickDelta(frozenTick.value() - currentTick);
                return;
            }
        }
        else if (value < 0)
        {
            // skipping backward
            if (captureSettings.endTick < currentTick && captureSettings.endTick > targetTick)
            {
                Components::Playback::SetTickDelta(captureSettings.endTick - currentTick, true);
                return;
            }
            else if (captureSettings.startTick < currentTick && captureSettings.startTick > targetTick)
            {
                Components::Playback::SetTickDelta(captureSettings.startTick - currentTick, true);
                return;
            }

            if (frozenTick.has_value() && frozenTick.value() < currentTick && frozenTick.value() > targetTick)
            {
                Components::Playback::SetTickDelta(frozenTick.value() - currentTick, true);
                return;
            }
        }
        Components::Playback::SetTickDelta(value);
    }

	void HandlePlaybackInput()
    {
        if (Components::CaptureManager::Get().IsCapturing())
            return;
        
        static constexpr std::array TIMESCALE_STEPS = IWXMVM::Components::Playback::TIMESCALE_STEPS;

        if (Input::BindDown(Action::PlaybackToggle))
        {
            Components::Playback::TogglePaused();
        }

        if (Input::BindDown(Action::PlaybackFaster))
        {
            float& fTimescale = timescale.value().value->floating_point;

            if (const auto it = std::upper_bound(TIMESCALE_STEPS.begin(), TIMESCALE_STEPS.end(), fTimescale);
                it != TIMESCALE_STEPS.end())
                fTimescale = *it;
        }

        if (Input::BindDown(Action::PlaybackSlower))
        {
            float& fTimescale = timescale.value().value->floating_point;

            if (const auto it = std::upper_bound(TIMESCALE_STEPS.rbegin(), TIMESCALE_STEPS.rend(), fTimescale,
                                                 std::greater<float>());
                it != TIMESCALE_STEPS.rend())
                fTimescale = *it;
        }

        if (Input::BindDown(Action::PlaybackSkipForward))
        {
            SmartSetTickDelta(1000);
        }

        if (Input::BindDown(Action::PlaybackSkipBackward))
        {
            SmartSetTickDelta(-1000);
        }

        if (Input::BindDown(Action::TimeFrameMoveStart))
        {
            auto& captureManager = Components::CaptureManager::Get();
            auto& captureSettings = captureManager.GetCaptureSettings();
            captureSettings.startTick = Components::Playback::GetTimelineTick();
        }

        if (Input::BindDown(Action::TimeFrameMoveEnd))
        {
            auto& captureManager = Components::CaptureManager::Get();
            auto& captureSettings = captureManager.GetCaptureSettings();
            captureSettings.endTick = Components::Playback::GetTimelineTick();
        }
    }

    void ControlBar::Show(ImVec2 size, ImVec2 pos)
    {
        if (!timescale.has_value())
        {
            timescale = Mod::GetGameInterface()->GetDvar("timescale");
            return;
        }

		HandlePlaybackInput();

        const auto demoInfo = Mod::GetGameInterface()->GetDemoInfo();
        const auto currentTick = Components::Playback::GetTimelineTick();

        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;

        ImGui::Begin("ControlBar", nullptr, flags);

        float playButtonHeight = size.y / 2.0f;
        float playButtonWidth = playButtonHeight;

        float windowPadding = (size.y - playButtonHeight) / 2.0f;
        float horizontalSpacing = windowPadding / 2.0f;

        ImGui::SetCursorPos({windowPadding, windowPadding});
        if (ImGui::Button(Components::Playback::IsPaused() ? ICON_FA_PLAY : ICON_FA_PAUSE, { playButtonWidth, playButtonHeight }))
        {
            Components::Playback::TogglePaused();
        }

        ImGui::PushFont(Manager::GetBoldFont());

		char timescaleText[32] = {};
		snprintf(timescaleText, sizeof(timescaleText), "%05.2f", timescale->value->floating_point);
        ImVec2 timescalePos = {windowPadding + playButtonWidth + horizontalSpacing, windowPadding};
        ImGui::SetCursorPos(timescalePos);
        ImGui::Text(timescaleText);
        ImVec2 timescaleSize = ImGui::CalcTextSize(timescaleText);

        ImGui::PopFont();

        ImVec2 barPos = {windowPadding + playButtonWidth + horizontalSpacing + timescaleSize.x + horizontalSpacing,
                         windowPadding};
        float barWidth = size.x - barPos.x - windowPadding;
        ImGui::SetNextItemWidth(barWidth);
        static std::int32_t tickValue = 0;
        ImGui::SetCursorPos(barPos);

        // Use frame padding to set the height of the slider
        float remainingSliderHeight = (Manager::GetBoldFontSize() - Manager::GetFontSize()) / 2.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, remainingSliderHeight});
        bool draggedProgressBar = ImGui::SliderInt("##Timeline", &tickValue, 0, demoInfo.endTick);
        ImGui::PopStyleVar();

		if (draggedProgressBar && !Components::Rewinding::IsRewinding())
		{
			Components::Playback::SmartSetTickDelta(tickValue - currentTick);
		}
		else
		{
			tickValue = currentTick;
		}

        ImGui::End();
    }
}  // namespace IWXMVM::UI