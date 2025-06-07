#include "StdInclude.hpp"
#include "CameraMenu.hpp"

#include "Components/CameraManager.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "UI/Animations.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM::UI
{
    void CameraMenu::Show()
    {
		ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Active Camera: ");
        ImGui::SameLine();

		auto& cameraManager = Components::CameraManager::Get();
        auto& currentCamera = cameraManager.GetActiveCamera();
        if (ImGui::BeginCombo("##cameraCombo", cameraManager.GetCameraModeLabel(currentCamera->GetMode()).data()))
        {
            for (auto cameraMode : cameraManager.GetCameraModes())
            {
                bool isSelected = currentCamera->GetMode() == cameraMode;
                if (ImGui::Selectable(cameraManager.GetCameraModeLabel(cameraMode).data(),
                                      currentCamera->GetMode() == cameraMode))
                {
                    cameraManager.SetActiveCamera(cameraMode);
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (currentCamera->GetMode() == Components::Camera::Mode::Bone)
        {
            auto boneCamera = static_cast<Components::BoneCamera*>(currentCamera.get());
            auto entities = Mod::GetGameInterface()->GetEntities();

            ImGui::Text("Entity");
            if (ImGui::BeginCombo("##boneCameraTargetCombo", entities[boneCamera->GetEntityId()].ToString().c_str()))
            {
                for (std::size_t i = 0; i < entities.size(); i++)
                {
                    if (entities[i].type == Types::EntityType::Unsupported)
                    {
                        continue;
                    }

                    bool isSelected = boneCamera->GetEntityId() == i;
                    if (ImGui::Selectable(entities[i].ToString().c_str(), boneCamera->GetEntityId() == i))
                    {
                        boneCamera->GetEntityId() = i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const auto& supportedBoneNames = Mod::GetGameInterface()->GetSupportedBoneNames();
            ImGui::Text("Bone");
            if (ImGui::BeginCombo("##boneCameraBoneCombo", supportedBoneNames[boneCamera->GetBoneIndex()].c_str()))
            {
                for (std::size_t i = 0; i < supportedBoneNames.size(); i++)
                {
                    bool isSelected = boneCamera->GetBoneIndex() == i;
                    if (ImGui::Selectable(supportedBoneNames[i].data(), boneCamera->GetBoneIndex() == i))
                    {
                        boneCamera->GetBoneIndex() = i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const auto& boneData = Mod::GetGameInterface()->GetBoneData(
                boneCamera->GetEntityId(), supportedBoneNames.at(boneCamera->GetBoneIndex()));
            if (boneData.id == -1)
            {
                ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION " Bone not found on entity");
            }
            else
            {
                ImGui::Text("Offset");
                ImGui::DragFloat3("##boneCameraOffset", &boneCamera->GetPositionOffset().x, 1.0f, -10000.0f, 10000.0f,
                                  "%.1f");

                ImGui::Text("Rotation");
                ImGui::DragFloat3("##boneCameraRotation", &boneCamera->GetRotationOffset().x, 1.0f, -180.0f, 180.0f,
                                  "%.1f");

                ImGui::BeginDisabled();

                ImGui::Text("Smooth Movement");
                ImGui::Checkbox("##boneCameraTemporalSmoothing", &boneCamera->UseTemporalSmoothing());

                ImGui::EndDisabled();

                ImGui::Text("Show Bone");
                ImGui::Checkbox("##boneCameraShowBone", &boneCamera->ShowBone());
            }
        }

        ImGui::End();
    }

    const char* CameraMenu::GetWindowName() noexcept
	{
            return "Camera";
    }
}  // namespace IWXMVM::UI