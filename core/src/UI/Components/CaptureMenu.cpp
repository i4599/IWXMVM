#include "StdInclude.hpp"
#include "CaptureMenu.hpp"

#include "Resources.hpp"
#include "Mod.hpp"
#include "UI/UIManager.hpp"
#include "Components/CaptureManager.hpp"
#include "Components/CameraManager.hpp"
#include "Utilities/PathUtils.hpp"
#include "Configuration/PreferencesConfiguration.hpp"
#include "UI/TaskbarProgress.hpp"

namespace IWXMVM::UI
{
    void CaptureMenu::Initialize()
    {
    }

    void CaptureMenu::Render()
    {
        using namespace Components;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;
        if (ImGui::Begin("Capture", NULL, flags))
        {
            if (Mod::GetGameInterface()->GetGameState() != Types::GameState::InDemo)
            {
                UI::DrawInaccessibleTabWarning();
                ImGui::End();
                return;
            }

            auto& cameraManager = CameraManager::Get();
            auto& captureManager = CaptureManager::Get();
            auto& captureSettings = captureManager.GetCaptureSettings();

            const auto fieldLayoutPercentage = 0.4f;

            ImGui::BeginDisabled(captureManager.IsCapturing());

            ImGui::PushFont(UIManager::Get().GetBoldFont());
            ImGui::Text("Capture Settings");
            ImGui::PopFont();

            auto halfWidth = ImGui::GetWindowWidth() * (1 - fieldLayoutPercentage) - ImGui::GetStyle().WindowPadding.x;
            halfWidth /= 2;
            halfWidth -= ImGui::GetStyle().ItemSpacing.x / 2;
            auto endTick = Mod::GetGameInterface()->GetDemoInfo().endTick;

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Timeframe");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() * fieldLayoutPercentage);
            ImGui::SetNextItemWidth(halfWidth);
            ImGui::DragInt("##startTickInput", (int32_t*)&captureSettings.startTick, 10, 0, captureSettings.endTick);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(halfWidth);
            ImGui::DragInt("##endTickInput", (int32_t*)&captureSettings.endTick, 10, captureSettings.startTick, endTick);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Output Format");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() * fieldLayoutPercentage);
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * (1 - fieldLayoutPercentage) -
                                    ImGui::GetStyle().WindowPadding.x);
            if (ImGui::BeginCombo("##captureMenuOutputFormatCombo",
                                  captureManager.GetOutputFormatLabel(captureSettings.outputFormat).data()))
            {
                for (auto outputFormat = 0; outputFormat < (int)OutputFormat::Count; outputFormat++)
                {
                    bool isSelected = captureSettings.outputFormat == (OutputFormat)outputFormat;
                    if (ImGui::Selectable(captureManager.GetOutputFormatLabel((OutputFormat)outputFormat).data(),
                            captureSettings.outputFormat == (OutputFormat)outputFormat))
                    {
                        captureSettings.outputFormat = (OutputFormat)outputFormat;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (captureSettings.outputFormat == OutputFormat::Video)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Video Codec");
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() * fieldLayoutPercentage);
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * (1 - fieldLayoutPercentage) -
                                                        ImGui::GetStyle().WindowPadding.x);
                if (ImGui::BeginCombo("##captureMenuVideoCodecCombo",
                                        captureManager.GetVideoCodecLabel(captureSettings.videoCodec.value())
                                            .data()))
                {
                    for (auto videoCodec = 0; videoCodec < (int)VideoCodec::Count; videoCodec++)
                    {
                        bool isSelected = captureSettings.videoCodec == (VideoCodec)videoCodec;
                        if (ImGui::Selectable(captureManager.GetVideoCodecLabel((VideoCodec)videoCodec).data(),
                                captureSettings.videoCodec == (VideoCodec)videoCodec))
                        {
                            captureSettings.videoCodec = (VideoCodec)videoCodec;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Resolution");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() * fieldLayoutPercentage);
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * (1 - fieldLayoutPercentage) -
                                    ImGui::GetStyle().WindowPadding.x);
            if (ImGui::BeginCombo("##captureMenuResolutionCombo", captureSettings.resolution.ToString().c_str()))
            {
                for (auto resolution : captureManager.GetSupportedResolutions())
                {
                    bool isSelected = captureSettings.resolution == resolution;
                    if (ImGui::Selectable(resolution.ToString().c_str(),
                                          captureSettings.resolution == resolution))
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

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Framerate");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() * fieldLayoutPercentage);
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * (1 - fieldLayoutPercentage) -
                                    ImGui::GetStyle().WindowPadding.x);
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

            ImGui::EndDisabled();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y * 5);
            auto label = captureManager.IsCapturing() ? ICON_FA_STOP " Stop" : ICON_FA_CIRCLE " Capture";
            if (ImGui::Button(label, ImVec2(ImGui::GetFontSize() * 6, ImGui::GetFontSize() * 2)))
            {
                if (captureManager.IsCapturing())
                    captureManager.StopCapture();
                else
                    captureManager.StartCapture();
            }

            ImGui::SameLine();

            ImGui::BeginDisabled(captureManager.IsCapturing());

            if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse",
                                ImVec2(ImGui::GetFontSize() * 6, ImGui::GetFontSize() * 2)))
            {
                auto folder = PathUtils::OpenFolderBrowseDialog();
                if (folder.has_value())
                {
                    LOG_INFO("Set recording output directory to: {}", folder.value().string());
                    PreferencesConfiguration::Get().captureOutputDirectory = folder.value();
                    Configuration::Get().Write(true); 
                }
            }

            ImGui::EndDisabled();

            if (!captureManager.IsFFmpegPresent())
            {
                ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * 4));
                ImGui::PushStyleColor(ImGuiCol_Text, {249.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 1.0f});
                ImGui::PushFont(UIManager::Get().GetBoldFont());
                ImGui::TextWrapped("Could not find ffmpeg. Please restart the mod through the codmvm launcher!");
                ImGui::PopFont();
                ImGui::PopStyleColor();
            }

            ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * 4));
            ImGui::PushFont(UIManager::Get().GetBoldFont());
            ImGui::Text("Output Directory");
            ImGui::PopFont();
            const auto& outputDirectory = PreferencesConfiguration::Get().captureOutputDirectory;
            ImGui::TextWrapped(outputDirectory.string().c_str());

            if (captureManager.IsCapturing())
            {
                ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * 4));
                ImGui::PushFont(UIManager::Get().GetBoldFont());
                ImGui::Text("Progress");
                ImGui::PopFont();
                ImGui::Text("Captured %d frames", captureManager.GetCapturedFrameCount());
                auto totalFrames = (captureSettings.endTick - captureSettings.startTick) * (captureSettings.framerate/1000.0f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetColorU32(ImGuiCol_Button));
                ImGui::ProgressBar((float)captureManager.GetCapturedFrameCount() / totalFrames, ImVec2(-1, 0), "");
                ImGui::PopStyleColor();

                TaskbarProgress::SetProgressValue((int)captureManager.GetCapturedFrameCount(), totalFrames);
                TaskbarProgress::SetProgressState(TBPF_NORMAL);
            }
            else
            {
                TaskbarProgress::SetProgressState(TBPF_NOPROGRESS);
            }

            ImGui::End();
        }
    }

    void CaptureMenu::Release()
    {
    }
}  // namespace IWXMVM::UI