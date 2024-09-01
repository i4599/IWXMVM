#include "StdInclude.hpp"
#include "Tabs.hpp"

#include "UI/Animations.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM::UI
{
    struct Tab
    {
        const char* icon;
        bool* show;
    };

    static constexpr std::size_t MAX_TABS = 16;

    static Tab tabs[MAX_TABS];
    static std::size_t idx = 0;

    void Tabs::Add(const char* icon, bool* show) noexcept
    {
        if (idx >= MAX_TABS)
        {
            LOG_WARN("Tab manager: idx >= MAX_TABS");
            return;
        }

        tabs[idx] = {
            .icon = icon,
            .show = show,
        };

        idx++;
    }

    void Tabs::Render()
    {
        idx = 0;

        float tabWidth = Manager::GetWindowSizeX() / 36.0f;
        float tabHeight = tabWidth;
        float tabGap = Manager::GetFontSize() * 0.4f;
        float additionalGap = tabGap;

        for (std::size_t i = 0; i < MAX_TABS; i++)
        {
            if (!tabs[i].icon)
            {
                break;
            }

            float X = Manager::GetWindowSizeX() - (tabGap + tabWidth) * static_cast<float>(i / 2 + 1) - additionalGap;
            float Y = Manager::GetWindowSizeY() - tabGap - tabHeight - ((i % 2 == 0) ? 0.0f : tabGap + tabHeight) -
                      additionalGap;
            ImVec2 tab_pos = {X, Y};

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoResize;
            char name[16];
            std::snprintf(name, sizeof(name), "##tab_%d", i);

            ImGui::SetNextWindowPos({X, Y}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({tabWidth, tabHeight}, ImGuiCond_Always);
            if (ImGui::Begin(name, nullptr, flags))
            {
                ImGui::SetCursorPos({});
                if (ImGui::Button(tabs[i].icon, {tabWidth, tabHeight}))
                {
                    *tabs[i].show = !*tabs[i].show;
                }
            }
            ImGui::End();

            tabs[i].icon = nullptr;
        }
    }
}  // namespace IWXMVM::UI