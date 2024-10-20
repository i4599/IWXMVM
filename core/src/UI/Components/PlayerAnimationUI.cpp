#include "StdInclude.hpp"
#include "PlayerAnimationUI.hpp"

#include "Components/PlayerAnimation.hpp"
#include "Resources.hpp"
#include "UI/Blur.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    static bool show = false;

    void PlayerAnimationUI::Render()
    {
        if (!show)
        {
            return;
        }

		float width = Manager::GetWindowSizeX() / 5.0f;
        float height = Manager::GetWindowSizeY() / 1.7f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);

        float X = Manager::GetWindowSizeX() / 1.4f - width / 2.0f;
        float Y = Manager::GetWindowSizeY() / 2.0f - height / 2.0f;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
        if (ImGui::Begin("##PlayerAnimationUI", &show, flags))
        {
            size = ImGui::GetWindowSize();
            pos = ImGui::GetWindowPos();

            Blur::RenderToWindow(size, pos, ImGui::GetScrollY());

            float windowGap = Manager::GetFontSize() * 0.4f;
            ImGui::PushFont(Manager::GetBoldFont());
            
            ImVec2 headerPos = {
                (size.x - ImGui::CalcTextSize("Death Animations").x) / 2.0f,
                windowGap,
            };
            ImGui::SetCursorPos(headerPos);
            ImGui::Text("Death Animations");
            ImGui::PopFont();

            ImGui::SetCursorPosY(headerPos.y + Manager::GetBoldFontSize() + windowGap);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            static std::int32_t selected = -1;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, Manager::GetFontSize() * 0.2f});

            ImGui::SetCursorPosX(windowGap);
            ImGui::SetCursorPosY(headerPos.y + Manager::GetBoldFontSize() + windowGap + 2.0f + windowGap);
            ImGui::Text("Latest death animation: %s", Components::PlayerAnimation::GetLatestAnimationName().data());

            ImGui::SetCursorPosX(windowGap);
            if (ImGui::Checkbox("Hide all corpses", &Components::PlayerAnimation::HideAllCorpses()))
            {
                Components::PlayerAnimation::SetSelectedAnimIndex(selected = -1);
            }

            ImGui::SetCursorPosX(windowGap);
            ImGui::Checkbox("Attach weapon to player", &Components::PlayerAnimation::AttachWeaponToCorpse());

            ImGui::PopStyleVar();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + windowGap);
            float oldPosY = ImGui::GetCursorPosY();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);
            ImGui::SetCursorPosY(oldPosY + 2.0f + windowGap);
            oldPosY = ImGui::GetCursorPosY();

            ImVec2 resetButtonSize = {
                size.x - windowGap * 2.0f,
                Manager::GetFontSize() * 1.1f,
            };
            ImGui::SetCursorPosX(windowGap);
            if (ImGui::Button(ICON_FA_REPEAT " Reset", resetButtonSize))
            {
                Components::PlayerAnimation::SetSelectedAnimIndex(selected = -1);
                Components::PlayerAnimation::HideAllCorpses() = false;
                Components::PlayerAnimation::AttachWeaponToCorpse() = false;
            }

            ImGui::SetCursorPosY(oldPosY + resetButtonSize.y + windowGap);

            const auto& anims = Components::PlayerAnimation::GetAnimations();
            for (std::int32_t i = 0; i < std::ssize(anims); i++)
            {
                assert(anims[i].first.length() > 0 && *(anims[i].first.data() + anims[i].first.length()) == '\0');

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {windowGap, 0.0f});
				ImGui::SetCursorPosX(windowGap);
                if (ImGui::Selectable(anims[i].first.data(), selected == i))
                {
                    Components::PlayerAnimation::SetSelectedAnimIndex(selected = i);
                }
                ImGui::PopStyleVar();
            }

        }
        ImGui::End();
    }

    bool* PlayerAnimationUI::GetShowPtr() noexcept
    {
        return &show;
    }
}  // namespace IWXMVM::UI