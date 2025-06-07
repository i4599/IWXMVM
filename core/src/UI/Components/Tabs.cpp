#include "StdInclude.hpp"
#include "Tabs.hpp"

#include "Resources.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM::UI
{
	void Tabs::RenderKeyframeEditorToggle(ImVec2 timelineSize, ImVec2 timelinePos, bool* show)
	{
		const float windowGap = Manager::GetFontSize() * 0.3f;

        // Render toggle button to the left of the timeline
        ImVec2 buttonWndSize = {timelineSize.y, timelineSize.y};
        ImVec2 buttonWndPos = {timelinePos.x - (buttonWndSize.x + windowGap) * 2.0f, timelinePos.y};
        ImGui::SetNextWindowSize(buttonWndSize, ImGuiCond_Once);
        ImGui::SetNextWindowPos(buttonWndPos, ImGuiCond_Once);

		ImGuiWindowFlags buttonWndFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
        if (ImGui::Begin("##KeyframeEditorToggle", nullptr, buttonWndFlags))
        {
            ImGui::SetCursorPos({});
            if (ImGui::Button(ICON_FA_CIRCLE_NODES, ImGui::GetContentRegionAvail()))
            {
                *show = !(*show);
            }
        }
        ImGui::End();
		ImGui::PopStyleVar();
	}

	void Tabs::RenderSettingsToggle(ImVec2 timelineSize, ImVec2 timelinePos, bool* show)
	{
		const float windowGap = Manager::GetFontSize() * 0.3f;

        ImVec2 buttonWndSize = {timelineSize.y, timelineSize.y};
        ImVec2 buttonWndPos = {timelinePos.x - buttonWndSize.x - windowGap, timelinePos.y};
        ImGui::SetNextWindowSize(buttonWndSize, ImGuiCond_Once);
        ImGui::SetNextWindowPos(buttonWndPos, ImGuiCond_Once);

		ImGuiWindowFlags buttonWndFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
        if (ImGui::Begin("##SettingsToggle", nullptr, buttonWndFlags))
        {
            ImGui::SetCursorPos({});
            if (ImGui::Button(ICON_FA_WRENCH, ImGui::GetContentRegionAvail()))
            {
                *show = !(*show);
            }
        }
        ImGui::End();
		ImGui::PopStyleVar();
	}
}  // namespace IWXMVM::UI