#include "StdInclude.hpp"
#include "PlayerAnimationUI.hpp"

#include "Components/PlayerAnimation.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    void PlayerAnimationUI::Show()
    {
        ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoCollapse);

        ImVec2 size = ImGui::GetWindowSize();
        static std::int32_t selected = -1;

        ImGui::Text("Latest death animation: %s", Components::PlayerAnimation::GetLatestAnimationName().data());
        if (ImGui::Checkbox("Hide all corpses", &Components::PlayerAnimation::HideAllCorpses()))
        {
            Components::PlayerAnimation::SetSelectedAnimIndex(selected = -1);
        }
        ImGui::Checkbox("Attach weapon to player", &Components::PlayerAnimation::AttachWeaponToCorpse());

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);
	    if (ImGui::Button(ICON_FA_REPEAT " Reset"))
        {
            Components::PlayerAnimation::SetSelectedAnimIndex(selected = -1);
            Components::PlayerAnimation::HideAllCorpses() = false;
            Components::PlayerAnimation::AttachWeaponToCorpse() = false;
        }

        if (Mod::GetGameInterface()->GetGameState() == Types::GameState::InDemo)
        {
            const auto& anims = Components::PlayerAnimation::GetAnimations();
            for (std::int32_t i = 0; i < std::ssize(anims); i++)
            {
                assert(anims[i].first.length() > 0 && *(anims[i].first.data() + anims[i].first.length()) == '\0');

                if (ImGui::Selectable(anims[i].first.data(), selected == i))
                {
                    Components::PlayerAnimation::SetSelectedAnimIndex(selected = i);
                }
            }
        }

        ImGui::End();
    }

    const char* PlayerAnimationUI::GetWindowName() noexcept
    {
        return "Animations";
    }
}  // namespace IWXMVM::UI