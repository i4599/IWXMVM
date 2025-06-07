#include "StdInclude.hpp"
#include "VisualsMenu.hpp"

#include "Components/CameraManager.hpp"
#include "Components/VisualConfiguration.hpp"
#include "Mod.hpp"
#include "Utilities/PathUtils.hpp"
#include "Resources.hpp"
#include "UI/Animations.hpp"
#include "UI/ImGuiEx/KeyframeableControls.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    struct Preset
    {
        std::string name;
        std::filesystem::path path;
    };

    static bool initialized = false;

    static Components::VisualConfiguration::Settings visuals;
    static Components::VisualConfiguration::Settings defaultVisuals;
    static bool visualsInitialized = false;

    static std::vector<Preset> recentPresets;
    static Preset defaultPreset;
    static Preset currentPreset;

    static void UpdateDof()
    {
        Mod::GetGameInterface()->SetDof(visuals.dof);
    }

    static void UpdateSun()
    {
        IWXMVM::Types::Sun sunSettings = {glm::make_vec3(visuals.sunColor), glm::make_vec3(visuals.sunDirection),
                                          visuals.sunBrightness};
        Mod::GetGameInterface()->SetSun(sunSettings);
    }

    static void UpdateFilmtweaks()
    {
        Mod::GetGameInterface()->SetFilmtweaks(visuals.filmtweaks);
    }

    static void UpdateHudInfo()
    {
        // Do not update HUD settings when using any free-floating camera
        // We do not want to overwrite cg_draw2D for example
        auto& camera = Components::CameraManager::Get().GetActiveCamera();
        if (camera->IsModControlledCameraMode())
        {
            return;
        }

        Mod::GetGameInterface()->SetHudInfo(visuals.hudInfo);
    }

    static void AddPresetToRecent(Preset newPreset)
    {
        for (size_t i = 0; i < recentPresets.size(); ++i)
        {
            Preset preset = recentPresets[i];
            if (preset.path == newPreset.path)
            {
                recentPresets.erase(recentPresets.begin() + i);
                break;
            }
        }
        recentPresets.push_back(newPreset);
    }

    static void LoadPreset(Preset preset)
    {
        auto newConfiguration = Components::VisualConfiguration::Load(preset.path);
        if (newConfiguration.has_value())
        {
            visuals = newConfiguration.value();

            UpdateDof();
            UpdateSun();
            UpdateFilmtweaks();
            UpdateHudInfo();

            AddPresetToRecent(preset);
            currentPreset = preset;
        }
    }

    void VisualsMenu::Show()
    {
        if (!initialized)
        {
            auto dof = Mod::GetGameInterface()->GetDof();
            auto sun = Mod::GetGameInterface()->GetSun();
            auto filmtweaks = Mod::GetGameInterface()->GetFilmtweaks();
            auto hudInfo = Mod::GetGameInterface()->GetHudInfo();

            visuals = {dof, sun.color, sun.direction, sun.brightness, filmtweaks, hudInfo};
            recentPresets = {};

            // We do this once to force r_dof_enable and r_dof_tweak
            // into sync with each other
            UpdateDof();

            defaultVisuals = visuals;
            defaultPreset = {"Default"};
            currentPreset = defaultPreset;

            initialized = true;
        }

        ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoCollapse);

        ImVec2 size = ImGui::GetWindowSize();

        // Save
        {
            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save"))
            {
                auto path = PathUtils::OpenFileDialog(true, OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT,
                                                      "Config\0*.cfg\0", "cfg");
                if (path.has_value())
                {
                    Components::VisualConfiguration::Save(path.value(), visuals);
                    AddPresetToRecent({path.value().filename().string(), path.value()});
                }
            }

            ImGui::SameLine();
            if (ImGui::BeginCombo("##ConfigSelect", currentPreset.name.c_str()))
            {
                if (ImGui::Selectable(ICON_FA_CHESS_KING " Default Preset", currentPreset.name == "Default"))
                {
                    visuals = defaultVisuals;
                    currentPreset = defaultPreset;

                    UpdateDof();
                    UpdateSun();
                    UpdateFilmtweaks();
                    UpdateHudInfo();
                }

                ImGui::Separator();

                for (auto& preset : recentPresets)
                {
                    auto label =
                        std::string(ICON_FA_ARROW_ROTATE_RIGHT) + " " + preset.name + "##" + preset.path.string();
                    if (ImGui::Selectable(label.c_str(), currentPreset.name == preset.name))
                    {
                        LoadPreset(preset);
                    }
                }

                ImGui::Separator();
                if (ImGui::Selectable(ICON_FA_FOLDER_OPEN " Load from file", false))
                {
                    auto path = PathUtils::OpenFileDialog(false, OFN_EXPLORER | OFN_FILEMUSTEXIST,
                                                          "Config\0*.cfg\0All Files\0*.*\0", "cfg");
                    if (path.has_value())
                    {
                        Preset preset;
                        preset.name = path.value().filename().string();
                        preset.path = path.value();
                        LoadPreset(preset);
                    }
                }

                ImGui::EndCombo();
            }
        }

        // Sun Section
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

        ImGui::PushFont(Manager::GetBoldFont());
		ImGui::Text("Sun");
        ImGui::PopFont();
        {
            bool modified = false;

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            if (ImGui::BeginTable("##SunTable", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Color");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                modified |= ImGui::ColorEdit3("##SunColor", glm::value_ptr(visuals.sunColor));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Brightness");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                modified |= ImGui::SliderFloat("##SunBrightness", &visuals.sunBrightness, 0.0f, 8.0f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Direction");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                modified |=
                    ImGui::SliderFloat3("##SunDirection", glm::value_ptr(visuals.sunDirection), -180.0f, 180.0f);

                ImGui::EndTable();
            }
            if (modified)
            {
                UpdateSun();
            }
        }

        // HUD Section
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

        ImGui::PushFont(Manager::GetBoldFont());
        ImGui::Text("HUD");
        ImGui::PopFont();
        {
            bool modified = false;

            ImGui::SameLine();
			modified |= ImGui::Checkbox("##HudToggle", &visuals.hudInfo.show2DElements);

            if (visuals.hudInfo.show2DElements)
            {
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                if (ImGui::BeginTable("##HudTable", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Player HUD");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##PlayerHud", &visuals.hudInfo.showPlayerHUD);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Shellshock/Flashbang");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##Shellshock/Flashbang", &visuals.hudInfo.showShellshock);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Crosshair");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##Crosshair", &visuals.hudInfo.showCrosshair);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Score");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##Score", &visuals.hudInfo.showScore);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("\"Text with effects\"");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##TextWithEffects", &visuals.hudInfo.showOtherText);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Blood Overlay");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##BloodOverlay", &visuals.hudInfo.showBloodOverlay);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Killfeed");
					ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##Killfeed", &visuals.hudInfo.showKillfeed);

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Team 1 Color");
					ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::ColorEdit3("##Team1Color", glm::value_ptr(visuals.hudInfo.killfeedTeam1Color));

                    ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Team 2 Color");
					ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::ColorEdit3("##Team2Color", glm::value_ptr(visuals.hudInfo.killfeedTeam2Color));

                    ImGui::EndTable();
                }
            }

            if (modified)
            {
                UpdateHudInfo();
            }
        }

        // Filmtweaks section
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

        ImGui::PushFont(Manager::GetBoldFont());
        ImGui::Text("Filmtweaks");
        ImGui::PopFont();
        {
            bool modified = false;

            ImGui::SameLine();
			modified |= ImGui::Checkbox("##TweaksToggle", &visuals.filmtweaks.enabled);

            if (visuals.filmtweaks.enabled)
            {
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                if (ImGui::BeginTable("##FilmtweaksTable", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Brightness");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::SliderFloat("##TweaksBrightness", &visuals.filmtweaks.brightness, -1.0f, 1.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Contrast");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::SliderFloat("##TweaksContrast", &visuals.filmtweaks.contrast, 0.0f, 4.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Desaturation");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::SliderFloat("##TweaksDesat", &visuals.filmtweaks.desaturation, 0.0f, 1.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Tint Light");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::ColorEdit3("##TweaksLight", glm::value_ptr(visuals.filmtweaks.tintLight));

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Tint Dark");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    modified |= ImGui::ColorEdit3("##TweaksDark", glm::value_ptr(visuals.filmtweaks.tintDark));

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Invert");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::Checkbox("##TweaksInvert", &visuals.filmtweaks.invert);

                    ImGui::EndTable();
                }
            }

            if (modified)
            {
                UpdateFilmtweaks();
            }
        }

        // DOF Section
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

        ImGui::PushFont(Manager::GetBoldFont());
        ImGui::Text("DOF");
        ImGui::PopFont();
        {
            bool modified = false;

            ImGui::SameLine();
            modified |= ImGui::Checkbox("##DofToggle", &visuals.dof.enabled);

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            if (visuals.dof.enabled)
            {
                if (ImGui::BeginTable("##DOFTable", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Far Blur");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##FarBlur", &visuals.dof.farBlur, 0.0f, 10.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
				    ImGui::Text("Far Start");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##FarStart", &visuals.dof.farStart, 0.0f, 5000.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Far End");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##FarEnd", &visuals.dof.farEnd, 0.0f, 10000.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Near Blur");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##NearBlur", &visuals.dof.nearBlur, 0.0f, 10.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Near Start");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##NearStart", &visuals.dof.nearStart, 0.0f, 5000.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Near End");
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##NearEnd", &visuals.dof.nearEnd, 0.0f, 5000.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Bias");
                    ImGui::SameLine();
                    ImGui::TableSetColumnIndex(1);
                    modified |= ImGui::SliderFloat("##DofBias", &visuals.dof.bias, 0.1f, 10.0f);

                    ImGui::EndTable();
                }
            }

            if (modified)
            {
                UpdateDof();
            }
        }

        ImGui::End();
    }

    const char* VisualsMenu::GetWindowName() noexcept
    {
        return "Visuals";
    }
}  // namespace IWXMVM::UI