#include "StdInclude.hpp"
#include "VisualsMenu.hpp"

#include "Components/CameraManager.hpp"
#include "Components/VisualConfiguration.hpp"
#include "Mod.hpp"
#include "Utilities/PathUtils.hpp"
#include "Resources.hpp"
#include "UI/Animations.hpp"
#include "UI/Blur.hpp"
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

    static bool show = false;
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

    void VisualsMenu::Render()
    {
        if (!show)
        {
            return;
        }

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

        float width = Manager::GetWindowSizeX() / 5.0f;
        float height = Manager::GetWindowSizeY() / 1.7f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);

        float X = Manager::GetWindowSizeX() / 1.4f - width / 2.0f;
        float Y = Manager::GetWindowSizeY() / 2.0f - height / 2.0f;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);

        float windowGap = Manager::GetFontSize() * 1.2f;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

        if (ImGui::Begin("##VisualsMenu", NULL, flags))
        {
            size = ImGui::GetWindowSize();
            pos = ImGui::GetWindowPos();

            Blur::RenderToWindow(size, pos, ImGui::GetScrollY());

            // Save
            {
                ImGui::SetCursorPos({windowGap, windowGap});

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
                float gap = Manager::GetFontSize() * 0.2f;
                float buttonSize = ImGui::GetCursorPosX() - windowGap;
                ImGui::SetNextItemWidth(size.x - windowGap * 2.0f - buttonSize - gap);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap);
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

            float separatorGap = Manager::GetFontSize() * 0.24f;
            float textIndent = Manager::GetFontSize() * 0.7f;
            float checkboxIndent = windowGap + textIndent * 2.0f;
            float colorIndent = checkboxIndent + Manager::GetFontSize() * 8.0f;

            // Sun Section
            ImGui::SetCursorPos({0.0f, windowGap + Manager::GetFontSize() * 1.2f + separatorGap});
            float oldCursorY = ImGui::GetCursorPosY();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            ImGui::PushFont(Manager::GetBoldFont());
            ImVec2 sunTextPos = {
                windowGap,
                oldCursorY + 2.0f + separatorGap,
            };
            ImVec2 sunTextSize = ImGui::CalcTextSize("Sun");
            ImGui::SetCursorPos(sunTextPos);
            ImGui::Text("Sun");
            ImGui::PopFont();
            {
                bool modified = false;

                ImGui::SetCursorPos({0.0f, sunTextPos.y + sunTextSize.y + separatorGap});
                ImVec2 oldCursor = ImGui::GetCursorPos();
                ImGui::SetNextItemWidth(size.x);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, Manager::GetFontSize() * 0.24f});
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {Manager::GetFontSize() * 0.1f, 0.0f});

                ImGui::SetCursorPos({windowGap + textIndent, oldCursor.y + 2.0f + separatorGap});
                ImGui::Text("Color");
                ImGui::SameLine(size.x - colorIndent);
                ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                modified |= ImGui::ColorEdit3("##SunColor", glm::value_ptr(visuals.sunColor));

                ImGui::SetCursorPosX(windowGap + textIndent);
                ImGui::Text("Brightness");
                ImGui::SameLine(size.x - colorIndent);
                ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                modified |= ImGui::SliderFloat("##SunBrightness", &visuals.sunBrightness, 0.0f, 8.0f);

                ImGui::SetCursorPosX(windowGap + textIndent);
                ImGui::Text("Direction");
                ImGui::SameLine(size.x - colorIndent);
                ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                modified |=
                    ImGui::SliderFloat3("##SunDirection", glm::value_ptr(visuals.sunDirection), -180.0f, 180.0f);

                if (modified)
                {
                    UpdateSun();
                }

                ImGui::PopStyleVar(2);
            }

            // HUD Section
            ImGui::SetCursorPos({0.0f, ImGui::GetCursorPosY() + separatorGap});
            oldCursorY = ImGui::GetCursorPosY();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            ImGui::PushFont(Manager::GetBoldFont());
            ImVec2 hudTextPos = {
                windowGap,
                oldCursorY + 2.0f + separatorGap,
            };
            ImVec2 hudTextSize = ImGui::CalcTextSize("HUD");
            ImGui::SetCursorPos(hudTextPos);
            ImGui::Text("HUD");
            ImGui::PopFont();
            {
                bool modified = false;

                ImGui::SetCursorPos({hudTextPos.x + hudTextSize.x + Manager::GetFontSize() * 0.5f,
                                     hudTextPos.y + (hudTextSize.y - Manager::GetFontSize()) / 2.0f + Manager::GetFontSize() * 0.05f});
                modified |= ImGui::Checkbox("##HudToggle", &visuals.hudInfo.show2DElements);

                if (visuals.hudInfo.show2DElements)
                {
                    ImGui::SetCursorPos({0.0f, hudTextPos.y + hudTextSize.y + separatorGap});
                    ImVec2 oldCursor = ImGui::GetCursorPos();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, Manager::GetFontSize() * 0.24f});
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {Manager::GetFontSize() * 0.1f, 0.0f});

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::SetCursorPosY(oldCursor.y + 2.0f + separatorGap);
                    ImGui::Text("Player HUD");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##PlayerHud", &visuals.hudInfo.showPlayerHUD);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Shellshock/Flashbang");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##Shellshock/Flashbang", &visuals.hudInfo.showShellshock);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Crosshair");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##Crosshair", &visuals.hudInfo.showCrosshair);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Score");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##Score", &visuals.hudInfo.showScore);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("\"Text with effects\"");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##TextWithEffects", &visuals.hudInfo.showOtherText);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Blood Overlay");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##BloodOverlay", &visuals.hudInfo.showBloodOverlay);

                    ImGui::NewLine();

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Killfeed");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##Killfeed", &visuals.hudInfo.showKillfeed);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Team 1 Color");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::ColorEdit3("##Team1Color", glm::value_ptr(visuals.hudInfo.killfeedTeam1Color));

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Team 2 Color");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::ColorEdit3("##Team2Color", glm::value_ptr(visuals.hudInfo.killfeedTeam2Color));

                    ImGui::PopStyleVar(2);
                }

                if (modified)
                {
                    UpdateHudInfo();
                }
            }

            // Filmtweaks section
            if (visuals.hudInfo.show2DElements)
            {
                ImGui::SetCursorPos({0.0f, ImGui::GetCursorPosY() + separatorGap});
            }
            else
            {
                ImGui::SetCursorPos({0.0f, hudTextPos.y + hudTextSize.y + separatorGap});
            }
            oldCursorY = ImGui::GetCursorPosY();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            ImGui::PushFont(Manager::GetBoldFont());
            ImVec2 filmTextPos = {windowGap, oldCursorY + 2.0f + separatorGap};
            ImVec2 filmTextSize = ImGui::CalcTextSize("Filmtweaks");
            ImGui::SetCursorPos(filmTextPos);
            ImGui::Text("Filmtweaks");
            ImGui::PopFont();
            {
                bool modified = false;

                ImGui::SetCursorPos({filmTextPos.x + filmTextSize.x + Manager::GetFontSize() * 0.5f,
                                     filmTextPos.y + (filmTextSize.y - Manager::GetFontSize()) / 2.0f + Manager::GetFontSize() * 0.05f});
                modified |= ImGui::Checkbox("##TweaksToggle", &visuals.filmtweaks.enabled);

                if (visuals.filmtweaks.enabled)
                {
                    ImGui::SetCursorPos({0.0f, filmTextPos.y + filmTextSize.y + separatorGap});
                    oldCursorY = ImGui::GetCursorPosY();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, Manager::GetFontSize() * 0.24f});
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {Manager::GetFontSize() * 0.1f, 0.0f});

                    ImGui::SetCursorPos({windowGap + textIndent, oldCursorY + 2.0f + separatorGap});
                    ImGui::Text("Brightness");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##TweaksBrightness", &visuals.filmtweaks.brightness, -1.0f, 1.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Contrast");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##TweaksContrast", &visuals.filmtweaks.contrast, 0.0f, 4.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Desaturation");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##TweaksDesat", &visuals.filmtweaks.desaturation, 0.0f, 1.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Tint Light");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::ColorEdit3("##TweaksLight", glm::value_ptr(visuals.filmtweaks.tintLight));

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Tint Dark");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::ColorEdit3("##TweaksDark", glm::value_ptr(visuals.filmtweaks.tintDark));

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Invert");
                    ImGui::SameLine(size.x - checkboxIndent);
                    modified |= ImGui::Checkbox("##TweaksInvert", &visuals.filmtweaks.invert);

                    ImGui::PopStyleVar(2);
                }

                if (modified)
                {
                    UpdateFilmtweaks();
                }
            }

            // DOF Section
            if (visuals.filmtweaks.enabled)
            {
                ImGui::SetCursorPos({0.0f, ImGui::GetCursorPosY() + separatorGap});
            }
            else
            {
                ImGui::SetCursorPos({0.0f, filmTextPos.y + filmTextSize.y + separatorGap});
            }

            oldCursorY = ImGui::GetCursorPosY();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            ImGui::PushFont(Manager::GetBoldFont());
            ImVec2 dofTextPos = {
                windowGap,
                oldCursorY + 2.0f + separatorGap,
            };
            ImVec2 dofTextSize = ImGui::CalcTextSize("DOF");
            ImGui::SetCursorPos(dofTextPos);
            ImGui::Text("DOF");
            ImGui::PopFont();
            {
                bool modified = false;

                ImGui::SetCursorPos({dofTextPos.x + dofTextSize.x + Manager::GetFontSize() * 0.5f,
                                     dofTextPos.y + (dofTextSize.y - Manager::GetFontSize()) / 2.0f + Manager::GetFontSize() * 0.05f});
                modified |= ImGui::Checkbox("##DofToggle", &visuals.dof.enabled);

                ImGui::SetCursorPosY(dofTextPos.y + dofTextSize.y + separatorGap);
                oldCursorY = ImGui::GetCursorPosY();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                if (visuals.dof.enabled)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, Manager::GetFontSize() * 0.24f});
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {Manager::GetFontSize() * 0.1f, 0.0f});

                    ImGui::SetCursorPos({windowGap + textIndent, oldCursorY + 2.0f + separatorGap});
                    ImGui::Text("Far Blur");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##FarBlur", &visuals.dof.farBlur, 0.0f, 10.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Far Start");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##FarStart", &visuals.dof.farStart, 0.0f, 5000.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Far End");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##FarEnd", &visuals.dof.farEnd, 0.0f, 10000.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Near Blur");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##NearBlur", &visuals.dof.nearBlur, 0.0f, 10.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Near Start");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##NearStart", &visuals.dof.nearStart, 0.0f, 5000.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Near End");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##NearEnd", &visuals.dof.nearEnd, 0.0f, 5000.0f);

                    ImGui::SetCursorPosX(windowGap + textIndent);
                    ImGui::Text("Bias");
                    ImGui::SameLine(size.x - colorIndent);
                    ImGui::SetNextItemWidth(colorIndent - checkboxIndent + Manager::GetFontSize());
                    modified |= ImGui::SliderFloat("##DofBias", &visuals.dof.bias, 0.1f, 10.0f);

                    ImGui::PopStyleVar(2);
                }

                if (modified)
                {
                    UpdateDof();
                }
            }
        }
        ImGui::End();
    }

    bool* VisualsMenu::GetShowPtr() noexcept
    {
        return &show;
    }
}  // namespace IWXMVM::UI