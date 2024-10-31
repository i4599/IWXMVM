#include "StdInclude.hpp"
#include "CameraSelection.hpp"

#include "Components/CameraManager.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "UI/Animations.hpp"
#include "UI/Blur.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM::UI
{
    void CameraSelection::Render(ImVec2 controlBarPos)
    {
        if (controlBarPos.x == 0.0f || controlBarPos.y == 0.0f)
        {
            return;
        }
    
        float width = Manager::GetWindowSizeX() / 7.2f;
        float height = Manager::GetFontSize() * 1.3f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        float X = controlBarPos.x;
        float Y = controlBarPos.y - Manager::GetFontSize() * 0.2f - height;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 0.4f});

		auto& cameraManager = Components::CameraManager::Get();
		auto& currentCamera = cameraManager.GetActiveCamera();

        ImGui::PushFont(Manager::GetTQFont());

        if (ImGui::Begin("##Camera Selection", nullptr, flags))
        {
			// Blur::RenderToWindow(size, pos);

            float windowGap = Manager::GetFontSize() * 0.6f;
            ImGui::SetCursorPos({
                windowGap,
                (size.y - Manager::GetTQFontSize()) / 2.0f,
            });
            ImGui::Text("Active Camera: ");

            float remainingWidth = size.x - windowGap * 2.0f - ImGui::CalcTextSize("Active Camera: ").x;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(remainingWidth);
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
        }
        ImGui::End();

        float boneWidth = width / 3.0f;
        float boneHeight = height * 7.4f;
        ImVec2 boneSize = {boneWidth, boneHeight};
        ImGui::SetNextWindowSize(boneSize, ImGuiCond_Always);

        float boneX = X + size.x + Manager::GetFontSize() * 0.1f;
        float boneY = Y + height - boneHeight;
        ImVec2 bonePos = {boneX, boneY};
        ImGui::SetNextWindowPos(bonePos, ImGuiCond_Always);
        
		if (currentCamera->GetMode() == Components::Camera::Mode::Bone)
		{
			auto boneCamera = static_cast<Components::BoneCamera*>(currentCamera.get());
			auto entities = Mod::GetGameInterface()->GetEntities();
            if (ImGui::Begin("##boneCamInfo", nullptr, flags))
            {
                ImGui::Text("Entity");
                if (ImGui::BeginCombo("##boneCameraTargetCombo",
                    entities[boneCamera->GetEntityId()].ToString().c_str()))
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

                const auto& boneData = Mod::GetGameInterface()->GetBoneData(boneCamera->GetEntityId(), supportedBoneNames.at(boneCamera->GetBoneIndex()));
                if (boneData.id == -1)
                {
                    ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION " Bone not found on entity");
                }
                else
                {
                    ImGui::Text("Offset");
                    ImGui::DragFloat3("##boneCameraOffset", &boneCamera->GetPositionOffset().x, 1.0f, -10000.0f,
                                      10000.0f, "%.1f");

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

		ImGui::PopFont();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
	}
}  // namespace IWXMVM::UI