#include "StdInclude.hpp"
#include "CaptureMenu.hpp"

#include "Components/CaptureManager.hpp"
#include "Components/Rendering.hpp"
#include "Configuration/PreferencesConfiguration.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "UI/Blur.hpp"
#include "UI/UIManager.hpp"
#include "Utilities/PathUtils.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    static bool show = false;

    static std::string folderName;
    static bool multipass = false;
    static std::optional<int32_t> displayPassIndex = std::nullopt;

    void CaptureMenu::Render()
    {
        if (!show)
        {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, Manager::GetFontSize());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

        float width = Manager::GetWindowSizeX() / 5.0f;
        float height = Manager::GetWindowSizeY() / 1.7f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);

        float X = Manager::GetWindowSizeX() / 1.4f - width / 2.0f;
        float Y = Manager::GetWindowSizeY() / 2.0f - height / 2.0f;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

        if (ImGui::Begin("##CaptureMenu", nullptr, flags))
        {
            size = ImGui::GetWindowSize();
            pos = ImGui::GetWindowPos();

            Blur::RenderToWindow(size, pos, ImGui::GetScrollY());

            auto& captureManager = Components::CaptureManager::Get();
            auto& captureSettings = captureManager.GetCaptureSettings();
            auto endTick = Mod::GetGameInterface()->GetDemoInfo().endTick;

            float windowBorder = Manager::GetFontSize() * 1.0f;

            ImGui::PushFont(Manager::GetBoldFont());
            ImGui::SetCursorPos({windowBorder, windowBorder});
            ImGui::Text("Output Directory");
            ImGui::PopFont();

            ImGui::SetCursorPosX(windowBorder);
            if (ImGui::Button(ICON_FA_FOLDER, {Manager::GetFontSize(), Manager::GetFontSize()}))
            {
                auto folder = PathUtils::OpenFolderBrowseDialog();
                if (folder.has_value())
                {
                    LOG_INFO("Set recording output directory to: {}", folder.value().string());
                    PreferencesConfiguration::Get().captureOutputDirectory = folder.value();
                    Configuration::Get().Write(true);
                }
            }
            ImGui::SameLine(windowBorder + Manager::GetFontSize() * 1.25f);
            const auto& outputDirectory = PreferencesConfiguration::Get().captureOutputDirectory;
            ImGui::TextWrapped(outputDirectory.string().c_str());

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + windowBorder * 0.8f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            float optionsGap = Manager::GetFontSize() * 0.2f;
            float textIndent = Manager::GetFontSize() * 0.1f;
            float remainingSpace = size.x - windowBorder * 2.0f - optionsGap * 2.0f;

            float nameSize = remainingSpace / 2.8f;
            float resolutionMenuSize = (remainingSpace - nameSize) / 2.0f;
            float framerateMenuSize = resolutionMenuSize;

            ImGui::PushFont(Manager::GetTQFont());

            float nameTextSize = ImGui::CalcTextSize("Name").x;
            if (multipass)
            {
                nameTextSize = ImGui::CalcTextSize("Directory Name").x;
            }

            float resolutionTextSize = ImGui::CalcTextSize("Resolution").x;
            float framerateTextSize = ImGui::CalcTextSize("Framerate").x;

            ImGui::SetCursorPosX(windowBorder + textIndent);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + windowBorder * 0.45f);
            if (!multipass)
            {
                ImGui::Text("Name");
            }
            else
            {
                ImGui::Text("Directory Name");
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + nameSize + optionsGap + textIndent);
            ImGui::Text("Resolution");

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + nameSize + optionsGap + resolutionMenuSize + optionsGap + textIndent);
            ImGui::Text("Framerate");

            ImGui::PopFont();

            ImGui::SetCursorPosX(windowBorder);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textIndent * 2.0f);
            ImGui::SetNextItemWidth(nameSize);
            ImGui::InputText(
                "##FolderName", &folderName[0], folderName.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData* data) -> int {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        std::string* str = reinterpret_cast<std::string*>(data->UserData);
                        str->resize(data->BufTextLen);
                        data->Buf = &(*str)[0];
                    }
                    return 0;
                },
                &folderName);

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + nameSize + optionsGap);
            ImGui::SetNextItemWidth(resolutionMenuSize);
            if (ImGui::BeginCombo("##captureMenuResolutionCombo", captureSettings.resolution.ToString().c_str()))
            {
                for (auto resolution : captureManager.GetSupportedResolutions())
                {
                    bool isSelected = captureSettings.resolution == resolution;
                    if (ImGui::Selectable(resolution.ToString().c_str(), captureSettings.resolution == resolution))
                    {
                        captureSettings.resolution = resolution;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + nameSize + optionsGap + resolutionMenuSize + optionsGap);
            ImGui::SetNextItemWidth(framerateMenuSize);
            if (ImGui::BeginCombo("##captureMenuFramerateCombo", std::format("{0}", captureSettings.framerate).c_str()))
            {
                for (auto framerate : captureManager.GetSupportedFramerates())
                {
                    bool isSelected = captureSettings.framerate == framerate;
                    if (ImGui::Selectable(std::format("{0}", framerate).c_str(),
                                          captureSettings.framerate == framerate))
                    {
                        captureSettings.framerate = framerate;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            float codecMenuSize = remainingSpace / 2.0f;
            float timeframeSize = (remainingSpace - codecMenuSize) / 2.0f;

            ImGui::PushFont(Manager::GetTQFont());

            float codecTextSize = ImGui::CalcTextSize("Codec").x;
            float startTextSize = ImGui::CalcTextSize("Start Tick").x;
            float endTextSize = ImGui::CalcTextSize("End Tick").x;

            ImGui::SetCursorPosX(windowBorder + textIndent);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Manager::GetTQFontSize() * 0.36f);
            ImGui::Text("Codec");

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + codecMenuSize + optionsGap + textIndent);
            ImGui::Text("Start Tick");

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + codecMenuSize + optionsGap + timeframeSize + optionsGap + textIndent);
            ImGui::Text("End Tick");

            ImGui::PopFont();

            ImGui::SetCursorPosX(windowBorder);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textIndent * 2.0f);
            ImGui::SetNextItemWidth(codecMenuSize);
            if (ImGui::BeginCombo("##captureMenuVideoCodecCombo",
                                  captureManager.GetVideoCodecLabel(captureSettings.videoCodec.value()).data()))
            {
                for (auto videoCodec = 0; videoCodec < static_cast<std::int32_t>(Components::VideoCodec::Count);
                     videoCodec++)
                {
                    Components::VideoCodec codec = static_cast<Components::VideoCodec>(videoCodec);
                    bool isSelected = captureSettings.videoCodec == codec;
                    if (ImGui::Selectable(captureManager.GetVideoCodecLabel(codec).data(),
                                          captureSettings.videoCodec == codec))
                    {
                        captureSettings.videoCodec = codec;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + codecMenuSize + optionsGap);
            ImGui::SetNextItemWidth(timeframeSize);
            ImGui::SliderInt("##Start", reinterpret_cast<std::int32_t*>(&captureSettings.startTick), 0, endTick);
            ImGui::SameLine();
            ImGui::SetCursorPosX(windowBorder + codecMenuSize + optionsGap + timeframeSize + optionsGap);
            ImGui::SetNextItemWidth(timeframeSize);
            ImGui::SliderInt("##End", reinterpret_cast<std::int32_t*>(&captureSettings.endTick), 0, endTick);

            ImGui::SetCursorPosX(windowBorder);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + windowBorder * 0.6f);
            ImGui::Checkbox("Multi-pass Mode", &multipass);

            ImVec2 captureButtonPos = {0.0f, size.y - Manager::GetFontSize() * 1.6f};
            ImVec2 captureButtonSize = {size.x, size.y - captureButtonPos.y};

            if (multipass)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + windowBorder * 0.6f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

                float gapBetweenPasses = windowBorder * 0.8f;

                float vertOffset = ImGui::GetCursorPosY();

                ImGuiWindowFlags childWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
                ImGui::BeginChild("Passes Window", {size.x, size.y - captureButtonSize.y - vertOffset}, 0,
                                  childWindowFlags);

                for (auto it = captureSettings.passes.begin(); it != captureSettings.passes.end(); it++)
                {
                    auto i = std::distance(captureSettings.passes.begin(), it);

                    char passTextID[32] = {0};
                    std::snprintf(passTextID, sizeof(passTextID), "##Pass %d id", i + 1);
                    ImGui::PushID(passTextID);

                    ImGui::SetCursorPosX(windowBorder);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gapBetweenPasses);

                    ImVec2 layerIconSize = ImGui::CalcTextSize(ICON_FA_LAYER_GROUP);
                    ImGui::Text(ICON_FA_LAYER_GROUP);

                    ImGui::SameLine();

                    float elementGap = Manager::GetFontSize() * 0.2f;
                    float buttonWidth = Manager::GetFontSize();

                    char passText[32] = {0};
                    std::snprintf(passText, sizeof(passText), "Pass %d", i + 1);
                    ImGui::SetCursorPosX(windowBorder + layerIconSize.x + elementGap);
                    float inputTextAndComboWidth =
                        size.x - windowBorder * 2.0f - layerIconSize.x - elementGap * 3.0f - buttonWidth * 2.0f;
                    float inputTextWidth = inputTextAndComboWidth * 0.7f;
                    ImGui::SetNextItemWidth(inputTextWidth);
                    ImGui::InputTextWithHint(
                        passTextID, passText, &captureSettings.passes[i].name[0],
                        captureSettings.passes[i].name.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                        [](ImGuiInputTextCallbackData* data) -> int {
                            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                            {
                                std::string* str = reinterpret_cast<std::string*>(data->UserData);
                                str->resize(data->BufTextLen);
                                data->Buf = &(*str)[0];
                            }
                            return 0;
                        },
                        &captureSettings.passes[i].name);

                    float comboWidth = inputTextAndComboWidth - inputTextWidth - elementGap;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + layerIconSize.x + elementGap * 2.0f + inputTextWidth);
                    ImGui::SetNextItemWidth(comboWidth);
                    if (ImGui::BeginCombo("##passTypeCombo", magic_enum::enum_name(it->type).data()))
                    {
                        for (auto p = 0; p < (int)Components::PassType::Count; p++)
                        {
                            bool isSelected = it->type == (Components::PassType)p;
                            if (ImGui::Selectable(magic_enum::enum_name((Components::PassType)p).data(), isSelected))
                            {
                                it->type = (Components::PassType)p;
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + layerIconSize.x + elementGap * 2.0f + inputTextAndComboWidth);

                    if (displayPassIndex.has_value() && displayPassIndex.value() == i)
                    {
                        if (ImGui::Button(ICON_FA_EYE_SLASH, {buttonWidth, buttonWidth}))
                        {
                            displayPassIndex = std::nullopt;

                            // TODO: all of this below could be moved to another wrapper function
                            Components::Rendering::SetRenderingFlags(Types::RenderingFlags_DrawEverything);
                            // TODO: do other stuff necessary for the pass type (depth, normal, set clear color to 0 1 0
                            // for greenscreen, etc)
                            // TODO: hide viewmodel if DrawViewmodel is not set, because we cant do that via our
                            // R_SetMaterial hook
                        }
                    }
                    else if (!displayPassIndex.has_value())
                    {
                        if (ImGui::Button(ICON_FA_EYE, {buttonWidth, buttonWidth}))
                        {
                            displayPassIndex = i;
                            Components::Rendering::SetRenderingFlags(it->renderingFlags);
                        }
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + layerIconSize.x + elementGap * 3.0f + inputTextAndComboWidth +
                                         buttonWidth);
                    if (ImGui::Button(ICON_FA_MINUS, {buttonWidth, buttonWidth}))
                    {
                        captureSettings.passes.erase(it);
                        ImGui::PopID();
                        break;
                    }

                    float checkboxSpace = (size.x - windowBorder * 2.0f) / 4.0f;

                    ImGui::PushFont(Manager::GetTQFont());

                    ImGui::SetCursorPosX(windowBorder);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Manager::GetFontSize() * 0.3f);
                    bool drawPlayers = it->renderingFlags & Types::RenderingFlags_DrawPlayers;
                    if (ImGui::Checkbox("Players", &drawPlayers))
                    {
                        it->renderingFlags =
                            static_cast<Types::RenderingFlags>(it->renderingFlags ^ Types::RenderingFlags_DrawPlayers);
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + checkboxSpace);
                    bool drawWorld = it->renderingFlags & Types::RenderingFlags_DrawWorld;
                    if (ImGui::Checkbox("World", &drawWorld))
                    {
                        it->renderingFlags =
                            static_cast<Types::RenderingFlags>(it->renderingFlags ^ Types::RenderingFlags_DrawWorld);
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + checkboxSpace * 2.0f);
                    bool drawViewmodel = it->renderingFlags & Types::RenderingFlags_DrawViewmodel;
                    if (ImGui::Checkbox("Viewmodel", &drawViewmodel))
                    {
                        it->renderingFlags = static_cast<Types::RenderingFlags>(it->renderingFlags ^
                                                                                Types::RenderingFlags_DrawViewmodel);
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(windowBorder + checkboxSpace * 3.0f);
                    bool drawMuzzleFlash = it->renderingFlags & Types::RenderingFlags_DrawMuzzleFlash;
                    if (ImGui::Checkbox("Muzzle Flash", &drawMuzzleFlash))
                    {
                        it->renderingFlags = static_cast<Types::RenderingFlags>(it->renderingFlags ^
                                                                                Types::RenderingFlags_DrawMuzzleFlash);
                    }

                    ImGui::PopFont();

                    ImGui::PopID();
                }

                ImGui::SetCursorPosX(windowBorder);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gapBetweenPasses);
                if (ImGui::Button(ICON_FA_PLUS, {size.x - windowBorder * 2.0f, Manager::GetFontSize() * 1.1f}))
                {
                    captureSettings.passes.push_back(
                        {Components::PassType::Default, Types::RenderingFlags_DrawEverything});
                }

                ImGui::EndChild();
            }

            ImGui::SetCursorPos(captureButtonPos);
            if (ImGui::Button("Capture", captureButtonSize))
            {
                if (captureManager.IsCapturing())
                    captureManager.StopCapture();
                else
                    captureManager.StartCapture();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    bool* CaptureMenu::GetShowPtr() noexcept
    {
        return &show;
    }
}  // namespace IWXMVM::UI